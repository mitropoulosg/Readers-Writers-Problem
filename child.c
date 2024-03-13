#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>

#include "shared_memory.h"
#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

void child(int segs,int lines_per_seg,int i,sharedMemory sMemory,int requests,void* sem1, void* sem2, void* rw_mutex,sem_t* mutex[],void* rw_mutex2)
{
    struct timeval start1,end1,start2,end2;
    gettimeofday(&start1, NULL);  //this is for time


    char name[20];
    int random_seg;
    int previous_seg;
    sprintf(name,"child%d.log",i+1);
    FILE* fp;
    fp=fopen(name,"w");

    //rand seed
    unsigned int curtime = time(NULL);
    srand((unsigned int) curtime - getpid());
    
    for(int req=0;req<requests;req++)  
    {
        if (req!=0) //here the the childs selects the new segment (30%chance for new segment,70% for same with the previous)
        {
            if((double)rand()/RAND_MAX<0.3)
            {
                 random_seg=(rand() % segs )+ 1; 
            }
            else
            {
                random_seg=previous_seg;
            }
        }
        else
        {
             random_seg=(rand() % segs )+ 1;      
        }

        previous_seg=random_seg;

        int random_line=(rand() %  lines_per_seg)+ 1;  //random line
        gettimeofday(&end1, NULL);
        gettimeofday(&start2, NULL);

        if(sem_wait(mutex[random_seg-1]) < 0) //down the semaphore in order to increase the readers count (1 child by 1)
        {
            perror("sem_wait failed");
            exit(EXIT_FAILURE);
        }

        sMemory->readers_count[random_seg-1]++; //increase the readers count

        if(sMemory->readers_count[random_seg-1]==1) //if its the first reader
        {
        
        if(sem_wait(sem1)<0) //wait for parent to get ready for the request
        {
            perror("sem_wait failed on sem1");
            exit(EXIT_FAILURE);
        }

        sMemory->segment=random_seg;  //number of segment sent to shared memory

        if(sem_post(sem2)<0) //let parent write the segment
        {
            perror("sem_wait failed on sem2");
            exit(EXIT_FAILURE);
        }


        if(sem_wait(rw_mutex)<0) //wait for parent to write
        {
            perror("sem_wait failed ");
            exit(EXIT_FAILURE);
        }

        }

        gettimeofday(&end2, NULL);

        if(sem_post(mutex[random_seg-1]) < 0) //let the other child 
        {
            perror("sem_wait failed");
            exit(EXIT_FAILURE);
        }
        fprintf(fp,"Time taken to submit the request is : %ld micro seconds\n",
        ((end1.tv_sec * 1000000 + end1.tv_usec) -
        (start1.tv_sec * 1000000 + start1.tv_usec)));

        fprintf(fp,"Time taken to get the desired segment is : %ld micro seconds\n",
        ((end2.tv_sec * 1000000 + end2.tv_usec) -
        (start2.tv_sec * 1000000 + start2.tv_usec)));


        fprintf(fp,"<%d,%d>\n",random_seg,random_line);
        fprintf(fp,"%s\n",sMemory->buffer[random_line-1]);
        usleep(20000);

        if(sem_wait(mutex[random_seg-1])<0) //child again 1 by 1 to decrease the readers count
        {
            perror("sem_wait failed");
            exit(EXIT_FAILURE);
        }
        sMemory->readers_count[random_seg-1]--;
        if(sMemory->readers_count[random_seg-1]==0)
        if(sem_post(rw_mutex2)<0)   //let parent run again the while loop
        {
            perror("sem_wait failed");
            exit(EXIT_FAILURE);
        }
        sMemory->sunolo++; //request done
        
        if(sem_post(mutex[random_seg-1])<0)
        {
            perror("sem_wait failed");
            exit(EXIT_FAILURE);
        }

    }

    // Close semaphores used by this child (if not done, leaks are present)
    for (int i=0;i<segs;i++)
    {
        if(sem_close(mutex[i]) < 0)
        {
            perror("sem_close failed on child");
            exit(EXIT_FAILURE);
        }
    }

    if(sem_close(rw_mutex) < 0){
        perror("sem_close(0) failed on child");
        exit(EXIT_FAILURE);
    }
    if(sem_close(rw_mutex2) < 0){
        perror("sem_close(1) failed on child");
        exit(EXIT_FAILURE);
    }
    if(sem_close(sem1) < 0){
        perror("sem_close(2) failed on child");
        exit(EXIT_FAILURE);
    }

    if(sem_close(sem2) < 0){
        perror("sem_close(3) failed on child");
        exit(EXIT_FAILURE);
    }
    fclose(fp);

    return;
}
