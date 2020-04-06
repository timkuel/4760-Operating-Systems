#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/msg.h>
#include <sys/time.h>

#include "pcb.h"
#include "clock.h"
#include "mesq.h"

#define SHM_KEY 0x1596
#define MSG_KEY 0x1470
#define CLOCK_KEY 0x3574
#define threshold 0.006500000
#define thresholdString "0.006500000"
#define alpha .860000000
#define beta .920000000

void connectPcb(char *);

void connectClock(char *);

int childLauncher(char *, int);

void initPCB(int);

int getAvailableLocation(const int[]);

char* formatTimeToString(unsigned int, unsigned int);

struct Queue *createQueue(unsigned);

int isFull(struct Queue *);

int isEmpty(struct Queue *);

void enqueue(struct Queue *, int);

int dequeue(struct Queue *);

int front(struct Queue *);

void scheduler(struct Queue *, struct Queue *, struct Queue *, struct Queue *, int [], char*, int *, FILE*);

int dispatcher(struct Queue*, int , char *, int, FILE*);

int queueCalculations(int , FILE*);

float getQueueWaitTime(struct Queue* );

void resetQueueLooper(struct Queue*);

unsigned int kill_alarm(int);

void realTime(int);

void killHandler(int);


struct Queue {
    int front, rear, size;
    unsigned capacity;
    int *array;
} Queue;


jmp_buf buf;
jmp_buf timer;

PCB *shmPCB;
MESQ message;
CLOCK *shmClock;

int pcbId;
int msgId;
int clockId;

const int numPCB = 18;

int queue0Looped = 0;
int queue1Looped = 0;
int queue2Looped = 0;

