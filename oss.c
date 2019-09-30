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
#include <sys/msg.h>
#define PERMS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

//prototypes
void exitSafe(int);


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

//File pointer is global so it can be closed
//FILE* fp;

//get shared memory
static int shmid;
static int semid;
static struct Clock *shmclock;
        
//get message queue id
int msgid;
                       
//array to hold the pid's of all the child processes
int pidArray[20];

void timerHandler(int sig){
	printf("Process has ended due to timeout\n");
//	shmdt(shmclock);
	//kill the children
	int i = 0;
        for(i = 0; i < maxChildren; i++) {
                kill(pidArray[i],SIGABRT);
        }

	exitSafe(1);
	exit(1);
}

void exitSafe(int id){
	if(removesem(semid) == -1) {
               perror("Error: oss: Failed to clean up semaphore");
       }
	//destroy msg queue
	msgctl(msgid,IPC_RMID,NULL);
	//destroy shared memory 
        shmctl(shmid,IPC_RMID,NULL);
//	fclose(fp);
	exit(id);
}
void maxProcesses(){
	printf("You have hit the maximum number of processes of 100, killing remaining processes and terminating.\n");
	int i = 0;	
	for(i = 0; i < maxChildren; i++) {
		kill(pidArray[i],SIGABRT);
		waitpid(pidArray[i],0,0);
	}
	/*for(i = 0; i < maxChildren; i++) {
               // kill(pidArray[i],SIGKILL);
                waitpid(pidArray[i],0,0);
        }*/
	exitSafe(1);
}

struct Clock{
	int second;
	long long int nano;
};

