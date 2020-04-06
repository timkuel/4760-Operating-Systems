## Author:	Timothy Kuelker ##
## Date:	September 17, 2019 ##
## Course:	CMPSCI 4760 - Operating Systems ##

## ** Project Execution: ** ##

		To execute this program, type 'Make' or 'make' into the command line while inside
	the same directory as the .c file.  A Makefile will compile the program down into the executable
	'sum'.  To execute 'sum' type './sum' into the command line while in the same directory
	as the executable.  For all command line options, type './sum -h' into the command line, 
	while in the same directory as the executable.


## ** Project Description: ** ##

		This program will attempt to find if a subset that sums to a given number exists,
	and if it does, display that subset.  The program starts by reading from a file, if
	the command line option -i was used, it will be the specified file, if not it will default
	to input.dat.  The first line of the file will indicate how many subtasks will be launched
	to process the rest of the file.  Each subtask will be given a single line of the file to
	process, the first number in each line will be the desired number that the subset should sum
	too.  The rest of the line will be the set that could contain subsets that sum to the desired
	number.  If at any point a subtask spends more than 1 second trying to determine if a subset
	exists or not, kill that current subtask.  The user can also specify a -t option, which
	will put a limit to how long the entire program will run.  If that value is exceeded, the
	whole program will be terminated.  All data gathered throughout will be stored to output.dat,
	or you can specify where it will be written with the -o option.  Pass in the -h option for
	more help on how to execute the program.

## ** Difficulties: ** ##
	
		The biggest problem I came across when creating this program was getting all of
	the subsets that sum to the deired number to print.  I ended up finding a backtracking
	method that would print all the desired subsets.  Running the program with different sets
	and desired sum, I would get some odd results printed out, for instance something like
	" = 4" or "4 + 5 + 2 = 9".  Now obviously neither of those are correct, and after some
	debugging...

 ***I came to realize that arrays of size 0 were being passed into the
print_subset() function.  To fix this, I added an extra condition to the if-statement
that would print the subsets, and that is "if (... && t_size > 0){..." where t_size is 
the size of the subset.***

Edited above (in bold).  I initially had a check that solved the empty array problem,
but in some cases the set would return a subset that added up to be more than the target sum.
to fix this, which also took care of the empty array case, I add up all values in the subset
that was passed to the print_subset() function and check it with the target sum.  If it does
not equal the target sum, return out of the function.

		Another problem I had, which was fixed fairly easy, was killing a child if it
	lived longer than 1 second (sounds pretty horrible, right?).  I was doing it in a similar
	manor to how I handled the main program killer, but that kept causing the program to crash.
	I realized quickly that the program would terminate when a child ran longer than 1 second
	instead of just killing off that child.  I fixed this by creating a seperate interrupt handler
	for the child process and main process timed interrupts.  The seperate handler would
	"exit(0)" with the child instead of "kill(pid)" which is what was implemented originally.

