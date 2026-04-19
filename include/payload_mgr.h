#ifndef PAYLOAD_MGR_H
#define PAYLOAD_MGR_H

#include <stddef.h>

/* 
 * Scans directories and populates json_buf with a JSON array of payloads. 
 * Returns the length of the string written. 
 */
size_t payload_mgr_list_json(char *json_buf, size_t buf_size);
int payload_mgr_resolve_path(const char *filename, char *out_path, size_t out_size);

#endif
