#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>

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


void resourceAllocation(int );
bool isSafe(void);
void connectPcb(char *);
void connectClock(char *);
void connectSemaphore(char *);
void connectDescriptor(char *);
void initDescriptor(void);
pid_t childLauncher(char *, int);
void initPCB(int);
struct Queue *createQueue(unsigned);
int isFull(struct Queue *);
int isEmpty(struct Queue *);
void enqueue(struct Queue *, int);
int dequeue(struct Queue *);
int front(struct Queue *);
void resetDescription(int);
void generateNeedsMatrix(void);
int getAvailableLocation();
char *formatTimeToString(unsigned int, unsigned int);
void printVector(int [numResources]);
void printMatrix(int [numPCB][numResources]);
unsigned int kill_alarm(int);
void realTime(int);
void killHandler(int );
void help_message(char *executable);

struct Queue {
    int front, rear, size;
    unsigned capacity;
    int *array;
} Queue;




PCB *shmPcb;
CLOCK *shmClock;
SEMAPHORE *shmSem;
DESCRIPTOR *shmDes;
MESQ message;
struct Queue *q;
FILE *outFile;

int pcbId;
int clockId;
int semId;
int desId;
int msgId;
int c;
int bitVector[numPCB] = {0};
int currentLaunched = 0;
int requestsGranted = 0;
int linesWritten = 0;
int deadlocksDetected = 0;
int statuscode = 0;
bool verbose = false;

jmp_buf buf;
jmp_buf timer;

