#ifndef DESCRIPTOR_H

#define numPCB 18
#define numResources 20

typedef struct {
    /* Information about the resources */
    int request_matrix[numPCB][numResources]; // Resources each process needs to complete
    int allocation_matrix[numPCB][numResources]; // The currently allocated resources to the processes
    int need_matrix[numPCB][numResources]; // what each process will need to be able to complete

    int resource_vector[numResources]; // Total resources in the system
    int allocation_vector[numResources]; // Current resources avalible to be allocated
    
    int sharable_resources[numResources]; // If 1, then that resource is sharable 


    int request[numPCB];
    int allocate[numPCB];
    int release[numPCB];
    int terminating[numPCB];
    int suspended[numPCB];
    int timesChecked[numPCB];

} DESCRIPTOR;

#define DESCRIPTOR_H
#endif

