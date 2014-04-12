/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <fcntl.h>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fcntl.h>

static void bzero(void *address, int length)
{
	memset(address, 0, length);
}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <io.h>
#endif
void error(const char *fmt, ...)
{
    static char buffer[512];
    va_list a_list;
    va_start(a_list, fmt);
    vsprintf(buffer, fmt, a_list);
    perror(buffer);
    va_end(a_list);
    exit(1);
}


#ifdef WIN32
#undef socket
static int mingw_socket(int domain, int type, int protocol)
{
	int sockfd;
	SOCKET s;

	s = WSASocket(domain, type, protocol, NULL, 0, 0);
	if (s == INVALID_SOCKET) {
		/*
		 * WSAGetLastError() values are regular BSD error codes
		 * biased by WSABASEERR.
		 * However, strerror() does not know about networking
		 * specific errors, which are values beginning at 38 or so.
		 * Therefore, we choose to leave the biased error code
		 * in errno so that _if_ someone looks up the code somewhere,
		 * then it is at least the number that are usually listed.
		 */
		errno = WSAGetLastError();
		return -1;
	}
	/* convert into a file descriptor */
	if ((sockfd = _open_osfhandle(s, O_RDWR|O_BINARY)) < 0) {
		closesocket(s);
		fprintf(stderr, "unable to make a socket file descriptor: %s",
			strerror(errno));
		return -1;
	}
	return sockfd;
}
#define socket mingw_socket
#undef bind
static int mingw_bind(int sockfd, struct sockaddr *sa, size_t sz)
{
	SOCKET s = (SOCKET)_get_osfhandle(sockfd);
	return bind(s, sa, sz);
}
#define bind mingw_bind

#undef listen
int mingw_listen(int sockfd, int backlog)
{
	SOCKET s = (SOCKET)_get_osfhandle(sockfd);
	return listen(s, backlog);
}
#define listen mingw_listen
#undef accept
int mingw_accept(int sockfd1, struct sockaddr *sa, socklen_t *sz)
{
	int sockfd2;

	SOCKET s1 = (SOCKET)_get_osfhandle(sockfd1);
	SOCKET s2 = accept(s1, sa, sz);

	/* convert into a file descriptor */
	if ((sockfd2 = _open_osfhandle(s2, O_RDWR|O_BINARY)) < 0) {
		int err = errno;
		closesocket(s2);
		error("unable to make a socket file descriptor: %s",
			strerror(err));
	}
	if (s2)
	{
		BOOL bState;
		DWORD dwFlags;
		
		bState = GetHandleInformation((HANDLE)s2, &dwFlags);
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
			printf("accept socket handle info: \n"
			"Handle Inherit: %s\n"
			"Protect from close: %s\n", flag_inherit, protect_from_close);
		}
	}
	return sockfd2;
}
#define accept mingw_accept

int win_start_process(const char* exename, char * args, int exestdin, int exestdout, int exestderr)
{
	int result;
	STARTUPINFO si;
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	{
		struct {
			int fd;
			HANDLE fall_back_hdl;
			HANDLE *si_hdl;
		} ioe[] = {
			{
				exestdin, 
				GetStdHandle(STD_INPUT_HANDLE),
				&si.hStdInput
			},
			{ 
				exestdout,
				GetStdHandle(STD_OUTPUT_HANDLE),
				&si.hStdOutput
			},
			{ 
				exestderr, 
				GetStdHandle(STD_ERROR_HANDLE),
				&si.hStdError
			},

		};
		int i;
		for (i = 0; i < sizeof(ioe) / sizeof(ioe[0]); i++) {
			if (ioe[i].fd < 0) {
				*ioe[i].si_hdl = ioe[i].fall_back_hdl;
			}
			else {
				HANDLE hdl;
				hdl = (HANDLE)_get_osfhandle(ioe[i].fd);
				if (hdl != INVALID_HANDLE_VALUE) {
					*ioe[i].si_hdl = hdl;
				}
				else {
					*ioe[i].si_hdl = ioe[i].fall_back_hdl;
				}
			}
		}
	}
	
	{
		PROCESS_INFORMATION pi;
		BOOL state;
		state = CreateProcess(exename, args, NULL, NULL, TRUE, 0, 
			NULL, 
			NULL,
			&si,
			&pi);

		result = state ? 0 : -1;
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	return result;
}
#endif
int write_all_to_socket_with_process(int sockfd, int max_line)
{
	char buffer[128];
	int result;
	memset(buffer, 0, sizeof(buffer));
	sprintf(buffer, " %d", max_line);
	result = win_start_process("clt_writer.exe", buffer, -1, sockfd, -1);
	return result;
}

int write_all_to_socket_in_process(int sockfd, int max_line)
{
	int i;
	int result;
	char buffer[256];
	result = 0;
	for (i = 0; i < max_line; i++)
	{
		int write_size;
		int n;
		bzero(buffer,256);
		write_size = sprintf(buffer, "% 4d I got the connection\n",i);
		n = write(sockfd, buffer, write_size);
     		if (n < 0)
		{
			error("ERROR writing to socket");
			result = -1;
			break;
		}
	}
 
 	return result;
}

int write_all_to_socket(int sockfd, int line_count, int in_process)
{
	int result;
	if (in_process)
	{
		result = write_all_to_socket_in_process(sockfd, line_count);
	}
	else
	{
		result = write_all_to_socket_with_process(sockfd, line_count);
	}
	return result; 
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno;
     socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
     int n;
     int line_count;
     int in_proc;
     line_count = 10;
     in_proc = 1;
     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     {
     	int i;
	int parse_mode;
	const static int PM_NORMAL = 0;
	const static int PM_LINE = 1;
	const static int PM_PROC = 2;
	parse_mode = PM_NORMAL;
	for (i = 0; i < argc; i++)
	{
		if (parse_mode == PM_NORMAL)
		{
			if (strcmp(argv[i], "-l") == 0)
			{
				parse_mode = PM_LINE;
			}
			else if (strcmp(argv[i], "-p") == 0)
			{
				in_proc = 0;	
			}
		}
		else if (parse_mode == PM_LINE)
		{
			line_count = atoi(argv[i]);
			parse_mode = PM_NORMAL;
		}
	}
     }

#ifdef WIN32
     WSADATA wsa;
     if (WSAStartup(MAKEWORD(2,2), &wsa)) error("WSAStartup");
#endif
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     listen(sockfd,5);
     clilen = sizeof(cli_addr);
     newsockfd = accept(sockfd, 
                 (struct sockaddr *) &cli_addr, 
                 &clilen);
     if (newsockfd < 0) 
          error("ERROR on accept");
     write_all_to_socket(newsockfd, line_count, in_proc);
     close(newsockfd);
     close(sockfd);
#ifdef WIN32
     WSACleanup();
#endif
     return 0; 
}

