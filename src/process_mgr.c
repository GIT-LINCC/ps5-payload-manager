#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdint.h>
#include <errno.h>
#include "process_records.h"

#include "process_mgr.h"
#include "json_helpers.h"
#include "pldmgr.h"

#define MiB(x) ((x) / (1024.0 * 1024))

typedef struct app_info {
    uint32_t app_id;
    uint64_t unknown1;
    char     title_id[14];
    char     unknown2[0x3c];
} app_info_t;

extern int sceKernelGetAppInfo(pid_t pid, app_info_t *info);

static int is_user_daemon(const char *name, uint32_t app_id) {
    if (!name) return 0;

    /* Specifically exclude mini-syscore.elf */
    if (strcmp(name, "mini-syscore.elf") == 0) return 0;

    /* Must have app_id == 0000 */
    if (app_id != 0) return 0;

    const char *ext = strrchr(name, '.');
    if (ext && strcasecmp(ext, ".elf") == 0) return 1;

    return 0;
}

/* Query/fetch can race process creation. Retry ENOMEM a bounded number of
 * times, never parse a returned length beyond the allocation, and validate the
 * entire snapshot before exposing entries or returning a PID to startup. */
#define PROCESS_SNAPSHOT_LIMIT (16u * 1024u * 1024u)
static int process_snapshot(unsigned char **out, size_t *length) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
    *out = NULL;
    *length = 0;
    for (int attempt = 0; attempt < 3; attempt++) {
        size_t capacity = 0;
        if (sysctl(mib, 4, NULL, &capacity, NULL, 0) != 0 ||
            capacity > PROCESS_SNAPSHOT_LIMIT) return -1;
        if (!capacity) return 0;
        unsigned char *data = malloc(capacity);
        if (!data) return -1;
        size_t used = capacity;
        int result = sysctl(mib, 4, data, &used, NULL, 0);
        int saved_errno = errno;
        if (result != 0 || used > capacity) {
            free(data);
            if (result != 0 && saved_errno == ENOMEM) continue;
            return -1;
        }
        const unsigned char *cursor = data;
        size_t remaining = used;
        ProcessRecord record;
        int next;
        while ((next = process_record_next(&cursor, &remaining, &record)) > 0) {}
        if (next < 0) { free(data); return -1; }
        *out = data;
        *length = used;
        return 0;
    }
    return -1;
}

pid_t process_find_pid(const char *name) {
    if (!name) return -1;
    unsigned char *data;
    size_t remaining;
    if (process_snapshot(&data, &remaining) != 0) {
        pldmgr_log("[PLDMGR] Process snapshot failed\n");
        return -1;
    }
    const unsigned char *cursor = data;
    ProcessRecord record;
    pid_t found = -1, self = getpid();
    while (process_record_next(&cursor, &remaining, &record) > 0) {
        if (record.pid > 0 && record.pid != self && !strcmp(name, record.name)) found = record.pid;
    }
    free(data);
    return found;
}

size_t process_list_json(char *buf, size_t max_size) {
    if (!buf || !max_size) return 0;
    buf[0] = '\0';
    unsigned char *data;
    size_t remaining;
    if (process_snapshot(&data, &remaining) != 0) return 0;
    JsonListBuilder jb = { buf, max_size, 0, 1, 0 };
    json_append(&jb, "{\"processes\":[\n");
    const unsigned char *cursor = data;
    ProcessRecord record;
    int count = 0;
    while (process_record_next(&cursor, &remaining, &record) > 0) {
        app_info_t appinfo = {0};
        if (sceKernelGetAppInfo(record.pid, &appinfo)) memset(&appinfo, 0, sizeof(appinfo));
        int is_daemon = is_user_daemon(record.name, appinfo.app_id);
        char name_e[6 * sizeof(record.name)];
        json_escape_checked(&jb, record.name, name_e, sizeof(name_e));
        /* Convert before multiplying, avoiding signed page-count overflow. */
        double mem_mib = MiB((double)record.rss_pages * PAGE_SIZE);
        if (json_append(&jb, "%s  {\"pid\":%d,\"name\":\"%s\",\"memory\":%.1f,\"is_daemon\":%s}",
                        count ? ",\n" : "", (int)record.pid, name_e, mem_mib,
                        is_daemon ? "true" : "false") != 0) break;
        count++;
    }
    free(data);
    json_append(&jb, "\n]}\n");
    return jb.pos;
}

int process_kill(int pid) {
    if (pid <= 0) return -1; /* Prevent killing kernel or init */

    /* Prevent killing our own process */
    if (pid == getpid()) return -1;

    if (kill(pid, SIGTERM) != 0) {
        if (errno == ESRCH) return -1;
        return kill(pid, SIGKILL) == 0 ? 0 : -1;
    }

    for (int i = 0; i < 30; i++) {
        if (kill(pid, 0) != 0) {
            if (errno == ESRCH) {
                return 0;
            }
            break;
        }
        usleep(50000);
    }

    if (kill(pid, 0) != 0 && errno == ESRCH) {
        return 0;
    }

    if (kill(pid, SIGKILL) == 0 || errno == ESRCH) {
        return 0;
    }
    return -1;
}

