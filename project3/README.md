## Author:	Timothy Kuelker ##
### Date:	October 1, 2019 ###
### Course:	CMPSCI 4760 - Operating Systems ###


#### ** Project Execution: ** ####
	
		To execute the program, type 'Make' or 'make' into the command line while inside
		the same directory as the .c file. A Makefile will compile the program down into the executable
		'oss' and 'user'.  To execute 'oss' and 'user' type './oss' into the command line while in the same directory
		as the executable, 'oss' will launch 'user' itself.  For all command line options, type './oss -h' into the 
		command line, while in the same directory as the executable.



#### ** Program Description: ** ####

		This program acts like the shell of an OS simulator that does very basic tasks using fork, exec,
	shared memory, and semaphores. 

* ** ossim.c **
	'ossim.c' or 'oss' will start by either being launched with or without command
line options (use command line option -h for more info).  After execution, 'oss' will launch 's' child processes,
if one is not defined it will launch 5.  After the children launch, 'oss' starts to enter a while loop, with each
iteration 'oss' will increment a virtual clock thats in shared memory. Before 'oss' can increment the virtual
clock, it needs to gain access to a critical region within the while loop.  It will gain access from a 'user' child
process cedeing its turn to allow it into the critical region (more on 'user.c' in a moment).  In the critical region
'oss' will check if a child is trying to terminate by checking some shared memory segment, if one is trying to terminate,
'oss' will wait for it to finish and then it will display some information to a file, which can be specified with the -l 
option, but is 'output.dat' by default. 'oss' will continue to loop for either 2 virtual seconds, or 100 children have
been spawned.  

- 'user.c'
	'user.c' or 'user' when launched, will attach to the shared memory created by 'oss'.  'user' will look at the
virtual clock nanoseconds and it will generate a random number between 1 and 1000000.  'user' will add that random 
number to the nanoseconds it saw when it peeked at the shared memory.  'user' will then go into a while loop and try to
enter into a critical region.  Each iteration of the while loop, 'user' process wil try to gain access to a critical region.
When a 'user' process gains access to the critical region (after either 'oss' or another 'user' process ceeded the 
critical region) it will look at the virtual clock again, if the virtual time is greater than the nanoseconds + random number 
that the process got earlier, 'user' will put a message into shared memory, which 'oss'will see, and try to exit. If the 
message is full, that current 'user' process will go back into the critical region until it can post a message and exit.





Virtual Timer:

	When creating the the virtual timer, I had slight difficulties on deciding on a number to use as an incrementer.
I tried to find one that wouldn't count too fast or too slow, but I never know when the 'oss' process will gain
access to the critical region to increment the clock.  This made it difficult to use a stable number, so I decided
to use a random number.  The random number will be in the range of 3,000,000 to 5,000,000, which increments the nanoseconds
part of the clock ( 1 Second is 1,000,000,000 nano seconds).  This number, after running 'oss' several times with different
number of concurrent child process, seemed to give me the best results.  It gave me a fair race between the timer reaching
2 seconds and 'oss' spawining 100 children when 'oss' is launched with the default 5 processes.  Also allowed a smaller
number of child processes launched (usually less than 4) to have a fair chance of spawining a couple more children.



Difficulties:

	The most difficult thing I faced was getting the virtual timer to increment on a good time, which was explained
above.  Another difficulty I did face was trying to kill a 'user' process when one of the interrupts occured (2 virtual 
seconds or 100 processes spawned).  I solved this issue with a linked list that I found online that allowed a link to
be deleted by the key.  Everytime a child terminated, I removed its PID from the list, and each time one is launched, its
added to the list.  At the end, 'oss' will kill anything left over in the list.  Getting the semaphore in place wasnt too hard,
I did initially use a POSIX restricted method, but swapped it so that it now shouldn't be an issue.

-Seeding 'user.c's random number generator was kind of difficult, it uses the virtual times nanoseconds and adds the current time
of the system.  Some processes will get the same seed since 'user' didnt need access to critical region to generate this number.
This allowed some processes to see the same virtual time and start to generate the random number at the same system time as well,
allowing the random number to be the same.
