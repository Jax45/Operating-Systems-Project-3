#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <errno.h>
#define PERMS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

void setsembuf(struct sembuf *s, int num, int op, int flg) {
	s->sem_num = (short)num;
	s->sem_op = (short)op;
	s->sem_flg = (short)flg;
	return;
}

int r_semop(int semid, struct sembuf *sops, int nsops) {
	while(semop(semid, sops, nsops) == -1){
		if(errno != EINTR){
			return -1;
		}
	}
	return 0;
}

int removesem(int semid) {
	return semctl(semid, 0, IPC_RMID);
}


int initElement(int semid, int semnum, int semvalue) {
	union semun {
		int val;
		struct semid_ds *buf;
		unsigned short *array;
	} arg;
	arg.val = semvalue;
	return semctl(semid, semnum, SETVAL, arg);
}

//Global option values
int maxChildren = 5;
char* logFile = "log.txt";
int timeout = 5;

//get shared memory
static int shmid;
static int semid;
static struct Clock *shmclock;
                               
//array to hold the pid's of all the child processes
int pidArray[20];

void timerHandler(int sig){
	printf("Process has ended due to timeout\n");
//	shmdt(shmclock);
	//kill the children
	int i = 0;
        for(i = 0; i < maxChildren; i++) {
                kill(pidArray[i],SIGKILL);
        }

	//destroy the semaphore
	if(removesem(semid) == -1) {
	       perror("Error: oss: Failed to clean up semaphore");
	}
	
	shmctl(shmid,IPC_RMID,NULL);
	exit(1);
}

void maxProcesses(){
	printf("You have hit the maximum number of processes of 100, killing remaining processes and terminating.\n");
	int i = 0;	
	for(i = 0; i < maxChildren; i++) {
		kill(pidArray[i],SIGKILL);
	}

	if(removesem(semid) == -1) {
               perror("Error: oss: Failed to clean up semaphore");
       }

        shmctl(shmid,IPC_RMID,NULL);
        exit(1);
}

struct Clock{
	int second;
	int nano;
};

int main(int argc, char **argv){
	int opt;
	while((opt = getopt (argc, argv, "hs:l:t:")) != -1){
		switch(opt){
			case 'h':
				printf("Usage: ./oss [-s Number Of Max Children]\n-l Log File Name\n-t timeout in seconds\n");
				exit(1);
			case 's':
				maxChildren = atoi(optarg);
				if (maxChildren > 20){
					printf("-s option argument must be less than 20.");
					exit(0);
				}
				break;
			case 'l':
				logFile = optarg;
				break;
			case 't':
				timeout = atoi(optarg);
				break;
			
			default:
				printf("Wrong Option used.\nUsage: ./oss [-s Number Of Max Children]\n-l Log File Name\n-t timeout in seconds\n");
				exit(1);
		}
	}
	alarm(timeout);
        signal(SIGALRM,timerHandler);
	
	//get shared memory
	key_t key = ftok("./oss",45);

	shmid = shmget(key,1024,0666|IPC_CREAT);
	if (shmid == -1){
		perror("Error: oss: Failed to get shared memory. ");
		exit(1);
	}
	shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
	shmclock->second = 0;
	shmclock->second = 0;
	shmdt(shmclock);


	//Initialize semaphore
	//get the key for shared memory
	key_t semKey = ftok("/tmp",'j');
	if( semKey == (key_t) -1){
                perror("Error: oss: IPC error: ftok");
                exit(1);
        }
	//get the semaphore id
	semid = semget(semKey, 1, PERMS);
       	if(semid == -1){
		perror("Error: oss: Failed to create a private semaphore");
		exit(1);	
	}
	/* if(semid != -1){
                struct sembuf spbuffer;
                spbuffer.sem_num = 0;
                spbuffer.sem_op = 1;
                spbuffer.sem_flg = 0;
                if(semop(semid, &spbuffer, 1) == -1){
                        perror("Error: child: IPC error: semop");
                         exit(1);
                }
        }
        else if (errno == EEXIST) {
                if((semid = semget(semKey, 0, 0)) == -1) {
                        perror("Error: oss: IPC error 1: semget");
                        exit(1);
                }
        }
        else {
                perror("Error: oss: IPC error 2: semget");
                        exit(1);
        }*/
	//declare semwait and semsignal
	struct sembuf semwait[1];
	struct sembuf semsignal[1];
	setsembuf(semwait, 0, -1, 0);
	setsembuf(semsignal, 0, 1, 0);
	if (initElement(semid, 0, 1) == -1){
		perror("Failed to init semaphore element to 1");
		if(removesem(semid) == -1){
			perror("Failed to remove failed semaphore");
		}
		return 1;
	}
	


	
	int i = 0;
	for(i = 0; i < maxChildren; i++){
		//printf("fork %d\n",i+1);
		pidArray[i] = fork();
		
		if(pidArray[i] == -1){
			//fork failed
			perror("Error: oss: Fork Failed.");
		}
		else if(pidArray[i] == 0){
			char arg[20];
			snprintf(arg, sizeof(arg), "%d", shmid);
			char spkey[20];
			snprintf(spkey, sizeof(spkey), "%d", semid);
			execl("./child","./child",arg,spkey,NULL);
			perror("Error: oss: exec failed. ");
			//if this runs then execl failed
			exit(0);
		}
		else{
			//printf("Parent Process\n");
			//wait nonblocking
			//wait(NULL);
			//shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
			//printf("%d sec, %d nano\n",shmclock->second,shmclock->nano);
			//shmdt(shmclock);
			//shmctl(shmid,IPC_RMID,NULL);
			//return 0;
			//exit(1);
		}
	
	}
	int processes = maxChildren;
	while(1){	
		//int cpid = waitpid(, &stat, WNOHANG);
		for(i = 0; i < maxChildren; i++){
			int stat;
			/*if(pidArray[i] == -1){
				continue;
			}*/
			int cpid = waitpid(pidArray[i], &stat, WNOHANG);
			if (WIFEXITED(stat)){
			//if(cpid == pidArray[i]){
				//child terminated
				int index;
				for(index = 0; index < maxChildren; index++){
					if (pidArray[index] == cpid){
						break;
					}
				}
				if(processes >= 100){
					maxProcesses();
				}
				pidArray[index] = fork();
				processes++;
				if(pidArray[index] == 0){
					printf("Forking process: %ld",(long)getpid());
					char arg[20];
                        		snprintf(arg, sizeof(arg), "%d", shmid);
                        		char spkey[20];
                        		snprintf(spkey, sizeof(spkey), "%d", semid);
                        		execl("./child","./child",arg,spkey,NULL);
                        		perror("Error: oss: exec failed. ");
                        		//if this runs then execl failed
                                        exit(0);
				}
				else if(pidArray[i] == -1){
					perror("Error: oss: fork failed");
				}
				else{
					printf("%ld finished\n",(long)pidArray[i]);
				}
			}
		}
	}

	//WE WILL NEVER REACH HERE BECAUSE OF TIMER.
	


	//detach from shared memory  
	//shmdt(shmclock); 
	            
	//destroy the shared memory
	shmctl(shmid,IPC_RMID,NULL); 
	

	//destroy the semaphore
	if(removesem(semid) == -1) {
		perror("Error: oss: Failed to clean up semaphore");
		return 1;
	}
	return 0;
}
