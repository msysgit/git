#ifndef WINSOCK_PROC_H
#define WINSOCK_PROC_H
#include "run-command.h"

typedef struct _winsock_proc winsock_proc;
typedef struct _winsock_proc_cmd winsock_proc_cmd;

struct _winsock_proc {
	struct child_process * child_process;
};
struct _winsock_proc_cmd {
	const char * const * env;
	const char * const * argv;
	int socket_in_fd; /* input file descripter on target process */
	int socket_out_fd; /* out file descripter on target process */
	int close_in_fd; 
	/* close input file descripter when start command. */
	/* on starting time, input file descripter is opened as pipe.*/
	int initial_out_fd; /* how to use stdout on target process */
	int initial_err_fd; /* how to use stderr on target process */
	int git_cmd;
	int socket_fd;
	const char *dir;
};

winsock_proc *
winsock_proc_start_cmd(const winsock_proc_cmd *cmd);

struct child_process *
winsock_proc_get_process_info(winsock_proc *proc);

void
winsock_proc_free(winsock_proc *proc);

int
winsock_proc_get_in(winsock_proc *proc);

int
winsock_proc_get_out(winsock_proc *proc);

int
winsock_proc_get_err(winsock_proc *proc);

int 
winproc_setup_io_care_socket();

#endif

