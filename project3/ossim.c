#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <time.h>

#define SHM_KEY 0x1596

#define SEM_KEY 0x0852


/* Shared memory segment */
struct shMem {
    int seconds;
    long nanoseconds;
    long double message;
};


/* Semaphore segment */
//union semun {
//    int val;
//    struct semid_ds *buf;
//    unsigned short *array;
//};

/* A linked list node */
struct Node {
    int data;
    struct Node *next;
};

struct sembuf p = {0, -1, SEM_UNDO};
struct sembuf v = {0, +1, SEM_UNDO};


jmp_buf buf;
jmp_buf rt;

void help_message(char *);

bool is_number(char []);

void sem_init(int *, char *);

struct shMem *connectShMem(struct shMem *, int *, char *);

pid_t childLauncher(char *);

long double formatTime(int, long);

void push(struct Node **, int);

void deleteKey(struct Node **, int);

unsigned int kill_alarm(int);

void realTimeKiller(int);

void killHandler(int);


int main(int argc, char **argv) {
    int initChildren = 5;
    int totalChildren = 0;
    int maxChildren = 100;
    char *fileName = "output.dat";
    FILE *outFile;
    int realTime = 5;
    int time_incrementer = 0;
    int index;
    int c;
    struct shMem *shmPtr;
    int shmId;
    int statuscode = 0;
    int semId;
    pid_t pid = 0;
    char error_string[100];

    struct Node *pids = NULL;

    signal(SIGINT, killHandler);
    signal(SIGTSTP, killHandler);
    signal(SIGALRM, realTimeKiller);


    /*If signal was caught, jump UP THE CALL STACK to here
     *setjmp can only jump up the call stack. */
    if (setjmp(buf) == 1) {
        errno = EINTR;
        snprintf(error_string, 100, "\n\n%s: Child process failed or an interrupt signal was sent, cleaning up memory segments ",
                 argv[0]);
        perror(error_string);
        fprintf(stderr, "\tValue of errno: %d\n\n", errno);

        while (pids != NULL) {
            pid = pids->data;
            fprintf(stderr, "\n\n\tMaster: killing child %ld \n\n", (long) pid);
            kill(pid, SIGKILL);
            deleteKey(&pids, pid);

        }

        shmdt(shmPtr);
        shmctl(shmId, IPC_RMID, NULL);
        semctl(semId, 0, IPC_RMID);
        exit(EXIT_SUCCESS);
    }

    if (setjmp(rt) == 1) {
        errno = ETIME;
        snprintf(error_string, 100, "\n\n%s: Error: Real-Time timer of %d second(s) expired ", argv[0], realTime);
        perror(error_string);
        fprintf(stderr, "\tValue of errno: %d\n\n", errno);


	while (pids != NULL) {
            pid = pids->data;
            fprintf(stderr, "\n\n\tMaster: killing child %ld \n\n", (long) pid);
            kill(pid, SIGKILL);
            deleteKey(&pids, pid);

        }

        shmdt(shmPtr);
        shmctl(shmId, IPC_RMID, NULL);
        semctl(semId, 0, IPC_RMID);
        exit(EXIT_SUCCESS);
    }

    opterr = 0;

    while ((c = getopt(argc, argv, "hs:l:t:")) != -1)
        switch (c) {
            case 'h':
                help_message(argv[0]);
                break;
            case 's':
                if (!is_number(optarg)) {
                    errno = EINVAL;
                    snprintf(error_string, 100, "\n\n%s: Error: Argument passed in was not an integer ", argv[0]);
                    perror(error_string);
                    fprintf(stderr, "\tVale of errno: %d\n\n", errno);
                    exit(EXIT_FAILURE);
                }
                initChildren = atoi(optarg);
                break;
            case 'l':
                fileName = optarg;
                break;
            case 't':
                if (!is_number(optarg)) {
                    errno = EINVAL;
                    snprintf(error_string, 100, "\n\n%s: Error: Argument passed in was not an integer ", argv[0]);
                    perror(error_string);
                    fprintf(stderr, "\tVale of errno: %d\n\n", errno);
                    exit(EXIT_FAILURE);
                }
                realTime = atoi(optarg);
                break;

            case '?':
		errno = EINVAL;
                if (optopt == 's') {
                    snprintf(error_string, 100, "\n\n%s: Error: Option -%c requires an argument ", argv[0], optopt);
                    perror(error_string);
                    fprintf(stderr, "\tVale of errno: %d\n\n", errno);
                } else if (optopt == 'l') {
                    snprintf(error_string, 100, "\n\n%s: Error: Option -%c requires an argument ", argv[0], optopt);
                    perror(error_string);
                    fprintf(stderr, "\tVale of errno: %d\n\n", errno);
                } else if (optopt == 't') {
                    snprintf(error_string, 100, "\n\n%s: Error: Option -%c requires an argument ", argv[0], optopt);
                    perror(error_string);
                    fprintf(stderr, "\tVale of errno: %d\n\n", errno);
                } else {
                    snprintf(error_string, 100, "\n\n%s: Error: Unknown option character '-%c' ", argv[0], optopt);
                    perror(error_string);
                    fprintf(stderr, "\tVale of errno: %d\n\n", errno);
                }
                exit(EXIT_FAILURE);
            default:
                abort();
        }
  

    /* Calculating the time incrementer, using the number of concurrent children allowed */
    time_incrementer = initChildren * (rand() % 2000000 + 1000000);

    /* Starting the real-time timer */
    kill_alarm(realTime);

    /* Opening the output file in write mode */
    outFile = fopen(fileName, "w");

    /* Attaching to shared memory segment */
    shmPtr = connectShMem(shmPtr, &shmId, argv[0]);

    /* Initializing semaphore  */
    sem_init(&semId, argv[0]);

    shmPtr->seconds = 0;
    shmPtr->nanoseconds = 0;
    shmPtr->message = 0;

    int i;

    fprintf(stderr, "\nMaster: Launching %d children!\n", initChildren);

    /* Launching desired number of children */
    for (i = 0; i < initChildren; i += 1) {
        push(&pids, childLauncher(argv[0]));
        totalChildren += 1;
    }

    /* Sleeping for .05 seconds to allow children to attach to semaphores and begin watching clock before clock starts */
    usleep(500);
    fprintf(stderr,"\nMaster: Starting Virtual-Clock!\n");

    while (formatTime(shmPtr->seconds, shmPtr->nanoseconds) < 2 && totalChildren < maxChildren) {
	
	fprintf(stderr, "\n\nMaster: Wating to gain access to critical region.\n\n");
        /* Before critical region, waiting to enter  */
        if (semop(semId, &p, 1) < 0) {
            snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to gain access to semaphore critical region ",
                     argv[0], (long) getpid());
            perror(error_string);
            fprintf(stderr, "\tValue of errno: %d\n\n", errno);
        }
	
	fprintf(stderr, "\n\nMaster: Gained access to critical region.\n\n");

        if (shmPtr->message != 0) {
            pid = wait(&statuscode);

            /* If Worker failed in someway */
            if (statuscode != 0) {longjmp(buf, 1);}

            fprintf(outFile,
                    "Master: Child PID %ld is terminating at my time %.9Lf because it reached %.9Lf in child process.\n\n",
                    (long) pid, formatTime(shmPtr->seconds, shmPtr->nanoseconds), shmPtr->message);

	    /* Removing the terminating pid fro the linked list */
            deleteKey(&pids, pid);

            /* Launching another child and adding that pid to the liinked list */
            push(&pids, childLauncher(argv[0]));

            totalChildren += 1;

            shmPtr->message = 0;
        }
	srand(shmPtr->nanoseconds + time(NULL));
        shmPtr->nanoseconds += initChildren * (rand() % 2000000 + 3000001);//time_incrementer;

        if (shmPtr->nanoseconds > 999999999) {
            shmPtr->seconds += 1;
            shmPtr->nanoseconds = 0;
        }
	
	fprintf(stderr, "\n\nMaster: Cedeing access to critical region!\n\n");

        /* Exiting the critical region, allowing another to enter  */
        if (semop(semId, &v, 1) < 0) {
            snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to cede access to critical region ", argv[0],
                     (long) getpid());
            perror(error_string);
            fprintf(stderr, "\tValue of errno: %d\n\n", errno);
        }

    }

    //usleep(500);

    if (totalChildren >= maxChildren){
        fprintf(outFile, "\n\nMaster: %d children spawned, killing remaining children", totalChildren);
        fprintf(stderr, "\n\n\n\nMaster: %d children spawned, killing remaining children", totalChildren);
    }
    else{
        fprintf(outFile, "\n\nMaster: Virtual-Tie passed 2 seconds after spawning %d children, killing remaining children", totalChildren);
        fprintf(stderr, "\n\n\n\nMaster: Virtual-Tie passed 2 seconds after spawning %d children, killing remaining children", totalChildren);
    }

    while (pids != NULL) {
        pid = pids->data;
        fprintf(stderr, "\n\nMaster: killing child %ld", (long) pid);
        kill(pid, SIGKILL);
        deleteKey(&pids, pid);

    }


    for (index = optind; index < argc; index++)
        printf("Non-option argument %s\n", argv[index]);


    fprintf(stderr, "\n\nMaster: Check %s for program information.  Un-linking semaphore, detaching and clearing shared memory.\n\n", fileName);

    /* Closing output file */
    fclose(outFile);

    /* Un-linking semaphore */
    if (semctl(semId, 0, IPC_RMID) < 0) {
        snprintf(error_string, 100, "\n\n%s: Error: sem_unlink(3) failed ", argv[0]);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
    }

    /* Detaching the shmPtr */
    if (shmdt(shmPtr) == -1) {
        snprintf(error_string, 100, "\n\n%s: Error: Failed to detach from shared memory ", argv[0]);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }

    /* Clearing shared memory related to clockId */
    shmctl(shmId, IPC_RMID, NULL);

    return 0;
}


