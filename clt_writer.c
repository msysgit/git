#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <io.h>
#if WIN32
#include <windows.h>
#endif
int write_all(int max_line)
{
	int i;
	int result;
	char buffer[256];
	result = 0;
	for (i = 0; i < max_line; i++)
	{
		int write_size;
		size_t n;
		memset(buffer, 0, 256);
		write_size = sprintf(buffer, "% 4d I got the connection\n",i);
		n = fwrite(buffer, 1, write_size, stdout);
     		if (n < write_size)
		{
			fputs("ERROR writing to socket\n", stderr);
			result = -1;
			break;
		}
	}
 
 	return result;
}


#if WIN32
int write_handle_info(int fd)
{
	HANDLE osh;
	int result;
	osh = (HANDLE)_get_osfhandle(fd);
	if (osh != INVALID_HANDLE_VALUE)
	{
		BOOL bState;
		DWORD dwFlags;
		
		bState = GetHandleInformation(osh, &dwFlags);
		if (bState)
		{
			const char* flag_inherit;
			const char* protect_from_close;
			if (dwFlags & HANDLE_FLAG_INHERIT)
			{
				flag_inherit = "Yes";
			}
			else
			{
				flag_inherit = "No";
			}
			if (dwFlags & HANDLE_FLAG_PROTECT_FROM_CLOSE)
			{
				protect_from_close = "Yes";
			}
			else
			{
				protect_from_close = "No";
			}
			printf("file descripter(%d): \n"
			"Handle Inherit: %s\n"
			"Protect from close: %s\n", 
			fd, flag_inherit, protect_from_close);
			result = 0;
		}	
		else
		{
			result = -1;
		}
	}
	else
	{
		result = -1;
	}
	return result;
}
#else
int write_handle_info(int fd)
{
	return 0;
}

#endif

int write_file_status(FILE *f)
{
	int fd;
	int result;
#if WIN32
	fd = _fileno(f);
#else
	fd = fileno(f);
#endif
	if (fd != -1)
	{
		result = write_handle_info(fd);
	}
	else
	{
		result = -1;
	}
	return result;
}

int main(int argc, char *argv[])
{
	int result;
	int count_of_line;
	if (argc < 2) {
		count_of_line = 10;
	} else {
		count_of_line = atoi(argv[1]);
	}
	result = write_file_status(stdout);
	result = write_all(count_of_line);
	return result;
}

