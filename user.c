#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/msg.h>
#include <stdbool.h>
#define PERMS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

void signalHandler(int sig);

void setsembuf(struct sembuf *s, int num, int op, int flg) {
        s->sem_num = (short)num;
        s->sem_op = (short)op;
        s->sem_flg = (short)flg;
        return;
}

int r_semop(int semid, struct sembuf *sops, int nsops) {
        signal(SIGKILL,signalHandler);
	while(semop(semid, sops, nsops) == -1){
                if(errno != EINTR){
                        return -1;
                }
	}
        return 0;
}


struct Clock{
	int second;
	long long int nano;
};


void signalHandler(int sig){
	exit(1);
}

struct mesg_buffer {
	long mesg_type;
	char mesg_text[100];
} message;

int main(int argc, char  **argv) {
	srand(getpid());
	signal(SIGABRT,signalHandler);	
	struct sembuf semsignal[1];
	struct sembuf semwait[1];
	int error;	
	//printf("Inside process: %ld\nsemid after: %d\n", (long)getpid(),atoi(argv[2]));
	//get semaphore id	
	int shmid = atoi(argv[1]);	
	int semid = atoi(argv[2]);
	//int semid = semget(semKey, 1, PERMS);
        if(semid == -1){
                perror("Error: user: Failed to create a private semaphore");
                exit(1);
        }	
	
	setsembuf(semwait, 0, -1, 0);
	setsembuf(semsignal, 0, 1, 0);
	
	//get the random numbers
	long long int ns = rand() % 1000000 + 1;
	int sec;		
	if ((error = r_semop(semid, semwait, 1)) == -1){
		//uncomment this for debugging but it shows up after the parent alarm triggers.

		//	perror("Error: user: Child failed to lock semid. ");
		exit(1);
	}
	else if (!error) {
		//inside critical section
	        struct Clock *shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
	 //       printf("%d seconds and %d nanoseconds",shmclock->second,shmclock->nano);
		ns += shmclock->nano;
		sec = shmclock->second;
		printf("\nentering loop, current ns: %lld, end ns: %lld\n",shmclock->nano,ns);
	      
	        shmdt(shmclock);
	
	
		//exit the Critical Section
		if ((error = r_semop(semid, semsignal, 1)) == -1) {
			printf("Failed to clean up");
			return 1;
		}
        //        printf("Sending message back from %ld\n", (long)getpid());
	
	}
	//printf("entering loop, current time: %d, end time: %d\n",n
	int clockSec;
	long long int clockNs;
	bool timeElapsed = false;
	while(!timeElapsed){
		printf("%ld: still working...\n",getpid());
		if ((error = r_semop(semid, semwait, 1)) == -1){
	                perror("Error: user: Child failed to lock semid. ");
	                return 1;
	        }
	        else if (!error) {
		
			//inside CS
	                struct Clock *shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
			if(shmclock->nano >= ns || shmclock->second >= sec){
				timeElapsed = true;
				clockNs = shmclock->nano;
				clockSec = shmclock->second;
			}
			printf("\nGot this from shmclock: %d, %d, bool: %d\n",clockSec,clockNs,timeElapsed);
			shmdt(shmclock);

			//exit CS
	                if ((error = r_semop(semid, semsignal, 1)) == -1) {
	                        printf("Failed to clean up");
       		                return 1;
                	}

		}
	}
	//send message
	int msgid = atoi(argv[3]);
        message.mesg_type = 1;
        sprintf(message.mesg_text, "%d:%lld", clockSec, clockNs);
        msgsnd(msgid, &message, sizeof(message), 0);

	return 0;
}	