int main(int argc, char **argv) {
    char error_string[100];

    unsigned int launchTimeSeconds;
    unsigned int launchTimeNS;
    unsigned int differenceSeconds;
    unsigned int differenceNS;

    int pctLocation;
    int totalLaunched = 0;
    int i = 0;
    int j = 0;
    int toKill;
    int temp;

    bool flag;
    bool stopLaunching = false;

    pid_t childpid;

    q = createQueue(numPCB);

    /*Opening outfile*/
    outFile = fopen("kuelkerLog.dat", "w");

    srand(time(0));

    /* Signal Handling */
    signal(SIGINT, killHandler);
    signal(SIGTSTP, killHandler);
    signal(SIGALRM, realTime);

    if (setjmp(buf) == 1) {
        errno = EINTR;
        snprintf(error_string, 100, "\n\n%s: Interrupt signal was sent, cleaning up memory segments ", argv[0]);
        perror(error_string);
        fprintf(stderr, "\tValue of errno: %d\n\n", errno);

	
        for (i = 0; i < numPCB; i++) {
            if (bitVector[i] == 1) {
                kill(shmPcb[i].pid, SIGKILL);
            }
        }

        while (!isEmpty(q)) {
            toKill = dequeue(q);
            childpid = shmPcb[toKill].pid;
            kill(childpid, SIGKILL);
        }

	wait(&statuscode);


	fprintf(stderr, "\nTotal requests granted %d\n", requestsGranted);
        fprintf(stderr, "\nTotal lines written %d\n", linesWritten);

        linesWritten += 2;
        fprintf(outFile, "\nTotal requests granted %d\n", requestsGranted);
        fprintf(outFile, "\nTotal lines written %d\n", linesWritten);



	fclose(outFile);


        /* Detaching shared memory */
        shmdt(shmPcb);
        shmdt(shmClock);
        shmdt(shmSem);
        shmdt(shmDes);

        /* Freeing shared memory */
        shmctl(pcbId, IPC_RMID, NULL);
        shmctl(clockId, IPC_RMID, NULL);
        shmctl(semId, IPC_RMID, NULL);
        shmctl(desId, IPC_RMID, NULL);

        /* to destroy the message queue */
        msgctl(msgId, IPC_RMID, NULL);

        exit(EXIT_SUCCESS);
    }

    /* Checking command line options */
    while ((c = getopt(argc, argv, "hv")) != -1)
        switch (c) {
            case 'h':
                help_message(argv[0]);
                break;
            case 'v':
                verbose = true;
                break;
            default:
                verbose = false;
        }


    /* Connecting to shared memory segments */
    connectPcb(argv[0]);
    connectClock(argv[0]);
    connectSemaphore(argv[0]);
    connectDescriptor(argv[0]);


    /* Connecting to message queue */
    if ((msgId = msgget(MSG_KEY, 0666 | IPC_CREAT)) == -1) {
        perror("Error: OSS msg queue connection failed");
        exit(1);
    }


    /* Initilizing Clock */
    shmClock->seconds = 0;
    shmClock->nanoseconds = 0;


    /* Initilizing semaphore */
    sem_init(&(shmSem->mutex), 1, 1);


    /* Initilizing the vector and matrix descriptors */
    initDescriptor();


    /*Calculating the launch time of the child process */
    launchTimeNS = ((rand() % 499000001) + 1000000);

    launchTimeSeconds += shmClock->seconds;
    launchTimeNS += shmClock->nanoseconds;

    if(launchTimeNS > 999999999){
        launchTimeNS -= 1000000000;

        launchTimeSeconds += 1;
    }


    kill_alarm(3);


    while (1) {
	/*If timer signal was caught, stop spawning children*/
        if(setjmp(timer) == 1){
            stopLaunching = true;
        }


        /* Termination criteria.  100 launched or no more launching and no more running */
        if((stopLaunching || totalLaunched >= 100) && currentLaunched == 0){
            fprintf(stderr, "\n\nOSS: Receiving that either 100 process were launched or the program ran for more than 3 real life seconds\nand there are no more processes in the system.\n");

	    fprintf(outFile, "\n\nOSS: Receiving that either 100 process were launched or the program ran for more than 3 real life seconds\nand there are no more processes in the system.\n");
	    linesWritten += 2;
            break;
        }

	/* If 100,000 lines written to file, stop writing */
	if(linesWritten >= 100000){
	    fclose(outFile);
	}


        /* Checking whether to launch another process or not */
        if(strcmp(formatTimeToString(shmClock->seconds, shmClock->nanoseconds), formatTimeToString(launchTimeSeconds, launchTimeNS)) >= 0 && totalLaunched < 100 && currentLaunched < 18 && !stopLaunching){

            /* If the bitvector has an avalible location, launch another child */
            if((pctLocation = getAvailableLocation(bitVector)) != -1){

                childpid = childLauncher(argv[0], pctLocation);
                bitVector[pctLocation] = 1;

                /*Calculating the launch time of the next child process */
                launchTimeNS = ((rand() % 499000001) + 1000000);

                launchTimeSeconds += shmClock->seconds;
                launchTimeNS += shmClock->nanoseconds;

                if(launchTimeNS > 999999999){
                    launchTimeNS -= 1000000000;

                    launchTimeSeconds += 1;
                }

                currentLaunched++;
                totalLaunched++;
            }
            else{
                /*Calculating the launch time of the next child process */
                launchTimeNS = ((rand() % 499000001) + 1000000);

                launchTimeSeconds += shmClock->seconds;
                launchTimeNS += shmClock->nanoseconds;

                if(launchTimeNS > 999999999){
                    launchTimeNS -= 1000000000;

                    launchTimeSeconds += 1;
                }

            }

        }



        usleep(500);
        /* critical section */
        sem_wait(&(shmSem->mutex));

        /* Checking if a child is trying to terminate */
        for(i = 0; i < numPCB; i++){
            /* If a process is trying to terminate, de-allocate all resources */
            if(shmDes->terminating[i] == 1){
                if(verbose){
                    fprintf(outFile, "\nOSS: Receiving child %d is terminating.", shmPcb[i].pid);
                }

                /* Since terminating, resetting all allocated data structures */
                resetDescription(i);

                /* Process terminated, and resources freed. Try and allow a blocked process to run. */
                if(!isEmpty(q)){
                    flag = true;
                    temp = dequeue(q);

                    generateNeedsMatrix();

                    for(j = 0; j < numResources; j++){
                        /* If the blocked process can't be allocated the resources it needs, requeue that process */
                        if(shmDes->allocation_vector[j] - shmDes->need_matrix[temp][j] < 0){
                            enqueue(q, temp);
                            flag = false;
                            break;
                        }
                    }

                    /* If the front process could run, allow it to */
                    if(flag){
                        message.mesq_type = shmPcb[temp].pid;

                        snprintf(message.mesq_text, sizeof(message.mesq_text), "Run & Terminate");
                        if (msgsnd(msgId, &message, sizeof(message.mesq_text), MSG_NOERROR) == -1) {
                            perror("Error: OSS failed to send message");
                            exit(1);
                        }

                        /* Process was allowed to be granted it's resources */
                        requestsGranted++;
                        shmDes->suspended[temp] = 0;
                        shmDes->allocate[temp] = 1;
                        break;
                    }

                }

            }

        }

        /* Checking to see if any processes requested for a resource */
        for(i = 0; i < numPCB; i++){
            /* If process requested resource, allocate resources and check if in safe state */
            if(shmDes->request[i] == 1){
                resourceAllocation(i);
            }
        }


        /* If q not empty, check condition to see if can terminate */
        if(!isEmpty(q)){
            /* If whats is currently launched is all in the queue, wake the queue all in request mode*/
            if(q->size == currentLaunched){
                while(!isEmpty(q)){
                    temp = dequeue(q);

                    /* Allowing the process to request for resources again */
                    shmDes->request[temp] = 1;
                    shmDes->allocate[temp] = 0;
                    shmDes->release[temp] = 0;
                    shmDes->terminating[temp] = 0;
                    shmDes->suspended[temp] = 0;

                    message.mesq_type = shmPcb[temp].pid;

                    snprintf(message.mesq_text, sizeof(message.mesq_text), "Run & Terminate");
                    if (msgsnd(msgId, &message, sizeof(message.mesq_text), MSG_NOERROR) == -1) {
                        perror("Error: OSS failed to send message");
                        exit(1);
                    }

                }
            }

        }

        /*Incrementing time in Critical Section */
        shmClock->nanoseconds += (rand() % 5000001 + 10000000);

        if (shmClock->nanoseconds > 999999999) {
            shmClock->nanoseconds -= 1000000000;

            shmClock->seconds += 1;
        }

        /* Ceeding the critical section */
        sem_post(&(shmSem->mutex));
    }

    wait(&statuscode);





    fprintf(stderr, "\nOSS: Receiving that the program is terminating.\n");


    fprintf(stderr, "\nOSS: final time of %u seconds and %u nanoseconds\n", shmClock->seconds, shmClock->nanoseconds);


    linesWritten++;
    fprintf(outFile, "\nPrinting the allocation matrix (What each process currently has)\n");
    printMatrix(shmDes->allocation_matrix);


    fprintf(stderr, "\nTotal requests granted %d\n", requestsGranted);
    fprintf(stderr, "\nTotal lines written %d\n", linesWritten);

    linesWritten += 2;
    fprintf(outFile, "\nTotal requests granted %d\n", requestsGranted);
    fprintf(outFile, "\nTotal lines written %d\n", linesWritten);

    fclose(outFile);

    while (!isEmpty(q)) {
        toKill = dequeue(q);
        childpid = shmPcb[toKill].pid;
        kill(childpid, SIGKILL);
    }


    /* Detaching shared memory */
    shmdt(shmPcb);
    shmdt(shmClock);
    shmdt(shmSem);
    shmdt(shmDes);

    /* Freeing shared memory */
    shmctl(pcbId, IPC_RMID, NULL);
    shmctl(clockId, IPC_RMID, NULL);
    shmctl(semId, IPC_RMID, NULL);
    shmctl(desId, IPC_RMID, NULL);

    /* to destroy the message queue */
    msgctl(msgId, IPC_RMID, NULL);

    return 0;
}


