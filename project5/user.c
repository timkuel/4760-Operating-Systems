#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <setjmp.h>


#include "pcb.h"
#include "clock.h"
#include "sem.h"
#include "descriptor.h"
#include "mesq.h"



#define PCB_KEY 0x9630
#define CLOCK_KEY 0x3596
#define SEM_KEY 0x1470
#define DES_KEY 0x1596
#define MSG_KEY 0x0752


void connectPcb(char* );
void connectClock(char* );
void connectSemaphore(char* );
void connectDescriptor(char* );
char *formatTimeToString(unsigned int , unsigned int );
void killHandler(int );


PCB *shmPcb;
CLOCK *shmClock;
SEMAPHORE *shmSem;
DESCRIPTOR *shmDes;
MESQ message;


int pcbId;
int clockId;
int semId;
int desId;
int msgId;

jmp_buf buf;

int main(int argc, char **argv){
    int pctLocation = atoi(argv[1]);
    int i;
    int terminate = 0;
    int held = 0;

    char error_string[100];
 
    unsigned int timeToCheckSeconds;
    unsigned int timeToCheckNS;
    unsigned int timeToTerminateSeconds;
    unsigned int timeToTerminateNS;

    /* Signal Handling */
    signal(SIGINT, killHandler);
    signal(SIGTSTP, killHandler);

    if (setjmp(buf) == 1) {
	errno = EINTR;
        snprintf(error_string, 100, "\n\n%s: Interrupt signal was sent, cleaning up memory segments ", argv[0]);
        perror(error_string);
        fprintf(stderr, "\tValue of errno: %d\n\n", errno);

	/* Detaching shared memory */
	shmdt(shmPcb);
        shmdt(shmClock);
        shmdt(shmSem);
        shmdt(shmDes);

	exit(EXIT_SUCCESS);
    }


    /* Connecting to shared memory segments */
    connectPcb(argv[0]);
    connectClock(argv[0]);
    connectSemaphore(argv[0]);
    connectDescriptor(argv[0]);

    /* Connecting to message queue */
    if ((msgId = msgget(MSG_KEY, 0666 | IPC_CREAT)) == -1) {
        perror("Error: USER attaching to msgqueue");
        exit(1);
    }


    /* Setting the time to check resources */
    srand(time(0) * shmClock->nanoseconds);
    timeToCheckNS = (unsigned int)(rand() % 250000000);

    timeToCheckSeconds = shmClock->seconds;
    timeToCheckNS += shmClock->nanoseconds;

    if(timeToCheckNS > 999999999){
	timeToCheckNS -= 1000000000;

	timeToCheckSeconds += 1;
    }



    /* Setting the time to try and terminate (1 second after starting to run) */
    timeToTerminateSeconds = (shmClock->seconds + 1);
    timeToTerminateNS = shmClock->nanoseconds;


    /* Generating the max claims for the process (What the process is asking for to complete) 
     * If what each resource holds is greater than 10, only allow a process to grab upto 10,
     * else, allow the process to grab upto what each resource holds.*/
    for(i = 0; i < numResources; i++){
	if(shmDes->resource_vector[i] > 10){
	    shmDes->request_matrix[pctLocation][i] = (rand() % 11);
	}
	else{
	    shmDes->request_matrix[pctLocation][i] = ((rand() % (shmDes->resource_vector[i] + 1)));
	}
    }



    /* Generating the resources that this process will be granted.
     * Only for resources its requesting from, and in the range of whats left to be given out currently. */
    for(i = 0; i < numResources; i++){

	/* Rand will fail if it tries to calculate a number in the range of 0 (not a range) */
	if(shmDes->request_matrix[pctLocation][i] != 0){

	    /*If whats being requested is greater then whats left, allocate it in range of whats left.
 	     *Else allocate a resource in the range of whats being requested		*/
	    if(shmDes->request_matrix[pctLocation][i] > shmDes->allocation_vector[i]){
		shmDes->allocation_matrix[pctLocation][i] = ((rand() % (shmDes->allocation_vector[i] + 1)));
	    }
	    else{
		shmDes->allocation_matrix[pctLocation][i] = ((rand() % (shmDes->request_matrix[pctLocation][i] + 1)));
	    }

            /* Removing resources that were allocated out, ignoring sharable resources.
 	     * In the else block, since resources are sharable, allocating what that process needs for that resource */
            if(shmDes->sharable_resources[i] != 1){
                shmDes->allocation_vector[i] -= shmDes->allocation_matrix[pctLocation][i];
            }
	    else{
		shmDes->allocation_matrix[pctLocation][i] = shmDes->request_matrix[pctLocation][i];
	    }

	}

    }


    i = 0;
    while (true) {
	usleep(500);
	/* critical section */
	sem_wait(&(shmSem->mutex));


	/* Checking whether to do the task for the resource or not */	
	if(strcmp(formatTimeToString(shmClock->seconds, shmClock->nanoseconds), formatTimeToString(timeToCheckSeconds, timeToCheckNS)) > 0){

	    /* If the process has been in the system for at least 1 second, try and terminate */
	    if(strcmp(formatTimeToString(shmClock->seconds, shmClock->nanoseconds), formatTimeToString(timeToTerminateSeconds, timeToTerminateNS)) >= 0){
		/* Termination criteria, if rand number is a multiple of 5 (5 or 10) terminate.  There is a 1/5 chance to terminate after 1 second. */
		terminate = (rand() % 11);
		if(terminate % 5 == 0){
		    /*dont request more, and tell parent termianting.*/
                    shmDes->terminating[pctLocation] = 1;
		}
	    }
	    /* Process was granted resources, but is still running */
	    else if(shmDes->allocate[pctLocation] == 1){
		/*Dont request for more, parent should do this*/
		shmDes->request[pctLocation] = 0;
		held++;		

		
	    	if(held >= 5){
		    shmDes->terminating[pctLocation] = 1;
		}

	    }
	    /* Process was told to terminate without being granted any resource (used to help manage deadlocks) */
	    else if(shmDes->release[pctLocation] == 1){
                shmDes->terminating[pctLocation] = 1;
	    }
	    /* Proces has not requested a resource before */
	    else if(shmDes->request[pctLocation] == 0){
		shmDes->request[pctLocation] = 1;
	    }
	    /* Process is in suspended state, wait for a message to awaken */
	    else if(shmDes->suspended[pctLocation] == 1){
		/* Ceeding the critical section */
        	sem_post(&(shmSem->mutex));		
	
		if (msgrcv(msgId, &message, sizeof(message.mesq_text), getpid(), MSG_NOERROR) == -1) {
        	    perror("Error: USER receiving message");
        	    exit(1);
    		}

		continue;
	    }
	    /* Process has requested resources, but they have not been granted */
	    else{
                shmDes->request[pctLocation] = 0;

                timeToCheckNS = (unsigned int)((rand() % 99000001) + 1000000);

                timeToCheckSeconds = shmClock->seconds;
                timeToCheckNS += shmClock->nanoseconds;

                if(timeToCheckNS > 999999999){
                    timeToCheckNS -= 1000000000;

                    timeToCheckSeconds += 1;
                }
	    }
	}
	/* Ceeding the critical section */
	sem_post(&(shmSem->mutex));

	if(shmDes->terminating[pctLocation] == 1){
            break;
        }
    }






    /* Detaching from shared memory */
    shmdt(shmPcb);
    shmdt(shmClock);
    shmdt(shmSem);
    shmdt(shmDes);

    return 0;
}


