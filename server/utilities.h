#include <dirent.h>

// 改自http://www.cnblogs.com/xudong-bupt/p/3504442.html
extern char* get_dir(char* filePath) {
	DIR *dir;
	struct dirent* ptr;
	char* dir_information = (char*)malloc(8192 * sizeof(char));
	
	bzero(dir_information, 8192);
	if ((dir = opendir(filePath)) == NULL) {
		printf("open dir error!\n");
		strcpy(dir_information, "553 No such directory!\r\n");
		return dir_information;
	}
	
	strcpy(dir_information, "200-File list of current directory.\r\n200-./\r\n");
	while ((ptr = readdir(dir)) != NULL) {
		if (strcmp(ptr->d_name, ".") == 0
			|| strcmp(ptr->d_name, "..") == 0) {
			continue;	
		} else if (ptr->d_type == 8) {	// file
			sprintf(dir_information, "%s200-./%s\r\n", dir_information, ptr->d_name);
		} else if (ptr->d_type == 10) {	// link file
			sprintf(dir_information, "%s200-./%s\r\n", dir_information, ptr->d_name);
		} else if (ptr->d_type == 4) {	// dir
			sprintf(dir_information, "%s200-./%s/\r\n", dir_information, ptr->d_name);
		}
	}
	closedir(dir);
	
	return dir_information;
}

extern char* get_host_ip() {
	struct ifaddrs* ifAddrStruct = NULL;  
    void* tmpAddrPtr = NULL;
    char* addressBuffer = (char*)malloc(INET_ADDRSTRLEN * sizeof(char));
  
    getifaddrs(&ifAddrStruct);
  
    while (ifAddrStruct != NULL) {  
        if (ifAddrStruct->ifa_addr->sa_family == AF_INET) {
            tmpAddrPtr = &((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            if (strcmp(ifAddrStruct->ifa_name, "lo") != 0) {
            	return addressBuffer;
            } 
        }
        ifAddrStruct = ifAddrStruct->ifa_next;  
    }  
}

extern int get_file_size(char* fileName) {
	struct stat statbuf;
	stat(fileName, &statbuf);
	int size = statbuf.st_size;
	printf("size of %s: %d\n", fileName, size);
	return size;
}

extern char** split(char* str, const char* delimiters, int length) {
	char **array;
	char *p;
	int i;
	
	array = (char**)malloc(length * sizeof(char*));
	
	p = strtok(str, delimiters);
	array[0] = p;
	
	i = 1;
	while (i < length && p != NULL) {
		p = strtok(NULL, delimiters);
		array[i++] = p;
	}
	
	if (p == NULL) {
		--i;
		for (; i < length; ++i) {
			array[i] = (char*)malloc(sizeof(char));
			array[i][0] = '\0';
		}
	}
	
	return array;
}

extern void generate_port(int* a, int* b) {
	srand((unsigned int)time(0));
	int random;
	random = rand() % 45536 + 20000;
	*a = random / 256;
	*b = random % 256;
}

extern void extract_ip_port(char* str, char* ip, int* port) {
	int length;
	int i, k;
	int nums[6];
	
	for (i = 0; i < 6; ++i) {
		nums[i] = 0;
	}
	
	length = strlen(str);
	for (i = 0; i < length; ++i) {
		if (str[i] == '(') {
			++i;
			break;
		}
	}
	
	k = 0;
	for (; i < length; ++i) {
		if (str[i] == ')') {
			break;
		}
		
		if (str[i] == ',') {
			++k;
		} else {
			nums[k] *= 10;
			nums[k] += (str[i] - '0');
		}
	}
	
	sprintf(ip, "%d.%d.%d.%d", nums[0], nums[1], nums[2], nums[3]);
	port[0] = nums[4];
	port[1] = nums[5];
}
