## Author:	Timothy Kuelker ##
### Date:	September 1, 2019 ###
### Course:	CMPSCI 4760 - Operating Systems ###

#### **Project Description:** ####

		Our first project will display directories and their contents using a recursive depth-first search.  Depending
	on options passed in, it will display information on these directories. This project was made using several
	smaller programs.  The main program, runner.c, will get compiled down into the executable dt after 'making' the files.
	dt can then take in several command-line arguments, type in [-h] for help. When an option is passed to dt, it will
	then execute that specific option, passing it the path to the current directory unless specified otherwise. dt will
	launch a new process when executing the desired option.  The idea behind me setting up the project this way was to
	save run time memory.  This way only desired parts of the program get launched and not the whole thing.  One of the
	only problems that you may come across when displaying directory information is when a directory has a depth of 4 or
	more.  The spacing in between the directories and the  displayed information becomes 0 forcing information onto the
	next lines.  The same thing happens when a directory or file has a name that's fairly large.  If this happens, and
	you're wanting to see the information but disregard indentations, set the n in the  [-I n] option to 0.  This should
	allow the directories and their information to be displayed on the same line.