void resourceAllocation(int pctLocation){
    int i;
    int j;
    bool suspend = false;

    generateNeedsMatrix();

    for(i = 0; i < numResources; i++){
        if((shmDes->allocation_matrix[pctLocation][i] + shmDes->need_matrix[pctLocation][i] > shmDes->request_matrix[pctLocation][i])){
            /*Total request greater than claims, error */
            shmDes->release[i] = 1;
            break;
        }
        else if(shmDes->need_matrix[pctLocation][i] > shmDes->allocation_vector[i]){
            /* Suspend process, its asking more than whats avalible */
            suspend = true;
            break;
        }
        else{
            /* Put in new state */
            shmDes->allocation_matrix[pctLocation][i] += shmDes->need_matrix[pctLocation][i];
            shmDes->allocation_vector[i] -= shmDes->need_matrix[pctLocation][i];
        }

    }


    if(isSafe() && !suspend){
	fprintf(stderr, "\nOSS: Receiving that child %d was granted its resources.", shmPcb[pctLocation].pid);
        if(verbose){
            fprintf(outFile, "\nOSS: Receiving that child %d was granted its resources.", shmPcb[pctLocation].pid);
            linesWritten++;

	    shmDes->request[pctLocation] = 0;
	    shmDes->allocate[pctLocation] = 1;
            requestsGranted++;

            if(requestsGranted % 20 == 0){
                fprintf(outFile, "\nReceiving OSS granted 20 requests, printing matrix\n");
                linesWritten++;
                printMatrix(shmDes->allocation_matrix);
            }
        }
    }
    else{
	deadlocksDetected++;
	if(deadlocksDetected % 10 == 0){
	    fprintf(stderr, "\nOSS: Receiving that %d deadlocks have been detected", deadlocksDetected);
	}	
	

        /* not save, revert back to previous state then suspend */
        fprintf(outFile, "\nOSS: Receiving that a deadlock was detected");
        if(verbose){
            fprintf(outFile, " at %u seconds %u nanoseconds", shmClock->seconds, shmClock->nanoseconds);
            linesWritten++;
        }
        fprintf(outFile, ".");
        linesWritten++;

        shmDes->suspended[pctLocation] = 1;
        shmDes->request[pctLocation] = 0;
        enqueue(q, pctLocation);
    }



}


