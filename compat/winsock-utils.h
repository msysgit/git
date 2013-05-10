#ifndef WINSOCK_UTIL_H
#define WINSOCK_UTIL_H
#include <time.h>

int winsock_read_reg_time_wait(time_t *time_wait);

int winsock_read_time_wait_from_registry(time_t *time_wait);

int winsock_read_time_wait_from_config(time_t *time_wait);

time_t get_socket_time_wait();


#endif
