Author:	Timothy Kuelker
Date:	October 27, 2019
Course:	CMPSCI 4760 - Operating Systems


**IMPORTANT**

**Before execution**
	I had an issue where my program would freeze randomly when going to re-queue a process. Due to this
I have some output going to the screen, that way if theres a major pause in the program, you can see it and
terminate the program.  It is setup to where all data will be removed if program is terminated manually. 



Project Execution:
	
	To execute the program, type 'Make' or 'make' into the command line while inside
the same directory as the .c file. A Makefile will compile the program down into the executable
'oss' and 'user'.  To execute 'oss' and 'user' type './oss' into the command line while in the same directory
as the executable, 'oss' will launch 'user' itself.


Project Description:

	Program acts like an operating system scheduler.  It starts off by loading some process into a queue, assigning each
process a Process Control Block that is part of a Process Control Table.  After a certain amount of time, the program
will then begin to 'schedule' all these processes. All process will be initially put inside queue-0, but if it's accumulated
wait time and the average wait time of that queue is above a threshold, move it up a queue.  This program has a 3 level queue
system and a blocked queue.  Each user process has a chance to terminate, be blocked, or run for a certain amount of time and
be re-entered into a queue.  Program will run until 100 process have been launched and completed or 3 seconds have passed and
all the processes launched before that point will be allowed to finish.


Difficulties:

	I had a difficult time allotting myself time to finish this project.  I underestimated how difficult it would be to
tune my scheduler, and I'm pretty sure it still is not the best.  I see processes moving into each queue and processes from
each queue being launched.  

	When I initially started, I had a funciton that would format the time for me, I realized later that it was causing
a data type overflow.  So I rewrote my method to format the time, and had to rewrite all my comparisons.  This took a bunch of time,
even with verison control, since no matter what I had to re-calculate how the string comparison was going to work.