bool isSafe(void){
    int i = 0;
    int j = 0;
    int work[numResources];
    bool found;
    bool finish[numPCB] = {0};
    int safeSeq[numPCB];

    generateNeedsMatrix();

    /* Initializing work to the resources currently available */
    for(i = 0; i < numResources; i++){
        work[i] = shmDes->resource_vector[i];
    }



    /* Find a process which is not finished and whose needs can be satisfied */
    int count = 0;
    while(count < numPCB){
        found = false;

        /* First check if a process is finished. if no, go to next condition */
        for(i = 0; i < numPCB; i++){
            if(finish[i] == 0){
                /* check if for all resources of current PCB need is less than work */
                for(j = 0; j < numResources; j++){
                    if(shmDes->need_matrix[i][j] > work[j])
                        break;
                }

                /* if all needs of i were satisfied */
                if(j == numResources){
                    /* add allocated resources of current i to the avalible/work reosurces (free resources) */

                    for(j = 0; j < numResources; j++){
                        work[j] += shmDes->allocation_matrix[i][j];
                    }

                    safeSeq[count++] = i;

                    finish[i] = 1;
                    found = true;
                }

            }

        }

        /* if we could not find a next process in safe sequence */
        if(found == false){
            return false;
        }

    }

    return true;
}


void connectPcb(char *executable) {
    char error_string[100];

    /* Getting the PCB ID  */
    if ((pcbId = shmget(PCB_KEY, numPCB * sizeof(PCB), 0644 | IPC_CREAT)) == -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to create PCB shared memory ", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        killHandler(1);
    }

    /* Attaching to PCB shared memory */
    if ((shmPcb = shmat(pcbId, NULL, 0)) == (void *) -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to attach to PCB shared memory ", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        killHandler(1);
    }
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


void connectSemaphore(char *executable){
    char error_string[100];

    /* Getting the semaphore ID */
    if((semId = shmget(SEM_KEY, sizeof(SEMAPHORE), 0644 | IPC_CREAT)) == -1){
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to allocate shared memory region for semaphore", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        killHandler(1);
    }

    /* Attaching to semaphore shared memory */
    if((shmSem = (SEMAPHORE*)shmat(semId, (void *)0, 0)) == (void *)-1){
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to attach to shared memory region for semaphore", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        killHandler(1);
    }
}


void connectDescriptor(char *executable){
    char error_string[100];

    /* Getting the semaphore ID */
    if((desId = shmget(DES_KEY, sizeof(DESCRIPTOR), 0644 | IPC_CREAT)) == -1){
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to allocate shared memory region for resource descriptor", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        killHandler(1);
    }

    /* Attaching to semaphore shared memory */
    if((shmDes = (DESCRIPTOR*)shmat(desId, (void *)0, 0)) == (void *)-1){
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to attach to shared memory region for resource descriptor", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        killHandler(1);
    }
}


/* Function that initilizes the vector and matrix descriptors in shared memorhy*/
void initDescriptor(void){
    int i;
    int j;

    for(i = 0; i < numResources; i++){
        shmDes->resource_vector[i] = ((rand() % 11) + 20);          //This is the total amount of resources avalible
        shmDes->allocation_vector[i] = shmDes->resource_vector[i]; //This is the amount of resources still avalible to give out

        /* Declaring if a resouces is sharable */
        if(i % 4 == 0){
            shmDes->sharable_resources[i] = 1;
        }


        shmDes->request[i] = 0;
        shmDes->allocate[i] = 0;
        shmDes->release[i] = 0;
        shmDes->terminating[i] = 0;
        shmDes->suspended[i] = 0;
        shmDes->timesChecked[i] = 0;
    }


    /* Initilizing matricies to 0 (Process will change values as they enter the system)*/
    for(i = 0; i < numPCB; i++){
        for(j = 0; j < numResources; j++){
            shmDes->request_matrix[i][j] = 0;
            shmDes->allocation_matrix[i][j] = 0;
            shmDes->need_matrix[i][j] = 0;
        }
    }

}


/* Function that launches a child process */
pid_t childLauncher(char *executable, int pctLocation) {
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
        /* Initilizing the PCB */
        initPCB(pctLocation);

        /* Fork and exec off a child */
        char *arglist[]={"./user", pctStr, NULL};
        execvp(arglist[0],arglist);

        /*only reached if execl fails*/
        snprintf(error_string, 100, "\n\n%s: Error: Failed to execute subprogram %s ", executable, arglist[0]);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
    }
    else {
        usleep(500);
        return pid;
    }
}


