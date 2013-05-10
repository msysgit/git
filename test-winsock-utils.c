#include "cache.h"
#include "winsock-utils.h"
#include <stdio.h>

int test_1(void)
{
	int result;
	int state;
	time_t time_wait;
	char *value_str;
	state = winsock_read_reg_time_wait(&time_wait);

	if (state) {
		int state_2;
		state_2 = winsock_read_time_wait_from_registry(&time_wait);
		result = state - state_2;
	} else {
		result = 0;
	}
	if (state == 0) {
		static char buffer[512];
		snprintf(buffer, sizeof(buffer), "%d", (int)time_wait);
		value_str = buffer; 
	} else {
		value_str = "Not available";
	}
	printf("TIME_WAIT from registry : %s\n", value_str);
	return result;

}
int test_2(void)
{
	int result;
	int state;
	time_t time_wait;
	char *value_str;
	state = winsock_read_time_wait_from_config(&time_wait);

	result = 0;
	if (state == 0) {
		static char buffer[512];
		snprintf(buffer, sizeof(buffer), "%d", (int)time_wait);
		value_str = buffer; 
	} else {
		value_str = "Not available";
	}
	printf("TIME_WAIT from git-config : %s\n", value_str);
	return result;

}

int test_3(void)
{
	int result;
	result = 0;
	
	printf("TIME_WAIT : %d\n", (int)get_socket_time_wait());
	return result;

}


int main(int argc, char **argv)
{
	int (*test_func[])() = {
		test_1,
		test_2,
		test_3
	};
	int i;
	int result;
	result = 0;
	for (i = 0; i < sizeof(test_func) / sizeof(test_func[0]); i++) {
		result = test_func[i]();
		if (result) {
			break;
		}
	}
	return result;
}