int main(int argc, char **argv) {
    const unsigned int maxTimeBetweenNewProcsSecs = 0;
    const unsigned int maxTimeBetweenNewProcsNS = 10000000;

    unsigned int nextSecond = 0;
    unsigned int nextNS = 0;
    char* currentTime;
    char* nextLaunchTime;
    char* startScheduling = "025000000";

    char error_string[100];
    int pid = 0;
    int pctLocation = 0;
    int bitVector[18] = {0};
    int toKill;
    int stopLaunching = 0;

    FILE *outFile;

    struct Queue *q_0 = createQueue(18);
    struct Queue *q_1 = createQueue(18);
    struct Queue *q_2 = createQueue(18);
    struct Queue *q_Blocked = createQueue(18);


    /*Opening outfile*/
    outFile = fopen("Kuelker_Data.dat", "w");

    signal(SIGINT, killHandler);
    signal(SIGTSTP, killHandler);
    signal(SIGALRM, realTime);

    if (setjmp(buf) == 1) {
        errno = EINTR;
        snprintf(error_string, 100,
                 "\n\n%s: Interrupt signal was sent, cleaning up memory segments ", argv[0]);
        perror(error_string);
        fprintf(stderr, "\tValue of errno: %d\n\n", errno);

        fprintf(stderr, "\nCheck 'Kuelker_Data.dat' for run-time information!\n");

        fclose(outFile);

        while (!isEmpty(q_0)) {
            toKill = dequeue(q_0);
            pid = shmPCB[toKill].pid;
            kill(pid, SIGKILL);
        }

        while (!isEmpty(q_1)) {
            toKill = dequeue(q_1);
            pid = shmPCB[toKill].pid;
            kill(pid, SIGKILL);
        }

        while (!isEmpty(q_2)) {
            toKill = dequeue(q_2);
            pid = shmPCB[toKill].pid;
            kill(pid, SIGKILL);
        }

        while (!isEmpty(q_Blocked)) {
            toKill = dequeue(q_Blocked);
            pid = shmPCB[toKill].pid;
            kill(pid, SIGKILL);
        }

        /* Detaching the shmPcb pointer */
        if (shmdt(shmPCB) == -1) {
            snprintf(error_string, 100, "\n\n%s: Error: Failed to detach from shared memory ", argv[0]);
            perror(error_string);
            fprintf(stderr, "\tVale of errno: %d\n\n", errno);
            exit(EXIT_FAILURE);
        }

        /* Detaching the shmClock pointer*/
        if (shmdt(shmClock) == -1) {
            snprintf(error_string, 100, "\n\n%s: Error: Failed to detach from shared memory ", argv[0]);
            perror(error_string);
            fprintf(stderr, "\tVale of errno: %d\n\n", errno);
            exit(EXIT_FAILURE);
        }

        /* Clearing shared memory related to pcbId */
        shmctl(pcbId, IPC_RMID, NULL);


        /* Clearing shared memory related to clockId */
        shmctl(clockId, IPC_RMID, NULL);

        /* to destroy the message queue */
        msgctl(msgId, IPC_RMID, NULL);

        exit(EXIT_SUCCESS);
    }


    connectPcb(argv[0]);

    connectClock(argv[0]);


    if ((msgId = msgget(MSG_KEY, 0666 | IPC_CREAT)) == -1) {
        perror("Error: OSS msg queue connection failed");
        exit(1);
    }

    kill_alarm(3);

    shmClock->seconds = 0;
    shmClock->nanoseconds = 0;

    srand(time(0));
    /*nextSecond = (rand() % maxTimeBetweenNewProcsSecs);*/
    nextNS = (rand() % maxTimeBetweenNewProcsNS + 1);


    nextLaunchTime = formatTimeToString(shmClock->seconds + nextSecond, shmClock->nanoseconds + nextNS);


    int totalLaunched = 0;
    int currentLaunched = 0;
    while (1) {
        currentTime = formatTimeToString(shmClock->seconds, shmClock->nanoseconds);

        /*If timer signal was caught, stop spawning children*/
        if(setjmp(timer) == 1){
            stopLaunching = 1;
        }

        /*Stops launching children if there is 18 currently running, 100 launched or if the timer passes 3 seconds and changes
 *          * 'stopLaunching' to 1 by sending a signal.*/
        if (strcmp(currentTime, nextLaunchTime) >= 0 && totalLaunched < 100 && currentLaunched < 18 && stopLaunching == 0) {

            if((pctLocation = getAvailableLocation(bitVector)) != -1){
                pid = childLauncher(argv[0], pctLocation);


                /*Adding a 1 to bitVector to show Process Control Table location is taken, and enqueu that location into Queue-0*/
                bitVector[pctLocation] = 1;
                enqueue(q_0, pctLocation);


                fprintf(outFile, "\nOSS: Generating process with PID %d, putting into Queue-0 at time %s.\n", pid, currentTime);

                totalLaunched += 1;
                currentLaunched += 1;


                /*Calculating next Random launch time*/
                srand(time(0) * shmClock->nanoseconds);
                /*nextSecond = (rand() % maxTimeBetweenNewProcsSecs);*/
                nextNS = (rand() % maxTimeBetweenNewProcsNS + 1);

                nextLaunchTime = formatTimeToString(shmClock->seconds + nextSecond, shmClock->nanoseconds + nextNS);

            }
            else{
                /*Calculating next Random Launch time*/
                srand(time(0) * shmClock->nanoseconds);
                /*nextSecond = (rand() % maxTimeBetweenNewProcsSecs);*/
                nextNS = (rand() % maxTimeBetweenNewProcsNS + 1);

                nextLaunchTime = formatTimeToString(shmClock->seconds + nextSecond, shmClock->nanoseconds + nextNS);
            }


        }


        /*If there is a process running right now, try and schedule it. WILL NEED TO CHANGE TO A 1, need 2 processes for scheduling*/
        if (strcmp(currentTime, startScheduling) >= 0){
            /*Since processes have started to run, if any queue is empty, that means all process have been extinguished*/
            if(isEmpty(q_0) && isEmpty(q_1) && isEmpty(q_2) && isEmpty(q_Blocked)){
                fprintf(outFile, "\n\nOSS: All Queues contain nothing, no more were being generated after %d were generated, exit\n\n", totalLaunched);
                break;
            }

            else{
                scheduler(q_0, q_1, q_2, q_Blocked, bitVector, currentTime, &currentLaunched, outFile);
            }
        }


        /*Increment time to show OSS running*/
        shmClock->nanoseconds += (rand() % 1001 + 2000);

        if (shmClock->nanoseconds > 999999999) {
            shmClock->seconds += 1;
            shmClock->nanoseconds = shmClock->nanoseconds - 1000000000;
        }
    }

    fprintf(outFile, "\nOSS: ran for a total of %s, and launched %d processes.\n", formatTimeToString(shmClock->seconds, shmClock->nanoseconds), totalLaunched);

    fprintf(stderr, "\nCheck 'Kuelker_Data.dat' for run-time information!\n");

    fclose(outFile);

    while (!isEmpty(q_0)) {
        toKill = dequeue(q_0);
        pid = shmPCB[toKill].pid;
        printf("\nMaster: Killing child %d\n", pid);
        kill(pid, SIGKILL);
    }

    while (!isEmpty(q_1)) {
        toKill = dequeue(q_1);
        pid = shmPCB[toKill].pid;
        printf("\nMaster: Killing child %d\n", pid);
        kill(pid, SIGKILL);
    }

    while (!isEmpty(q_2)) {
        toKill = dequeue(q_2);
        pid = shmPCB[toKill].pid;
        printf("\nMaster: Killing child %d\n", pid);
        kill(pid, SIGKILL);
    }

    while (!isEmpty(q_Blocked)) {
        toKill = dequeue(q_Blocked);
        pid = shmPCB[toKill].pid;
        printf("\nMaster: killing child %i\n", pid);
        kill(pid, SIGKILL);
    }

    /* Detaching the shmPcb pointer */
    if (shmdt(shmPCB) == -1) {
        snprintf(error_string, 100, "\n\n%s: Error: Failed to detach from shared memory ", argv[0]);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }

    /* Detaching the shmClock pointer*/
    if (shmdt(shmClock) == -1) {
        snprintf(error_string, 100, "\n\n%s: Error: Failed to detach from shared memory ", argv[0]);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }

    /* Clearing shared memory related to pcbId */
    shmctl(pcbId, IPC_RMID, NULL);


    /* Clearing shared memory related to clockId */
    shmctl(clockId, IPC_RMID, NULL);

    /* to destroy the message queue */
    msgctl(msgId, IPC_RMID, NULL);

    return 0;
}


