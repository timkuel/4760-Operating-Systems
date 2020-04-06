## Author:	Timothy Kuelker ##
### Date:	November 10, 2019 ###
### Course:	CMPSCI 4760 - Operating Systems ###

#### **Project Description:** ####

**Before Executing**
	Ocassionally, the program will run into situations where the system is always in a deadlock.  Sometimes,
this will occur for multiple executions in a row (possible due to random number generator).  So to help with that,
I will have some physical printing that may help indicate if the program has run into a constant deadlock.

 
**IMPORTANT**
Running in verbose mode tends to give less deadlocks.

	
** Executing **
	To execute the program, type 'Make' or 'make' into the command line while inside
the same directory as the .c file. A Makefile will compile the program down into the executable
'oss' and 'user'.  To execute 'oss' and 'user' type './oss' into the command line while in the same directory
as the executable, 'oss' will launch 'user' itself.

	'oss' can be executed with 2 different parameters, a '-h' parameter that will display a help message
if need be, and a '-v' option that will turn on verbose mode. Normally, without verbose mode on, the program
will only write to the log file (kuelkerLog.dat) when it detects a deadlock.  With verbose mode on, it will 
write when a request was granted, a process is terminating, a deadlock is detected, and when 20 requests
were granted, it will display the current state of the allocation matrix (the number of resources each process
currently has).


** Difficulties **
	This prject was less difficult than the previous scheduler, but I still ran into some problems.
One of them occured when I went to add the verbose mode into the program.  For whatever reason my system
seemed to run into deadlocks more often.  I tweaked the numbers to where it will give good output if 
verbose mode is turned on rather than off.


