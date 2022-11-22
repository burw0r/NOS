
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>

#define PROC_NUM 6

struct proc_t {
	pid_t pid;
	long logic_clock;
	long crit_cntr;
};
typedef struct proc_t proc_t;

struct pipe_fd {
	int write;
	int read;
};
typedef struct pipe_fd pipe_fd;

enum msg_type { req, reply, done };

struct msg {
	msg_type type;
	int i;
	int time;
};
typedef struct msg msg;

pid_t parent_pid;
int shm_id;
proc_t *shm_ptr;
pipe_fd pipe_descriptors[PROC_NUM];

void shm_init()
{
	/*
	creates shared memory segment 
	with size of PROC_NUM*sizeof(proc_t)
	*/
	if ((shm_id = shmget(IPC_PRIVATE, (PROC_NUM * sizeof(proc_t)), 0600)) == -1) {
		perror("[!] shmget error\n");
		exit(1);
	}

	shm_ptr = (proc_t *)shmat(shm_id, NULL, 0);
	if (shm_ptr == (proc_t *)-1) {
		perror("[!] shmat error\n");
		exit(1);
	}
}

void pipes_init()
{
	/*
	create PROC_NUM pipes and save their 
	descriptors to array pipe_descriptors
	*/
	for (int i = 0; i < PROC_NUM; i++) {
		int fd[2];

		if (pipe(fd) == -1) {
			perror("[!] error creating pipe\n");
			exit(1);
		}
		pipe_descriptors[i].read = fd[0];
		pipe_descriptors[i].write = fd[1];
	}
}

bool compare_msg(msg m1, msg m2)
{
	if (m1.time < m2.time) {
		return true;
	} else if (m1.time > m2.time) {
		return false;
	} else {
		return m1.i < m2.i;
	}
}

