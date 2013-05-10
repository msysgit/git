#include "cache.h"
#include "win-fd.h"

#include <windows.h>
#include <io.h>
#include <stdarg.h>
#include <stddef.h>

int
win_fd_status_count_fd(va_list args);

int
win_fd_status_arg_to_array(va_list args, int *size, int **fd_array);


win_fd_status*
win_fd_status_alloc(int size);

win_fd_status*
win_fd_status_create(int size, const int* fd_array);

win_fd_status *win_fd_apply_status_0(DWORD flag_bits, DWORD flag, 
	int size, const int* fd);

int
win_fd_status_count_fd(va_list args)
{
	va_list args_wk;
	int result;
	int val;
	result = 0;
	va_copy(args_wk, args);
	val = va_arg(args_wk, int);
	while (val >= 0)
	{
		result++;
		val = va_arg(args_wk, int);	
	}
	va_end(args_wk);
	return result;
}

win_fd_status*
win_fd_status_alloc(int size)
{
	win_fd_status *result;
	size_t status_size;
	size_t header_size;
	header_size = offsetof(win_fd_status, status);
	status_size = sizeof(win_fd_status) - header_size;

	result = (win_fd_status *)xmalloc(header_size + status_size * size);
	if (result)
	{
		result->size = size;
		memset(&result->status[0], 0, status_size * size);
	}
	return result;
}

int
win_fd_status_arg_to_array(va_list args, int *size, int **fd_array)
{
	int num_of_fd;
	va_list args_wk;
	int* fd_array_wk;
	int result;
	va_copy(args_wk, args);
	num_of_fd = win_fd_status_count_fd(args_wk);

	if (num_of_fd) {
		fd_array_wk = (int *)xmalloc(sizeof(int) * num_of_fd);
		if (fd_array_wk) {
			int i;
			for (i = 0; i < num_of_fd; i++) {
				fd_array_wk[i] = va_arg(args_wk, int);	
			}
			result = 0;
		}
		else {
			result = -1;
		}

	}
	else {
		fd_array_wk = NULL;
		result = 0;
	}
	if (result == 0)
	{
		*size = num_of_fd;
		*fd_array = fd_array_wk;
	}
	
	va_end(args_wk);

	return result;
}

win_fd_status*
win_fd_status_create(int size, const int* fd_array)
{
	win_fd_status *result;
	result = win_fd_status_alloc(size);
	if (result) {
		int i;
		for (i = 0; i < size; i++) {
			HANDLE hdl;
			hdl = (HANDLE)_get_osfhandle(fd_array[i]);
			if (hdl != INVALID_HANDLE_VALUE) {
				DWORD flags;
				if (GetHandleInformation(hdl, &flags)) {
					result->status[i].fd = fd_array[i];
					result->status[i].info = flags;	
				}
				else {
					result->status[i].fd = -1;
				}
			}
			else {
				result->status[i].fd = -1;
			}
		}
	}
	return result;
}

win_fd_status *win_fd_apply_inheritance_0(int inheritance, 
	int size, const int* fd)
{
	win_fd_status *result;
	result = win_fd_apply_status_0(HANDLE_FLAG_INHERIT,
		inheritance ? HANDLE_FLAG_INHERIT : 0,
		size, fd);

	return result;
}

win_fd_status *win_fd_apply_inheritance_1(int inheritance, ...)
{
	int size;
	va_list args;
	int *fd_array;
	int state;
	win_fd_status *result;
	va_start(args, inheritance);
	state = win_fd_status_arg_to_array(args, &size, &fd_array);
	if (state == 0) {
		result = win_fd_apply_inheritance_0(inheritance, 
			size, fd_array); 
		free(fd_array);
	}
	else {
		result = NULL;
	}
	va_end(args);
	return result;
}

win_fd_status *win_fd_apply_status_0(DWORD flag_bits, DWORD flag, 
	int size, const int* fd)
{
	win_fd_status *result;
	
	result = win_fd_status_create(size, fd);
	if (result) {
		int i;
		for (i = 0; i < size; i++) {
			if (result->status[i].fd >= 0) {
				HANDLE hdl;
				hdl = (HANDLE)_get_osfhandle(result->status[i].fd);
				if (hdl != INVALID_HANDLE_VALUE) {
					SetHandleInformation(hdl, flag_bits,
	 					flag);
				}
			}	
		}
		
	}
	return result;
}

void win_fd_restore(win_fd_status *fd_status)
{
	if (fd_status) {
		int i;
		for (i = 0; i < fd_status->size; i++) {
			if (fd_status->status[i].fd >= 0) {
				HANDLE hdl;
				hdl = (HANDLE)_get_osfhandle(fd_status->status[i].fd);
				if (hdl != INVALID_HANDLE_VALUE) {
					SetHandleInformation(hdl, 
						HANDLE_FLAG_INHERIT | HANDLE_FLAG_PROTECT_FROM_CLOSE, 
						fd_status->status[i].info);
				}
			}
		}
	
	}
}

void win_fd_free(win_fd_status *fd_status)
{
	if (fd_status) {
		free(fd_status);
	}
}
void win_fd_restore_and_free(win_fd_status *fd_status)
{
	win_fd_restore(fd_status);
	win_fd_free(fd_status);
}

char *win_fd_status_to_string(int fd, int info)
{
	struct strbuf strb;
	
	strbuf_init(&strb, 10); 

	strbuf_addf(&strb, "[fd = %d, info = %#X]", fd, info);
	return strbuf_detach(&strb, NULL);
}

char *win_fd_to_string(win_fd_status *fd_status)
{
	char *result;
	if (fd_status) {
		int i;	
		struct strbuf stb;
		strbuf_init(&stb, 10); 
		for (i = 0; i < fd_status->size; i++) {
			char *info_str;
			info_str = win_fd_status_to_string(
				fd_status->status[i].fd,
				fd_status->status[i].info);
			strbuf_addstr(&stb, info_str);
			free(info_str);
			if (i < fd_status->size - 1) {
				strbuf_addstr(&stb, ", ");
			}
		}
		result = strbuf_detach(&stb, NULL);
	}
	else {
		result = NULL;
	}
	return result;
}


