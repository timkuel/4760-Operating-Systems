#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/msg.h>
#include <stdbool.h>

#include "pcb.h"
#include "clock.h"
#include "mesq.h"

#define SHM_KEY 0x1596
#define MSG_KEY 0x1470
#define CLOCK_KEY 0x3574

void connectPCB(char *);

void connectClock(char *);

void caseZeroWork(int);

void caseOneWork(int);

void caseTwoWork(int);

void caseThreeWork(int);

char *formatTimeToString(unsigned int, unsigned int);

PCB *shmPCB;
MESQ message;
CLOCK *shmClock;

int pcbId;
int msgId;
int clockId;

int numPCB = 18;

int main(int argc, char **argv) {
    char error_string[100];
    char *executable = argv[0];
    int pctLocation = atoi(argv[1]);
    int task;
    bool notComplete = true;

    unsigned int quantum;

    connectPCB(executable);

    connectClock(executable);

    if ((msgId = msgget(MSG_KEY, 0666 | IPC_CREAT)) == -1) {
        perror("Error: USER attaching to msgqueue");
        exit(1);
    }

    /*Telling OSS that this Process is ready to receive a message and run*/
    shmPCB[pctLocation].ready = 1;

    if (msgrcv(msgId, &message, sizeof(message.mesq_text), getpid(), MSG_NOERROR) == -1) {
        perror("Error: USER receiving message");
        exit(1);
    }


    /*Setting initial data that changes with each access*/
    shmPCB[pctLocation].lastWaitTimeSecs = shmClock->seconds - shmPCB[pctLocation].lastTimeAccessedSecs;
    shmPCB[pctLocation].lastWaitTimeNS = shmClock->nanoseconds - shmPCB[pctLocation].lastTimeAccessedNS;


    /*Incrementing nanoseconds, which could end up over a second*/
    shmPCB[pctLocation].totalWaitTimeNS += shmPCB[pctLocation].lastWaitTimeNS;

    if (shmPCB[pctLocation].totalWaitTimeNS > 999999999) {
        shmPCB[pctLocation].totalWaitTimeNS -= 1000000000;
        shmPCB[pctLocation].totalWaitTimeSecs += 1;
    }

    shmPCB[pctLocation].totalWaitTimeSecs += shmPCB[pctLocation].lastWaitTimeSecs;


    shmPCB[pctLocation].lastTimeAccessedSecs = shmClock->seconds;
    shmPCB[pctLocation].lastTimeAccessedNS = shmClock->nanoseconds;

    while (notComplete) {

        /*Retrieving the quantum and task from OSS*/
        quantum = atoi(message.mesq_text);
        shmPCB[pctLocation].quantum = quantum;
        task = shmPCB[pctLocation].task;

        switch (task) {

            case 0:
                caseZeroWork(pctLocation);
                notComplete = false;
                shmPCB[pctLocation].completed = 1;

                message.mesq_type = getppid();
                snprintf(message.mesq_text, sizeof(message.mesq_text),
                         "used a random amount of its time quantum then terminated.");

                if (msgsnd(msgId, &message, sizeof(message.mesq_text), IPC_NOWAIT) == -1) {
                    perror("Error: USER failed to send message");
                    exit(1);
                }

                /*This case terminates, so it will not be needing another mesgrcv method after, just terminate.*/

                break;

            case 1:
                caseOneWork(pctLocation);
                notComplete = true;
                shmPCB[pctLocation].completed = 0;

                message.mesq_type = getppid();
                snprintf(message.mesq_text, sizeof(message.mesq_text), "ran for its time quantum of %u.%.9d seconds.",
                         shmPCB[pctLocation].quantum / 1000000000, shmPCB[pctLocation].quantum % 1000000000);


                if (msgsnd(msgId, &message, sizeof(message.mesq_text), IPC_NOWAIT) == -1) {
                    perror("Error: USER failed to send message");
                    exit(1);
                }

                /*This case ran for its quantum time, it will then wait for a message giving the process a new task*/
                if (msgrcv(msgId, &message, sizeof(message.mesq_text), getpid(), MSG_NOERROR) == -1) {
                    perror("Error: USER receiving message");
                    exit(1);
                }
                break;
            case 2:
                caseTwoWork(pctLocation);
                notComplete = true;
                shmPCB[pctLocation].completed = 0;

                message.mesq_type = getppid();
                snprintf(message.mesq_text, sizeof(message.mesq_text), "will be blocked until time %s.",
                         formatTimeToString(shmPCB[pctLocation].timeToUnblockSecs,
                                            shmPCB[pctLocation].timeToUnblockNS));


                if (msgsnd(msgId, &message, sizeof(message.mesq_text), IPC_NOWAIT) == -1) {
                    perror("Error: USER failed to send message");
                    exit(1);
                }

                /*This case will block the process until it has passed a time to unblock. It will wait for a*/
                /* message saying to unblock it and giving it a new task*/
                if (msgrcv(msgId, &message, sizeof(message.mesq_text), getpid(), MSG_NOERROR) == -1) {
                    perror("Error: USER receiving message");
                    exit(1);
                }
                break;

            case 3:
                caseThreeWork(pctLocation);
                notComplete = true;
                shmPCB[pctLocation].completed = 0;

                message.mesq_type = getppid();
                snprintf(message.mesq_text, sizeof(message.mesq_text),
                         "ran %u.%.9d of its allotted time quantum of %u.%.9d nanoseconds.",
                         shmPCB[pctLocation].lastBurstDuration / 1000000000,
                         shmPCB[pctLocation].lastBurstDuration % 1000000000, shmPCB[pctLocation].quantum / 1000000000,
                         shmPCB[pctLocation].quantum % 1000000000);

                if (msgsnd(msgId, &message, sizeof(message.mesq_text), IPC_NOWAIT) == -1) {
                    perror("Error: USER failed to send message");
                    exit(1);
                }

                /*Case will run for a portion of its quantum time, it will then wait for a message from*/
                /* OSS allowing it to run and giving it a new task.*/
                if (msgrcv(msgId, &message, sizeof(message.mesq_text), getpid(), MSG_NOERROR) == -1) {
                    perror("Error: USER receiving message");
                    exit(1);
                }
                break;
        }

        if (shmPCB[pctLocation].completed == 1)
            break;


        /*Changing the data on each access*/
        shmPCB[pctLocation].lastWaitTimeSecs = shmClock->seconds - shmPCB[pctLocation].lastTimeAccessedSecs;
        shmPCB[pctLocation].lastWaitTimeNS = shmClock->nanoseconds - shmPCB[pctLocation].lastTimeAccessedNS;


        /*Incrementing nanoseconds, which could end up over a second*/
        shmPCB[pctLocation].totalWaitTimeNS += shmPCB[pctLocation].lastWaitTimeNS;

        if (shmPCB[pctLocation].totalWaitTimeNS > 999999999) {
            shmPCB[pctLocation].totalWaitTimeNS -= 1000000000;
            shmPCB[pctLocation].totalWaitTimeSecs += 1;
        }

        shmPCB[pctLocation].totalWaitTimeSecs += shmPCB[pctLocation].lastWaitTimeSecs;

    }



    /* Detaching the shmPtr */
    if (shmdt(shmPCB) == -1) {
        snprintf(error_string, 100, "\n\n%s: Error: Failed to detach from shared memory ", executable);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }
    return 0;
}


