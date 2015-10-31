#include <sys/socket.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <pthread.h>

#include "utilities.h"
#include "user.h"

char* generate_pasv_ok(char* pasv_ok, char* ip, int* port);
int pasv_open_port(int port, int* listenfd);
int send_file(char* fileName, int fd);
int recv_file(char* fileName, int fd);
int pasv_handler(char* fileName, int listenfd, int retr_stor, int clientfd, char* fileInformation);
int port_handler(char* ip, int port, char* fileName, int retr_stor, int clientfd, char* fileInformation);

void* pasv_thread(void* arg);
void* port_thread(void* arg);

typedef struct args {
	/* arguments in pasv */
	int listenfd;
	
	/* arguments in port */
	char* ip;
	int port;
	
	/* arguments both in pasv and port */
	char* fileName;
	int retr_stor;
	int clientfd;
	char* fileInformation;
	int fd_index;
}args;

extern int command_handler(int connfd, int fd_index, int* user_flag, int* pass_flag) {
	char sentence[8192];
	int len;
	int tmp;
	char **tmp_str;
	char command[8192], arguments[8192];
	int pasv_listenfd;
	pthread_t thread;
	
	len = recv(connfd, sentence, 8192, 0);
	if (len == -1) {
		printf("Error recv()\n");
		return STATUS_ERROR;
	} else if (len == 0) {
		return STATUS_PASS;
	}
	
	tmp_str = split(sentence, " ", 2);
	strcpy(command, tmp_str[0]);
	strcpy(arguments, tmp_str[1]);
	if (strcmp(command, "QUIT") == 0 ||
		strcmp(command, "ABOR") == 0) {
		/* Send goodbye message */
		char goodbye_message[8192];
		int bytes, files;
		get_bytes(user_table, fd_index, &bytes, &files);
		sprintf(goodbye_message, "221-You have transferred %d bytes in %d files.\r\n%s", bytes, files, goodbye);
		send(connfd, goodbye_message, strlen(goodbye_message), 0);
		logout_user(&user_table, fd_index);
		return STATUS_QUIT;
	} else if (*user_flag == 0) {
		if (strcmp(command, "USER") == 0) {
			if (strcmp(arguments, "anonymous") == 0) {
				send(connfd, user_ok, strlen(user_ok), 0);
				*user_flag = 1;
			} else {
				send(connfd, user_fail, strlen(user_fail), 0);
			}
		} else {
			send(connfd, no_user, strlen(no_user), 0);
		}
	} else if (*pass_flag == 0) {
		if (strcmp(command, "PASS") == 0) {
			if (strlen(arguments) == 0) {
				send(connfd, user_ok, strlen(user_ok), 0);
				return STATUS_PASS;
			}
			
			/* Create a new user */
			if (has_same_password(user_table, arguments, &tmp) != 0) {
				tmp = append_user(&user_table, arguments, fd_index);
				printf("tmp: %d\n", tmp);
				printf("%d\n", find_user(user_table, fd_index));
			}
		
			pass_success(&user_table, tmp);
			*pass_flag = 1;
			send(connfd, password_ok, strlen(password_ok), 0);
		} else {
			send(connfd, no_user, strlen(no_user), 0);
		}
	} else if (strcmp(command, "SYST") == 0) {
		send(connfd, system_inf, strlen(system_inf), 0);
	} else if (strcmp(command, "TYPE") == 0) {
		if (strcmp(arguments, "I") == 0) {
			send(connfd, type_ok, strlen(type_ok), 0);
		} else {
			send(connfd, type_error, strlen(type_error), 0);
		}
	} else if (strcmp(command, "CWD") == 0) {
		char userPath[1024];
		char respond[1024];
		if (strlen(arguments) == 0) {
			strcpy(userPath, filePath);
		} else {
			if (generate_path(arguments, userPath) == STATUS_ERROR) {
				send(connfd, cwd_fail, strlen(cwd_fail), 0);
				return STATUS_PASS;
			}
		}
		set_filePath(&user_table, fd_index, userPath);
		sprintf(respond, "%s%s.\r\n", cwd_ok, userPath);
		send(connfd, respond, strlen(respond), 0);
	} else if (strcmp(command, "CDUP") == 0) {
		char *userPath;
		char respond[1024];
		get_filePath(user_table, fd_index, &userPath);
		if (dir_up(&userPath) == 1) {
			set_filePath(&user_table, fd_index, userPath);
			send(connfd, cdup_fail, strlen(cdup_fail), 0);
		} else {
			set_filePath(&user_table, fd_index, userPath);
			sprintf(respond, "%s%s.\r\n", cwd_ok, userPath);
			send(connfd, respond, strlen(respond), 0);
		}
	} else if (strcmp(command, "LIST") == 0) {
		char *checkPath;
		get_filePath(user_table, fd_index, &checkPath);
		if (strlen(arguments) != 0) {
			strcat(checkPath, arguments);
		}
		char *dir = get_dir(checkPath);
		send(connfd, dir, strlen(dir), 0);
	} else if (strcmp(command, "MKD") == 0) {
		if (arguments[0] == '.') {
			send(connfd, mkd_fail, strlen(mkd_fail), 0);
			return STATUS_PASS;
		}
		
		int arg_len = strlen(arguments);
		int k;
		
		for (k = 0; k < arg_len; ++k) {
			if (arguments[k] == '/') {
				send(connfd, mkd_fail, strlen(mkd_fail), 0);
				return STATUS_PASS;
			}
		}
		
		char* newPath;
		get_filePath(user_table, fd_index, &newPath);
		strcat(newPath, arguments);
		
		if (access(newPath, 0) == -1) {
			if (mkdir(newPath, 0777) == 1) {
				send(connfd, mkd_fail_2, strlen(mkd_fail_2), 0);
			}
			send(connfd, mkd_ok, strlen(mkd_ok), 0);
		} else {
			send(connfd, mkd_has_same, strlen(mkd_has_same), 0);
		}
	} else if (strcmp(command, "DELE") == 0) {
		char *delePath;
		get_filePath(user_table, fd_index, &delePath);
		strcat(delePath, arguments);
		printf("delePath: %s\n", delePath);
		if (remove(delePath) < 0) {
			send(connfd, dele_fail, strlen(dele_fail), 0);
		} else {
			send(connfd, dele_ok, strlen(dele_ok), 0);
		}
	} else if (strcmp(command, "PWD") == 0) {
		char *userPath, sendMessage[1024];
		get_filePath(user_table, fd_index, &userPath);
		sprintf(sendMessage, "200-Current directory:\r\n200-%s\r\n", userPath);
		send(connfd, sendMessage, strlen(sendMessage), 0);
	} else if (strcmp(command, "PASV") == 0) {
		set_port_pasv(&user_table, fd_index, 1);
		char *ip = get_host_ip();
		int port[2];
		generate_port(&port[0], &port[1]);
		if (pasv_open_port(port[0] * 256 + port[1], &pasv_listenfd) == STATUS_OK) {
			set_pasv_fds(&user_table, fd_index, pasv_listenfd);
			char *pasv_ok_ip_port = generate_pasv_ok(pasv_ok, ip, port);
			send(connfd, pasv_ok_ip_port, strlen(pasv_ok_ip_port), 0);
		} else {
			send(connfd, pasv_fail, strlen(pasv_fail), 0);
		}
	} else if (strcmp(command, "PORT") == 0) {
		set_port_pasv(&user_table, fd_index, 0);
		char tmp_str[128];
		sprintf(tmp_str, "(%s)", arguments);
		char ip[64];
		int port[2];
		extract_ip_port(tmp_str, ip, port);
		set_port_ip_port(&user_table, fd_index, ip, port);
		send(connfd, port_ok, strlen(port_ok), 0);
	} else if (strcmp(command, "RETR") == 0
				|| strcmp(command, "STOR") == 0) {
		int retr_stor;
		char* tmp_filePath;
		int port_pasv = get_port_pasv(user_table, fd_index);
		char tmp_str[1024];
		FILE *fp;
		
		if (port_pasv == -1) {
			send(connfd, no_pasv_port, strlen(no_pasv_port), 0);
			return STATUS_PASS;
		}
		
		if (arguments[0] == '.' && arguments[1] == '.' && arguments[2] == '/') {
			send(connfd, dele_fail, strlen(dele_fail), 0);
			return STATUS_PASS;
		}
		
		get_filePath(user_table, fd_index, &tmp_filePath);
		strcat(tmp_filePath, arguments);
		
		if (strcmp(command, "RETR") == 0) {
			retr_stor = 0;
			sprintf(tmp_str, "%s%s (%d bytes).\r\n", send_type_bin, arguments, get_file_size(tmp_filePath));
			if ((fp = fopen(tmp_filePath, "rb")) == NULL) {
				printf("fopen() error!\n");
				send(connfd, send_data_fail, strlen(send_data_fail), 0);
				return STATUS_OK;
			}
		} else {
			retr_stor = 1;
			sprintf(tmp_str, "%s%s ", send_type_bin, arguments);
			if ((fp = fopen(tmp_filePath, "wb")) == NULL) {
				printf("fopen() error!\n");
				send(connfd, send_data_fail, strlen(send_data_fail), 0);
				return STATUS_OK;
			}
		}
		
		printf("port_pasv: %d\n", port_pasv);
		if (port_pasv == 0) {
			char *ip = (char*)malloc(64 * sizeof(char));
			int *port = (int*)malloc(2 * sizeof(int));
			get_port_ip_port(user_table, fd_index, ip, port);
			args* m_arg = (args*)malloc(sizeof(args));
			m_arg->fileName = tmp_filePath;
			m_arg->retr_stor = retr_stor;
			m_arg->clientfd = connfd;
			m_arg->fileInformation = tmp_str;
			m_arg->fd_index = fd_index;
			m_arg->ip = ip;
			m_arg->port = port[0] * 256 + port[1];
			pthread_create(&thread, NULL, port_thread, (void*)m_arg);
		} else if (port_pasv == 1) {
			get_pasv_fds(user_table, fd_index, &pasv_listenfd);
			args* m_arg = (args*)malloc(sizeof(args));
			m_arg->fileName = tmp_filePath;
			m_arg->listenfd = pasv_listenfd;
			m_arg->retr_stor = retr_stor;
			m_arg->clientfd = connfd;
			m_arg->fileInformation = tmp_str;
			m_arg->fd_index = fd_index;
			pthread_create(&thread, NULL, pasv_thread, (void*)m_arg);
		}
	} else {
		send(connfd, no_such_command, strlen(no_such_command), 0);
	}
	
	return STATUS_OK;
}