/* Function to initilize PCB locations in the PCT */
void initPCB(int pctLocation) {
    shmPcb[pctLocation].pid = getpid();
    shmPcb[pctLocation].ppid = getppid();
    shmPcb[pctLocation].uId = getuid();
    shmPcb[pctLocation].gId = getgid();

    shmPcb[pctLocation].pctLocation = pctLocation;
    shmPcb[pctLocation].timeCreatedSecs =  shmClock->seconds;
    shmPcb[pctLocation].timeCreatedNS = shmClock->nanoseconds;
    shmPcb[pctLocation].maxQueueTimeSecs = 0;
    shmPcb[pctLocation].maxQueueTimeNS = 0;
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


void resetDescription(int i){
    int j;

    bitVector[i] = 0;
    currentLaunched--;

    shmDes->terminating[i] = 0;
    shmDes->request[i] = 0;
    shmDes->allocate[i] = 0;
    shmDes->release[i] = 0;
    shmDes->suspended[i] = 0;


    /* Reallocating resources and resetting*/
    for(j = 0; j < numResources; j++){
        /* If not a sharable resource, add it back to the allocation vector */
        if(shmDes->sharable_resources[j] != 1){
            shmDes->allocation_vector[j] += shmDes->allocation_matrix[i][j];
        }
        shmDes->allocation_matrix[i][j] = 0;
        shmDes->request_matrix[i][j] = 0;
    }
}


void generateNeedsMatrix(void){
    int i;
    int j;

    /* Calculating Needs Matrix (What was requested minus what the process has) */
    for(i = 0; i < numPCB; i++){
        for(j = 0; j < numResources; j++){
            shmDes->need_matrix[i][j] = shmDes->request_matrix[i][j] - shmDes->allocation_matrix[i][j];
        }
    }
}


/* Returns a location in bitVector if available */
int getAvailableLocation() {
    int i;

    for (i = 0; i < numPCB; i += 1) {
        if (bitVector[i] == 0) {
            return i;
        }
    }
    return -1;
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


/* Function to print a vector */
void printVector(int vector[numResources]){
    int i;

    fprintf(stderr, "\n  ");
    for(i = 0; i < numResources; i++){
        fprintf(stderr, "R%02d  ", i);
    }

    fprintf(stderr, "\n");

    for(i = 0; i < numResources; i++){
        fprintf(stderr, "| %02d ", vector[i]);
    }
    fprintf(stderr, "|\n\n");
    linesWritten += 6;
}


/* Function to print a matrix */
void printMatrix(int matrix[numPCB][numResources]){
    int i;
    int j;

    fprintf(outFile, "\n\t  ");
    for(i = 0; i < numResources; i++){
        fprintf(outFile, "R%02d  ", i);
    }

    fprintf(outFile, "\n\n");

    for(i = 0; i < numPCB; i++){
        fprintf(outFile, "P%d\t", i);
        for(j = 0; j < numResources; j++){
            fprintf(outFile, "| %02d ", matrix[i][j]);
        }
        fprintf(outFile, "|\n");
    }
    fprintf(outFile, "\n");
    linesWritten += 23;
}


void help_message(char *executable) {
    fprintf(stderr, "\nHelp Option Selected [-h]: The following is the proper format for executing the file:\n\n%s", executable);
    fprintf(stderr, " [-h] [-v]\n\n");
    fprintf(stderr, "[-h] : Print a help message and exit.\n"
                    "[-v] : This option will be used to enable a verbose mode.  Prints more data than required\n"
                    "\t(default: verbose is false)\n\n");

    exit(EXIT_SUCCESS);

}


/* Starts a timer that will release an alarm signal at the desired time */
unsigned int kill_alarm(int seconds) {
    struct itimerval old, new;
    new.it_interval.tv_usec = 0;
    new.it_interval.tv_sec = 0;
    new.it_value.tv_usec = 0;
    new.it_value.tv_sec = (long int) seconds;
    if (setitimer(ITIMER_REAL, &new, &old) < 0) {
        return 0;
    } else {
        return (unsigned int) old.it_value.tv_sec;
    }
}


void realTime(int dummy) { longjmp(timer, 1); }

void killHandler(int dummy) { longjmp(buf, 1); }


