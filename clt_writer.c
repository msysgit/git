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


int main(int argc, char *argv[])
{
	int result;
	int count_of_line;
	if (argc < 2) {
		count_of_line = 10;
	} else {
		count_of_line = atoi(argv[1]);
	}
	result = write_all(count_of_line);
	return result;
}

