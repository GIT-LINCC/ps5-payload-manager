#include "ps5_launcher.h"
#include "pldmgr.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <stdint.h>
#include <sys/sysctl.h>
#include <sys/user.h>

static int connect_elfldr(void) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket");
    return -1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(ELFLDR_PORT);
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    pldmgr_log("[PLDMGR] !!! Connection to elfldr (port %d) failed. Is it "
           "running?\n",
           ELFLDR_PORT);
    close(sock);
    return -1;
  }

  return sock;
}

static int send_all(int sock, const void *data, size_t size) {
  const char *ptr = (const char *)data;
  size_t sent_total = 0;

  while (sent_total < size) {
    ssize_t sent = send(sock, ptr + sent_total, size - sent_total, 0);
    if (sent <= 0) {
      return -1;
    }
    sent_total += (size_t)sent;
  }

  return 0;
}

static int ps5_send_file_uri_to_loader(const char *path) {
  char uri[640];
  char response[128];
  struct timeval timeout;
  int sock;

  snprintf(uri, sizeof(uri), "file:%s\n", path);
  sock = connect_elfldr();
  if (sock < 0) {
    return -1;
  }

  pldmgr_log("[PLDMGR] Sending file URI to local loader: %s\n", path);
  if (send_all(sock, uri, strlen(uri)) != 0) {
    close(sock);
    return -1;
  }

  shutdown(sock, SHUT_WR);

  timeout.tv_sec = 0;
  timeout.tv_usec = 350000;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  ssize_t n = recv(sock, response, sizeof(response) - 1, 0);
  close(sock);

  if (n > 0) {
    response[n] = '\0';
    if ((strstr(response, "[elfldr.elf]") ||
         strstr(response, "Unknown payload format")) &&
        (strstr(response, "Error") || strstr(response, "Unknown") ||
         strstr(response, "error"))) {
      pldmgr_log("[PLDMGR] file URI launch rejected by elfldr: %s\n",
                 response);
      return -1;
    }
  }

  return 0;
}

static int ps5_send_elf_bytes_to_loader(const char *path) {
  pldmgr_log("[PLDMGR] Sending raw ELF to local loader: %s\n", path);

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    pldmgr_log("[PLDMGR] !!! Failed to open payload: %s\n", path);
    return -1;
  }

  struct stat st;
  fstat(fd, &st);
  size_t total_sent = 0;

  int sock = connect_elfldr();
  if (sock < 0) {
    close(fd);
    return -1;
  }

  char buffer[8192];
  ssize_t read_bytes;
  while ((read_bytes = read(fd, buffer, sizeof(buffer))) > 0) {
    ssize_t sent = send(sock, buffer, read_bytes, 0);
    if (sent < 0) {
      perror("send");
      break;
    }
    total_sent += sent;
  }

  pldmgr_log("[PLDMGR] Sent %zu bytes to loader.\n", total_sent);

  close(sock);
  close(fd);
  return 0;
}

int ps5_launch_elf(const char *path) {
  if (ps5_send_file_uri_to_loader(path) == 0) {
    return 0;
  }

  pldmgr_log("[PLDMGR] Falling back to raw ELF streaming for: %s\n", path);
  return ps5_send_elf_bytes_to_loader(path);
}

static const char *path_basename(const char *path) {
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

  strncpy(file_base, filename, sizeof(file_base) - 1);
  file_base[sizeof(file_base) - 1] = '\0';
  strip_payload_extension(file_base);

  strncpy(proc_base, process_name, sizeof(proc_base) - 1);
  proc_base[sizeof(proc_base) - 1] = '\0';
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

static int scan_processes(int (*on_process)(pid_t pid, const char *name,
                                            void *ctx),
                          void *ctx) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
  size_t buf_size;
  void *buf;

  if (sysctl(mib, 4, NULL, &buf_size, NULL, 0)) {
    return -1;
  }

  buf = malloc(buf_size);
  if (!buf) {
    return -1;
  }

  if (sysctl(mib, 4, buf, &buf_size, NULL, 0)) {
    free(buf);
    return -1;
  }

  for (char *ptr = (char *)buf; ptr < ((char *)buf + buf_size);) {
    struct kinfo_proc *ki = (struct kinfo_proc *)ptr;
    if (ki->ki_structsize < sizeof(struct kinfo_proc)) {
      break;
    }
    if (on_process(ki->ki_pid, ki->ki_comm, ctx) != 0) {
      break;
    }
    ptr += ki->ki_structsize;
  }

  free(buf);
  return 0;
}

