#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>


#include "oss.h"


struct Queue{
    int front, rear, size;
    unsigned capacity;
    int *array;
} Queue;


struct Process{
    /* Pid is used for termination criteria */
    long pid;
    /* Page table information (32 cells for the 32K of memory, with each cell being 1K) */
    int pageTable[PTABLE_SIZE];
} Process;


typedef struct {
    int occupiedBit;
    int referenceByte;
    int dirtyBit;
    long pid;
    int processPageReference;
} Frame;


void connectClock(char *);
void incrementClock(unsigned int, unsigned int);
pid_t childLauncher(int);
void initPCB(int, int);
void terminatingWork(Frame [MEMORY]);
void noPageFaultWork(Frame [MEMORY]);
void pageFaultWork(Frame [MEMORY]);
bool isFrameTableFull(Frame [MEMORY]);
struct Queue *createQueue(unsigned);
int isFull(struct Queue *);
int isEmpty(struct Queue *);
void enqueue(struct Queue *, int);
int dequeue(struct Queue *);
int front(struct Queue *);
int getAvailableLocation();
void help_message(char *executable);
unsigned int kill_alarm(int);
void realTime(int);
void killHandler(int );
void cleanUp();
void detachMemory();
void runtimeInfo(Frame [MEMORY]);

int bitVector[MAX_PROCESSES] = {0};
int currentLaunched = 0;
int totalLaunched = 0;
int totalPageFaults = 0;
int totalRequests = 0;
int totalMemoryAccesses = 0;
char* executable;
bool stopWriting = false;
int totalLines = 0;

struct Queue* q_frame;
struct Process Pcb[MAX_PROCESSES];
FILE *outFile;

jmp_buf timer;

