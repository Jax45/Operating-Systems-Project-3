//Name: Jackson Hoenig
//Class: CMPSCI-4780-001
//Description:
//This program takes in data from an input file that is set with the option -i.
//if -i is not given then the default input file name is set to "input.dat". furthermore,
//The program also takes in options -o and -t, -o specifies the output file which by
//default is set to "output.dat", and -t specifies the time allowed for the entire program to run before it timesout
//and terminates. this timer is set at the very start of the program and can trigger at anytime which then
//calls the signal handler which closes the files and kills the child process.
//before timeing out. The format for the input file will have the first line be the number of subtask lines following
//that first line. once the number of subtasks is retrieved from the first line the program
//goes into a loop. at the start of the loop the input file is read and a child process is forked off
//to work on that subtask line. while the child is working the parent process will wait. The child
//process will then start a timer of 1 second and if it takes longer than that timer the child process
//prints a message and ends prematurely. if the child process is faster than 1 second then it looks for
//a subset of numbers that add up to the first number of the line. Note that the subset does not include the
//first number that will be used as the sum. that subset is then printed to screen or a message is printed that
//says that there was no subset sum possible. after all of this, the process ends. After all of the processes
//are completed the pid's of the child processes and the parent process are displayed and the program terminates.

//Import libraries
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>


//Global option values
char inputFile[50] = "input.dat";
char outputFile[50] = "output.dat";
int duration = 10;

//Global file pointers
FILE * outfilePtr;
FILE * infilePtr;

//Global Child PID for access in durationHandler
pid_t childPID;
//Handler for if the parent alarm goes off. it then kills children 
//reopens output and writes a message, then closes both input and output
//and exits.
void durationHandler(int sig){
	//kill children processes;
	//since parent calls this kill the only child process
	kill(childPID,SIGABRT);
	
	//fclose(outfilePtr);
	outfilePtr = freopen(outputFile,"w",outfilePtr);
	printf("The process timed out after %d second(s)\n",duration);
	fprintf(outfilePtr, "The process timed out after %d second(s)\n",duration);
	fclose(outfilePtr);
	fclose(infilePtr);
	exit(0);
}
//handler for just the child process's 1 second timeout.
//just prints a message and exits.
void alarmHandler(int sig){
	printf("No valid subset found after 1 second");
        fprintf(outfilePtr, "No valid subset found after 1 second\n");
	exit(0);
}
//this function is the function from GeeksForGeeks website but it has been modified
//the variable print determines if the function will print the data to the file or not.
//the bool recursionTop determines what level is the top level in order to correctly
//display the equation.
bool subSetSum(int arr[], int n, int sum, bool print, bool recursionTop){
	//Ending case
	if(sum == 0){
		return true;
	}
	if (n == 0 && sum != 0){
		return false;
	}
	// If last element is greater than sum, then ignore it 
	if (arr[n-1] > sum){
		
		return subSetSum(arr, n-1, sum, print, recursionTop); 
        }
        /* else, check if sum can be obtained by any of the following 
          (a) excluding the last element 
          (b) including the last element   */
	if(subSetSum(arr, n-1, sum, print, recursionTop)){
		
		return true;
	}
	if(subSetSum(arr, n-1, sum-arr[n-1], print, false)){
		if(print){
			//print arr[n-1]
			if(recursionTop){
				printf("%d = %d",arr[n-1],sum);
				fprintf(outfilePtr,"%d = %d\n",arr[n-1],sum);
			}
			else{
				printf("%d + ",arr[n-1]);
				fprintf(outfilePtr,"%d + ",arr[n-1]);
			}
		}
		return true;
	}
	else{
		return false;
	} 
}


