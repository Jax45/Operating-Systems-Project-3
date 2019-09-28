#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>

struct Clock{
	int second;
	int nano;
};

int main(int argc, char  **argv) {
	printf("Inside process: %ld\n", (long)getpid());
	int shmid = atoi(argv[1]);
        struct Clock *shmclock = (struct Clock*) shmat(shmid,(void*)0,0);
        printf("%d seconds and %d nanoseconds",shmclock->second,shmclock->nano);
	shmclock->second += 1;
        shmclock->nano += 10;
        shmdt(shmclock);
	//exit(0);
	return 0;
}