int main(int argc, char **argv) {
    executable = argv[0];
    char error_string[100];

    unsigned int launchTimeSeconds = 0;
    unsigned int launchTimeNS = 0;

    int pctLocation;
    int i = 0;
    int c;

    bool stopLaunching = false;
    bool pageFault;

    pid_t childpid;



    while ((c = getopt(argc, argv, "h")) != -1)
        switch (c) {
            case 'h':
                help_message(argv[0]);
                break;
        }


    /* Creating a queue large enough to hold all frames if necessary */
    q_frame = createQueue(MEMORY);


    /* Opening outfile.  All previous data will be over written */
    outFile = fopen("kuelkerLog.dat", "w");

    srand(time(0));

    /* Signal Handling */
    signal(SIGINT, killHandler);
    signal(SIGTSTP, killHandler);
    signal(SIGALRM, realTime);


    /* Connecting to shared memory segments */
    connectClock(argv[0]);


    /* Connecting to message queue */
    if ((msgId = msgget(MSG_KEY, 0666 | IPC_CREAT)) == -1) {
        perror("Error: OSS msg queue connection failed");
        exit(1);
    }


    /* Initializing Clock */
    shmClock->seconds = 0;
    shmClock->nanoseconds = 0;


    /* Setting up Frame Table */
    Frame frameTable[MEMORY];

    for(i = 0; i < MEMORY; i++){
        frameTable[i].occupiedBit = 0;
    }

    int j;
    for(i = 0; i < MAX_PROCESSES; i++){
	for(j = 0; j < PTABLE_SIZE; j++){
   	    Pcb[i].pageTable[j] = -1;
	}
    }



    /*Calculating the launch time of the child process */

    launchTimeNS = ((rand() % 499000001) + 1000000);
 
    launchTimeSeconds = shmClock->seconds;
    launchTimeNS += shmClock->nanoseconds;


    kill_alarm(5);
 
    int timeToPrintTable = 1;
    
    while (true) {

	if(timeToPrintTable == shmClock->seconds){
	    fprintf(stderr, "\nOSS has been running for %u second(s)\n", shmClock->seconds);

	    if(!stopWriting){
		runtimeInfo(frameTable);
	    }

	    timeToPrintTable += 1;
	}

	if(totalLines >= 10000 && !stopWriting){
	    fprintf(stderr, "\nOSS: %ld has written over 1000 lines to the file. It will only log important information.\n", (long)getpid());
	    fprintf(outFile, "\nOSS: %ld has written over 1000 lines to the file. It will only log important information.\n", (long)getpid());
	    stopWriting = true;
	}

        /*If timer signal was caught, stop spawning children*/
        if(setjmp(timer) == 1){
            fprintf(stderr, "\n\nOSS: %ld kill timer hit. current value of stop launching %d\n", (long)getpid(), stopLaunching);
	    totalLines += 3;
            stopLaunching = true;
        }


        /* Termination criteria.  100 launched or no more launching and no more running */
        if((stopLaunching || totalLaunched >= 100) && currentLaunched == 0){
            break;
        }

        /* Checking whether to launch another process or not */
        if( ( (shmClock->seconds >= launchTimeSeconds) && (shmClock->nanoseconds >= launchTimeNS) ) && totalLaunched < 100 && !stopLaunching) {

            /* If the bitvector has an available location, launch another child */
            if((pctLocation = getAvailableLocation(bitVector)) != -1){

                childpid = childLauncher(pctLocation);
                bitVector[pctLocation] = childpid;

                /*Calculating the launch time of the next child process */
                launchTimeNS = ((rand() % 1000001) + 35000000);

                launchTimeSeconds = shmClock->seconds;
                launchTimeNS += shmClock->nanoseconds;

                if(launchTimeNS > 999999999){
                    launchTimeNS -= 1000000000;

                    launchTimeSeconds += 1;
                }


                currentLaunched++;
                totalLaunched++;
            }
        }


	/* If there is no child process, OSS cannot wait, increment clock and continue through the loop */
	if(currentLaunched == 0){
	    incrementClock(0, (rand() % 10001 + 490000));
 	    continue;
	}

        /* Waiting to receive a message from User */
        if(msgrcv(msgId, &message, sizeof(message), getpid(), 0) == -1){
            perror("\nOSS: Failed to receive message\n");
            exit(1);
        }
	totalRequests++;


        /* If a process is terminating */
        if(message.mesq_terminate == 1){
	    if(!stopWriting){
		fprintf(outFile, "OSS: %d User process %ld is terminating at time %u seconds %u nanoseconds!\n", getpid(), message.mesq_pid, message.mesq_sentSeconds, message.mesq_sentNS);
            	totalLines++;
	    }

            /* Do all the work necessary for a terminating process */
            terminatingWork(frameTable);

	    currentLaunched--;

            /* Incrementing clock and skipping over rest of while loop since that process terminated */
            incrementClock(0, (rand() % 5000001 + 10000000));

            continue;
        }

        /* Automatically assuming a page fault per request */
        pageFault = true;


        /* Calculating whether the requested page is in the frame table (if it exists, no page fault) */
        if(Pcb[message.mesq_pctLocation].pageTable[message.mesq_pageReference] != -1){
            /* No page fault, page was found in the table */
	    if(!stopWriting){
		fprintf(outFile, "OSS: %ld Address %d is in a frame %d\n", (long)getpid(), message.mesq_memoryAddress, Pcb[message.mesq_pctLocation].pageTable[message.mesq_pageReference]);
            	totalLines++;
	    }

            pageFault = false;

            noPageFaultWork(frameTable);

            /*Incrementing time after no pageFault by 10 nanoseconds */
            incrementClock(0, 10);

        }


        /* PageFault, page was not located in the virtual memory */
        if(pageFault){

	    if(!stopWriting){
		fprintf(outFile, "OSS: %ld Address %d is not in a frame, pagefault\n", (long)getpid(), message.mesq_memoryAddress);
                totalLines++;
	    }

            totalPageFaults++;

            /* Do the work necessary in case of a page-fault */
            pageFaultWork(frameTable);

            /*Incrementing time after pageFault by roughly 14MS */
            incrementClock(0, (rand() % 10000001 + 13500000));
        }


        /*Incrementing time in Critical Section */
        incrementClock(0, (rand() % 5000001 + 10000000));



        message.mesq_type = message.mesq_pid;

        if(msgsnd(msgId, &message, sizeof(message), 0) == -1){
            perror("\nOSS: Failed to send message\n");
            exit(1);
        }

    }

    wait(NULL);

    fprintf(outFile, "\nOSS: %d is terminating at time %u seconds %u nanoseconds\n", getpid(), shmClock->seconds, shmClock->nanoseconds);
    fprintf(stderr, "\nOSS: %d is terminating at time %u seconds %u nanoseconds\n", getpid(), shmClock->seconds, shmClock->nanoseconds);

    fprintf(outFile, "\nTOTAL LAUNCHED %d TOTAL PAGE FAULTS %d. TOTAL REQUESTS %d. TOTAL LINES %d\n", totalLaunched, totalPageFaults, totalRequests, totalLines);
    fprintf(stderr, "\nTOTAL LAUNCHED %d TOTAL PAGE FAULTS %d. TOTAL REQUESTS %d. TOTAL LINES %d\n", totalLaunched, totalPageFaults, totalRequests, totalLines);

    float endTime;
    endTime = shmClock->seconds + (shmClock->nanoseconds * .000000001);


    float memAccessedSec = totalRequests / endTime;
    float pageFaultsSec = totalPageFaults / endTime;

    fprintf(outFile, "\nTotal Run Time: %.9f   Memory Accessed Per Second: %.2f     Page Faults Per Second %.2f\n", endTime, memAccessedSec, pageFaultsSec);
    fprintf(stderr, "\nTotal Run Time: %.9f   Memory Accessed Per Second: %.2f     Page Faults Per Second %.2f\n", endTime, memAccessedSec, pageFaultsSec);

    fprintf(stderr, "\nOSS: %ld has finished running.  Check 'kuelkerLog.dat' for more information. Terminating\n", (long)getpid());

    /* Detach from all shared memory segments */
    cleanUp();

    return 0;
}


