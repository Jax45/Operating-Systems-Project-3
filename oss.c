#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 

//Global option values
int maxChildren = 5;
char* logFile = "log.txt";
int timeout = 5;

//get shared memory
static int shmid;
static struct Clock *shmclock;
                               

void timerHandler(int sig){
	printf("Process has ended\n");
//	shmdt(shmclock);
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
	key_t key = ftok("shmclock",45);

	shmid = shmget(key,1024,0666|IPC_CREAT);
	shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
	shmclock->second = 0;
	shmclock->second = 0;
	shmdt(shmclock);
	int currentIndex = 0;
	int pidArray[20] = {-1};	
	int i = 0;
	for(i =0; i < maxChildren; i++){
		//printf("fork %d\n",i+1);
		pidArray[currentIndex] = fork();
		
		if(pidArray[currentIndex] == -1){
			//fork failed
			perror("Error: oss: Fork Failed.");
		}
		else if(pidArray[currentIndex] == 0){
			printf("Child Process\n");
			char arg[20];
			snprintf(arg, sizeof(arg), "%d", shmid);
			//argv[0] = "./child";
			//sprintf(argv,"%d",shmid);
			//argv[2] = "NULL";
			execl("./child","./child",arg,NULL);
			perror("Error: oss: exec failed. ");
			//execvp("./child",argv);	
			//execl("./child",argv[0],shmid,NULL);
			//if this runs then execvp failed
			//printf("Unknown Command\n");
			exit(0);
		}
		else{
			printf("Parent Process\n");
			//wait nonblocking
			wait(NULL);
			shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
			printf("%d sec, %d nano\n",shmclock->second,shmclock->nano);
			shmdt(shmclock);
			//shmctl(shmid,IPC_RMID,NULL);
			//return 0;
			//exit(1);
		}
	}
	
	//detach from shared memory  
	//shmdt(shmclock); 
	            
	//destroy the shared memory
	shmctl(shmid,IPC_RMID,NULL); 
	
	return 0;
}