void scheduler(struct Queue *q_0, struct Queue *q_1, struct Queue *q_2, struct Queue *q_Blocked, int bitVector[], char * currentTime, int *currentLaunched, FILE* outFile){
    int toRequeue = 0;
    float averageWait;

    /*Time Quantums for q_0, q_1, q_2 respectivley*/
    int quantum_0 = 40000000;
    int quantum_1 = quantum_0 / 2;
    int quantum_2 = quantum_0 / 4;

    //printf("\nScheduling...\n");

    /*All process will enter q_0 in a suspended state.*/
    /* Will decide which queue to place in based off wait time, and avg wait time in q_0*/
    /*After it it is placed in a proper queue it will have its suspend flag turned off.*/
    if(!isEmpty(q_0)){
        if(shmPCB[front(q_0)].suspend == 1){
            toRequeue = dequeue(q_0);

            /*printf("Master: Assigning process %d a task, it had a wait time of %u nanoseconds, placing in queue",  shmPCB[toRequeue].pid,  shmPCB[toRequeue].lastWaitTime);*/

            srand(time(0) * shmClock->nanoseconds);
            shmPCB[toRequeue].task = (rand() % 3 + 1);

            shmPCB[toRequeue].suspend = 0;
            shmPCB[toRequeue].ready = 1;

            enqueue(q_0, toRequeue);
            return;
        }
    }

    if(!isEmpty(q_Blocked)){
        /*If time ready to unblock. Move to suspended state and reassign task.*/

        if(strcmp(currentTime, formatTimeToString(shmPCB[front(q_Blocked)].timeToUnblockSecs, shmPCB[front(q_Blocked)].timeToUnblockNS)) >= 0){
            toRequeue = dequeue(q_Blocked);

            fprintf(outFile, "\nOSS: Waking Child %d from Blocked Queue at time %s.\n", shmPCB[toRequeue].pid, currentTime);

            printf("\nAwaking...\n");

            /*Unblocking*/
            shmPCB[toRequeue].blocked = 0;

            /*Making sure process is in a ready state*/
            shmPCB[toRequeue].suspend = 0;
            shmPCB[toRequeue].ready = 1;

            /*Assigning a new task*/

            /*Assigning new task, if process used cpu time greater than 4ms, then givve it a chance to terminate*/
            /*Task 0 would give the process that chance, otherwise give it a task 1-3*/
            if(shmPCB[toRequeue].cpuTimeUsed > 40000000)
                shmPCB[toRequeue].task = (rand() % 4);
            else
                shmPCB[toRequeue].task = (rand() % 3 + 1);


            enqueue(q_0, toRequeue);


        }
    }


    if(!isEmpty(q_0)){
        if(shmPCB[front(q_0)].ready == 1 && shmPCB[front(q_0)].suspend != 1 && queue0Looped == 0){


            toRequeue = queueCalculations(dispatcher(q_0, quantum_0, currentTime, 0, outFile), outFile);

            currentTime = formatTimeToString(shmClock->seconds, shmClock->nanoseconds);


            /*Process completed running*/
            if(shmPCB[toRequeue].completed == 1){
                fprintf(outFile, "\nOSS: Process %d was in the system for %s seconds, used the CPU for %u.%.9d seconds, and waited in a queue for a total time of %s seconds.\n", shmPCB[toRequeue].pid, formatTimeToString(shmPCB[toRequeue].systemTimeUsedSecs, shmPCB[toRequeue].systemTimeUsedNS), shmPCB[toRequeue].cpuTimeUsed / 1000000000, shmPCB[toRequeue].cpuTimeUsed % 1000000000, formatTimeToString(shmPCB[toRequeue].totalWaitTimeSecs, shmPCB[toRequeue].totalWaitTimeNS));
                printf("\nCompleted!\n");
                *currentLaunched -= 1;
                bitVector[toRequeue] = 0;
            }
            else if(shmPCB[toRequeue].blocked == 1){
                printf("\nBlocked...\n");

                /*put in blocked queue*/
                enqueue(q_Blocked, toRequeue);

                /*Incrementing clock to add time for placing a process in a blocked queue of 100-300 nanoseconds*/
                shmClock->nanoseconds += (rand() % 201 + 100);

                if (shmClock->nanoseconds > 999999999) {
                    shmClock->seconds += 1;
                    shmClock->nanoseconds = shmClock->nanoseconds - 1000000000;
                }
            }
            else{
                /*Process still running, requeue in new queue*/
                fprintf(outFile, "\nOSS: Process %d is not finished. Has a systemTime of %s, cpu time of %u.%.9d and the current time is %s\n", shmPCB[toRequeue].pid, formatTimeToString(shmPCB[toRequeue].systemTimeUsedSecs, shmPCB[toRequeue].systemTimeUsedNS), shmPCB[toRequeue].cpuTimeUsed / 1000000000, shmPCB[toRequeue].cpuTimeUsed % 1000000000, currentTime);

                printf("\nRe-Queueing...\n");

                if((averageWait = getQueueWaitTime(q_0)) == -1){
                    averageWait = atof(formatTimeToString(shmPCB[toRequeue].lastWaitTimeSecs, shmPCB[toRequeue].lastWaitTimeNS));
                }

                /*Deciding whether to move a process to a third queue*/
                if( (averageWait * alpha) >= threshold && strcmp(formatTimeToString(shmPCB[toRequeue].lastWaitTimeSecs, shmPCB[toRequeue].lastWaitTimeNS), thresholdString) >= 0   ){
                    fprintf(outFile, "\nOSS: Moving Process %d from Queue-0 to Queue-1.\n", shmPCB[toRequeue].pid);

                    /*Putting in new queue and assigning new task*/
                    shmPCB[toRequeue].task = (rand() % 4);

                    shmPCB[toRequeue].queueLooper = 0;
                    enqueue(q_1, toRequeue);
                }
                else{
                    /*Keeping in old queue and assigning new task*/
                    fprintf(outFile, "\nOSS: Keeping Process %d in Queue-0.\n", shmPCB[toRequeue].pid);

                    shmPCB[toRequeue].task = (rand() % 4);

                    shmPCB[toRequeue].queueLooper = 1;
                    enqueue(q_0, toRequeue);
                }
            }
        }

        if(shmPCB[front(q_0)].queueLooper == 1){
            queue0Looped = 1;
        }
    }


    if(!isEmpty(q_1)){
        if(shmPCB[front(q_1)].ready == 1 && queue1Looped == 0){


            toRequeue = queueCalculations(dispatcher(q_1, quantum_1, currentTime, 1, outFile), outFile);

            currentTime = formatTimeToString(shmClock->seconds, shmClock->nanoseconds);


            /*Process completed running*/
            if(shmPCB[toRequeue].completed == 1){
                fprintf(outFile, "\nOSS: Process %d was in the system for %s seconds, used the CPU for %u.%.9d, and waited in a queue for a total time of %s\n", shmPCB[toRequeue].pid, formatTimeToString(shmPCB[toRequeue].systemTimeUsedSecs, shmPCB[toRequeue].systemTimeUsedNS), shmPCB[toRequeue].cpuTimeUsed / 1000000000, shmPCB[toRequeue].cpuTimeUsed % 1000000000, formatTimeToString(shmPCB[toRequeue].totalWaitTimeSecs, shmPCB[toRequeue].totalWaitTimeNS));
                printf("\nCompleted!\n");
                *currentLaunched -= 1;
                bitVector[toRequeue] = 0;
            }
            else if(shmPCB[toRequeue].blocked == 1){
                printf("\nBlocked...\n");

                /*put in blocked queue*/
                enqueue(q_Blocked, toRequeue);

                /*Incrementing clock to add time for placing a process in a blocked queue of 100-300 nanoseconds*/
                shmClock->nanoseconds += (rand() % 201 + 100);

                if (shmClock->nanoseconds > 999999999) {
                    shmClock->seconds += 1;
                    shmClock->nanoseconds = shmClock->nanoseconds - 1000000000;
                }

            }
            else{
                /*Process still running, requeue in new queue*/
                fprintf(outFile, "\nOSS: Process %d is not finished. Has a systemTime of %s, cpu time of %u.%.9d and the current time is %s\n", shmPCB[toRequeue].pid, formatTimeToString(shmPCB[toRequeue].systemTimeUsedSecs, shmPCB[toRequeue].systemTimeUsedNS), shmPCB[toRequeue].cpuTimeUsed / 1000000000, shmPCB[toRequeue].cpuTimeUsed % 1000000000, currentTime);

                printf("\nRe-Queueing...\n");

                if((averageWait = getQueueWaitTime(q_1)) == -1){
                    averageWait = atof(formatTimeToString(shmPCB[toRequeue].lastWaitTimeSecs, shmPCB[toRequeue].lastWaitTimeNS));
                }


                /*Deciding whether to move a process to a third queue*/
                if( (averageWait * beta) >= threshold && strcmp(formatTimeToString(shmPCB[toRequeue].lastWaitTimeSecs, shmPCB[toRequeue].lastWaitTimeNS), thresholdString) >= 0   ){
                    fprintf(outFile, "\nOSS: Moving Process %d from Queue-1 to Queue-2\n", shmPCB[toRequeue].pid);

                    /*Putting in new queue and assigning new task*/
                    shmPCB[toRequeue].task = (rand() % 4);

                    shmPCB[toRequeue].queueLooper = 0;
                    enqueue(q_2, toRequeue);
                }
                else{
                    /*Keeping in old queue and assigning new task*/
                    fprintf(outFile, "\nOSS: Keeping Process %d in Queue-1\n", shmPCB[toRequeue].pid);

                    shmPCB[toRequeue].task = (rand() % 4);

                    shmPCB[toRequeue].queueLooper = 1;
                    enqueue(q_1, toRequeue);
                }
            }
        }

        if(shmPCB[front(q_1)].queueLooper == 1){
            queue1Looped = 1;
        }
    }


    if(!isEmpty(q_2)){
        if(shmPCB[front(q_2)].ready == 1 && queue2Looped == 0){


            toRequeue = queueCalculations(dispatcher(q_2, quantum_2, currentTime, 2, outFile), outFile);

            currentTime = formatTimeToString(shmClock->seconds, shmClock->nanoseconds);


            /*Process completed running*/
            if(shmPCB[toRequeue].completed == 1){
                fprintf(outFile, "\nOSS: Process %d was in the system for %s seconds, used the CPU for %u.%.9d seconds, and waited in a queue for a total time of %s seconds.\n", shmPCB[toRequeue].pid, formatTimeToString(shmPCB[toRequeue].systemTimeUsedSecs, shmPCB[toRequeue].systemTimeUsedNS), shmPCB[toRequeue].cpuTimeUsed / 1000000000, shmPCB[toRequeue].cpuTimeUsed % 1000000000, formatTimeToString(shmPCB[toRequeue].totalWaitTimeSecs, shmPCB[toRequeue].totalWaitTimeNS));
                printf("\nCompleted!\n");
                *currentLaunched -= 1;
                bitVector[toRequeue] = 0;
            }
            else if(shmPCB[toRequeue].blocked == 1){
                printf("\nBlocked...\n");

                /*put in blocked queue*/
                enqueue(q_Blocked, toRequeue);
                /*Incrementing clock to add time for placing a process in a blocked queue of 100-300 nanoseconds*/
                shmClock->nanoseconds += (rand() % 201 + 100);

                if (shmClock->nanoseconds > 999999999) {
                    shmClock->seconds += 1;
                    shmClock->nanoseconds = shmClock->nanoseconds - 1000000000;
                }

            }
            else{
                /*Process still running, requeue in new queue*/
                fprintf(outFile, "\nMaster: Process %d is not finished. Has a systemTime of %s, cpu time of %u.%.9d and the current time is %s\n", shmPCB[toRequeue].pid, formatTimeToString(shmPCB[toRequeue].systemTimeUsedSecs, shmPCB[toRequeue].systemTimeUsedNS), shmPCB[toRequeue].cpuTimeUsed / 1000000000, shmPCB[toRequeue].cpuTimeUsed % 1000000000, currentTime);

                printf("\nRe-Queueing...\n");
                /*Assigning new task, if process used cpu time greater than 4ms, then given it a chance to terminate*/
                /*Task 0 would give the process that chance, otherwise give it a task 1-3*/
                if(shmPCB[toRequeue].cpuTimeUsed > 40000000)
                    shmPCB[toRequeue].task = (rand() % 4);
                else
                    shmPCB[toRequeue].task = (rand() % 3 + 1);


                shmPCB[toRequeue].queueLooper = 1;
                enqueue(q_2, toRequeue);
            }
        }

        if(shmPCB[front(q_2)].queueLooper == 1){
            queue2Looped = 1;
        }
    }


    /*Resets the Queue Looper in different cases of whether another queue is being used at the moment*/



    if(!isEmpty(q_0)){
        if(!isEmpty(q_1)){
            if(!isEmpty(q_2)){
                if(queue0Looped && queue1Looped && queue2Looped){
                    resetQueueLooper(q_0);
                    resetQueueLooper(q_1);
                    resetQueueLooper(q_2);

                    queue0Looped = 0;
                    queue1Looped = 0;
                    queue2Looped = 0;
                }
            }
            else{
                if(queue0Looped && queue1Looped ){
                    resetQueueLooper(q_0);
                    resetQueueLooper(q_1);

                    queue0Looped = 0;
                    queue1Looped = 0;
                }
            }
        }
        else{
            if(queue0Looped){
                resetQueueLooper(q_0);
                queue0Looped = 0;
            }
        }
    }

    /*Incrementing clock to show schedule time of 500-1000 nanoseconds*/
    shmClock->nanoseconds += (rand() % 501 + 500);

    if (shmClock->nanoseconds > 999999999) {
        shmClock->seconds += 1;
        shmClock->nanoseconds = shmClock->nanoseconds - 1000000000;
    }


}


