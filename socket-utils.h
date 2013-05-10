#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#define SOCK_UTIL_PROTOCOL_ERROR -100

extern const char * socket_utils_strerror(int errnum);

extern int is_socket(int fd);

extern void set_socket_to_time_wait(int fd, int fd_is_out);

#endif
