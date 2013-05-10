#include "cache.h"
#include "winsock-proc.h"
#include <winsock2.h>
#include <io.h>
#include "env-utils.h"
#include "argv-array.h"
#include "run-command.h"
#include "socket-utils.h"
#include "win-fd.h"
#ifdef WIN32
#define fd_to_socket(fd) ((SOCKET)_get_osfhandle(fd))
#else
#define fd_to_socket(fd) (fd)
#endif

static winsock_proc *
winsock_proc_alloc();

char **
winsock_proc_prepare_env_for_socket(int fdin, int fdout, 
	const char * const *env);

static char **
winsock_copy_argv(const char * const *argv);

static void
winsock_free_argv(char **argv);

static int 
winsock_proc_send_sockinfo(int pid, int child_in, int socket_fd);

static void
winsock_proc_socket_once_init();

static void winsock_proc_socket_once_init(void)
{
	mingw_ensure_socket_initialization();
}

static winsock_proc *
winsock_proc_alloc()
{
	winsock_proc *result;
	struct child_process *cp;
	result = NULL;
	cp = (struct child_process *)xmalloc(sizeof(*cp));
	if (cp) {
		memset(cp, 0, sizeof(*cp));
		result = (winsock_proc *)xmalloc(sizeof(*result));
		if (result) {
			result->child_process = cp;
		}
		else {
			free(cp);
		}
	}
	return result;
}


winsock_proc *
winsock_proc_start_cmd(const winsock_proc_cmd *cmd)
{
	winsock_proc *result;
	char **argv;
	char **env;
	char *dir;
	int state;
	result = winsock_proc_alloc();
	
	state = result ? 0 : -1;
	if (state == 0) {	
		env = winsock_proc_prepare_env_for_socket(
			cmd->socket_in_fd, 
			cmd->socket_out_fd, 
			(const char * const *)cmd->env);
		state = env ? 0 : -1;
	}
	else {
		env = NULL;	
	}
	if (state == 0) {
		if (cmd->dir) {
			dir = xstrdup(cmd->dir);
		}
		else {
			dir = NULL;
		}
	}
	else {
		dir = NULL;
	}

	if (state == 0) {
		argv = winsock_copy_argv((const char * const *) cmd->argv);
		state = argv ? 0 : -1;
	}
	else {
		argv = NULL;
	}

	if (state == 0) {
		win_fd_status *fd_status;
		result->child_process->env = (const char **)env;
		result->child_process->argv = (const char **)argv;
		result->child_process->in = -1;
		result->child_process->out = cmd->initial_out_fd;
		result->child_process->err = cmd->initial_err_fd;
		result->child_process->git_cmd = cmd->git_cmd;
		result->child_process->dir = dir;
		fd_status = win_fd_apply_inheritance_1(0, 0, -1);
		state = start_command(result->child_process);
		win_fd_restore_and_free(fd_status);
		env = NULL;
		argv = NULL;
		dir = NULL;
	}
	if (state == 0) {
		state = winsock_proc_send_sockinfo(
			result->child_process->pid, 
			result->child_process->in,
			cmd->socket_fd);

		if (state) {
			kill(result->child_process->pid, SIGTERM);
		}	

	}
	if (state) {
		winsock_proc_free(result);
		result = NULL;
	}
	if (argv) {
		winsock_free_argv(argv);
	}

	if (env) {
		env_buffer_free_env((char **)env);
	}
	if (dir) {
		free(dir);
	}

	return result;	
}

struct child_process *
winsock_proc_get_process_info(winsock_proc *proc)
{
	struct child_process *result;
	result = NULL;
	if (proc) {
		result = proc->child_process;
	}
	return result;
}

void
winsock_proc_free(winsock_proc *proc)
{
	if (proc) {
		if (proc->child_process) {
			if (proc->child_process->argv) {
				winsock_free_argv(
					(char **)proc->child_process->argv);
			}
			if (proc->child_process->env) {
				env_buffer_free_env(
					(char **)proc->child_process->env);
			}
			if (proc->child_process->dir) {
				free((void *)proc->child_process->dir);
			}
			free(proc->child_process);
		}
		free(proc);
	}
}

int
winsock_proc_get_in(winsock_proc *proc)
{
	int result;
	result = -1;
	if (proc) {
		if (proc->child_process) {
			result = proc->child_process->in;
		}
	}
	return result;
}

