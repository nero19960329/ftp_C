#include "globals.h"
#include "tcp.h"
#include "handler.h"

int main(int argc, char **argv) {
	init_table(&user_table);
	if (run_ftp(23456, command_handler) == STATUS_ERROR) {
		printf("Error building tcp connection!\n");
	}
	return 0;
}