char* generate_pasv_ok(char* pasv_ok, char* ip, int* port) {
	char **ips;
	char *result = (char*)malloc(1024 * sizeof(char));
	
	ips = split(ip, ".", 4);
	sprintf(result, "%s(%s,%s,%s,%s,%d,%d)\r\n", pasv_ok, ips[0], ips[1], ips[2], ips[3], port[0], port[1]);
	
	return result;
}

int pasv_open_port(int port, int* listenfd) {
	struct sockaddr_in addr;
	
	if ((*listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return STATUS_ERROR;
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (bind(*listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return STATUS_ERROR;
	}
	
	if (listen(*listenfd, 10) == -1) {
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return STATUS_ERROR;
	}
	
	return STATUS_OK;
}

int send_file(char* fileName, int fd) {
	char buffer[8192];
	FILE *fp;
	int len;
	
	bzero(buffer, 8192);
	printf("fileName : %s\n", fileName);
	fp = fopen(fileName, "rb");
	
	len = 0;
	while((len = fread(buffer, 1, 8192, fp)) > 0) {
		int send_len;
		if ((send_len = send(fd, buffer, len, 0)) < 0) {
			printf("Error send(): %s(%d)\n", strerror(errno), errno);
		}
		bzero(buffer, 8192);
	}
	
	fclose(fp);
	
	return STATUS_OK;
}

int recv_file(char* fileName, int fd) {
	char buffer[8192];
	int recv_len, write_len;
	FILE *fp;
	int lock_status;
	
	bzero(buffer, 8192);
	fp = fopen(fileName, "wb");
	
	/* lock this file */
	lock_status = flock(fileno(fp), LOCK_EX);
	printf("lock_status: %d\n", lock_status);
	
	if (lock_status) {
		return STATUS_ERROR;
	}
	
	while((recv_len = recv(fd, buffer, 8192, 0))) {
		write_len = fwrite(buffer, sizeof(char), recv_len, fp);
		if (write_len > recv_len) {
			printf("file write failed.\n");
			flock(fileno(fp), LOCK_UN);
			fclose(fp);
			return STATUS_ERROR;
		}
		bzero(buffer, 8192);
	}
	
	/* unlock this file */
	flock(fileno(fp), LOCK_UN);
	
	fclose(fp);

	return STATUS_OK;
}

int pasv_handler(char* fileName, int listenfd, int retr_stor, int clientfd, char* fileInformation) {
	int connfd;
	
	if ((connfd = accept(listenfd, NULL, NULL)) == -1) {
		printf("Error accept(): %s(%d)\n", strerror(errno), errno);
		return STATUS_ERROR;
	}
	
	send(clientfd, fileInformation, strlen(fileInformation), 0);
	if (retr_stor == 0) {
		if (send_file(fileName, connfd) == STATUS_ERROR) {
			close(connfd);
			close(listenfd);
			return STATUS_ERROR;
		}
	} else {
		if (recv_file(fileName, connfd) == STATUS_ERROR) {
			close(connfd);
			close(listenfd);
			return STATUS_ERROR;
		}
	}
	
	close(connfd);
	close(listenfd);
	return STATUS_OK;
}

int port_handler(char* ip, int port, char* fileName, int retr_stor, int clientfd, char* fileInformation) {
	printf("ip: %s, port: %d\n", ip, port);
	int sockfd;
	struct sockaddr_in addr;
	
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return STATUS_ERROR;
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
		close(sockfd);
		return STATUS_ERROR;
	}
	
	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		printf("Error connect(): %s(%d)\n", strerror(errno), errno);
		close(sockfd);
		return STATUS_ERROR;
	}
	
	send(clientfd, fileInformation, strlen(fileInformation), 0);
	if (retr_stor == 0) {
		if (send_file(fileName, sockfd) == STATUS_ERROR) {
			close(sockfd);
			return STATUS_ERROR;
		}
	} else {
		if (recv_file(fileName, sockfd) == STATUS_ERROR) {
			close(sockfd);
			return STATUS_ERROR;
		}
	}
	
	close(sockfd);
	return STATUS_OK;
}

void* pasv_thread(void* arg) {
	args *m_arg = (args*)arg;
	
	if (pasv_handler(m_arg->fileName, m_arg->listenfd, m_arg->retr_stor, m_arg->clientfd, m_arg->fileInformation) == STATUS_OK) {
		send(m_arg->clientfd, send_data_ok, strlen(send_data_ok), 0);
		increase_bytes(&user_table, m_arg->fd_index, get_file_size(m_arg->fileName));
	} else {
		send(m_arg->clientfd, send_data_fail, strlen(send_data_fail), 0);
	}
	
	return (void*)NULL;
}

void* port_thread(void* arg) {
	args *m_arg = (args*)arg;
	
	if (port_handler(m_arg->ip, m_arg->port, m_arg->fileName, m_arg->retr_stor, m_arg->clientfd, m_arg->fileInformation) == STATUS_OK) {
		send(m_arg->clientfd, send_data_ok, strlen(send_data_ok), 0);
		increase_bytes(&user_table, m_arg->fd_index, get_file_size(m_arg->fileName));
	} else {
		send(m_arg->clientfd, send_data_fail, strlen(send_data_fail), 0);
	}
	
	return (void*)NULL;
}

