#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "next_menu.h"
#include "autoload.h"
#include "payload_mgr.h"
#include "ps5_launcher.h"

static volatile int abort_flag = 0;
static pthread_t autoload_thread;

void* nm_autoload_worker(void* arg) {
    nm_log("[Autoload] Checking for %s...\n", AUTOLOAD_CONFIG_PATH);
    
    struct stat st;
    if (stat(AUTOLOAD_CONFIG_PATH, &st) != 0) {
        /* Silently skip if no config exists */
        return NULL;
    }

    nm_log("[Autoload] Config found. Starting 10s countdown...\n");
    nm_notify("Autoload in 10s...\nVisit /abort to cancel");

    for (int i = 10; i > 0; i--) {
        if (abort_flag) {
            nm_log("[Autoload] ABORTED by user during countdown.\n");
            nm_notify("Autoload Cancelled");
            return NULL;
        }
        sleep(1);
    }

    FILE *f = fopen(AUTOLOAD_CONFIG_PATH, "r");
    if (!f) {
        nm_log("[Autoload] !!! Failed to open %s\n", AUTOLOAD_CONFIG_PATH);
        return NULL;
    }

    nm_log("[Autoload] Starting sequence...\n");
    nm_notify("Autoload: Starting sequence");

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (abort_flag) {
            nm_log("[Autoload] ABORTED by user during execution.\n");
            nm_notify("Autoload Aborted");
            break;
        }

        /* Strip newline characters */
        line[strcspn(line, "\r\n")] = 0;
        
        /* Skip empty lines or comments if we wanted them, but user specified format */
        if (strlen(line) == 0) continue;

        if (line[0] == '!') {
            /* Delay command: !2000 means 2000ms */
            int delay = atoi(line + 1);
            if (delay > 0) {
                nm_log("[Autoload] Delaying for %d ms...\n", delay);
                /* usleep takes microseconds */
                usleep(delay * 1000);
            }
        } else {
            /* Payload command: search for the ELF in scan directories */
            char full_path[512];
            if (payload_mgr_resolve_path(line, full_path, sizeof(full_path)) == 0) {
                nm_log("[Autoload] Launching: %s\n", full_path);
                nm_notify("Autoload: Launching %s", line);
                
                /* Give a small delay before launch so notification can be seen */
                usleep(500000); 
                
                ps5_launch_elf(full_path);
                
                /* Give a small delay after launch to ensure elfldr handles it */
                sleep(1);
            } else {
                nm_log("[Autoload] !!! Payload not found: %s\n", line);
            }
        }
    }

    fclose(f);
    nm_log("[Autoload] Sequence complete.\n");
    return NULL;
}

int nm_autoload_start() {
    abort_flag = 0;
    if (pthread_create(&autoload_thread, NULL, nm_autoload_worker, NULL) != 0) {
        nm_log("[Autoload] !!! Failed to create background thread\n");
        return -1;
    }
    pthread_detach(autoload_thread);
    return 0;
}

void nm_autoload_abort() {
    abort_flag = 1;
}
