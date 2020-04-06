#ifndef SEM_H

#include <semaphore.h>

typedef struct {
    int shmInt;
    sem_t mutex;
} SEMAPHORE;

#define SEM_H
#endif