int dispatcher(struct Queue* q, int quantum, char * currentTime, int queueLocation, FILE* outFile){
    int toLaunch = 0;
    int pid = 0;
    char mesg[20];

    printf("\nDispatching...\n");

    toLaunch = dequeue(q);

    pid = shmPCB[toLaunch].pid;

    srand(time(0) * shmClock->nanoseconds);
    shmPCB[toLaunch].dispatchTime = (rand() % 701 + 300);

    fprintf(outFile, "\nOSS: Dispatching process with pid %ld from Queue-%d at time %s.\n",(long)pid, queueLocation, currentTime);

    message.mesq_type = pid;

    snprintf(mesg, 20, "%d", quantum);
    strcpy(message.mesq_text, mesg);
    if (msgsnd(msgId, &message, sizeof(message.mesq_text), MSG_NOERROR) == -1) {
        perror("Error: OSS failed to send message");
        exit(1);
    }

    shmClock->nanoseconds += shmPCB[toLaunch].dispatchTime;

    if (shmClock->nanoseconds > 999999999) {
        shmClock->seconds += 1;
        shmClock->nanoseconds = shmClock->nanoseconds - 1000000000;
    }

    return toLaunch;
}


int queueCalculations(int toCalculate, FILE* outFile){
    printf("\nDoing Calculations...\n");

    if (msgrcv(msgId, &message, sizeof(message.mesq_text), getpid(), MSG_NOERROR) == -1) {
        perror("Error: OSS receiving message");
        exit(1);
    }

    fprintf(outFile, "\nOSS: Receiving that process %d %s\n", shmPCB[toCalculate].pid, message.mesq_text);

    /*Adding lastBurstDuration to the value of cpuTimeUsed, which is just an*/
    /*accumulation of burstDurations*/
    shmPCB[toCalculate].cpuTimeUsed += shmPCB[toCalculate].lastBurstDuration;


    /*Incrementing clock to simulate child passing time*/
    shmClock->nanoseconds += shmPCB[toCalculate].lastBurstDuration;

    while (shmClock->nanoseconds > 999999999) {
        shmClock->seconds += 1;
        shmClock->nanoseconds = shmClock->nanoseconds - 1000000000;
    }

    shmPCB[toCalculate].lastTimeAccessedSecs = shmClock->seconds;
    shmPCB[toCalculate].lastTimeAccessedNS = shmClock->nanoseconds;


    /*Calculating systemTimeUsed, after the last burst, the total system time will be the current time minus*/
    /*the time the process was created.  Since nanoseconds could end up negative, the if statement should prevent that.*/
    shmPCB[toCalculate].systemTimeUsedNS = shmClock->nanoseconds - shmPCB[toCalculate].timeCreatedNS;

    if(shmPCB[toCalculate].systemTimeUsedNS < 0){
        shmPCB[toCalculate].systemTimeUsedNS += 1000000000;
        shmPCB[toCalculate].systemTimeUsedSecs = shmClock->seconds - shmPCB[toCalculate].timeCreatedSecs - 1;

    }else{
        shmPCB[toCalculate].systemTimeUsedSecs = shmClock->seconds - shmPCB[toCalculate].timeCreatedSecs;
    }

    return toCalculate;
}