struct ProcessJsonContext {
  char *json;
  size_t size;
  size_t pos;
  int first;
};

static int append_process_json(pid_t pid, const char *name, void *ctx) {
  struct ProcessJsonContext *pj = (struct ProcessJsonContext *)ctx;
  char escaped[128];
  int written;

  if (pid == getpid() || !name || name[0] == '\0') {
    return 0;
  }

  pldmgr_json_escape(name, escaped, sizeof(escaped));
  written = snprintf(pj->json + pj->pos, pj->size - pj->pos,
                     "%s{\"pid\":%d,\"name\":\"%s\"}",
                     pj->first ? "" : ",", (int)pid, escaped);
  if (written < 0 || (size_t)written >= pj->size - pj->pos) {
    pj->pos = pj->size - 1;
    return 1;
  }

  pj->pos += (size_t)written;
  pj->first = 0;
  return 0;
}

size_t ps5_running_processes_json(char *json_buf, size_t buf_size) {
  struct ProcessJsonContext ctx;
  int written;

  if (!json_buf || buf_size == 0) {
    return 0;
  }

  ctx.json = json_buf;
  ctx.size = buf_size;
  ctx.pos = 0;
  ctx.first = 1;

  written = snprintf(json_buf, buf_size, "{\"processes\":[");
  if (written < 0 || (size_t)written >= buf_size) {
    json_buf[0] = '\0';
    return 0;
  }
  ctx.pos = (size_t)written;

  if (scan_processes(append_process_json, &ctx) != 0) {
    snprintf(json_buf, buf_size, "{\"processes\":[],\"error\":\"scan_failed\"}");
    return strlen(json_buf);
  }

  snprintf(json_buf + ctx.pos, buf_size - ctx.pos, "]}");
  return strlen(json_buf);
}

struct StopPayloadContext {
  const char *filename;
  int matched;
  int killed;
  int failed;
};

static int stop_matching_payload(pid_t pid, const char *name, void *ctx) {
  struct StopPayloadContext *sp = (struct StopPayloadContext *)ctx;

  if (pid == getpid() || !process_name_matches_payload(name, sp->filename)) {
    return 0;
  }

  sp->matched++;
  pldmgr_log("[PLDMGR] Stopping payload process %s (PID: %d)\n", name,
             (int)pid);
  if (kill(pid, SIGKILL) == 0) {
    sp->killed++;
  } else {
    sp->failed++;
    pldmgr_log("[PLDMGR] !!! Failed to stop PID %d: errno %d\n", (int)pid,
               errno);
  }

  return 0;
}

int ps5_stop_payload_by_filename(const char *filename, char *msg_buf,
                                 size_t msg_buf_size) {
  struct StopPayloadContext ctx;
  const char *safe_filename = path_basename(filename);

  if (!safe_filename || safe_filename[0] == '\0' ||
      is_manager_process_name(safe_filename)) {
    snprintf(msg_buf, msg_buf_size, "Invalid payload filename");
    return -1;
  }

  ctx.filename = safe_filename;
  ctx.matched = 0;
  ctx.killed = 0;
  ctx.failed = 0;

  if (scan_processes(stop_matching_payload, &ctx) != 0) {
    snprintf(msg_buf, msg_buf_size, "Failed to scan running processes");
    return -1;
  }

  if (ctx.killed > 0) {
    snprintf(msg_buf, msg_buf_size, "Stopped %d process%s", ctx.killed,
             ctx.killed == 1 ? "" : "es");
    return 0;
  }

  if (ctx.matched > 0 && ctx.failed > 0) {
    snprintf(msg_buf, msg_buf_size, "Matched process but kill failed");
  } else {
    snprintf(msg_buf, msg_buf_size, "No matching running process");
  }
  return -1;
}