void connectClock(char *executable) {
    char error_string[100];

    /* Getting the clock ID */
    if ((clockId = shmget(CLOCK_KEY, sizeof(CLOCK), 0644 | IPC_CREAT)) == -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to create Clock shared memory ", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        killHandler(1);
    }

    /* Attaching to clock shared memory */
    if ((shmClock = shmat(clockId, NULL, 0)) == (void *) -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to attach to Clock shared memory ", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        killHandler(1);
    }
}

void incrementClock(unsigned int seconds, unsigned int nanoseconds){
    shmClock->seconds += seconds;
    shmClock->nanoseconds += nanoseconds;

    if (shmClock->nanoseconds > 999999999) {
        shmClock->nanoseconds -= 1000000000;

        shmClock->seconds += 1;
    }
}


/* Function that launches a child process */
pid_t childLauncher(int pctLocation) {
    char error_string[100];
    char pctStr[5];

    snprintf(pctStr, sizeof(pctStr), "%d", pctLocation);

    pid_t pid = fork();
    if (pid == -1) {
        snprintf(error_string, 100, "\n\n%s: Error: Failed to fork ", executable);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
    }
    if (pid == 0) {
        /* Fork and exec off a child */
        char *arglist[]={"./user", pctStr, NULL};
        execvp(arglist[0],arglist);

        /*only reached if execl fails*/
        snprintf(error_string, 100, "\n\n%s: Error: Failed to execute subprogram %s ", executable, arglist[0]);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
    }
    else {
        /* Initializing PCB per child launched */
        initPCB(pctLocation, pid);
        usleep(500);
        return pid;
    }
}


/* Function to initialize PCB locations in the PCT */
void initPCB(int pctLocation, int pid) {
    Pcb[pctLocation].pid = pid;

    /* Setting page value for each index's page table to -1 */
    int i = 0;
    for(; i < PTABLE_SIZE; i++){
        Pcb[pctLocation].pageTable[i] = -1;
    }
}


void terminatingWork(Frame frameTable[MEMORY]){
    int i;
    /* Removing used pct location from bitVector */
    bitVector[message.mesq_pctLocation] = 0;


    for(i = 0; i < PTABLE_SIZE; i++){
        /* If the pageTable was referenced, reset all associated data */
        if(Pcb[message.mesq_pctLocation].pageTable[i] != -1){
            /* Resetting the frame that was mapped to this page */
            frameTable[Pcb[message.mesq_pctLocation].pageTable[i]].pid = 0;
            frameTable[Pcb[message.mesq_pctLocation].pageTable[i]].occupiedBit = 0;
            frameTable[Pcb[message.mesq_pctLocation].pageTable[i]].dirtyBit = 0;
            frameTable[Pcb[message.mesq_pctLocation].pageTable[i]].referenceByte = 0;
            frameTable[Pcb[message.mesq_pctLocation].pageTable[i]].processPageReference = -1;

            /* Reset the pageTable so it's not referenced anymore */
            Pcb[message.mesq_pctLocation].pageTable[i] = -1;
        }
    }

    /* The process will kill itself, so wait for it to finish */
    wait(NULL);
}



/*
 * No page fault was found, incrementing clock based off of dirty bit reference 
 */

