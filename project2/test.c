#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

int main(int argc, char const *argv[])
{
	pid_t pid = fork();
	if (pid == 0){
		execl("./shell", "shell", 0, 1, "Server", (char *)0);
	}
	else {
		wait(NULL);
		printf("Parent\n");
	}
}