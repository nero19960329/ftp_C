#include <string.h>

typedef struct User {
	int id;
	int index;
	char password[1024];
	int pass_flag;
	int port_pasv;
	char ip[64];
	int port[2];
	int pasv_listenfd;
	int bytes;
	int files;
	char filePath[1024];
}User;

typedef struct User_table {
	User users[128];
	int length;
}User_table;

static User_table user_table;

void init_table(User_table* t) {
	t->length = 0;
}

int find_user(User_table t, int index) {
	int length;
	int i;
	
	length = t.length;
	for (i = 0; i < length; ++i) {
		if (t.users[i].index == index) {
			return i;
		}
	}
	
	return -1;
}

int has_same_password(User_table t, char* password, int* id) {
	int length;
	int i;
	
	length = t.length;
	for (i = 0; i < length; ++i) {
		if (strcmp(t.users[i].password, password) == 0) {
			*id = t.users[i].index;
			return 0;
		}
	}
	
	*id = -1;
	return 1;
}

int append_user(User_table* t, char* password, int index) {
	printf("t.length: %d, index: %d\n", t->length, index);

	t->users[t->length].id = t->length;
	t->users[t->length].index = index;
	strcpy(t->users[t->length].password, password);
	t->users[t->length].pass_flag = 0;
	t->users[t->length].port_pasv = -1;
	t->users[t->length].pasv_listenfd = 0;
	t->users[t->length].bytes = 0;
	t->users[t->length].files = 0;
	strcpy(t->users[t->length].filePath, filePath);
	++t->length;
	
	return t->length - 1;
}

int logout_user(User_table* t, int index) {
	int i;
	
	i = find_user(*t, index);
	if (i >= 0) {
		t->users[i].pass_flag = 0;
		t->users[i].port_pasv = -1;
		return 1;
	} else {
		return 0;
	}
}

void pass_success(User_table* t, int index) {
	int i;
	
	i = find_user(*t, index);
	printf("pass_success, i = %d\n", i);
	if (i >= 0) {
		t->users[i].pass_flag = 1;
		t->users[i].bytes = 0;
		t->users[i].files = 0;
	}
}

void set_port_pasv(User_table* t, int index, int port_pasv) {
	int i;
	
	i = find_user(*t, index);
	if (i >= 0) {
		t->users[i].port_pasv = port_pasv;
	} else {
		printf("no such user!");
	}
}

int get_port_pasv(User_table t, int index) {
	int i;
	
	i = find_user(t, index);
	if (i >= 0) {
		return t.users[i].port_pasv;
	} else {
		return -1;
	}
}

void set_pasv_fds(User_table* t, int index, int listenfd) {
	int i;
	
	i = find_user(*t, index);
	if (i >= 0) {
		t->users[i].pasv_listenfd = listenfd;
	}
}

int get_pasv_fds(User_table t, int index, int* listenfd) {
	int i;
	
	i = find_user(t, index);
	if (i >= 0) {
		*listenfd = t.users[i].pasv_listenfd;
		return 1;
	} else {
		return 0;
	}
}

void set_port_ip_port(User_table* t, int index, char* ip, int* port) {
	int i;
	
	i = find_user(*t, index);
	if (i >= 0) {
		strcpy(t->users[i].ip, ip);
		t->users[i].port[0] = port[0];
		t->users[i].port[1] = port[1];
	}
}

void get_port_ip_port(User_table t, int index, char* ip, int* port) {
	int i;
	
	i = find_user(t, index);
	if (i >= 0) {
		strcpy(ip, t.users[i].ip);
		port[0] = t.users[i].port[0];
		port[1] = t.users[i].port[1];
	}
}

void increase_bytes(User_table* t, int index, int bytes) {
	int i;
	
	i = find_user(*t, index);
	if (i >= 0) {
		t->users[i].bytes += bytes;
		++t->users[i].files;
	}
}

void get_bytes(User_table t, int index, int* bytes, int* files) {
	int i;
	
	i = find_user(t, index);
	if (i >= 0) {
		*bytes = t.users[i].bytes;
		*files = t.users[i].files;
	} else {
		*bytes = 0;
		*files = 0;
	}
}

void set_filePath(User_table* t, int index, char* userPath) {
	int i;
	
	i = find_user(*t, index);
	if (i >= 0) {
		strcpy(t->users[i].filePath, userPath);
	}
}

void get_filePath(User_table t, int index, char* *userPath) {
	int i;
	
	i = find_user(t, index);
	if (i >= 0) {
		*userPath = (char*)malloc(1024 * sizeof(char));
		strcpy(*userPath, t.users[i].filePath);
	} else {
		*userPath = NULL;
	}
}

