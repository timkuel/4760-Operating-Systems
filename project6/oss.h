#ifndef OSS_H

/* 
 * Header file containing structures, libraries, and a key for shared memory
 * segments to share between a Master and Child processes. This helps to ensure
 * all references to the structures are the exact same between both processes.
 * To ensure that I didn't abuse including libraries inside the header file, I
 * limited it to headers that directly relate to shared memory or shared memory
 * structures.
 */

#include <sys/ipc.h>
#include <sys/shm.h>

#define MAX_PROCESSES 18
#define PTABLE_SIZE 32
#define MEMORY 256
#define READ 0
#define WRITE 1


/*
 * Clock structure, key, and its reference.
 */
#define CLOCK_KEY 0x3596

int clockId;
typedef struct {
    unsigned int seconds;
    unsigned int nanoseconds;
} CLOCK;

CLOCK *shmClock;


/*
 * Message Queue structure, key, and its reference.
 */
#include <sys/msg.h>
#define MSG_KEY 0x0752

int msgId;
typedef struct {
    long mesq_type;
    long mesq_pid;
    int mesq_pctLocation;
    int mesq_terminate;
    int mesq_pageReference;
    int mesq_memoryAddress;
    int mesq_requestType;
    unsigned int mesq_sentSeconds;
    unsigned int mesq_sentNS;


} MESQ;

MESQ message;


#define OSS_H
#endif 