void noPageFaultWork(Frame frameTable[MEMORY]) {
    /* Flip the reference byte to show that that frame was referenced */
    frameTable[Pcb[message.mesq_pctLocation].pageTable[message.mesq_pageReference]].referenceByte = 1;

    if(message.mesq_requestType == READ){
        if(!stopWriting){
	    fprintf(outFile, "OSS: %ld Process %ld is requesting to READ address %d at time %u seconds %u nanoseconds\n", (long)getpid(), message.mesq_pid, message.mesq_memoryAddress, message.mesq_sentSeconds, message.mesq_sentNS);
            totalLines++;
	}

        /* If dirtyBit not set, take less time */
        if(frameTable[Pcb[message.mesq_pctLocation].pageTable[message.mesq_pageReference]].dirtyBit == 0){
	    if(!stopWriting){
		fprintf(outFile, "OSS: %ld Address %d in Frame %d. Giving data to Process %ld at %u seconds %u nanoseconds\n", (long)getpid(), message.mesq_memoryAddress, Pcb[message.mesq_pctLocation].pageTable[message.mesq_pageReference], message.mesq_pid, shmClock->seconds, shmClock->nanoseconds);
                totalLines++;
	    }

            incrementClock(0, 10);
        }
        else{
	    if(!stopWriting){
		 fprintf(outFile, "OSS: %ld Address %d in Frame %d. Giving data to Process %ld at %u seconds %u nanoseconds\n", (long)getpid(), message.mesq_memoryAddress, Pcb[message.mesq_pctLocation].pageTable[message.mesq_pageReference], message.mesq_pid, shmClock->seconds, shmClock->nanoseconds);
            	fprintf(outFile, "OSS: %ld Dirty bit of Frame %d set, adding additional time to the clock\n", (long)getpid(), Pcb[message.mesq_pctLocation].pageTable[message.mesq_pageReference]);

            	totalLines += 2;
	    }

            incrementClock(0, 15);
        }

    }


    if(message.mesq_requestType == WRITE){
	if(!stopWriting){
	    fprintf(outFile, "OSS: %ld Process %ld is requesting write of address %d at time %u seconds %u nanoseconds\n", (long)getpid(), message.mesq_pid, message.mesq_memoryAddress, message.mesq_sentSeconds, message.mesq_sentNS);
            fprintf(outFile, "OSS: %ld Address %d in frame %d, writing data to frame at time %u seconds, %u nanoseconds\n", (long)getpid(), message.mesq_memoryAddress, Pcb[message.mesq_pctLocation].pageTable[message.mesq_pageReference], shmClock->seconds, shmClock->nanoseconds);

            totalLines += 2;
	}

        incrementClock(0, 10);
    }
}


/*
 * Page Fault.  
 */


void pageFaultWork(Frame frameTable[MEMORY]) {
    bool emptyFrameFound = false;
    int i = 0;
    int temp = 0;


    /* Since page was not virtual memory, need to see if a location is avalible */
    for(i = 0; i < MEMORY; i++){
        /* If the page is currently not occupied */
        if(frameTable[i].occupiedBit == 0){
	    if(!stopWriting){
		fprintf(outFile, "OSS: %ld Placing Process %d Page %d into Frame %d\n", (long)getpid(), message.mesq_pid, message.mesq_pageReference, i);
                totalLines++;
	    }	    
	    
            frameTable[i].pid = message.mesq_pid;
            frameTable[i].occupiedBit = 1;
            frameTable[i].dirtyBit = 0;
            frameTable[i].referenceByte = 1;
            frameTable[i].processPageReference = message.mesq_pageReference;
		


            /* Putting the frame index inside queue */
            enqueue(q_frame, i);

            /* Store frame index in the process's page to correctly map its location in the frame table. */
            Pcb[message.mesq_pctLocation].pageTable[message.mesq_pageReference] = i;


            /* Changing value of loction found to true */
            emptyFrameFound = true;
	    break;
        }

    }
    

    int frameToRemove = 0;


    /* If an empty frame was NOT found */
    /* FIFO ALGORITHM */
    if(!emptyFrameFound && !isEmpty(q_frame)){
        frameToRemove = dequeue(q_frame);

	if(!stopWriting){
	    fprintf(outFile, "OSS: %ld Clearing frame %d and swapping in Process %d Page %d\n", (long)getpid(), frameToRemove, message.mesq_pid, message.mesq_pageReference);
            totalLines++;
	}
    

    	/* Need to update the page for the process that had its page replaced */
    	for(i = 0; i < MAX_PROCESSES; i++){
            if(bitVector[i] == frameTable[frameToRemove].pid){
            	temp = i;
            	break;
            }
    	}

    	Pcb[temp].pageTable[frameTable[frameToRemove].processPageReference] = frameToRemove;




    	/* Updating the dirtybit info */
    	if(message.mesq_requestType == READ)
            frameTable[frameToRemove].dirtyBit = 0;
    	else
            frameTable[frameToRemove].dirtyBit = 1;



    	frameTable[frameToRemove].pid = message.mesq_pid;
    	frameTable[frameToRemove].occupiedBit = 1;
    	frameTable[frameToRemove].processPageReference = message.mesq_pageReference;
    	frameTable[frameToRemove].referenceByte = 1;


    }
}


