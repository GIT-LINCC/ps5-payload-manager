#pragma once

#include <stddef.h>

int ps5_elfldr_service_is_running(void);
int ps5_elfldr_service_start(char *msg_buf, size_t msg_buf_size);
int ps5_elfldr_service_stop(char *msg_buf, size_t msg_buf_size);
size_t ps5_elfldr_service_status_json(char *json_buf, size_t buf_size,
                                      int enabled);
