#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include <memory.h>
#include <stdio.h>

typedef int (*Function)(int , int , int* , int* );

extern int run_ftp(int port, Function fp) {
	int listenfd, connfd, maxfd;
	struct sockaddr_in addr;
	int sock_number;
	int sock_array[20];
	int user_flag[20], pass_flag[20];
	fd_set fds;
	struct timeval timeout = {3, 0};
	int i, return_value;
	
	char welcome[] = "220 FTP server ready.\r\n";

	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return STATUS_ERROR;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	printf("%d\n", (int)addr.sin_port);
	
	if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return STATUS_ERROR;
	}

	if (listen(listenfd, 10) == -1) {
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return STATUS_ERROR;
	}
	
	sock_number = 0;
	sock_array[sock_number++] = listenfd;
	maxfd = listenfd;
	
	while (1) {
		FD_ZERO(&fds);
		
		for (i = 0; i < sock_number; ++i) {
			if (sock_array[i] > maxfd) {
				maxfd = sock_array[i];
			}
			FD_SET(sock_array[i], &fds);
		}
		
		return_value = select(maxfd + 1, &fds, NULL, NULL, (struct timeval*)&timeout);
		if (return_value < 0) {
			printf("Error select!\n");
			return STATUS_ERROR;
		} else if (return_value == 0) {
			continue;
		}
		
		if (FD_ISSET(listenfd, &fds)) {
			if ((connfd = accept(listenfd, NULL, NULL)) == -1) {
				printf("Error accept(): %s(%d)\n", strerror(errno), errno);
				return STATUS_ERROR;
			}
			
			/* Send welcome message */
			send(connfd, welcome, strlen(welcome), 0);
			
			user_flag[sock_number] = 0;
			pass_flag[sock_number] = 0;
			sock_array[sock_number++] = connfd;
		}
		
		for (i = 1; i < sock_number; ++i) {
			if (FD_ISSET(sock_array[i], &fds)) {
				int return_value = fp(sock_array[i], i, &user_flag[i], &pass_flag[i]);
				if (return_value == STATUS_ERROR) {
					continue;
				} else if (return_value == STATUS_QUIT) {
					FD_CLR(sock_array[i], &fds);
					close(sock_array[i]);
					printf("%d is closed.\n", sock_array[i]);
					sock_array[i] = sock_array[--sock_number];
					user_flag[i] = user_flag[sock_number];
					pass_flag[i] = pass_flag[sock_number];
					continue;
				}
			}
		}
	}

	close(listenfd);
	
	return 0;
}