bool isFrameTableFull(Frame frameTable[MEMORY]){
    int i = 0;
    for(; i < MEMORY; i++){
	/* If an empty frame is found, table is not full */
	if(frameTable[i].occupiedBit == 0){
	    return false;
	}
    }

    return true;
}




struct Queue *createQueue(unsigned capacity) {
    struct Queue *queue = (struct Queue *) malloc(sizeof(struct Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0;
    queue->rear = capacity - 1;  // This is important, see the enqueue
    queue->array = (int *) malloc(queue->capacity * sizeof(int));
    return queue;
}


int isFull(struct Queue *queue) { return (queue->size == queue->capacity); }


int isEmpty(struct Queue *queue) { return (queue->size == 0); }


void enqueue(struct Queue *queue, int item) {
    if (isFull(queue)){
        return;
    }
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->array[queue->rear] = item;
    queue->size = queue->size + 1;
}


int dequeue(struct Queue *queue) {
    if (isEmpty(queue)) {
        return -1;
    }
    int item = queue->array[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size = queue->size - 1;
    return item;
}


int front(struct Queue *queue) {
    if (isEmpty(queue)) {
        return -1;
    }
    return queue->array[queue->front];
}


/* Returns a location in bitVector if available */
int getAvailableLocation() {
    int i;

    for (i = 0; i < MAX_PROCESSES; i += 1) {
        if (bitVector[i] == 0) {
            return i;
        }
    }
    return -1;
}


/* Print message with executing info  */
void help_message(char *executable) {
    fprintf(stderr, "\nHelp Option Selected [-h]: The following is the proper format for executing the file:\n\n%s", executable);
    fprintf(stderr, " [-h]\n\n");
    fprintf(stderr, "[-h] : Print a help message and exit.\n"
                    "Program will use a FIFO replacement algorithm for page replacement..\n\n");

    exit(EXIT_SUCCESS);
}


/* Starts a timer that will release an alarm signal at the desired time */
unsigned int kill_alarm(int seconds) {
    struct itimerval old, new;
    new.it_interval.tv_usec = 0;
    new.it_interval.tv_sec = 0;
    new.it_value.tv_usec = 0;
    new.it_value.tv_sec = (long int) seconds;

    if (setitimer(ITIMER_REAL, &new, &old) < 0)
        return 0;
    else
        return (unsigned int) old.it_value.tv_sec;
}


void realTime(int dummy) { longjmp(timer, 1); }


void killHandler(int dummy) {
    char error_string[100];

    errno = EINTR;
    snprintf(error_string, 100, "\n\n%s: Interrupt signal was sent, cleaning up memory segments ", executable);
    perror(error_string);
    fprintf(stderr, "\tValue of errno: %d\n\n", errno);



    fprintf(stderr, "\nOSS: %d is terminating. Current time is %u seconds %u nanoseconds\n", getpid(), shmClock->seconds, shmClock->nanoseconds);

    fprintf(stderr, "\nTOTAL LAUNCHED %d TOTAL PAGE FAULTS %d. TOTAL REQUESTS %d\n", totalLaunched, totalPageFaults, totalRequests);


    /* Removing any children still running */
    cleanUp();

    exit(EXIT_SUCCESS);
}


/* Beginning of cleanup process. */
void cleanUp() {
    int i;

    /* Freeing up the queue if still has items in it */
    while(!isEmpty(q_frame)){
        dequeue(q_frame);
    }

    fclose(outFile);

    /* Detaching from memory segments */
    detachMemory();
}


/* End of cleanup process. */
void detachMemory() {
    /* Detaching shared memory */
    shmdt(shmClock);

    /* Freeing shared memory */
    shmctl(clockId, IPC_RMID, NULL);

    /* to destroy the message queue */
    msgctl(msgId, IPC_RMID, NULL);
}


void runtimeInfo(Frame frameTable[MEMORY]){
    int i;
    fprintf(outFile, "\nCurrent memory layout at time %u seconds %u nanoseconds.  (+: Occupied    .: UnOccupied)\n", shmClock->seconds, shmClock->nanoseconds);

    fprintf(outFile, "\n");

    for(i = 0; i < MEMORY; i++){

	if(frameTable[i].occupiedBit == 1){
	    fprintf(outFile, "+ ");
	}
	else{
	    fprintf(outFile, ". ");
	}

    }
    fprintf(outFile, "\n");


    totalLines += 6;


}

