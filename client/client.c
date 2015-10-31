#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>

#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "globals.h"
#include "utilities.h"

char* waiting_for(int sockfd, char* target);
int send_file(int work_sockfd, int sockfd, char* fileName);
int recv_file(int work_sockfd, int sockfd, char* fileName);
int port_open_port(int work_sockfd, int port, int* listenfd);
int pasv_handler(int work_sockfd, char* ip, int port, char* fileName, int retr_stor);
int port_handler(int work_sockfd, int listenfd, int port, char* fileName, int retr_stor);

int main(int argc, char **argv) {
	int sockfd;
	struct sockaddr_in addr;
	char sentence[8192];
	int len;
	char ip[1024];
	int port[2];
	char** tmp_str;
	char command[8192], arguments[8192];
	int port_pasv = -1;
	int port_listenfd;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = 21;
	if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0) {
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		printf("Error connect(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	
	waiting_for(sockfd, "220 FTP server ready.\r\n");
	
	while (1) {
		char tmp_sentence[8192];
	
		fgets(sentence, 4096, stdin);
		len = strlen(sentence);
		
		if (sentence[0] == '\n') {
			continue;
		}
		
		sentence[len - 1] = '\0';
		strcpy(tmp_sentence, sentence);
		tmp_str = split(tmp_sentence, " ", 2);
		strcpy(command, tmp_str[0]);
		strcpy(arguments, tmp_str[1]);
		
		if (strcmp(command, "STOR") == 0) {
			char filePath[] = "./";
			strcat(filePath, arguments);
			FILE *tmp_fp = fopen(filePath, "rb");
			if (tmp_fp == NULL) {
				printf("No such file!\n");
				continue;
			}
		} else if (strcmp(command, "PORT") == 0) {
			char tmp_str[128];
			sprintf(tmp_str, "(%s)", arguments);
			extract_ip_port(tmp_str, ip, port);
			if (port_open_port(sockfd, port[0] * 256 + port[1], &port_listenfd) == 1) {
				continue;
			}
		}
			
		if (send(sockfd, sentence, len, 0) == -1) {
			printf("Error send()\n");
		}
			
		if (strcmp(command, "RETR") == 0
			|| strcmp(command, "STOR") == 0) {
			int retr_stor;
			char filePath[256] = "./";
			strcat(filePath, arguments);
			
			if (strcmp(command, "RETR") == 0) {
				retr_stor = 0;
			} else {
				retr_stor = 1;
			}
			
			if (port_pasv == 0) {
				port_handler(sockfd, port_listenfd, port[0] * 256 + port[1], filePath, retr_stor);
			} else if (port_pasv == 1) {
				pasv_handler(sockfd, ip, port[0] * 256 + port[1], filePath, retr_stor);
			} else {
				waiting_for(sockfd, "");
			}
			continue;
		} else if (strcmp(command, "PASV") == 0) {
			port_pasv = 1;
			char *tmp = waiting_for(sockfd, "");
			extract_ip_port(tmp, ip, port);
			continue;
		} else if (strcmp(command, "PORT") == 0) {
			port_pasv = 0;
			waiting_for(sockfd, "");
			continue;
		}
		
		if ((len = recv(sockfd, sentence, 8191, 0)) == -1) {
			printf("Error recv()\n");
		}
		
		if (len > 0) {
			sentence[len] = '\0';
			printf("%s", sentence);
		}
		
		if (sentence[0] == '2' && sentence[1] == '2' && sentence[2] == '1') {
			break;
		}
	}

	close(sockfd);

	return 0;
}

char* waiting_for(int sockfd, char* target) {
	int len;
	char* sentence = (char*)malloc(8192 * sizeof(char));
	int flag;
	
	if (target[0] == '\0') {
		flag = 0;
	} else {
		flag = 1;
	}
	
	while (1) {
		if ((len = recv(sockfd, sentence, 8191, 0)) == -1) {
			printf("Error recv()\n");
		}
		
		if (len == 0) {
			continue;
		}
		
		int i;
		for (i = 0; i < len; ++i) {
			printf("%c", sentence[i]);
		}
		
		if ((flag == 1 && strcmp(sentence, target) == 0) || flag == 0) {
			return sentence;
		}
	}
}

int send_file(int work_sockfd, int sockfd, char* fileName) {
	char buffer[8192];
	FILE *fp;
	int len;
	
	bzero(buffer, 8192);
	if ((fp = fopen(fileName, "rb")) == NULL) {
		printf("fopen() error!\n");
		close(sockfd);
		return 1;
	}
	
	char *tmp_str = waiting_for(work_sockfd, "");
	char num[16];
	strcpy(num, split(tmp_str, " ", 2)[0]);
	if (strcmp(num, "150") != 0) {
		fclose(fp);
		close(sockfd);
		return 1;
	}
	sprintf(tmp_str, "(%d bytes).\r\n", get_file_size(fileName));
	printf("%s", tmp_str);
	
	len = 0;
	while((len = fread(buffer, 1, 8192, fp)) > 0) {
		int send_len;
		if ((send_len = send(sockfd, buffer, len, 0)) < 0) {
			printf("Error send(): %s(%d)\n", strerror(errno), errno);
		}
		bzero(buffer, 8192);
	}
	
	fclose(fp);
	close(sockfd);
	
	waiting_for(work_sockfd, "");
	
	return STATUS_OK;
}

int recv_file(int work_sockfd, int sockfd, char* fileName) {
	char buffer[8192];
	int recv_len, write_len;
	FILE *fp;
	
	bzero(buffer, 8192);
	if ((fp = fopen(fileName, "wb")) == NULL) {
		printf("fopen() error!\n");
		return 1;
	}
	
	while(recv_len = recv(sockfd, buffer, 8192, 0)) {
		write_len = fwrite(buffer, sizeof(char), recv_len, fp);
		if (write_len > recv_len) {
			printf("file write failed.\n");
			waiting_for(work_sockfd, "");
			fclose(fp);
			return 1;
		}
		bzero(buffer, 8192);
	}
	waiting_for(work_sockfd, "");
	
	fclose(fp);
	
	return 0;
}

int port_open_port(int work_sockfd, int port, int* listenfd) {
	struct sockaddr_in addr;
	
	if ((*listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (bind(*listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		close(*listenfd);
		return 1;	
	}
	
	if (listen(*listenfd, 10) == -1) {
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		close(*listenfd);
		return 1;	
	}
	
	return 0;
}

int pasv_handler(int work_sockfd, char* ip, int port, char* fileName, int retr_stor) {
	int sockfd;
	struct sockaddr_in addr;
	int ok_flag;
	
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		ok_flag = 1;
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
		ok_flag = 1;
	}
	
	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		printf("Error connect(): %s(%d)\n", strerror(errno), errno);
		ok_flag = 1;
	}
	
	if (ok_flag == 1) {
		close(sockfd);
		waiting_for(work_sockfd, "");
		return 1;
	}
	
	if (retr_stor == 0) {
		char *tmp_str = waiting_for(work_sockfd, "");
		char num[256];
		strcpy(num, split(tmp_str, " ", 2)[0]);
		if (strcmp(num, "150") != 0) {
			return 1;
		}
		
		if (recv_file(work_sockfd, sockfd, fileName) == 1) {
			close(sockfd);
			return 1;
		}
	} else {
		if (send_file(work_sockfd, sockfd, fileName) == 1) {
			close(sockfd);
			return 1;
		}
	}
	
	close(sockfd);
	
	return 0;
}

int port_handler(int work_sockfd, int listenfd, int port, char* fileName, int retr_stor) {
	int connfd;
	char buffer[8192];
	int recv_len, write_len;
	FILE *fp;
	
	if (retr_stor == 0) {
		char *tmp_str = waiting_for(work_sockfd, "");
		char num[256];
		strcpy(num, split(tmp_str, " ", 2)[0]);
		if (strcmp(num, "150") != 0) {
			close(listenfd);
			return 1;
		}
	
	}
	
	if ((connfd = accept(listenfd, NULL, NULL)) == -1) {
		printf("Error accept(): %s(%d)\n", strerror(errno), errno);
		waiting_for(work_sockfd, "");
		close(listenfd);
		return 1;
	}
	
	if (retr_stor == 0) {
		if (recv_file(work_sockfd, connfd, fileName) == 1) {
			close(connfd);
			close(listenfd);
			return 1;
		}
	} else {
		if (send_file(work_sockfd, connfd, fileName) == 1) {
			close(connfd);
			close(listenfd);
			return 1;
		}
	}
		
	close(connfd);
	close(listenfd);
	
	return STATUS_OK;
}

