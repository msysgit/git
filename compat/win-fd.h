#ifndef WIN_FD_H
#define WIN_FD_H
typedef struct _win_fd_status win_fd_status;

struct _win_fd_status
{
	int size;
	struct {
		int fd;
		int info;
	} status[1];
};
win_fd_status *win_fd_apply_inheritance_0(int inheritance, int size, 
	const int* fd);

win_fd_status *win_fd_apply_inheritance_1(int inheritance, ...);

void win_fd_restore(win_fd_status *fd_status);

void win_fd_free(win_fd_status *fd_status);

void win_fd_restore_and_free(win_fd_status *fd_status);

char *win_fd_to_string(win_fd_status *fd_status);


#endif
