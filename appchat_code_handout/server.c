#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include "util.h"

/*
 * Identify the command used at the shell 
 */
int parse_command(char *buf)
{
	int cmd;

	if (starts_with(buf, CMD_CHILD_PID))
		cmd = CHILD_PID;
	else if (starts_with(buf, CMD_P2P))
		cmd = P2P;
	else if (starts_with(buf, CMD_LIST_USERS))
		cmd = LIST_USERS;
	else if (starts_with(buf, CMD_ADD_USER))
		cmd = ADD_USER;
	else if (starts_with(buf, CMD_EXIT))
		cmd = EXIT;
	else if (starts_with(buf, CMD_KICK))
		cmd = KICK;
	else
		cmd = BROADCAST;

	return cmd;
}
void initialize_users(user_chat_box_t *users)
{	int i;
	for (i = 0; i < MAX_USERS; i++)
	{
		users[i].status = SLOT_EMPTY;
	}
}

/*
 * List the existing users on the server shell
 */
int list_users(user_chat_box_t *users, int fd)
{
	/* 
	 * Construct a list of user names
	 * Don't forget to send the list to the requester!
	 */
	 int i;
	 int user_count = 0;
	 char list[MSG_SIZE];
	 for (i = 0; i < MAX_USERS; i++)
	 {
	 	if (users[i].status == SLOT_FULL)
	 	{
	 		strcat(list, users[i].name);
	 		strcat(list, "\n");
	 		user_count++;
	 	}
	 }
	 if (user_count == 0)
	 	strcpy(list, "No users.\n");
	 if (write(fd, list, strlen(list) + 1) < 0)
	 	perror("Write user-list error.");
	 return user_count;
}

/*
 * Utility function.
 * Find user index for given user name.
 */
int find_user_index(user_chat_box_t *users, char *name)
{
	int i, user_idx = -1;

	if (name == NULL) {
		fprintf(stderr, "NULL name passed.\n");
		return user_idx;
	}
	for (i = 0; i < MAX_USERS; i++) {
		if (users[i].status == SLOT_EMPTY)
			continue;
		if (strncmp(users[i].name, name, strlen(name)) == 0) {
			user_idx = i;
			break;
		}
	}

	return user_idx;
}

int find_empty_slot(user_chat_box_t *users)
{
	int i;
	for (i = 0; i < MAX_USERS; i++)
	{
		if (users[i].status == SLOT_EMPTY)
			return i;
	}
	return -1;
}

/*
 * Utility function.
 * Given a command's input buffer, extract name.
 */
char *extract_name(int cmd, char *buf)
{
	char *s = NULL;

	s = strtok(buf, " ");
	s = strtok(NULL, " ");
	if (cmd == P2P)
		return s;	/* s points to the name as no newline after name in P2P */
	s = strtok(s, "\n");	/* other commands have newline after name, so remove it*/
	return s;
}
/*
 * Add a new user
 */
void add_user(user_chat_box_t *users, char *buf, int server_fd)
{
	/***** Insert YOUR code *******/
	
	/* 
	 * Check if user limit reached.
	 *
	 * If limit is okay, add user, set up non-blocking pipes and
	 * notify on server shell
	 *
	 * NOTE: You may want to remove any newline characters from the name string 
	 * before adding it. This will help in future name-based search.
	 */
	int i;
	pid_t pid;
	char* user_name = extract_name(ADD_USER, buf);
	char msg[MSG_SIZE];
	char arg1[MSG_SIZE];
	char arg2[MSG_SIZE];
	if (find_user_index(users, user_name) != -1)
		perror("Username already exits.");
	else
	{
		if ((i = find_empty_slot(users)) == -1)
		{
			printf("Users are full.\n");
			return;
		}
		else
		{
			sprintf(msg, "Adding %s...\n", user_name);
			users[i].status = SLOT_FULL;
			pipe(users[i].ptoc);
			pipe(users[i].ctop);
			fcntl(users[i].ptoc[0],F_SETFL, O_NONBLOCK);
			fcntl(users[i].ptoc[1],F_SETFL,O_NONBLOCK);
			fcntl(users[i].ctop[0],F_SETFL,O_NONBLOCK);
			fcntl(users[i].ctop[1],F_SETFL,O_NONBLOCK);
			if (write(server_fd, msg, strlen(msg) + 1) < 0)
				perror("Writing to server shell");
		}
	}
	//Fork a child process to execute added user's Xterm
	if ((pid = fork()) < 0)
		perror("Add_user fork error");
	else if (pid == 0)
	{
		sprintf(arg1, "%d", users[i].ptoc[0]);
		sprintf(arg2, "%d", users[i].ctop[1]);
		execl(XTERM_PATH, XTERM, "+hold", "-e", "./shell", arg1, arg2, user_name, (char *)0);
	}
	else
		wait(NULL);
	return;
}

