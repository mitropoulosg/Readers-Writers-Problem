#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <sys/wait.h>

#include "shared_memory.h"

#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
void child(int segs,int lines_per_seg,int i,sharedMemory sMemory,int requests,void* sem1, void* sem2, void* rw_mutex,sem_t* mutex[],void* rw_mutex2);


int num_of_lines(char* filename)
{
    FILE* fp=fopen(filename, "r");//open text file
    int count=0;//count lines
    char c;

    for (c = getc(fp); c != EOF; c = getc(fp))
    {
        if (c == '\n') // Increment count if this character is newline
        count = count + 1;
    }
    fclose(fp);    // Close the file
    return count;
}

int longest_line(char* filename)
{
  FILE* fp=fopen(filename, "r");//open text file
  char str[100],longest[100];
  int len=0;
  if(fp==NULL)
  {
    printf("error\n");
      return 0;
  }
  while(fgets(str,sizeof(str),fp)!=NULL)  //find longest line and copy to str
  {
     if(len<strlen(str))
     {
         strcpy(longest,str);
         len=strlen(str);
     }
  }
  fclose(fp);
  return strlen(longest);
}

int main(int argc,char* argv[])
{
    if (argc != 5) { 
		printf("Arguments must be 4 \n"); exit(1);
	}
    char* filename=argv[1];   //arguments:filename
    int lines_per_seg=atoi(argv[2]); //lines per segment
    int n_childs=atoi(argv[3]);  //number of childs
    int requests=atoi(argv[4]); //number of requests per child




    //Unlink 4 named semaphores
    sem_unlink("sem1");
    sem_unlink("sem2");
    sem_unlink("rw_mutex");
    sem_unlink("rw_mutex2");

    // Initialize 4 named semaphores


    sem_t* sem1 = sem_open("sem1", O_CREAT | O_EXCL, SEM_PERMS, 0);

    if(sem1 == SEM_FAILED){
        perror("sem_open(1) failed");
        exit(EXIT_FAILURE);
    }

    sem_t* sem2 = sem_open("sem2", O_CREAT | O_EXCL, SEM_PERMS, 0);

    if(sem2 == SEM_FAILED){
        perror("sem_open(2) failed");
        exit(EXIT_FAILURE);
    }

    sem_t* rw_mutex = sem_open("rw_mutex", O_CREAT| O_EXCL , SEM_PERMS, 0);

    if(rw_mutex == SEM_FAILED){
        perror("sem_open(3) failed");
        exit(EXIT_FAILURE);
    }

    sem_t* rw_mutex2 = sem_open("rw_mutex2", O_CREAT| O_EXCL , SEM_PERMS, 0);

    if(rw_mutex == SEM_FAILED){
        perror("sem_open(4) failed");
        exit(EXIT_FAILURE);
    }

    int num_lines=num_of_lines(filename);//number of lines
    int segs=num_lines/lines_per_seg;//number of segments
    int size_line=longest_line(filename);//size of longest line
    char names[segs][segs];  //array of names for an array of names semaphores
    sem_t* mutex[segs]; //array of semaphores


    for(int i=0;i<segs;i++)
    {
        sprintf(names[i],"%d",i);
        mutex[i]=sem_open(names[i], O_CREAT , SEM_PERMS, 1);
        if(mutex[i] == SEM_FAILED)
        {
            perror("sem_mutex failed");
            exit(EXIT_FAILURE);
        }
    }

    int shmid;
    sharedMemory sMemory;

    // Create memory segment
    if((shmid = shmget(IPC_PRIVATE,sizeof(sharedMemory), (S_IRUSR|S_IWUSR))) == -1)
    {
        perror("Failed to create shared memory segment1");
        return 1;
    }

    // Attach memory segment
    if((sMemory = (sharedMemory)shmat(shmid, NULL, 0)) == (void*)-1){
        perror("Failed to attach memory segment");
        return 1;
    }

    //create and attach shared memory for a 2d array
    if((shmid =shmget(IPC_PRIVATE,(lines_per_seg)*sizeof(char*), (S_IRUSR|S_IWUSR))) == -1)
    {
        perror("Failed to create shared memory segment2");
        return 1;
    }
    if((sMemory->buffer = (char**)shmat(shmid, NULL, 0)) == (void*)-1){
        perror("Failed to attach memory segment");
        return 1;
    }

    for (int index=0;index<lines_per_seg;++index)
    {
        if((shmid =shmget(IPC_PRIVATE,size_line*sizeof(char), (S_IRUSR|S_IWUSR))) == -1)
        {
            perror("Failed to create shared memory segment3");
            return 1;
        }  
        if((sMemory->buffer[index] = (char*)shmat(shmid, NULL, 0)) == (void*)-1)
        {
        perror("Failed to attach memory segment");
        return 1;
        }        
    }

    //create and attach shared memory for an array (readers_count)
    if((shmid =shmget(IPC_PRIVATE,segs*sizeof(int), (S_IRUSR|S_IWUSR))) == -1)
    {
        perror("Failed to create shared memory segment4");
        return 1;
    }
    if((sMemory->readers_count = (int*)shmat(shmid, NULL, 0)) == (void*)-1){
        perror("Failed to attach memory segment");
        return 1;
    }

    //initialize some values
    sMemory->sunolo=0;
    for(int i=0;i<segs;i++)
    {
        sMemory->readers_count[i]=0;
    }

    // Children array
    pid_t pids[n_childs];
    struct timeval start,end;

    // Initialize K kids
    int i = 0;
    for(i = 0; i < n_childs; i++){

        if((pids[i] = fork()) < 0){ // Fork new process
            perror("Failed to create process");
            return 1;
        }
        if(pids[i] == 0){   
               // If it is child process
            child(segs,lines_per_seg,i,sMemory,requests, sem1,sem2,rw_mutex,mutex,rw_mutex2);
            exit(0);
        }
    }

    while(sMemory->sunolo<requests*n_childs)
    {
        gettimeofday(&start, NULL);
        // child is waiting to request a segment, so if parent is ready, posts
        if(sem_post(sem1) < 0)
        {
            perror("sem_wait failed on parent");
            exit(EXIT_FAILURE);
        }

        // Wait for child to request
        if(sem_wait(sem2) < 0)
        {
            perror("sem_wait failed on parent");
            exit(EXIT_FAILURE);
        }

        int desired_segment = sMemory->segment;  //take the number of segment
        // Open file and search for the first line of the segment
        FILE* fp1 = fopen(filename, "r");
        char line[size_line + 2]; // size line+2 characters to also save '\n' and '\0' at the end of the line
        int desired_line = (desired_segment-1)*lines_per_seg+1; //find first line
        int j=0;
        int i=0;
        while(fgets(line, sizeof(line), fp1))
        {
            j++;
            if(j == desired_line)
            {
                if(i<lines_per_seg)
                {
                strcpy(sMemory->buffer[i], line);    //copies all lines needed in order to store the segment in shared memory
                i++; 
                desired_line++;
                }
            }
        }
        fclose(fp1);
        
        if(sem_post(rw_mutex) < 0)  //the write is finished, so unblock the child
        {
            perror("sem_post failed on parent");
            exit(EXIT_FAILURE);
        }

        // Wait for consumer to read the segment and tell that has finished 
        if(sem_wait(rw_mutex2) < 0)
         {
             perror("sem_wait failed on parent");
             exit(EXIT_FAILURE);
         }
        gettimeofday(&end, NULL);
        printf("Time taken for segment %d to leave the shm is : %ld micro seconds\n",sMemory->segment,
        ((end.tv_sec * 1000000 + end.tv_usec) -
        (start.tv_sec * 1000000 + start.tv_usec)));
    }

    int status;

    for(int i = 0; i < n_childs; i++)
    {
        wait(&status);
    }


    //Detach shared memory

    for(int i=0;i<lines_per_seg;i++)
    {
        if(shmdt((void*)sMemory->buffer[i]) == -1){
        perror("Failed to destroy shared memory segment");
        return 1;
        }
    }

    if(shmdt((void*)sMemory->buffer) == -1){
       perror("Failed to destroy shared memory segment");
       return 1;
    }

    if(shmdt((void*)sMemory) == -1){
       perror("Failed to destroy shared memory segment");
       return 1;
    }

    // Close and unlink semaphores
    if(sem_close(rw_mutex2) < 0){
        perror("sem_close(0) failed");
        exit(EXIT_FAILURE);
    }

    if(sem_unlink("rw_mutex2") < 0){
        perror("sem_unlink(0) failed");
        exit(EXIT_FAILURE);
    }


    if(sem_close(rw_mutex) < 0){
        perror("sem_close(0) failed");
        exit(EXIT_FAILURE);
    }
    if(sem_unlink("rw_mutex") < 0){
        perror("sem_unlink(0) failed");
        exit(EXIT_FAILURE);
    }

    if(sem_close(sem1) < 0){
        perror("sem_close(1) failed");
        exit(EXIT_FAILURE);
    }
    if(sem_unlink("sem1") < 0){
        perror("sem_unlink(1) failed");
        exit(EXIT_FAILURE);
    }

    if(sem_close(sem2) < 0){
        perror("sem_close(2) failed");
        exit(EXIT_FAILURE);
    }
    if(sem_unlink("sem2") < 0){
        perror("sem_unlink(2) failed");
        exit(EXIT_FAILURE);
    }

    for(i=0;i<segs;i++)
    {
        if(sem_close(mutex[i]) < 0){
        perror("sem_close mutex failed");
        exit(EXIT_FAILURE);
    }
        if(sem_unlink(names[i]) < 0){
        perror("sem_unlink mutex failed");
        exit(EXIT_FAILURE);
    }

    }
    return 0;
}
