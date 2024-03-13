#include <stdio.h>
#include <stdlib.h>
struct shared_memory
{
    int *readers_count;
    int sunolo;
    int segment;   // Desired line
    char** buffer; 
};

typedef struct shared_memory* sharedMemory;
