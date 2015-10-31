#include "globals.h"
#include "tcp.h"
#include "handler.h"

int main(int argc, char **argv) {
	int port;
	int length;

	init_table(&user_table);
	
	if (argc < 3) {
		port = 21;
		strcpy(filePath, "./tmp/");
	} else if (argc <= 4) {
		port = str_to_int(argv[2]);
		strcpy(filePath, "./tmp/");
	} else if (argc == 5) {
		port = str_to_int(argv[2]);
		strcpy(filePath, ".");
		if (argv[4][0] != '/') {
			strcat(filePath, "/");
		}
		strcat(filePath, argv[4]);
		length = strlen(filePath);
		if (filePath[length - 1] != '/') {
			strcat(filePath, "/");
		}
	}
	
	printf("port: %d\nroot: %s\n", port, filePath);
	
	if (run_ftp(port, command_handler) == STATUS_ERROR) {
		printf("Error building tcp connection!\n");
	}
	return 0;
}