/*
 * Broadcast message to all users. Completed for you as a guide to help with other commands :-).
 */
void broadcast_msg(user_chat_box_t *users, char *buf, int fd, char *sender)
{
	int i;
	const char *msg = "Broadcasting...", *s;
	char text[MSG_SIZE];

	/* Notify on server shell */
	if (write(fd, msg, strlen(msg) + 1) < 0)
		perror("writing to server shell");
	
	/* Send the message to all user shells */
	s = strtok(buf, "\n");
	sprintf(text, "%s : %s", sender, s);
	for (i = 0; i < MAX_USERS; i++) {
		if (users[i].status == SLOT_EMPTY)
			continue;
		if (write(users[i].ptoc[1], text, strlen(text) + 1) < 0)
			perror("write to child shell failed");
	}
}

/*
 * Close all pipes for this user
 */
void close_pipes(int idx, user_chat_box_t *users)
{
	close(users[idx].ptoc[0]);
	close(users[idx].ptoc[1]);
	close(users[idx].ctop[0]);
	close(users[idx].ctop[1]);
}

/*
 * Cleanup single user: close all pipes, kill user's child process, kill user 
 * xterm process, free-up slot.
 * Remember to wait for the appropriate processes here!
 */
void cleanup_user(int idx, user_chat_box_t *users)
{
	/***** Insert YOUR code *******/
	close_pipes(idx, users);
	kill(users[idx].child_pid, SIGKILL);
	kill(users[idx].pid, SIGKILL);
	users[idx].status = SLOT_EMPTY;
}

/*
 * Cleanup all users: given to you
 */
void cleanup_users(user_chat_box_t *users)
{
	int i;
	for (i = 0; i < MAX_USERS; i++) {
		if (users[i].status == SLOT_EMPTY)
			continue;
		cleanup_user(i, users);
	}
}

/*
 * Cleanup server process: close all pipes, kill the parent process and its 
 * children.
 * Remember to wait for the appropriate processes here!
 */
void cleanup_server(server_ctrl_t server_ctrl)
{
	/***** Insert YOUR code *******/

	close(server_ctrl.ptoc[0]);
	close(server_ctrl.ptoc[1]);
	close(server_ctrl.ctop[0]);
	close(server_ctrl.ctop[1]);
	kill(server_ctrl.child_pid, SIGKILL);
	kill(server_ctrl.pid, SIGKILL);

}



/*
 * Send personal message. Print error on the user shell if user not found.
 */
void send_p2p_msg(int idx, user_chat_box_t *users, char *buf)
{
	/* get the target user by name (hint: call (extract_name() and send message */
	
	char *target_name;
	int target_idx;
	char *s;
	char text[MSG_SIZE];
	target_name = extract_name(P2P, buf);
	target_idx = find_user_index(users, target_name);
	s = strtok(buf, " ");
	s = strtok(buf, " ");
	s = strtok(buf, "\n"); //extract the messege
	sprintf(text, "%s : %s", users[idx].name, s);
	write(users[target_idx].ptoc[1], text, strlen(text) + 1);
}

