#include "cache.h"
#include <time.h>
#ifdef WIN32
#include "winsock-utils.h"
#endif

#ifdef EMULATE_TIME_WAIT_SOCKET
#ifdef WIN32
static int wait_for_time_out(void)
{
	time_t start_time;
	time_t wait_time;
	start_time = time(NULL);
	wait_time = get_socket_time_wait();
	while (time(NULL) - start_time < wait_time) {
		sleep(1);	
	}
	return 0;
}
#endif
#else
static int wait_for_time_out(void)
{
	return 0;
}
#endif
int main(int argc, char** argv)
{
	int result;
	
	result = wait_for_time_out();
	return result;
}
