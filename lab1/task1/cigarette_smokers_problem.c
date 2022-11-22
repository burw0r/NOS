
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

#define PROC_NUM 4
#define ALLOW_CONST 100
#define DONE_CONST 200
pid_t parent_pid;

struct my_msgbuf {
	long mtype;
	int data;
};

struct my_msgbuf buff;
int msqid;
key_t key = 31337;

void clear(int signum)
{
	// killing all children processes
	pid_t self_pid = getpid();
	if (self_pid != parent_pid) {
		printf("[+] killing process %d\n ", self_pid);
		exit(0);
	} else {
		//parent deletes the message queue
		msgctl(msqid, IPC_RMID, NULL);
		printf("[+] Message queue removed\n");
	}
}

void agent()
{
	// printf(" -- agent  pid: %d -- \n", getpid());

	// attach to msg queue
	if ((msqid = msgget(key, 0600 | IPC_CREAT)) == -1) {
		perror("[!] msgget error");
		exit(1);
	}

	srand(time(NULL));

	while (1) {
		int ingredients = ((rand() % 3) + 1);

		/*
        ingredients can be:
            tobacco + matches -> 1
            matches + papers  -> 2
            papers  + tobacco -> 3
    */

		switch (ingredients) {
		case 1:
			printf("[+] Agent puts tobacco and matches on the table\n");
			break;

		case 2:
			printf("[+] Agent puts matches and papers on the table\n");
			break;

		case 3:
			printf("[+] Agent puts papers and tobacco on the table\n");
			break;
		}

		if (msgrcv(msqid, (struct my_msgbuf *)&buff, sizeof(buff.data), ingredients, 0) == -1) {
			perror("[!] msgrcv error");
			exit(1);
		}
		printf("[+] Smoker #%ld requests to take ingredients, agent allows.\n", buff.mtype);

		//allow the smoker to smoker to loght the cigarette (enter critical section )
		buff.mtype = (ingredients + ALLOW_CONST);
		if (msgsnd(msqid, (void *)&buff, sizeof(buff.data), 0) == -1) {
			perror("[!] msgsnd error");
			exit(1);
		}
		//wait for smoker to signal done smoking
		if (msgrcv(msqid, (struct my_msgbuf *)&buff, sizeof(buff.data), (ingredients + DONE_CONST), 0) == -1) {
			perror("[!] msgrcv error");
			exit(1);
		}
		printf("[+] Smoker #%ld done done smoking\n\n", (buff.mtype - DONE_CONST));
		printf("--------------------------------------------------------- \n");
	}
}

void smoker(int i)
{
	// printf(" -- smoker pid: %d -- \n", getpid());

	// attach to msg queue
	if ((msqid = msgget(key, 0600 | IPC_CREAT)) == -1) {
		perror("[!] msgget error");
		exit(1);
	}

	while (1) {
		//request to take ingredients (enter critical section)
		buff.mtype = i;
		buff.data = 3108;
		if (msgsnd(msqid, (void *)&buff, sizeof(buff.data), 0) == -1) {
			perror("[!] msgsnd error");
			exit(1);
		}

		//wait for agents approval
		if (msgrcv(msqid, (struct my_msgbuf *)&buff, sizeof(buff.data), (i + (ALLOW_CONST)), 0) == -1) {
			perror("[!] msgrcv error");
			exit(1);
		}
		printf("[+] Smoker #%d lights and smokes the cigarette\n", i);
		sleep(3);

		//signal to agent that the smoker is done smoking
		buff.mtype = (i + DONE_CONST);
		if (msgsnd(msqid, (void *)&buff, sizeof(buff.data), 0) == -1) {
			perror("[!] msgsnd error");
			exit(1);
		}
	}
}

int main()
{
	signal(SIGINT, clear);
	parent_pid = getpid();

	printf("[+] Cigarette smoker #1 has papers  \n");
	printf("[+] Cigarette smoker #2 has tobacco \n");
	printf("[+] Cigarette smoker #3 has matches \n");
	printf("----------------------------------- \n");

	pid_t pid;
	for (int i = 0; i < PROC_NUM; i++) {
		pid = fork();

		switch (pid) {
		case -1:
			printf("Failed to create process\n");
			exit(1);

		case 0:
			if (i == 0) {
				agent();
			} else {
				smoker(i);
			}
			exit(0);
		}
	}

	for (int i = 0; i < PROC_NUM; i++) {
		wait(NULL);
	}

	return 0;
}