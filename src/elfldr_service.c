#include "elfldr_service.h"
#include "notification.h"
#include "pldmgr.h"

#include <stdint.h>
#include <ps5/kernel.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "assets_elfldr_socksrv_elf.h"
#include "elfldr.h"

#define ELFLDR_PROCESS_NAME "elfldr.elf"

int ps5_elfldr_service_is_running(void) {
  return elfldr_find_pid(ELFLDR_PROCESS_NAME) > 0;
}

int ps5_elfldr_service_start(char *msg_buf, size_t msg_buf_size) {
  static char *const argv[] = {ELFLDR_PROCESS_NAME, 0};
  uint8_t caps[16];
  uint64_t authid = 0;
  intptr_t vnode = 0;
  pid_t mypid = getpid();
  pid_t pid;
  int ret = -1;

  if (ps5_elfldr_service_is_running()) {
    snprintf(msg_buf, msg_buf_size, "9021 loader is already running");
    return 0;
  }

  if (elfldr_sanity_check((uint8_t *)assets_elfldr_socksrv_elf,
                          assets_elfldr_socksrv_elf_len) != 0) {
    snprintf(msg_buf, msg_buf_size, "Embedded loader is corrupted");
    return -1;
  }

  pldmgr_log("[PLDMGR] Starting embedded 9021 ELF loader...\n");

  vnode = kernel_get_proc_rootdir(mypid);
  if (!vnode) {
    snprintf(msg_buf, msg_buf_size, "Failed to read process rootdir");
    return -1;
  }

  if (kernel_get_ucred_caps(mypid, caps) != 0) {
    snprintf(msg_buf, msg_buf_size, "Failed to read process caps");
    return -1;
  }

  authid = kernel_get_ucred_authid(mypid);
  if (!authid) {
    snprintf(msg_buf, msg_buf_size, "Failed to read process authid");
    return -1;
  }

  if (elfldr_raise_privileges(mypid) != 0) {
    snprintf(msg_buf, msg_buf_size, "Failed to raise privileges");
  } else {
    signal(SIGCHLD, SIG_IGN);
    pid = elfldr_spawn(-1, argv, (uint8_t *)assets_elfldr_socksrv_elf,
                       assets_elfldr_socksrv_elf_len);
    if (pid > 0) {
      snprintf(msg_buf, msg_buf_size, "9021 loader started (pid %d)",
               (int)pid);
      pldmgr_log("[PLDMGR] Embedded 9021 ELF loader started as PID %d\n",
                 (int)pid);
      ret = 0;
    } else {
      snprintf(msg_buf, msg_buf_size, "Failed to spawn 9021 loader");
    }
  }

  if (kernel_set_proc_jaildir(mypid, vnode) != 0) {
    pldmgr_log("[PLDMGR] !!! Failed to restore process jaildir\n");
    ret = -1;
  }
  if (kernel_set_proc_rootdir(mypid, vnode) != 0) {
    pldmgr_log("[PLDMGR] !!! Failed to restore process rootdir\n");
    ret = -1;
  }
  if (kernel_set_ucred_caps(mypid, caps) != 0) {
    pldmgr_log("[PLDMGR] !!! Failed to restore process caps\n");
    ret = -1;
  }
  if (kernel_set_ucred_authid(mypid, authid) != 0) {
    pldmgr_log("[PLDMGR] !!! Failed to restore process authid\n");
    ret = -1;
  }

  if (ret == 0) {
    pldmgr_notify("Payload Manager\n9021 ELF loader started");
  }

  return ret;
}

int ps5_elfldr_service_stop(char *msg_buf, size_t msg_buf_size) {
  int killed = 0;
  pid_t pid;

  while ((pid = elfldr_find_pid(ELFLDR_PROCESS_NAME)) > 0) {
    pldmgr_log("[PLDMGR] Stopping embedded 9021 ELF loader PID %d\n",
               (int)pid);
    if (kill(pid, SIGKILL) != 0) {
      snprintf(msg_buf, msg_buf_size, "Failed to stop 9021 loader");
      return -1;
    }
    killed++;
    sleep(1);
  }

  if (killed > 0) {
    snprintf(msg_buf, msg_buf_size, "9021 loader stopped");
    pldmgr_notify("Payload Manager\n9021 ELF loader stopped");
  } else {
    snprintf(msg_buf, msg_buf_size, "9021 loader is not running");
  }

  return 0;
}

size_t ps5_elfldr_service_status_json(char *json_buf, size_t buf_size,
                                      int enabled) {
  int running = ps5_elfldr_service_is_running();

  if (!json_buf || buf_size == 0) {
    return 0;
  }

  snprintf(json_buf, buf_size,
           "{\"enabled\":%s,\"running\":%s,\"port\":%d}",
           enabled ? "true" : "false", running ? "true" : "false",
           ELFLDR_PORT);
  return strlen(json_buf);
}
