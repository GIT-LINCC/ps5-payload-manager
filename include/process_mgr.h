#ifndef PROCESS_MGR_H
#define PROCESS_MGR_H

#include <stddef.h>
#include <sys/types.h>

/* Build a JSON list of all running processes.
 * Returns complete bytes written, or 0 on query/record/serialization failure. */
size_t process_list_json(char *buf, size_t max_size);

/* Match the process name used by startup, excluding self/nonpositive PIDs.
 * Returns a matching PID, or -1 on no match/query/record failure. */
pid_t process_find_pid(const char *name);

/* Kill a process by PID. Returns 0 on success, -1 on failure. */
int process_kill(int pid);

/* Build a JSON list of running processes excluding self. */
size_t ps5_running_processes_json(char *json_buf, size_t buf_size);

/* Stop running payload processes matching filename (excluding manager/elfldr). */
int ps5_stop_payload_by_filename(const char *filename, char *msg_buf,
                                 size_t msg_buf_size);

#endif
