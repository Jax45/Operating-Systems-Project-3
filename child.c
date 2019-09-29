#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <errno.h>
#include <sys/stat.h>
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


struct Clock{
	int second;
	int nano;
};

int main(int argc, char  **argv) {
	struct sembuf semsignal[1];
	struct sembuf semwait[1];
	int error;	
	//printf("Inside process: %ld\nsemid after: %d\n", (long)getpid(),atoi(argv[2]));
	//get semaphore id	
	
	int semid = atoi(argv[2]);
	//int semid = semget(semKey, 1, PERMS);
        if(semid == -1){
                perror("Error: oss: Failed to create a private semaphore");
                exit(1);
        }	
	
	setsembuf(semwait, 0, -1, 0);
	setsembuf(semsignal, 0, 1, 0);
	
	if ((error = r_semop(semid, semwait, 1)) == -1){
		perror("Error: child: Child failed to lock semid. ");
		return 1;
	}
	else if (!error) {
		//inside critical section
		int shmid = atoi(argv[1]);
	        struct Clock *shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
	 //       printf("%d seconds and %d nanoseconds",shmclock->second,shmclock->nano);
	        shmclock->second += 1;
	        shmclock->nano += 10;
	        shmdt(shmclock);
	
	
		//exit the Critical Section
		if ((error = r_semop(semid, semsignal, 1)) == -1) {
			printf("Failed to clean up");
			return 1;
		}
		return 0;
	}
	return 0;
}	
/*	sem_wait(&mutex);
	int shmid = atoi(argv[1]);
        struct Clock *shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
        printf("%d seconds and %d nanoseconds",shmclock->second,shmclock->nano);
	shmclock->second += 1;
        shmclock->nano += 10;
        shmdt(shmclock);
	//exit(0);
	return 0;
}*/
