## Author:	Timothy Kuelker ##
### Date:	December 2, 2019 ###
### Course:	CMPSCI 4760 - Operating Systems ###

#### **Project Description:** ###


#### **Executing** ####
		To execute the program, type 'Make' or 'make' into the command line while inside
	the same directory as the .c file. A Makefile will compile the program down into the executable
	'oss' and 'user'.  To execute 'oss' and 'user' type './oss' into the command line while in the same directory
	as the executable, 'oss' will launch 'user' itself.


#### **Difficulties** ####
		I had a tough time getting a decent amount of processes launched.  In the beginning I kept
	getting hung up on the message queue.  To fix this, I added a check to see if a process was launched,
	if one was not, it will not try and wait for a message.  It will increment the clock before doing so.
	This helped with some of the efficiency.

	Also had a tough time with either the mapping of the page tables or how the references were
	cleared.  Not sure which, but when I fixed an error I had, I had configured both of those.  This issue
	was causing every page to page fault, regardless of whether in the frame table or not.