int
winsock_proc_get_out(winsock_proc *proc)
{
	int result;
	result = -1;
	if (proc) {
		if (proc->child_process) {
			result = proc->child_process->out;
		}
	}
	return result;
}

int
winsock_proc_get_err(winsock_proc *proc)
{
	int result;
	result = -1;
	if (proc) {
		if (proc->child_process) {
			result = proc->child_process->err;
		}
	}
	return result;
}

char **
winsock_proc_prepare_env_for_socket(int fdin, int fdout, 
	const char * const *env)
{
	env_buffer env_buf;
	char str_buffer[200];
	char **result;
	memset(&env_buf, 0, sizeof(env_buf));
	env_buffer_init(&env_buf, env);
	snprintf(str_buffer, sizeof(str_buffer), 
		"_WIN_SOCK_IO={%d, %d}", fdin, fdout);
	env_buffer_add(&env_buf, str_buffer);
	result = env_buffer_copy_env(&env_buf);
	env_buffer_close(&env_buf);
	return result;
}

int 
winsock_proc_send_sockinfo(int pid, int child_in, int socket_fd)
{
	int result;
	WSAPROTOCOL_INFO pi;
	result = 0;
	if (WSADuplicateSocket(fd_to_socket(socket_fd),
		pid, &pi) == 0) {
		ssize_t ws;
		ws = xwrite(child_in, &pi, sizeof(pi));
		if (ws == sizeof(pi)) {
			result = 0;
		}
		else {
			errno = SOCK_UTIL_PROTOCOL_ERROR;
			result = -1;
		}
			
	}
	else {
		result = -1;
		errno = WSAGetLastError();
	}
	return result;
}

void
winproc_decode_fd(char *str, int fd[])
{
	int result;
	char *token;
	fd[0] = fd[1] = -1;
	result = 0;
	token = strpbrk(str, " \t");
	token = strtok(str, "{");
	if (token) {
		char *end_ptr;
		long val;
		token = strtok(token, ",");
		val = strtol(token, &end_ptr, 10);
		if (end_ptr != token) {
			fd[0] = (int)val;
		}
		token = strtok(NULL, "}");
		if (token) {
			val = strtol(token, &end_ptr, 10);
			if (end_ptr != token) {
				fd[1] = (int)val;
			}
		}
	}
}

int
winproc_read_protocol_info(int fd, WSAPROTOCOL_INFO *pi)
{
	ssize_t rs;
	int result;
	rs = xread(fd, pi, sizeof(*pi));
	if (rs == sizeof(*pi)) {
		result = 0;
	}
	else {
		result = -1;
		errno = SOCK_UTIL_PROTOCOL_ERROR;
	}
	return result;
}

int 
winproc_setup_io_care_socket(void)
{
	int result;
	char *sock_info;
	sock_info = getenv("_WIN_SOCK_IO");	

	if (sock_info) {
		WSAPROTOCOL_INFO pi;
		SOCKET sock;
		int fd[2] = { -1, -1 };
		result = 0;
		sock_info = xstrdup(sock_info);	
		winproc_decode_fd(sock_info, fd);
		free(sock_info);
		result = winproc_read_protocol_info(0, &pi);
		if (result == 0) {
			winsock_proc_socket_once_init();
			sock = WSASocket(FROM_PROTOCOL_INFO, 
				FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, 
				&pi, 0, 0);
			if (sock == INVALID_SOCKET) {
				errno = WSAGetLastError();	
				result = -1;
			}
		} else {
			sock = INVALID_SOCKET;
		}
		
		if (result == 0) {
			int sock_fd = _open_osfhandle(sock, 0);	
			if (fd[0] != -1) {
				dup2(sock_fd, fd[0]);
			}
			if (fd[1] != -1) {
				dup2(sock_fd, fd[1]);
			}
			close(sock_fd);
		}
	}
	else
	{
		result = 0;	
	}
	return result;
}

static char **
winsock_copy_argv(const char * const *argv)
{
	char **result;
	result = NULL;
	if (argv) {
		struct argv_array aa;
		int i;
		argv_array_init(&aa);
		for (i = 0; argv[i]; i++) {
			argv_array_push(&aa, argv[i]);
		}
		result = (char **)argv_array_detach(&aa, NULL);
	}
	return result;		
}

static void
winsock_free_argv(char **argv)
{
	if (argv) {
		argv_array_free_detached((const char **)argv);
	}
}


