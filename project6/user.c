#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <setjmp.h>


#include "oss.h"


void connectClock(char* );
char *formatTimeToString(unsigned int , unsigned int );
void killHandler(int);


char* executable;


int main(int argc, char **argv){
    executable = argv[0];
    char error_string[100];
    int pctLocation = atoi(argv[1]);
    int i;

    pid_t pid = getpid();
    pid_t ppid = getppid(); 


    int pageRequesting;
    int address;
    int memoryRequestType;
    int memoryRequestChance;
    int terminationChance;
    int numberOfRequests = 0;

    bool initialRun = true;


    /* Seeding the random number generator with a unique number for each process */
    srand(time(0) * pid);


    /* Signal Handling */
    signal(SIGINT, killHandler);
    signal(SIGTSTP, killHandler);


    /* Connecting to shared memory segments */
    connectClock(executable);

    /* Connecting to message queue */
    if ((msgId = msgget(MSG_KEY, 0666 | IPC_CREAT)) == -1) {
        perror("Error: USER attaching to msgqueue");
        exit(1);
    }


    i = 0;
    while (true) {
	/* Setting the general message contents at the top so they get initilzied each run */
        message.mesq_type = ppid;
        message.mesq_pid = pid;
        message.mesq_pctLocation = pctLocation;
        message.mesq_terminate = 0;
        message.mesq_sentSeconds = shmClock->seconds;
        message.mesq_sentNS = shmClock->nanoseconds;


	/* Calculating whether to terminate or not will not*/
	terminationChance = (rand() % 10 + 1);
        if(numberOfRequests >= 100){
	    if(terminationChance > 7){
		/* Updating the message contents to let OSS know user is terminating */
            	message.mesq_terminate = 1;

            	if( msgsnd(msgId, &message, sizeof(message), 1) == 1){
                    perror("\n\tUSER: Failed to send message to OSS.\n");
                    exit(1);
            	}

            	break;
	    }

	}

	initialRun = false;


	/* Putting random numbers between 1 and 10.  If either is 1-5 it will do a task, if they are 6-10 they will do another task */
        memoryRequestChance = (rand() % 10 + 1);
        if(memoryRequestChance <= 5)
            memoryRequestType = READ;
        else
            memoryRequestType = WRITE;


	/* Calculating a random page to request, there are */
	pageRequesting = (rand() % 32);

	address = (rand() % 32000);


	/* Setting more specific message data. */
	message.mesq_pageReference = pageRequesting;
        message.mesq_memoryAddress = address;
        message.mesq_requestType = memoryRequestType;


        if( msgsnd(msgId, &message, sizeof(message), 1) == 1){
	    perror("\n\tUSER: Failed to send message to OSS.\n");
            exit(1);
        }


	/* Waiting for a response from OSS */
        if(msgrcv(msgId, &message, sizeof(message), pid, 0) == -1){
	    perror("\n\tUSER: Failed to receive message\n");
	    exit(1);
	}

        numberOfRequests++;
    }

    /* Detaching from shared memory */
    shmdt(shmClock);

    return 0;
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


void killHandler(int dummy) {
    char error_string[100]; 
    errno = EINTR;
    snprintf(error_string, 100, "\n\n%s: Interrupt signal was sent, cleaning up memory segments ", executable);
    perror(error_string);
    fprintf(stderr, "\tValue of errno: %d\n\n", errno);

    /* Detaching shared memory */
    shmdt(shmClock);

    exit(EXIT_SUCCESS);     
}

