#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/**
 * Launch an ELF payload from the filesystem.
 * @param path Full path to the .elf or .bin file.
 * @return 0 on success, -1 on failure.
 */
int ps5_launch_elf(const char* path);
int ps5_launch_self_update(const char *path);

/* Stop callback is checked before socket work and between send/wait chunks.
 * Connect and total transfer have finite deadlines. Cancellation cannot undo
 * bytes already delivered or a payload the loader has already executed. */
typedef int (*PayloadLaunchCancelled)(void *context);
int ps5_launch_elf_cancellable(const char *path, PayloadLaunchCancelled cancelled, void *context);

/**
 * Builds a JSON array of visible running processes.
 * @return Number of bytes written to json_buf.
 */
size_t ps5_running_processes_json(char *json_buf, size_t buf_size);

/**
 * Stop a running payload process whose process name matches filename.
 * @return 0 if at least one process was killed, -1 if none matched or on error.
 */
int ps5_stop_payload_by_filename(const char *filename, char *msg_buf,
                                 size_t msg_buf_size);

/**
 * Launch the PS5 web browser with a specific URL.
 * @param uri The URL to open.
 * @return 0 on success, -1 on failure.
 */
int ps5_launch_browser(const char* uri);

/**
 * Kill the "Disc Player" app (NPXS40140) if it is running.
 * @return 0 if not running or successfully killed, -1 on error.
 */
int ps5_kill_disc_player(void);

