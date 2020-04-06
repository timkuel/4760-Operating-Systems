#ifndef MESQ_H

#include <sys/msg.h>

typedef struct {    
    long mesq_type;
    char mesq_text[100];
} MESQ;

#define MESQ_H
#endif 