int main(int argc, char **argv)
{

	user_chat_box_t user_list[MAX_USERS];
	server_ctrl_t server_shell;
	char arg1[MSG_SIZE];
	char arg2[MSG_SIZE];
	char command[MSG_SIZE];
	int cmd_num;
	char* str_pid;
	char* user_name;
	int k;
	pid_t pid = fork();
	/* open non-blocking bi-directional pipes for communication with server shell */

	initialize_users(user_list);
	pipe(server_shell.ptoc);
	pipe(server_shell.ctop);
	fcntl(server_shell.ptoc[0],F_SETFL,O_NONBLOCK);
	fcntl(server_shell.ptoc[1],F_SETFL,O_NONBLOCK);
	fcntl(server_shell.ctop[0],F_SETFL,O_NONBLOCK);
	fcntl(server_shell.ctop[1],F_SETFL,O_NONBLOCK);



	/* Fork the server shell */
	

	if (pid < 0) 
		printf("Fork error.\n");
	else if (pid == 0)
	{
		// close(server_shell.ctop[0]);
		// close(server_shell.ptoc[1]);
		sprintf(arg1, "%d", server_shell.ptoc[0]);
		sprintf(arg2, "%d", server_shell.ctop[1]);
		execl("./shell", "shell", arg1, arg2, "Server", (char *)0);
	}
		/* 
	 	 * Inside the child.
		 * Start server's shell.
	 	 * exec the SHELL program with the required program arguments.
	 	 */

	/* Inside the parent. This will be the most important part of this program. */

		/* Start a loop which runs every 1000 usecs.
	 	 * The loop should read messages from the server shell, parse them using the 
	 	 * parse_command() function and take the appropriate actions. */
	else 
	{	
		server_shell.pid = pid;
		//close(server_shell.ctop[1]);
		//close(server_shell.ptoc[0]);

		while (1) 
		{
			/* Let the CPU breathe */
			usleep(1000);
			/* 
		 	 * 1. Read the message from server's shell, if any
		 	 * 2. Parse the command
		 	 * 3. Begin switch statement to identify command and take appropriate action
		 	 *
		 	 * 		List of commands to handle here:
		 	 * 			CHILD_PID
		 	 * 			LIST_USERS
		 	 * 			ADD_USER
		 	 * 			KICK
		 	 * 			EXIT
		 	 * 			BROADCAST 
		 	 */

			/* Fork a process if a user was added (ADD_USER) */
				/* Inside the child */
				/*
			 	 * Start an xterm with shell program running inside it.
			 	 * execl(XTERM_PATH, XTERM, "+hold", "-e", <path for the SHELL program>, ..<rest of the arguments for the shell program>..);
			 	 */
		 	if (read(server_shell.ctop[0], command, MSG_SIZE) > 0)
		 	{
		 		const char* a = "are you in\n";
		 		write(server_shell.ptoc[1], a, strlen(a) + 1);
		 		cmd_num = parse_command(command);
		 		switch (cmd_num)
		 		{
		 			case CHILD_PID:
		 				str_pid = extract_name(CHILD_PID, command);
		 				server_shell.child_pid = atoi(str_pid);
		 				break;
		 			case LIST_USERS:
		 				printf("listing...\n");
		 				list_users(user_list, server_shell.ptoc[1]);
		 				break;
		 			case ADD_USER:
		 				add_user(user_list, command, server_shell.ptoc[1]); break;
		 			case KICK:
		 				user_name = extract_name(KICK, command);
		 				cleanup_user(find_user_index(user_list, user_name), user_list);
		 				break;
		 			case EXIT:
		 				cleanup_users(user_list);
		 				cleanup_server(server_shell);
		 				break;
		 			case BROADCAST:
		 				broadcast_msg(user_list, command, server_shell.ptoc[1], "Server");
		 				break;
		 		}
		 	}
			/* Back to our main while loop for the "parent" */
			/* 
		 	 * Now read messages from the user shells (ie. LOOP) if any, then:
		 	 * 		1. Parse the command
		 	 * 		2. Begin switch statement to identify command and take appropriate action
		 	 *
		 	 * 		List of commands to handle here:
		 	 * 			CHILD_PID
		 	 * 			LIST_USERS
		 	 * 			P2P
		 	 * 			EXIT
		 	 * 			BROADCAST
		 	 *
		 	 * 		3. You may use the failure of pipe read command to check if the 
		 	 * 		user chat windows has been closed. (Remember waitpid with WNOHANG 
		 	 * 		from recitations?)
		 	 * 		Cleanup user if the window is indeed closed.
		 	 */
		 	for (int i = 0; i < MAX_USERS; i++)
		 	{
		 		 if (read(user_list[i].ctop[0], command, MSG_SIZE) >= 0)
		 		 {
			 		cmd_num = parse_command(command);
			 		switch (cmd_num)
			 		{
			 			case CHILD_PID:
			 				str_pid = extract_name(CHILD_PID, command);
			 				user_list[i].child_pid = atoi(str_pid);
			 				break;
			 			case LIST_USERS:
			 				list_users(user_list, user_list[i].ptoc[1]);
			 				break;
			 			case P2P:
			 				send_p2p_msg(i, user_list, command);
			 				break;
			 			case EXIT:
			 				cleanup_user(i, user_list);
			 				break;
			 			case BROADCAST:
			 				broadcast_msg(user_list, command, server_shell.ptoc[1], user_list[i].name);
			 				break;
			 		}
		 		 }
		 	}

		}
	}	/* while loop ends when server shell sees the \exit command */

	return 0;
}
