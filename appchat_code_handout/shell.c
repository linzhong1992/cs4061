#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "util.h"

/*
 * Read a line from stdin.
 */
char *sh_read_line(void)
{
	char *line = NULL;
	ssize_t bufsize = 0;

	getline(&line, &bufsize, stdin);
	printf("using getline(): %s", line);
	return line;
}

/*
 * Do stuff with the shell input here.
 */
int sh_handle_input(char *line, int fd_toserver)
{
	
	/***** Insert YOUR code *******/
	
 	/* Check for \seg command and create segfault */
	if(starts_with(line, CMD_SEG))
		raise(SIGSEGV);
	else
	/* Write message to server for processing */
		write(fd_toserver, line, strlen(line));
	return 0;
}

/*
 * Check if the line is empty (no data; just spaces or return)
 */
int is_empty(char *line)
{
	while (*line != '\0') {
		if (!isspace(*line))
			return 0;
		line++;
	}
	return 1;
}

/*
 * Start the main shell loop:
 * Print prompt, read user input, handle it.
 */
void sh_start(char *name, int fd_toserver)
{
	/***** Insert YOUR code *******/
	char command[MSG_SIZE];
	print_prompt(name);
	strcpy(command, sh_read_line());
	if(is_empty(command))
		return;
	sh_handle_input(command, fd_toserver);
}

int main(int argc, char **argv)
{	
	if (argc != 4) {
		printf("Usage: ./shell fd_from_server fd_to_server name.\n");
		return 0;
	}
	/* Extract pipe descriptors and name from argv */
	int fd_from_server = atoi(argv[1]);
	int fd_to_server = atoi(argv[2]);
	char* name = argv[3];

	pid_t pid;
	char output[MSG_SIZE];
	/* Fork a child to read from the pipe continuously */
	if((pid = fork()) < 0)
		printf("Fork error!\n");
	else if(pid == 0) {
	/*
	 * Once inside the child
	 * look for new data from server every 1000 usecs and print it
	 */ 
		while(1) {
			usleep(1000);
			if(read(fd_from_server, output, MSG_SIZE))
				printf("%s\n", output);
		}
	}
	else {
	/* Inside the parent
	 * Send the child's pid to the server for later cleanup
	 * Start the main shell loop
	 */
		while(1)
			sh_start(name, fd_to_server);
	}
}
