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

#include "utilities.h"
#include "user.h"

char* generate_pasv_ok(char* pasv_ok, char* ip, int* port);
int pasv_open_port(int port, int* listenfd);
int send_file(char* fileName, int fd);
int recv_file(char* fileName, int fd);
int pasv_handler(char* fileName, int listenfd, int retr_stor, int clientfd, char* fileInformation);
int port_handler(char* ip, int port, char* fileName, int retr_stor, int clientfd, char* fileInformation);

extern int command_handler(int connfd, int fd_index, int* user_flag, int* pass_flag) {
	char sentence[8192];
	int p;
	int len;
	int tmp;
	char **tmp_str;
	char command[8192], arguments[8192];
	int pasv_listenfd, pasv_connfd;
	
	char goodbye[] = "221-Thank you for using the FTP service.\r\n221-Goodbye.\r\n";
	char no_user[] = "332 Not logged in.\r\n";
	char user_ok[] = "331 Guest login ok, send your complete e-mail address as password.\r\n";
	char user_fail[] = "530 User cannot log in.\r\n";
	char password_ok[] = "230-Welcome to my FTP~\r\n230-Guest login ok, access restrictions apply.\r\n";
	char system[] = "215 UNIX Type: L8\r\n";
	char type_ok[] = "200 Type set to I.\r\n";
	char type_error[] = "500 Invalid command.\r\n";
	char mkd_ok[] = "257 Folder created.\r\n";
	char mkd_fail[] = "553 Requested action not taken. File name not allowed.\r\n";
	char mkd_fail_2[] = "550-Requested action not taken. Directory unavailable (e.g. no access).\r\n";
	char mkd_has_same[] = "553 Requested action not taken. Its name is as same as an another folder.\r\n";
	char dele_ok[] = "250 Delete file success.\r\n";
	char dele_fail[] = "550-Requested action not taken. File unavailable(e.g. file not found, no access).\r\n";
	char pasv_ok[] = "227 Entering Passive Mode ";
	char pasv_fail[] = "425 Can't open data connection.\r\n";
	char port_ok[] = "200 PORT command successful.\r\n";
	char send_type_bin[] = "150 Opening BINARY mode data connection for ";
	char send_data_ok[] = "226 Transfer complete.\r\n";
	char send_data_fail[] = "550-Requested action not taken. File unavailable (e.g. file not found, no access).\r\n550-You can try 'PASV' command again.\r\n";
	char no_such_command[] = "500 Syntax error, command unrecongnized.\r\n";
	
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
		send(connfd, system, strlen(system), 0);
	} else if (strcmp(command, "TYPE") == 0) {
		if (strcmp(arguments, "I") == 0) {
			send(connfd, type_ok, strlen(type_ok), 0);
		} else {
			send(connfd, type_error, strlen(type_error), 0);
		}
	} else if (strcmp(command, "CWD") == 0) {
		
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
		
		if (port_pasv == 0) {
			char ip[64];
			int port[2];
			get_port_ip_port(user_table, fd_index, ip, port);
			if (port_handler(ip, port[0] * 256 + port[1], tmp_filePath, retr_stor, connfd, tmp_str) == STATUS_OK) {
				send(connfd, send_data_ok, strlen(send_data_ok), 0);
				increase_bytes(&user_table, fd_index, get_file_size(tmp_filePath));
			} else {
				send(connfd, send_data_fail, strlen(send_data_fail), 0);
			}
		} else {
			get_pasv_fds(user_table, fd_index, &pasv_listenfd);
			if (pasv_handler(tmp_filePath, pasv_listenfd, retr_stor, connfd, tmp_str) == STATUS_OK) {
				send(connfd, send_data_ok, strlen(send_data_ok), 0);
				increase_bytes(&user_table, fd_index, get_file_size(tmp_filePath));
			} else {
				send(connfd, send_data_fail, strlen(send_data_fail), 0);
			}
		}
	} else {
		send(connfd, no_such_command, strlen(no_such_command), 0);
	}
	
	return STATUS_OK;
}

char* generate_pasv_ok(char* pasv_ok, char* ip, int* port) {
	char **ips;
	char *result = (char*)malloc(1024 * sizeof(char));
	int i;
	
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
	
	close(fd);
	
	return STATUS_OK;
}

int recv_file(char* fileName, int fd) {
	char buffer[8192];
	int recv_len, write_len;
	FILE *fp;
	
	bzero(buffer, 8192);
	fp = fopen(fileName, "wb");
	
	while(recv_len = recv(fd, buffer, 8192, 0)) {
		write_len = fwrite(buffer, sizeof(char), recv_len, fp);
		if (write_len > recv_len) {
			printf("file write failed.\n");
			fclose(fp);
			return STATUS_ERROR;
		}
		bzero(buffer, 8192);
	}
	
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
			return STATUS_ERROR;
		}
		close(listenfd);
	} else {
		if (recv_file(fileName, connfd) == STATUS_ERROR) {
			return STATUS_ERROR;
		}
	}
	
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