int main(int argc, char *argv[]) {
        //variable for option switch
        int opt;
        //use getopt to iterate through each option.
        while((opt = getopt(argc, argv, "hi:o:t:")) != -1){
        	switch(opt){
        	//print usage and exit
        		case 'h':
				printf("Options:"
					"\n-h --Help option."
					"\n-i --Input file option takes in an argument for the input file name."
					"\n-o --Output file option takes in an argument for the output file name."
					"\n-t --Timeout set option, takes in a number for the timeout duration in seconds.\n"
					"Default values:\n"
					"Timeout default to 10 seconds\n"
					"output.dat and input.dat are default output and input files respectively\n"
					"Normal Running:\n"
					"Expected to read from input file number of subtasks, then read that many more lines from file.\n"
					"Output file is then wrote to the calculations of those subtasks or a timeout message if the timeout is exceeded.\n");
				exit(1);
			case 'i':
				if(strlen(optarg) > 45 || strlen(optarg) < 1){
					printf("The input file name was too large please use a shorter name.\n");
					return 0;
				}
				strncpy(inputFile,optarg,sizeof(inputFile));
				break;
			case 'o':
				if(strlen(optarg) > 45 || strlen(optarg) < 1){
					printf("The output file name was too long please use a shorter name.\n");
					return 0;
				}
				strncpy(outputFile,optarg,sizeof(outputFile)); 
				break;
			case 't':
				if(atoi(optarg) < 1){
					printf("The number for duration was negative or 0, please use a positive number less than a maximum integer\n");
					return 0;	
				}
				duration = atoi(optarg);
				break;
			default:
				printf("You gave an option that was not usable. Use -h for help on how to use this program\n");
				exit(1);
		}
	}
	//first set the timer for the whole process.
	alarm(duration);
	signal(SIGALRM,durationHandler);
	//open the input file
	infilePtr = fopen(inputFile,"r");
	char * task = NULL;
	size_t len = 0;
	int subtasks = 0;
	//get the first line which is the number of subtasks
	if(getline(&task, &len, infilePtr) != -1){
		subtasks = atoi(task);
	}
	else{
		perror("ERROR: logParse: Reading first number from file failed: ");
		exit(2);
	}
	
	//open outputfile for writing to clear it.
	outfilePtr = fopen(outputFile,"w");
	fclose(outfilePtr);
	
	//open the outputfile for appending
	outfilePtr = fopen(outputFile,"a");
	
	int i = 0;
	int firstNumInTask = -1;
	
	//array to store the pid's of children
	int *pidArray = (int*)malloc(sizeof(int) *subtasks);
	
	for(i = 0; i < subtasks; i++){
		if(getline(&task, &len, infilePtr) != -1){
			//fork here the child will be if the childPid == 0.
			childPID = fork();
			if(childPID == 0){
				//timer for subtask
				alarm(1);
				signal(SIGALRM,alarmHandler);
				//child process
				//split the task up and count the elements into i
				int size = 0;
	        		char line[256] = "";

				//this is to know how many elements we can tokenize.
	        		strncpy(line,task,sizeof(line)-1);
	        		line[255] = '\0';
	        		char *ptr = strtok(line, " ");
	        		while(ptr != NULL){
	        		        ptr = strtok(NULL, " ");
	        		        size++;
	        		}
	
				//now that we know the number of elements, do the actual split.
        			char newLine[256] = "";
        			strncpy(newLine,task,sizeof(newLine)-1);
        			newLine[255] = '\0';
				//make the buffer 1 less than the numbers
				//to ignore the first number which is the sum
        			int *numArray = (int*)malloc(sizeof(int) * size - 1);
        			char *newPtr = strtok(newLine, " ");
				//get the sum
				firstNumInTask = atoi(newPtr);
				newPtr = strtok(NULL, " ");
				//set the size back to zero so we can increment it.
        			size = 0;
        			while (newPtr != NULL){
        			        numArray[size] = atoi(newPtr);
                			size++;
               				newPtr = strtok(NULL, " ");
        			}
				//don't print just check if there is a sum
        			if(subSetSum(numArray,size,firstNumInTask,false,false)){
				//	printf("\nThere is a sum!\n");
					//print the pid
					printf("%ld: ",(long)getpid());
					fprintf(outfilePtr, "%ld: ",(long)getpid());
					//print the sum
					subSetSum(numArray,size,firstNumInTask,true,true);
					                       	
        			        //return selectedNums;
        			        //find the sum by sending combinations into isZeroSum once its true continue. else break
       				}
        			else {
                	                //if here then there is no sum of the subset
					printf("%ld: No subset of numbers summed to %d",(long)getpid(),firstNumInTask);
                			fprintf(outfilePtr, "%ld: No subset of numbers summed to %d\n",(long)getpid(),firstNumInTask);
					
					
        			}
				//free the dynamic array
				free(numArray);
				exit(0);
			}
			else{ 
			//parent process
			//this function will either kill the process after timeout or will just wait on it.
			wait(NULL);
			pidArray[i] = childPID;
			//end the line on the screen here.
			printf("\n");
			}
		
		}
		else{
		perror("ERROR: logParse: Expected a line but did not find one in file. ");
		exit(0);
		}
	}
	
        int k = 0;
	//display all the child pid's from the array
	for(k = 0; k < subtasks; k++){
		printf("Child PID: %ld\n",(long)pidArray[k]);
		fprintf(outfilePtr, "Child PID: %ld\n",(long)pidArray[k]);
	}
	//display the parent pid
	printf("Parent PID: %ld\n",(long)getpid());
        fprintf(outfilePtr, "Parent PID: %ld\n",(long)getpid());
        fclose(outfilePtr);
	fclose(infilePtr);
	
	return 0;
}