void help_message(char *executable){
    fprintf(stderr, "\nHelp Option Selected [-h]: The following is the proper format for executing "
                    "the file:\n\n%s", executable);
    fprintf(stderr, " [-h] [-s x] [-l filename] [-t z]\n\n");
    fprintf(stderr, "[-h]   : Print a help message and exit.\n"
                    "[-s x] : This option will be used to specify the number of child processes spawned. "
                    "(default: 5 children spawned)\n"
                    "[-l filename] : This option will be used to specify the output file to write the data to. "
                    "(default: output.dat)\n"
                    "[-t z] : This option will allow the user to set up a timed interrupt.  "
                    "Program will run no-longer than 'n' seconds. (default: 5 seconds)\n\n"
                    "Default: If no options were passed in the defaults will go as followed:\n"
                    "\tChildren Spawned: 5\n\tOutput File: output.dat\n\tProgram Timeout: 5 seconds\n\n");
  
    exit(EXIT_SUCCESS);
}


bool is_number(char number[]) {
    int i = 0;

    /* checking for negative numbers */
    if (number[0] == '-')
        i = 1;
    for (; number[i] != 0; i++) {
        /* if (number[i] > '9' || number[i] < '0') */
        if (!isdigit(number[i]))
            return false;
    }
    return true;
}