void connectPcb(char *executable) {
    char error_string[100];

    /* Getting the PCB ID  */
    if ((pcbId = shmget(PCB_KEY, numPCB * sizeof(PCB), 0644 | IPC_CREAT)) == -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to create PCB shared memory ", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }

    /* Attaching to PCB shared memory */
    if ((shmPcb = shmat(pcbId, NULL, 0)) == (void *) -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to attach to PCB shared memory ", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }
}


void connectClock(char *executable) {
    char error_string[100];

    /* Getting the clock ID */
    if ((clockId = shmget(CLOCK_KEY, sizeof(CLOCK), 0644 | IPC_CREAT)) == -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to create Clock shared memory ", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }

    /* Attaching to clock shared memory */
    if ((shmClock = shmat(clockId, NULL, 0)) == (void *) -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to attach to Clock shared memory ", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }
}


void connectSemaphore(char *executable){
    char error_string[100];

    /* Getting the semaphore ID */
    if((semId = shmget(SEM_KEY, sizeof(SEMAPHORE), 0644 | IPC_CREAT)) == -1){
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to allocate shared memory region for semaphore", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(-1);
    }

    /* Attaching to semaphore shared memory */
    if((shmSem = (SEMAPHORE*)shmat(semId, (void *)0, 0)) == (void *)-1){
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to attach to shared memory region for semaphore", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(-1);
    }
}


void connectDescriptor(char *executable){
    char error_string[100];

    /* Getting the semaphore ID */
    if((desId = shmget(DES_KEY, sizeof(DESCRIPTOR), 0644 | IPC_CREAT)) == -1){
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to allocate shared memory region for resource descriptor", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(-1);
    }

    /* Attaching to semaphore shared memory */
    if((shmDes = (DESCRIPTOR*)shmat(desId, (void *)0, 0)) == (void *)-1){
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to attach to shared memory region for resource descriptor", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(-1);
    }
}


/* Function to format time to the nanoseconds */
char *formatTimeToString(unsigned int seconds, unsigned int nanoseconds) {
    if(seconds  < 10){
        char* nanoTime = (char *) malloc(sizeof(char) * 12);;
        snprintf(nanoTime, 15, "%u.%.9d", seconds, nanoseconds);
        return nanoTime;
    }
    else{
        char* nanoTime = (char *) malloc(sizeof(char) * 13);;
        snprintf(nanoTime, 15, "%u.%.9d", seconds, nanoseconds);
        return nanoTime;
    }
}


void killHandler(int dummy) { longjmp(buf, 1); }

