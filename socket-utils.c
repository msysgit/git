#include "cache.h"
#include "exec_cmd.h"
#include "socket-utils.h"
#include "run-command.h"
#ifndef WIN32
#else
#include "win-fd.h"
#include <io.h>
#endif

#ifdef WIN32
#define fd_to_socket(fd) ((SOCKET)_get_osfhandle(fd))
#else
#define fd_to_socket(fd) (fd)
#endif

#ifndef WIN32
int
is_socket(int fd)
{
	int result;
	const static char * func_name = "is_socket"; 
	struct stat st;
	if (fstat(fd, &st) < 0) {
		warning("failed to stat in %s: %s",
				func_name, strerror(errno));
		result = 0;
	}
	else {
		result = (st.st_mode & S_IFSOCK) == S_IFSOCK;
	}
	return result;
}
#else
int
is_socket(int fd)
{
	int st;
	int result;
	int optlen;
	optlen = sizeof(st);

	if (getsockopt(fd_to_socket(fd), SOL_SOCKET, SO_TYPE, (char *)&st, &optlen)) {
		result = 0;	
	} else {
		result = WSAGetLastError() == 0;
	}
	return result;
}

#endif
#ifdef EMULATE_TIME_WAIT_SOCKET
void set_socket_to_time_wait(int fd, int fd_is_out)
{
	struct linger lg;
	int state;
	int do_time_wait;
	int size;
	size = sizeof(lg);
	do_time_wait = 1;
	state = getsockopt(fd_to_socket(fd), 
		SOL_SOCKET, SO_LINGER, (char *)&lg, &size);
	if (state == 0)
	{
		do_time_wait = lg.l_onoff == 0;
	}
	if (do_time_wait)
	{
		struct child_process cmd;
		win_fd_status *fd_status;
		int fd_in;
		int fd_out;
		const char *argv[] = {
			"close-socket",
			NULL
		};
		if (fd_is_out)
		{
			fd_out = fd;
			fd_in = 0;
		}
		else
		{
			fd_in = fd;
			fd_out = 0;
		}
		memset(&cmd, 0, sizeof(cmd));
		cmd.argv = argv;
		cmd.in = fd_in;
		cmd.out = fd_out;
		cmd.git_cmd = 1;
		cmd.dir = git_exec_path();
		fd_status = win_fd_apply_inheritance_1(0, 2, -1);
		start_command(&cmd);
		win_fd_restore_and_free(fd_status);
	}
}

#else
void set_socket_to_time_wait(int fd, int fd_is_out)
{
}
#endif
const char * socket_utils_strerror_0(int errnum)
{
	const static struct 
	{
		int number;
		const char *message;
	} number_message[] = {
		{
			SOCK_UTIL_PROTOCOL_ERROR,
			"protocol error"
		}
	};
	int i;
	const char *result;
	result = NULL;
	for (i = 0; i < sizeof(number_message) / sizeof(number_message[0]);
		i++) {
		if (errnum == number_message[i].number) {
			result = number_message[i].message;
			break;
		}
	}
	return result;
}
#if WIN32
const char * socket_utils_strerror_win(int errnum)
{
	static char message_buffer[1024];
	int state;
	const char *result;
	state = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errnum, 0, 
		message_buffer, sizeof(message_buffer), NULL);
	result = state ? message_buffer : NULL;
	return result;
}
#endif

const char * socket_utils_strerror(int errnum)
{
	const char *(*strerror_funcs[])(int) = {
		socket_utils_strerror_0,
#if WIN32
		socket_utils_strerror_win
#endif
	};
	int i;
	const char *result;
	result = NULL;
	for (i = 0; i < sizeof(strerror_funcs) / sizeof(strerror_funcs[0]);
		i++) {
		result = strerror_funcs[i](errnum);
		if (result)
		{
			break;
		}
	}
	return result;
}


