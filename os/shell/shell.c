#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "../kernel/kernel.h"
#include "shell.h"

char current_directory[BERDOS_MAX_PATHNAME_SIZE + 1] = "/:)\0";
char parent_directory[BERDOS_MAX_PATHNAME_SIZE + 1] = "/:)\0";

static void __read_line(char *buffer) {
	memset(buffer, ' ', BERDOS_SHELL_BUFFER_SIZE + 1);
	buffer[BERDOS_SHELL_BUFFER_SIZE] = '\0';
	unsigned int index = 0;

	printf("%s > ", current_directory, parent_directory);
	while (true) {
		int c = getchar_timeout_us(0);
		
		switch (c) {
		case -0x1: // EOF
		case 0x00: // NUL
		case 0x01: // SOH
		case 0x03: // ETX
		case 0x05: // ENQ
		case 0x06: // ACK
		case 0x0E: // SO
		case 0x0F: // SI
		case 0x10: // DLE
		case 0x11: // DC1/XON
		case 0x12: // DC2
		case 0x13: // DC3/XOFF
		case 0x14: // DC4
		case 0x15: // NAK
		case 0x16: // SYN
		case 0x18: // CAN
		case 0x19: // EM
		case 0x1A: // SUB
		case 0x1C: // FS
		case 0x1D: // GS
		case 0x1E: // RS
		case 0x1F: // US
			continue;
		case 0x02: // STX
		case 0x0A: // LF
		case 0x0B: // VT
		case 0x0D: // CR
			buffer[index] = '\0';
			putchar('\n');
			return;
		case 0x04: // EOT
			os_yield();
		case 0x07: // BEL
			printf("\a\n");
			return;
		case 0x08: // BS
			if (index == 0) continue;
			buffer[index--] = '\0';
			printf("\b \b");
			continue;
		case 0x09: // HT
			for (; index < 4; index++) {printf(" "); buffer[index] = '\0';};
			continue;
		case 0x0C: // FF
			buffer[index] = '\0';
			putchar('\f');
			return;
		case 0x17: // ETB
			for (; index > 0 ;index--) printf("\b \b");
			continue;
		case 0x1B: // ESC
			putchar('\e');
			continue;
		case 0x7F: // DEL
			if (index == 0) continue;
			buffer[index--] = '\0';
			printf("\b \b");
			continue;
		default:
			buffer[index] = c;
			putchar(c);
			break;
		};

		index++;
		if (index >= BERDOS_SHELL_BUFFER_SIZE) {
			putchar('\n');
			return;
		};
	};
}

static void __parse_line(char *buffer, char token_table[BERDOS_COMMAND_LENGTH + 1][BERDOS_SHELL_BUFFER_SIZE + 1]) {
	char *token, *token_save;
	token = strtok_r(buffer, " ", &token_save);

	for (uint i = 0; i < BERDOS_COMMAND_LENGTH + 1 && token != NULL; i++) {
		memset(&token_table[i][0], '\0', BERDOS_SHELL_BUFFER_SIZE + 1);
		strncpy(&token_table[i][0], token, BERDOS_SHELL_BUFFER_SIZE + 1);
		token = strtok_r(NULL, " ", &token_save);
	};
}

void __translate_pathname(char *relative_pathname, char *absolute_pathname) {
	if (strncmp(relative_pathname, "..", 2) == 0) {
		strncpy(absolute_pathname, parent_directory, BERDOS_MAX_PATHNAME_SIZE);
		strncat(absolute_pathname, relative_pathname + 2, BERDOS_MAX_PATHNAME_SIZE - strlen(absolute_pathname));
	} else if (strncmp(relative_pathname, ".", 1) == 0) {
		strncpy(absolute_pathname, current_directory, BERDOS_MAX_PATHNAME_SIZE);
		strncat(absolute_pathname, relative_pathname + 1, BERDOS_MAX_PATHNAME_SIZE - strlen(absolute_pathname));
	} else {
		strncpy(absolute_pathname, current_directory, BERDOS_MAX_PATHNAME_SIZE);
		if (absolute_pathname[strlen(absolute_pathname)] != '/') memset(absolute_pathname + strlen(absolute_pathname), '/', 1);
		strncat(absolute_pathname, relative_pathname, BERDOS_MAX_PATHNAME_SIZE - strlen(absolute_pathname));
	};
}

static void __execute_line(char token_table[BERDOS_COMMAND_LENGTH + 1][BERDOS_SHELL_BUFFER_SIZE + 1]) {
	if (strncmp(token_table[0], "echo", 5) == 0) {
		printf("%s\n", token_table[1]);
	};
	if (strncmp(token_table[0], "help", 5) == 0) {
		printf("echo\thelp\ncd\tls\nmkdir\trmdir\ntouch\trm\ntop");
	};
	if (strncmp(token_table[0], "cd", 3) == 0) {
		if (BERDOS_MAX_PATHNAME_SIZE - strlen(current_directory) < strlen(*token_table) + 1) panic("ERROR: Excessive Pathname");

		char candiate_pathname[BERDOS_MAX_PATHNAME_SIZE] = "\0";
		__translate_pathname(token_table[1], candiate_pathname); 
		index_node *candiate_directory = os_open(candiate_pathname);

		if (candiate_directory != NULL && candiate_directory->mode == DIRECTORY_INODE) {
			strncpy(current_directory, candiate_pathname, BERDOS_MAX_PATHNAME_SIZE);
			strncpy(parent_directory, current_directory, (current_directory + strlen(current_directory)) - strrchr(current_directory, '/'));
			if (strncmp(current_directory, "/:)", 3) != 0) strncpy(parent_directory, "/:)", 3);
			if (strncmp(parent_directory, "/:)", 3) != 0) strncpy(parent_directory, "/:)", 3);
		} else {
			printf("cd: :(\n");
		}
	};
	if (strncmp(token_table[0], "ls", 3) == 0) {
		index_node *inode = os_open(current_directory);
		inode = inode->child_node;
		while (inode != NULL) {
			printf("%s\n", inode->name);
			inode = inode->sibling_node;
		};
	}
	if (strncmp(token_table[0], "mkdir", 6) == 0) {
		os_mkdir(current_directory, token_table[1]);
	};
	if (strncmp(token_table[0], "rmdir", 6) == 0) {
		char absolute_pathname[BERDOS_MAX_PATHNAME_SIZE] = "\0";
		__translate_pathname(token_table[1], absolute_pathname);
		os_rmdir(absolute_pathname);
	};
	if (strncmp(token_table[0], "top", 4) == 0) {
		print_diagnostics();
	};

	if (strncmp(token_table[0], "touch", 6) == 0) {
		char absolute_pathname[BERDOS_MAX_PATHNAME_SIZE] = "\0";
		__translate_pathname(token_table[1], absolute_pathname);
		os_create(token_table[1], 32, current_directory);
	};
	if (strncmp(token_table[0], "rm", 3) == 0) {
		char absolute_pathname[BERDOS_MAX_PATHNAME_SIZE] = "\0";
		__translate_pathname(token_table[1], absolute_pathname);
		os_delete(absolute_pathname);
	};

}

void shell(void) {	
	while (true) {
		char buffer[BERDOS_SHELL_BUFFER_SIZE + 1] = "\0";
		char token_table[BERDOS_COMMAND_LENGTH + 1][BERDOS_SHELL_BUFFER_SIZE + 1] = {"\0"};
		
		__read_line(buffer);
		__parse_line(buffer, token_table);
		__execute_line(token_table);
	};
}