struct mesg_buffer {
	long mesg_type;
	char mesg_text[100];
} message;

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
	if(errno){
		perror("Error: oss: Shared Memory key not obtained: ");
		exitSafe(1);
	}	
	shmid = shmget(key,1024,0666|IPC_CREAT);
	if (shmid == -1 || errno){
		perror("Error: oss: Failed to get shared memory. ");
		exitSafe(1);
	}
	shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
	shmclock->second = 0;
	shmclock->nano = 0;
	shmdt(shmclock);

	//Initialize semaphore
	//get the key for shared memory
	key_t semKey = ftok("/tmp",'j');
	if( semKey == (key_t) -1 || errno){
                perror("Error: oss: IPC error: ftok");
                exitSafe(1);
        }
	//get the semaphore id
	semid = semget(semKey, 1, PERMS);
       	if(semid == -1 || errno){
		perror("Error: oss: Failed to create a private semaphore");
		exitSafe(1);	
	}
	
	//declare semwait and semsignal
	struct sembuf semwait[1];
	struct sembuf semsignal[1];
	setsembuf(semwait, 0, -1, 0);
	setsembuf(semsignal, 0, 1, 0);
	if (initElement(semid, 0, 1) == -1 || errno){
		perror("Failed to init semaphore element to 1");
		if(removesem(semid) == -1){
			perror("Failed to remove failed semaphore");
		}
		return 1;
	}
	//get message queue key
	key_t msgKey = ftok("/tmp", 'k');
	if(errno){
		
		perror("Error: oss: Could not get the key for msg queue. ");
		exitSafe(1);
	}
	//set message type
	message.mesg_type = 1;
		
	//get the msgid
	msgid = msgget(msgKey, 0600 | IPC_CREAT);
	if(msgid == -1 || errno){
		perror("Error: oss: msgid could not be obtained");
		exitSafe(2);
	}
	
	//open logfile
	FILE *fp;
	fp = fopen(logFile, "a");
	
	
	int i = 0;
	for(i = 0; i < maxChildren; i++){
		printf("fork %d\n",i+1);
		pidArray[i] = fork();
		
		if(pidArray[i] == -1 || errno){
			//fork failed
			perror("Error: oss: Fork Failed.");
		}
		else if(pidArray[i] == 0){
			char arg[20];
			snprintf(arg, sizeof(arg), "%d", shmid);
			char spid[20];
			snprintf(spid, sizeof(spid), "%d", semid);
			char msid[20];
			snprintf(msid, sizeof(msid), "%d", msgid);
			execl("./user","./user",arg,spid,msid,NULL);
			perror("Error: oss: exec failed. ");
			//if this runs then execl failed
			exit(0);
		}
		else{
			//parent continues.
		}
	
	}
	int processes = maxChildren;
	while(1){

		//local variable to hold current time
		long long int ns;
		int sec;
	
		//increment the clock
		
		//get cs
		if (r_semop(semid, semwait, 1) == -1){
			perror("Error: oss: Failed to lock semid. ");
			exitSafe(1);
		}
		else {
			shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
				
			if(shmclock->nano >= 1000000){
				shmclock->second += shmclock->nano / 1000000;
				shmclock->nano = shmclock->nano % 1000000 + 1;
                	}
			else{
		        	shmclock->nano += 1;
			}
		
			//shmclock->second += 1;
			//shmclock->nano += 100000;
			ns = shmclock->nano;
			sec = shmclock->second;
			//printf("OSS: second %d, nano %lld\n",sec,ns);
			shmdt(shmclock);
			//exit CS
			if (r_semop(semid, semsignal, 1) == -1) {
				perror("Error: oss: failed to signal Semaphore. ");
				exitSafe(1);
			}
		}
		int stat = 0;	
		pid_t childPid;
		//printf(" Out of CS in Master");
		if (msgrcv(msgid, &message, sizeof(message), 1, IPC_NOWAIT) != -1){
                	
			printf("received message: %s\n",message.mesg_text);
                        fprintf(fp,"MASTER: Child pid is terminating at my time %d.%d because it reached %s in child process\n",sec,ns,message.mesg_text);
                
               
			while ((childPid = waitpid(-1, &stat, WNOHANG)) > 0){
			//the child terminated execute another
				//printf("Child has terminated %ld\n",childPid);
				if(processes >= 100){
					maxProcesses();
				}
				int newChildPid;
				newChildPid = fork();
                                if(newChildPid == 0){
                                	printf("Forking process: %ld",(long)getpid());
                                	char arg[20];
                                	snprintf(arg, sizeof(arg), "%d", shmid);
                                	char spid[20];
                                	snprintf(spid, sizeof(spid), "%d", semid);
					char msid[20];
                		        snprintf(msid, sizeof(msid), "%d", msgid);
		                        execl("./user","./user",arg,spid,msid,NULL);
                                	perror("Error: oss: exec failed. ");
                                	exit(0);
                                }
                                else if(newChildPid == -1 || errno){
                        	        perror("Error: oss: second set of Fork failed");
                 	                exitSafe(1);
                                }
				//parent
				processes++;


			}
		}
	        else if(errno == ENOMSG){
                	//we did not receive a message
			errno = 0;
			//printf("No message");
                }

	}
		
		/*
			int stat = 0;
			pid_t childPid = waitpid(-1, &stat, WNOHANG);
			
		
				if(errno){
					perror("Error: oss: Waitpid failed.");
					exitSafe(1);
				}else if(childPid > 0){
					if (msgrcv(msgid, &message, sizeof(message), 1, MSG_NOERROR | IPC_NOWAIT) != -1){
                                		printf("received message: %s\n",message.mesg_text);
                                		fprintf(fp,"MASTER: Child: xx.yy: %s",message.mesg_text);
                        		}
					if(errno == ENOMSG){
						errno = 0;
						//since there is not a message keep going like nothing went wrong
					}
					//child terminated
					if(processes >= 100){
						maxProcesses();
					}
					//pidArray[i] = fork();
					childPid = fork();
					if(childPid == 0){
						processes++;
						printf("Forking process: %ld",(long)getpid());
						char arg[20];
                        			snprintf(arg, sizeof(arg), "%d", shmid);
                        			char spkey[20];
                        			snprintf(spkey, sizeof(spkey), "%d", semid);
                        			execl("./user","./user",arg,spkey,NULL);
                        			perror("Error: oss: exec failed. ");
                                	        exit(0);
					}
					else if(childPid == -1 || errno){
						perror("Error: oss: second set of Fork failed");
						exitSafe(1);
					}
				}
			
		}
	*/

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
