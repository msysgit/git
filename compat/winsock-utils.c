#include "cache.h"
#include "winsock-utils.h"
#include "run-command.h"
#include <windows.h>

const static int DEFAULT_TIME_WAIT = 240;


int winsock_read_reg_time_wait(time_t *time_wait)
{
	HKEY hk;
	LONG state;
	int result;
	hk = NULL;
	state = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
			L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\"
			L"Parameters",
			0,
			KEY_READ,
			&hk);
	if (state == ERROR_SUCCESS) {
		DWORD data_type;
		DWORD value;
		DWORD value_size;
		value_size = sizeof(value);
		state = RegQueryValueExW(hk, L"TcpTimedWaitDelay", NULL, 
			&data_type, (LPBYTE)&value, &value_size);	
		if (state == ERROR_SUCCESS && data_type == REG_DWORD) {
			*time_wait = (time_t)value;
			result = 0;
		}
		else {
			result = -1;
		}
		RegCloseKey(hk);
	} else {
		result = -1;
	}
	return result;
}

int winsock_read_time_wait_from_registry(time_t *time_wait)
{
	int result;
	result = winsock_read_reg_time_wait(time_wait);
	if (result == 0)
	{
		*time_wait /= 2;
		result = -1;
	}
	return result;
}
int read_int(int fd, int *value)
{
	char buffer[512];
	ssize_t size_read;
	int result;
	size_read = xread(fd, buffer, sizeof(buffer));	
	if (size_read) {
		char *end_ptr;
		long l_value;
		l_value = strtol(buffer, &end_ptr, 10);	
		if (buffer != end_ptr) {
			*value = l_value;
			result = 0;
		}
		else {
			result = -1;
		}
	}
	else {
		result = -1;
	}
	return result;
}

int winsock_read_time_wait_from_config(time_t *time_wait)
{
	int result;

	struct child_process cld;
	char* argv[] = {
		"config",
		"--int",
		"win.sock.time.wait",
		NULL
	};
	memset(&cld, 0, sizeof(cld)); 
	cld.argv = (const char**)argv;
	cld.out = -1;
	cld.git_cmd = 1;
	result = start_command(&cld);
	if (result == 0) {
		int int_value;
		result = read_int(cld.out, &int_value);	
		if (result == 0) {
			*time_wait = (time_t)int_value;
		}
		finish_command(&cld);
	}	
	
	return result;
}

time_t get_socket_time_wait()
{

	time_t result;
	int i;
	int (*read_time_wait[])(time_t *) = {
		winsock_read_time_wait_from_config,
		winsock_read_time_wait_from_registry
	};
	for (i = 0; i < sizeof(read_time_wait) / sizeof(read_time_wait[0]); 
		i++) {
		if (read_time_wait[i](&result) == 0) {
			break;
		}
	}
	if (i == sizeof(read_time_wait) / sizeof(read_time_wait[0])) {
		result = (time_t)DEFAULT_TIME_WAIT;
	}
	return result;
}