void sem_init(int *semId, char *executable) {
    char error_string[100];

   /* Connecting semaphore */
    *semId = semget(SEM_KEY, 1, 0666 | IPC_CREAT);

    if (semId < 0) {
        fprintf(stderr, "\nFailed to open semaphore\n");
        perror("sem_open(3) failed");
        exit(EXIT_FAILURE);
    }

    union semun u;
    u.val = 1;
    if (semctl(*semId, 0, SETVAL, u) < 0) {
        snprintf(error_string, 100, "\n\n%s: Error: semctl(3) failed ", executable);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }
}


struct shMem *connectShMem(struct shMem *shmPtr, int *shmId, char *executable) {
    char error_string[100];


    *shmId = shmget(SHM_KEY, sizeof(struct shMem), 0644 | IPC_CREAT);
    if (shmId == (void *) -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to create shared memory ", executable,
                 (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }

    /* Attaching to the mem segment to get pointer */
    shmPtr = shmat(*shmId, NULL, 0);
    if (shmPtr == (void *) -1) {
        snprintf(error_string, 100, "\n\n%s: PID: %ld Error: Failed to attach to shared memory ", executable,
                 (long) getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }

    return shmPtr;
}


pid_t childLauncher(char *executable) {
    pid_t pid;
    char error_string[100];

    pid = fork();
    if (pid == -1) {
        snprintf(error_string, 100, "\n\n%s: Error: Failed to fork ", executable);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        execl("./user", "./user", NULL);
        /*only reached if execl fails*/
        snprintf(error_string, 100, "\n\n%s: Error: Failed to execute subprogram user ", executable);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);
        exit(EXIT_FAILURE);
    } else
        return pid;
}


/* Function to fomat time to the form seconds.nanoseconds */
long double formatTime(int seconds, long nanoseconds) {
    long double time;
    long double nano;

    nano = (nanoseconds * .000000001);
    time = (seconds + nano);

    return time;
}


/* Given a reference (pointer to pointer) to the head of a list
 * and an int, inserts a new node on the front of the list. */
void push(struct Node **head_ref, int new_data) {
    struct Node *new_node = (struct Node *) malloc(sizeof(struct Node));
    new_node->data = new_data;
    new_node->next = (*head_ref);
    (*head_ref) = new_node;
}


/* Given a reference (pointer to pointer) to the head of a list and
 * a key, deletes all occurrence of the given key in linked list */
void deleteKey(struct Node **head_ref, int key) {
    /* Store head node */
    struct Node *temp = *head_ref, *prev;

    /* If head node itself holds the key or multiple occurrences of key */
    while (temp != NULL && temp->data == key) {
        *head_ref = temp->next;   // Changed head
        free(temp);               // free old head
        temp = *head_ref;         // Change Temp
    }
/* Delete occurrences other than head */
    while (temp != NULL) {
        /* Search for the key to be deleted, keep track of the
           previous node as we need to change 'prev->next' */
        while (temp != NULL && temp->data != key) {
            prev = temp;
            temp = temp->next;
        }

        /* If key was not present in linked list */
        if (temp == NULL) return;

        /* Unlink the node from linked list */
        prev->next = temp->next;

        free(temp);  /* Free memory */

        /* Update Temp for next iteration of outer loop */
        temp = prev->next;
    }
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


void realTimeKiller(int dummy) { longjmp(rt, 1); }


void killHandler(int dummy) {longjmp(buf, 1);}


