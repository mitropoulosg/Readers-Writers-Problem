<sub? haha
Parent:
The parent process is initiated using 4 arguments given by the user, a filename,the number of lines each segment must have, the number of child processes to be used and the number of transactions each child process is involved to. The filename is a text file what contains random words(1000+lines).
Afterwards, 4 semaphores are created.
There are 2 functions. One finds how many lines the filename has and the other finds the size of the longest line of the filename.

Then, an array of semaphores is created with size (segments) and a shared memory segment.
The shared memory segment includes an array of readers (useful for child), 2 integers and a 2d array of char (segment)
For the 2 arrays ,shared memory is created too as we dont know the size of them from the start.
Then the given number of child processes are created.

Child:
The child processes act like the (Readers-Writer problem).
asking the parent process for a random segment and a random line.Child writes to a shared memory the number of segment that needs.The number is then read by the parent process, which also returns the content of the segment to a buffer of the shared memory segment, so the child can read this segment and take the desired line.
What is important here is that because both parent and all childs run at the same time, there must be more than 1 requests for the same segment from childs. So when parent writes a segment to shared memory all childs that need this segment can read it at the same time.when there is no more interest for this segment , parent proceeds to the next segment that has been requested.(Readers-Writer problem).
This whole process is controlled by the 4 semaphores  and an array of semaphores described before.

For every child,a log file is created wich contains:
1.Time taken to submit the request
2.Time taken to get the desired segment
3.Desired segment and line
4.The line

Parent prints the time taken for a segment to leave shared memory.

The processes are terminated when the desired number of transactions has been completed among all child processes.

In the end all shared memories are detached and semaphores closed and unlinked.

make
./parent a b c d

a:the name of the input file
b:the number of lines each segment must have
c:the number of childs
d:the number of request per child

I am using comments for extra information in the code.
