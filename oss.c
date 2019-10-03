/************************************************************************************
 * Name: Jackson Hoenig
 * Class: CMPSCI 4760
 * Project 3
 * Description:
 *     This program sets a shared memory clock which is a structure of and int,
 *     and a long long int. then sets up a semaphore to guard that shared memory.
 *     then, this program sets up a message queue for communication between it and
 *     its children processes. this program will spawn the maximum number of 
 *     child processes at a time and increment the shared memory clock one ns every 
 *     loop it goes through. this program also will read messages coming back from the
 *     child processes and log them in the logfile given in the options.
 *     this program will only end in one of three ways:
 *     timeout, 100 processes spawned, or 2 seconds of logical shm time passed
 *     for more information see the Readme.
 *************************************************************************************
*/

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

//prototype for exit safe
void exitSafe(int);

//this function is used to create the wait and signal functions.
//it populates the sembuf struct with the parameters given
//i found this code in the textbook on chapter 15
void setsembuf(struct sembuf *s, int num, int op, int flg) {
	s->sem_num = (short)num;
	s->sem_op = (short)op;
	s->sem_flg = (short)flg;
	return;
}
//this function runs a semaphore operation "sops" on the
//semaphore with the id of semid. it will loop continuously
//through the operation until it comes back as true.
int r_semop(int semid, struct sembuf *sops, int nsops) {
	while(semop(semid, sops, nsops) == -1){
		if(errno != EINTR){
			return -1;
		}
	}
	return 0;
}
//a simple function to destroy the semaphore data.
int removesem(int semid) {
	return semctl(semid, 0, IPC_RMID);
}

//a function to initialize the semaphone to a number in semvalue
//it is used later to set the critical resource to number to 1
//since we only have one shared memory clock.
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
        
//get message queue id
int msgid;
                       
//array to hold the pid's of all the child processes
int pidArray[20];

//function is called when the alarm signal comes back.
void timerHandler(int sig){
	printf("Process has ended due to timeout\n");
	//kill the children
	int i = 0;
        for(i = 0; i < maxChildren; i++) {
                kill(pidArray[i],SIGABRT);
        }

	exitSafe(1);
	exit(1);
}
//function to exit safely if there is an errory or anything
//it removes the IPC data.
void exitSafe(int id){
	//destroy the Semaphore memory
	if(removesem(semid) == -1) {
               perror("Error: oss: Failed to clean up semaphore");
       }
	//destroy msg queue
	msgctl(msgid,IPC_RMID,NULL);
	//destroy shared memory 
        shmctl(shmid,IPC_RMID,NULL);
	exit(id);
}
//function is called when the number of processes spawned has reached 100
void maxProcesses(){
	printf("You have hit the maximum number of processes of 100, killing remaining processes and terminating.\n");
	int i = 0;	
	for(i = 0; i < maxChildren; i++) {
		kill(pidArray[i],SIGABRT);
		//waitpid(pidArray[i],0,0);
	}
	exitSafe(1);
}

//function to be called when the logical shm time has
//exceeded 2 seconds.
void logicalTimeout(){
	printf("2 seconds of logical time have elapsed. killing the remaining processes and terminating.\n");
	int i = 0;
        for(i = 0; i < maxChildren; i++) {
             kill(pidArray[i],SIGABRT);
        }
        exitSafe(1);
}

//structure for the shared memory clock
struct Clock{
	int second;
	long long int nano;
};

//structure for the message queue
struct mesg_buffer {
	long mesg_type;
	char mesg_text[100];
} message;

int main(int argc, char **argv){
	int opt;
	//get the options from the user.
	while((opt = getopt (argc, argv, "hs:l:t:")) != -1){
		switch(opt){
			case 'h':
				printf("Usage: ./oss [-s [Number Of Max Children]]] [-l [Log File Name]] [-t [timeout in seconds]]\n");
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
				printf("Wrong Option used.\nUsage: ./oss [-s [Number Of Max Children]] [-l [Log File Name]] [-t [timeout in seconds]]\n");
				exit(1);
		}
	}
	//set the countdown until the timeout right away.
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
		//fork off a child process
		pidArray[i] = fork();
		
		//if the fork failed then exit safely and say so/
		if(pidArray[i] == -1 || errno){
			//fork failed
			perror("Error: oss: Fork Failed.");
			exitSafe(1);
		}
		else if(pidArray[i] == 0){
			//printf("Forking process %ld\n",getpid());
			char arg[20];
			snprintf(arg, sizeof(arg), "%d", shmid);
			char spid[20];
			snprintf(spid, sizeof(spid), "%d", semid);
			char msid[20];
			snprintf(msid, sizeof(msid), "%d", msgid);
			execl("./user","./user",arg,spid,msid,NULL);
			perror("Error: oss: exec failed. ");
			//if this runs then execl failed
			exitSafe(0);
		}
		else{
			//parent continues.
		}
	
	}
	int processes = maxChildren;

	//loop indefinitely so the timer can catch us later.
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
			//Have the Clock then increment the ns by one.
			shmclock = (struct Clock*) shmat(shmid, (void*)0,0);
				
			if(shmclock->nano >= 1000000000){
				shmclock->second += (int)(shmclock->nano / 1000000000);
				shmclock->nano = (shmclock->nano % 1000000000) + 100;
                	}
			else{
		        	shmclock->nano += 100;
			}
							
			ns = shmclock->nano;
			sec = shmclock->second;
			if(sec >= 2) {
				logicalTimeout();
			}
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
		
		if (msgrcv(msgid, &message, sizeof(message), 1, IPC_NOWAIT) != -1){
                	
			//printf("MASTER: Child pid is terminating at my time %d.%d because it reached %s in child process\n",sec,ns,message.mesg_text);
                        fprintf(fp,"MASTER: Child pid is terminating at my time %d.%lld because it reached %s in child process\n",sec,ns,message.mesg_text);
                
               
			while ((childPid = waitpid(-1, &stat, WNOHANG)) > 0){
			//the child terminated execute another
			//	printf("Child has terminated %ld\n",childPid);
				int j = 0;
				for(j = 0; j < 20; j++){
					if(pidArray[j] == childPid){
						//printf("Found pid %ld, matching %ld at index %d",childPid,pidArray[j],j);
						break;
					}
				}
				if(processes >= 100){
					maxProcesses();
				}
				pidArray[j] = fork();
				
                                if(pidArray[j] == 0){
					
                                //	printf("Forking process: %ld",(long)getpid());
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
                                else if(pidArray[j] == -1 || errno){
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
	        }

	}
	//WE WILL NEVER REACH HERE BECAUSE OF TIMER.
	perror("Error: oss: reached area that shouldn't be reached.");
	exitSafe(1);

	return 0;
}