extern int sceSystemServiceLaunchWebBrowser(const char *uri);

int ps5_launch_browser(const char *uri) {
  pldmgr_log("[PLDMGR] Launching browser: %s\n", uri);
  if (sceSystemServiceLaunchWebBrowser(uri) != 0) {
    pldmgr_notify("[PLDMGR] !!! Failed to launch browser.");
    return -1;
  }
  return 0;
}

/* LncUtil Externs */
extern int sceLncUtilGetAppIdOfRunningBigApp();
extern int sceLncUtilGetAppTitleId(uint32_t app_id, char *title_id);
extern int sceLncUtilSuspendApp(uint32_t app_id);
extern int sceLncUtilKillApp(uint32_t app_id);

static pid_t get_pid_by_name(const char *process_name) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
  size_t buf_size;

  if (sysctl(mib, 4, NULL, &buf_size, NULL, 0))
    return -1;

  void *buf = malloc(buf_size);
  if (!buf)
    return -1;

  if (sysctl(mib, 4, buf, &buf_size, NULL, 0)) {
    free(buf);
    return -1;
  }

  pid_t pid = -1;
  for (void *ptr = buf; ptr < (buf + buf_size);
       ptr += ((struct kinfo_proc *)ptr)->ki_structsize) {
    struct kinfo_proc *ki = (struct kinfo_proc *)ptr;
    if (ki->ki_structsize < sizeof(struct kinfo_proc))
      break;
    if (strncmp(ki->ki_comm, process_name, sizeof(ki->ki_comm)) == 0) {
      pid = ki->ki_pid;
      break;
    }
  }

  free(buf);
  return pid;
}

int ps5_kill_disc_player() {
  const char *target_process = "SceDiscPlayer";
  const char *target_title_id = "NPXS40140";

  uint32_t app_id = sceLncUtilGetAppIdOfRunningBigApp();
  if (app_id == 0xffffffff) {
    return 0; /* No app running */
  }

  char title_id[16] = {0};
  if (sceLncUtilGetAppTitleId(app_id, title_id) != 0) {
    return 0; /* Could not get title ID */
  }

  if (strcmp(title_id, target_title_id) != 0) {
    return 0; /* Not the disc player */
  }

  pldmgr_notify("Disc Player detected. Terminating...\n");

  /* 1. Suspend */
  if (sceLncUtilSuspendApp(app_id) != 0) {
    pldmgr_notify("Failed to suspend Disc Player");
    return -1;
  }
  pldmgr_notify("Disc Player Suspended\nWaiting for Home Screen...");

  /* Wait for home screen transition stability */
  sleep(5);
  pldmgr_notify("Killing Disc Player in 3 seconds...");
  sleep(3);

  /* 2. SIGKILL */
  pid_t pid = get_pid_by_name(target_process);
  if (pid != -1) {
    pldmgr_log("Sending SIGKILL to %s (PID: %d)\n", target_process, pid);
    if (kill(pid, SIGKILL) == 0) {
      pldmgr_log("SIGKILL Sent to Disc Player");
    } else {
      pldmgr_notify("Warning: SIGKILL Failed");
    }
    sleep(1);
  }

  /* 3. LncUtil Kill */
  int result = sceLncUtilKillApp(app_id);
  if (result == 0) {
    pldmgr_log("Disc Player Fully Closed");
  } else {
    /* Check if it's already gone (crashed/closed) */
    if (sceLncUtilGetAppIdOfRunningBigApp() == 0xffffffff) {
      pldmgr_log("Disc Player Closed");
    } else {
      pldmgr_notify("!!! Failed to kill Disc Player (result: %d)", result);
      return -1;
    }
  }

  return 0;
}