static const char *payload_path_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int is_manager_process_name(const char *name) {
    if (!name || name[0] == '\0') {
        return 1;
    }
    return strncasecmp(name, "pldmgr", 6) == 0 ||
           strcasecmp(name, "elfldr.elf") == 0 ||
           strcasecmp(name, "payload-manager") == 0;
}

static void strip_payload_extension(char *name) {
    char *dot = strrchr(name, '.');
    if (dot && (strcasecmp(dot, ".elf") == 0 || strcasecmp(dot, ".bin") == 0 ||
                strcasecmp(dot, ".lua") == 0)) {
        *dot = '\0';
    }
}

static int process_name_matches_payload(const char *process_name,
                                        const char *filename) {
    char file_base[256];
    char proc_base[256];
    size_t proc_len;

    if (!process_name || !filename || is_manager_process_name(process_name)) {
        return 0;
    }

    if (strcasecmp(process_name, filename) == 0) {
        return 1;
    }

    snprintf(file_base, sizeof(file_base), "%s", filename);
    strip_payload_extension(file_base);

    snprintf(proc_base, sizeof(proc_base), "%s", process_name);
    strip_payload_extension(proc_base);

    if (strcasecmp(proc_base, file_base) == 0) {
        return 1;
    }

    proc_len = strlen(process_name);
    if (proc_len >= 8 && strncasecmp(filename, process_name, proc_len) == 0) {
        return 1;
    }

    proc_len = strlen(proc_base);
    if (proc_len >= 8 && strncasecmp(file_base, proc_base, proc_len) == 0) {
        return 1;
    }

    return 0;
}

size_t ps5_running_processes_json(char *json_buf, size_t buf_size) {
    if (!json_buf || !buf_size) return 0;
    json_buf[0] = '\0';

    unsigned char *data;
    size_t remaining;
    if (process_snapshot(&data, &remaining) != 0) {
        int written = snprintf(json_buf, buf_size, "{\"processes\":[],\"error\":\"scan_failed\"}");
        return (written > 0 && (size_t)written < buf_size) ? (size_t)written : 0;
    }

    JsonListBuilder jb = { json_buf, buf_size, 0, 1, 0 };
    json_append(&jb, "{\"processes\":[");

    const unsigned char *cursor = data;
    ProcessRecord record;
    pid_t self = getpid();
    int count = 0;

    while (process_record_next(&cursor, &remaining, &record) > 0) {
        if (record.pid <= 0 || record.pid == self || record.name[0] == '\0') {
            continue;
        }
        char name_e[6 * sizeof(record.name)];
        json_escape_checked(&jb, record.name, name_e, sizeof(name_e));
        if (json_append(&jb, "%s{\"pid\":%d,\"name\":\"%s\"}",
                        count ? "," : "", (int)record.pid, name_e) != 0) {
            break;
        }
        count++;
    }

    free(data);
    json_append(&jb, "]}");
    return jb.pos;
}

int ps5_stop_payload_by_filename(const char *filename, char *msg_buf,
                                 size_t msg_buf_size) {
    const char *safe_filename = filename ? payload_path_basename(filename) : NULL;

    if (!safe_filename || safe_filename[0] == '\0' ||
        is_manager_process_name(safe_filename)) {
        if (msg_buf && msg_buf_size) {
            snprintf(msg_buf, msg_buf_size, "Invalid payload filename");
        }
        return -1;
    }

    unsigned char *data;
    size_t remaining;
    if (process_snapshot(&data, &remaining) != 0) {
        if (msg_buf && msg_buf_size) {
            snprintf(msg_buf, msg_buf_size, "Failed to scan running processes");
        }
        return -1;
    }

    const unsigned char *cursor = data;
    ProcessRecord record;
    pid_t self = getpid();
    int matched = 0;
    int killed = 0;
    int failed = 0;

    while (process_record_next(&cursor, &remaining, &record) > 0) {
        if (record.pid <= 0 || record.pid == self ||
            !process_name_matches_payload(record.name, safe_filename)) {
            continue;
        }
        matched++;
        pldmgr_log("[PLDMGR] Stopping payload process %s (PID: %d)\n",
                   record.name, (int)record.pid);
        if (process_kill((int)record.pid) == 0) {
            killed++;
        } else {
            failed++;
            pldmgr_log("[PLDMGR] !!! Failed to stop PID %d: errno %d\n",
                       (int)record.pid, errno);
        }
    }

    free(data);

    if (killed > 0) {
        if (msg_buf && msg_buf_size) {
            snprintf(msg_buf, msg_buf_size, "Stopped %d process%s", killed,
                     killed == 1 ? "" : "es");
        }
        return 0;
    }

    if (msg_buf && msg_buf_size) {
        if (matched > 0 && failed > 0) {
            snprintf(msg_buf, msg_buf_size, "Matched process but kill failed");
        } else {
            snprintf(msg_buf, msg_buf_size, "No matching running process");
        }
    }
    return -1;
}