void proc(int i)
{
	srand(time(NULL));
	std::vector<msg> wait_q;
	int local_logic_clock = 0;
	int critical_sec_counter_local = 5;

	/*
	initialize values in shared memory 
	for corresponding process
	*/
	shm_ptr[i].pid = getpid();
	shm_ptr[i].crit_cntr = 0;
	shm_ptr[i].logic_clock = 0;

	// close(pipe_descriptors[i].write); TODO

	while (critical_sec_counter_local > 0) {
		/****lamports distributed algorithm****/

		/*
		process wants to enter critical section,
		so generates request(i, time(i)) where time(i)
		is value of Logical watch for process i
			1) pushes req to own wait queue
			2) notifies other processes (broadcast)
		*/

		// 1)
		msg r1;
		r1.i = i;
		r1.time = local_logic_clock;
		r1.type = msg_type::req;
		wait_q.push_back(r1);
		std::sort(wait_q.begin(), wait_q.end(), compare_msg);

		// 2)
		for (int j = 0; j < PROC_NUM; j++) {
			if (j == i) {
				continue;
			} else {
				write(pipe_descriptors[j].write, &r1, sizeof(msg));
				printf("[+] process #%d sending  msg{type:%d i:%d time:%d}\n", i, r1.type, r1.i, r1.time);
			}
		}

		/*
		when process j receives req(i, time(i))
			1) sync local logical clock
			2) push req(i, time(i)) to wait queue
			3) sends reponose reply(i, time(j))
		*/
		msg r2;
		int reply_count = (PROC_NUM - 1);
		while (true) { // while ( (reply_count > 0))
			read(pipe_descriptors[i].read, &r2, sizeof(msg));
			printf("[+] process #%d received msg{type:%d i:%d time:%d}\n", i, r2.type, r2.i, r2.time);

			/* 1 */
			local_logic_clock =
				std::max(local_logic_clock, r2.time) + 1; // na svaku poruku ili svaki zahtjev?

			if (r2.type == msg_type::req) {
				/* 2 */
				wait_q.push_back(r2);
				std::sort(wait_q.begin(), wait_q.end(), compare_msg);

				/* 3 */
				msg temp;
				temp.type = msg_type::reply;
				temp.i = r2.i;
				temp.time = local_logic_clock;
				write(pipe_descriptors[r2.i].write, &temp, sizeof(msg));
				printf("[+] process #%d sending  msg{type:%d i:%d time:%d}\n", i, temp.type, temp.i, temp.time);

			} else if (r2.type == msg_type::reply) {
				reply_count--;

			} else if (r2.type == msg_type::done) {
				/*
				after receiving "done with critical section"
				message remove the corresponding request 
				from its own queue
				*/
				
				std::vector<msg>::iterator iter = wait_q.begin();
				while(iter != wait_q.end()){
					if(  ((*iter).i == r2.i)   && ((*iter).time == r2.time) ){
						iter = wait_q.erase(iter);
					}else{
						++iter;
					}
				}
				

			}

			//  -------------------------------- CRITICAL SECTION -----------------------------------------
			/*
				  if all (PROC_NUM-1) processes replied and
				  its own request is first in the sorted vector
				  allow entering critical section				
				*/
			if ((reply_count <= 0) && (wait_q.front().i == i)) {
				critical_sec_counter_local--;
				printf("[+] --- Process %d is entering critical section ---\n", i);
				//update db
				shm_ptr[i].crit_cntr++;
				shm_ptr[i].logic_clock = local_logic_clock;

				//print db content

				for (int j = 0; j < PROC_NUM; j++) {
					printf("-------------------------------------------------------------------\n");
					printf(" - process %d: pid=%d, critical section counter=%ld, logic clock=%ld\n",
					       j, shm_ptr[j].pid, shm_ptr[j].crit_cntr, shm_ptr[j].logic_clock);
				}
				printf("\n\n");

				//sleep
				int rnd = (rand() % (2000 - 100 + 1)) + 100;
				std::this_thread::sleep_for(std::chrono::milliseconds(rnd));

				/* 
					to exit critical section:
						1) remove processes request from the queue
						2) broadcast done message to other processes
					*/

				//1
				msg tmp;
				tmp.type = msg_type::done;
				tmp.i = i;
				tmp.time = r1.time;
				wait_q.erase(wait_q.begin()); // already at the front of the queue

				// 2
				for (int j = 0; j < PROC_NUM; j++) {
					if (j == i) {
						continue;
					} else {
						write(pipe_descriptors[j].write, &tmp, sizeof(msg));
						printf("[+] process #%d sending  msg{type:%d i:%d time:%d}\n", i, tmp.type, tmp.i, tmp.time);
					}
				}

				break;
				//-----------------------------------------------------------------------------------------------
			}
		}
	}

	printf("[+] proc[%d] done\n", i);
}

void clear(int signum)
{
	/*
	killing all children processes
	*/
	pid_t self_pid = getpid();
	if (self_pid != parent_pid) {
		printf("[+] killing process %d\n", self_pid);
		exit(0);

	} else {
		/*
		 clearing shared memory
		*/
		printf("[+] deleting shared memory segment\n");
		(void)shmdt((char *)shm_ptr);
		(void)shmctl(shm_id, IPC_RMID, NULL);

		/*
		 deleting pipe descriptors
		*/
		printf("[+] deleting pipe descriptors\n"); // i ostali procesi trebaju svoju kopiju
		for (int i = 0; i < PROC_NUM; i++) {
			close(pipe_descriptors[i].read);
			close(pipe_descriptors[i].write);
		}
	}
}

void processes_init()
{
	/*
	creates PROC_NUM child processes
	*/
	parent_pid = getpid();
	signal(SIGINT, clear);

	pid_t pid;
	for (int i = 0; i < PROC_NUM; i++) {
		pid = fork();

		switch (pid) {
		case -1:
			printf("Failed to create process\n");
			exit(1);

		case 0:
			proc(i);
			exit(0);
		}
	}

	for (int i = 0; i < PROC_NUM; i++) {
		wait(NULL);
	}
}

int main()
{

	shm_init();
	pipes_init();
	processes_init();

	return 0;
}