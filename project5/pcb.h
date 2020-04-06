#ifndef PCB_H


typedef struct {
    /*Identification*/
    int pid;
    int ppid;
    int uId;
    int gId;

    /*Resource Management*/
    int pctLocation;
    unsigned int timeCreatedSecs;
    unsigned int timeCreatedNS;
    unsigned int maxQueueTimeSecs;
    unsigned int maxQueueTimeNS;

} PCB;

#define PCB_H
#endif
