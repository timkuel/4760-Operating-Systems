#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>
#include <sys/sem.h>
#include <stdbool.h>

#define SHM_KEY 0x1596

#define SEM_KEY 0x0852

struct shMem {
    int seconds;
    long nanoseconds;
    long double message;
};


struct sembuf p = {0, -1, SEM_UNDO};
struct sembuf v = {0, +1, SEM_UNDO};

struct shMem *connectShMem(struct shMem *, int *, char *);

long addNano(long , long , int *);

bool isPastTime(int, long, int, long);

long double formatTime(int, long);

int main(int argc, char *argv[]) {
    char error_string[100];
    struct shMem *shmPtr;
    int shmId;
    long randNano;
    int seconds;
    long nanoseconds;
    int clockSeconds;
    long clockNanoseconds;


    /* Connecting to shared memory segment */
    shmPtr = connectShMem(shmPtr, &shmId, argv[0]);

    /* Connecting semaphore */
    int semId = semget(SEM_KEY, 1, 0666 | IPC_CREAT);

    if (semId < 0) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to open semaphore ", argv[0], (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }

    /* Seeding random number generator with virtual time  */
    srand(shmPtr->nanoseconds + time(NULL));
    randNano = rand() % 1000000 + 1;

    /* Storing clock info */
    clockSeconds = shmPtr->seconds;
    clockNanoseconds = shmPtr->nanoseconds;

    /* Referencing clockSeconds and clockNanoseconds to get elapsed time to stop executing child process */
    seconds = clockSeconds;
    nanoseconds = addNano(clockNanoseconds, randNano, &seconds);
	
    while(1){
	fprintf(stderr, "\n\n\tPID %ld is before crit zone\n\n", (long)getpid());
        /* Before critical region, waiting to enter  */
        if (semop(semId, &p, 1) < 0) {
           snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to gain access to semaphore critical region ", argv[0], (long) getpid());
           perror(error_string);
           fprintf(stderr, "\tValue of errno: %d\n\n", errno);
	   break;
        }

        if(formatTime(shmPtr->seconds, shmPtr->nanoseconds) > formatTime(seconds, nanoseconds)){
	    
	    if( shmPtr->message == 0){
		fprintf(stderr, "\n\n\tPID: %ld MESSAGE WAS EMPTY, changing message, ceding critical region, then terminating\n\n", (long)getpid());
		shmPtr->message = formatTime(seconds, nanoseconds);            
 
                /* Exiting the critical region, allowing another to enter  */
                if (semop(semId, &v, 1) < 0) {
                    snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to cede access to critical region ", argv[0], (long) getpid());
                    perror(error_string);
                    fprintf(stderr, "\tVaule of errno: %d\n\n", errno);
                }
                break;
	    }
	    fprintf(stderr, "\n\n\tPID %ld MESSAGE WAS FULL, ceding critical region until empty\n\n", (long)getpid());
        }

        /* Exiting the critical region, allowing another to enter  */
        if (semop(semId, &v, 1) < 0) {
            snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to cede access to critical region ", argv[0], (long) getpid());
            perror(error_string);
            fprintf(stderr, "\tValue of errno: %d\n\n", errno);
        }
    }

    /* Detaching shared memory pointers */
    if (shmdt(shmPtr) == -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to detach shared memory ", argv[0], (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
    }

    return 0;
}


struct shMem *connectShMem(struct shMem *shmPtr, int *shmId, char *executable) {
    char error_string[100];


    *shmId = shmget(SHM_KEY, sizeof(struct shMem), 0644 | IPC_CREAT);
    if (shmId == (void *) -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to create shared memory ", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }

    /* Attaching to the mem segment to get pointer */
    shmPtr = shmat(*shmId, NULL, 0);
    if (shmPtr == (void *) -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to attach to shared memory ", executable, (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }

    return shmPtr;
}


/* Function to add the randomly generated nanoseconds to the clock nanoseconds */
long addNano(long clockNano, long randNano, int *seconds){
    long nanoseconds = clockNano + randNano;

    if(nanoseconds > 999999999){
        nanoseconds -= 1000000000;
        *seconds += 1;
    }
    return nanoseconds;
}


/* Funciton that checks wether the clock has passed the desired time */
bool isPastTime(int clockSeconds, long clockNano, int seconds, long nanoseconds){
    if(clockSeconds >= seconds && clockNano > seconds)
        return true;
    else
        return false;
}


/* Function to fomat time to the form seconds.nanoseconds */
long double formatTime(int seconds, long nanoseconds){
    long double time;
    long double nano;

    nano = (nanoseconds * .000000001);
    time = (seconds + nano);
    
    return time;
}



