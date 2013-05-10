#include "cache.h"
#include "win-fd.h"
#include <stdio.h>
int test_1(void)
{
	int result;
	win_fd_status *fd_status_1;
	win_fd_status *fd_status_2;
	char *str_1;
	char *str_2;
	result = 0;
	fd_status_1 = win_fd_apply_inheritance_1(0, 0, 1, 2, -1);
	str_1 = win_fd_to_string(fd_status_1);
	fd_status_2 = win_fd_apply_inheritance_1(1, 0, 1, 2, -1);
	str_2 = win_fd_to_string(fd_status_2);
	fprintf(stdout,	"%s\n", str_1);
	fprintf(stdout, "%s\n", str_2);
	free(str_1);
	free(str_2);
	win_fd_restore(fd_status_2);
	win_fd_restore(fd_status_1);
	win_fd_free(fd_status_2);
	win_fd_free(fd_status_1);


	return result;
}
int main(int argc, char **argv)
{
	int result;
	int i;
	int (*test_funcs[])() = {
		test_1
	};
	result = 0;
	for (i = 0; i < sizeof(test_funcs) / sizeof(test_funcs[0]); i++) {
		result = test_funcs[i]();
		if (result) {
			break;
		}
	}
	return result;
}