void connectPcb(char *executable) {
    char error_string[100];


    pcbId = shmget(SHM_KEY, numPCB * sizeof(PCB), 0644 | IPC_CREAT);
    if (pcbId == -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to create shared memory ", executable,
                 (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }

    /* Attaching to the mem segment to get pointer */
    shmPCB = shmat(pcbId, NULL, 0);
    if (shmPCB == (void *) -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to attach to shared memory ", executable,
                 (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }
}


void connectClock(char *executable) {
    char error_string[100];


    clockId = shmget(CLOCK_KEY, sizeof(CLOCK), 0644 | IPC_CREAT);
    if (clockId == -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to create shared memory ", executable,
                 (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }

    /* Attaching to the mem segment to get pointer */
    shmClock = shmat(clockId, NULL, 0);
    if (shmClock == (void *) -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to attach to shared memory ", executable,
                 (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }
}


int childLauncher(char *executable, int pctLocation) {
    char error_string[100];
    int pid = -1;
    char pctStr[5];
    snprintf(pctStr, sizeof(pctStr), "%d", pctLocation);


    pid = fork();
    if (pid == -1) {
        snprintf(error_string, 100, "\n\n%s: Error: Failed to fork ", executable);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        longjmp(buf, 1);
    }

    if (pid == 0) {
        initPCB(pctLocation);
        execl("./user", "./user", pctStr, NULL);
        /*only reached if execl fails*/
        snprintf(error_string, 100, "\n\n%s: Error: Failed to execute subprogram user ", executable);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }
        /*Pid will be value of child process*/
    else {
        usleep(500);
        return pid;
    }
}


void initPCB(int pctLocation) {
    shmPCB[pctLocation].pid = getpid();
    shmPCB[pctLocation].ppid = getppid();
    shmPCB[pctLocation].uId = getuid();
    shmPCB[pctLocation].gId = getgid();

    shmPCB[pctLocation].priority = 0;
    shmPCB[pctLocation].pctLocation = pctLocation;
    shmPCB[pctLocation].timeCreatedSecs =  shmClock->seconds;
    shmPCB[pctLocation].timeCreatedNS = shmClock->nanoseconds;
    shmPCB[pctLocation].dispatchTime = 0;
    shmPCB[pctLocation].lastWaitTimeSecs = 0;
    shmPCB[pctLocation].lastWaitTimeNS = 0;
    shmPCB[pctLocation].totalWaitTimeSecs = 0;
    shmPCB[pctLocation].totalWaitTimeNS = 0;
    shmPCB[pctLocation].lastTimeAccessedSecs = shmClock->seconds;
    shmPCB[pctLocation].lastTimeAccessedNS = shmClock->nanoseconds;
    shmPCB[pctLocation].cpuTimeUsed = 0;
    shmPCB[pctLocation].systemTimeUsedSecs = 0;
    shmPCB[pctLocation].systemTimeUsedNS = 0;
    shmPCB[pctLocation].lastBurstDuration = 0;
    shmPCB[pctLocation].quantum = 0;
    shmPCB[pctLocation].task = 0;
    shmPCB[pctLocation].ready = 0;
    shmPCB[pctLocation].suspend = 1;
    shmPCB[pctLocation].blocked = 0;
    shmPCB[pctLocation].timeToUnblockSecs = 0;
    shmPCB[pctLocation].timeToUnblockNS = 0;
    shmPCB[pctLocation].queueLooper = 0;
    shmPCB[pctLocation].completed = 0;
}


int getAvailableLocation(const int bitVector[]) {
    int i;

    for (i = 0; i < numPCB; i += 1) {
        if (bitVector[i] == 0) {
            return i;
        }
    }
    return -1;
}


float getQueueWaitTime(struct Queue* q){
    int i;
    float d;
    float avg;

    unsigned int TWS = 0;
    unsigned int TWNS = 0;

    if(isEmpty(q))
        return -1;

    for(i = 0; i < q->size; i += 1){
        TWNS += shmPCB[q->array[i]].lastWaitTimeNS;

        if(TWNS > 999999999){
            TWS += 1;
            TWNS -= 1000000000;
        }

        TWS += shmPCB[q->array[i]].lastWaitTimeSecs;
    }


    sscanf(formatTimeToString(TWS, TWNS), "%f", &d);

    avg = d / i;

    return avg;
}


/*Function used to reset if a process has been through a queue or not*/
void resetQueueLooper(struct Queue* q){
    int i;

    if(q->size == 1){
        shmPCB[q->array[0]].queueLooper = 0;
        return;
    }

    for(i = 0; i < q->size; i += 1){
        shmPCB[q->array[i]].queueLooper = 0;
    }
}


/* Function to format time to the nanoseconds */
char* formatTimeToString(unsigned int seconds, unsigned int nanoseconds) {
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