void connectPCB(char *executable) {
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


void caseZeroWork(int pctLocation) {
    /*Even though the process is just terminating, it still used some time to decide*/
    /* Going to give this burst a random number within its quantum*/
    srand(time(0) * shmClock->nanoseconds);
    shmPCB[pctLocation].lastBurstDuration = (rand() % shmPCB[pctLocation].quantum);
}


void caseOneWork(int pctLocation) {
    shmPCB[pctLocation].lastBurstDuration = shmPCB[pctLocation].quantum;
}


void caseTwoWork(int pctLocation) {
    unsigned int r;
    unsigned int s;

    /*Putting process into a blocked queue, didnt use any quantum time but still took time for decision*/
    /* Process will generate a random number to use between 5000, and 10000 NS*/
    srand(time(0) * shmClock->nanoseconds);
    shmPCB[pctLocation].lastBurstDuration = (rand() % 5001 + 5000);

    r = (rand() % 6);
    s = (rand() % 1001);


    /*Formatting 's' so that it is comparable to nanoseconds*/
    if (s < 10) {
        s *= 100000000;
    } else if (s < 100) {
        s *= 10000000;
    } else if (s < 1000) {
        s *= 1000000;
    } else {
        s *= 100000;
    }


    /*Changing blocked flag to 1 and giving the PCB a time to wake up the process*/
    shmPCB[pctLocation].blocked = 1;
    shmPCB[pctLocation].timeToUnblockSecs = r + shmClock->seconds;
    shmPCB[pctLocation].timeToUnblockNS = s + shmClock->nanoseconds;
}


void caseThreeWork(int pctLocation) {
    double portion;
    double quantumPortion;

    srand(time(0) * shmClock->nanoseconds);
    portion = (rand() % 99 + 1);

    portion *= .01;


    /* Run for 1/4th of the quantum time each loop*/
    quantumPortion = (shmPCB[pctLocation].quantum * portion);

    /*Putting the portion of the quantum running into last burst duration*/
    shmPCB[pctLocation].lastBurstDuration = quantumPortion;
}


/* Function to format time to a string representation */
char *formatTimeToString(unsigned int seconds, unsigned int nanoseconds) {
    if (seconds < 10) {
        char *nanoTime = (char *) malloc(sizeof(char) * 12);;
        snprintf(nanoTime, 15, "%u.%.9d", seconds, nanoseconds);
        return nanoTime;
    } else {
        char *nanoTime = (char *) malloc(sizeof(char) * 13);;
        snprintf(nanoTime, 15, "%u.%.9d", seconds, nanoseconds);
        return nanoTime;
    }
}

