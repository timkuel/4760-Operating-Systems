// Author:	Tim Kuelker
// Date:	September 1, 2019
// Course:	CMPSCI 4760 - Operating Systems
// Description:	This program takes in user options on execution, based off
// of what the user passed in as their option, this process will launch 
// another that will do the desired task.  Pass in [-h] for help.


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

void help_message(char *);
void do_stat(char * , char * , char * , char * );
void pid_runner(char *, char *, char *, char *);
extern int errno;


int main(int argc, char *argv[]) {
    char *spacing = "55";
    char *indent = "4";
    int index, c;
    char *directory = NULL;

    opterr = 0;

    if(argc < 2)
	help_message(argv[0]);

    // Getopt allows passed in variables to be checked and handled a lot eaiser
    while ((c = getopt (argc, argv, "hI:Ldgipstul")) != -1)
        switch (c) {
            case 'h':
                help_message(argv[0]);
                break;
	    case 'I':
                if(!isdigit(*optarg)){
                    errno = EINVAL;
		    perror("\ndt:  Error:  Option [-I n] selected but 'n' is not a digit ");
		    fprintf(stderr, "Value of errno: %d\n\n", errno);
                    return errno;
                }
                indent = optarg;

		// If the indention exceeds a certain size, increase spacing
		if(atoi(indent) > 4)
			spacing = "75";
		if(atoi(indent) > 8)
			spacing = "105";

		break;
            case '?':
                if (optopt == 'I'){
		    errno = EINVAL;
		    perror("\ndt:  Error:  Option [-I n] requires an argument ");
                    fprintf (stderr, "Value of errno: %d\n\n", errno);
		}
                else {
		    errno = EINVAL;
		    perror("\ndt:  Error:  Unknown option character ");
                    fprintf (stderr, "\nUnknown Character `\\x%x'.\nValue of errno: %d\n\n", optopt, errno);
		}
                return 1;
            case 'L':
                for (index = optind; index < argc; index++)
                    directory = argv[index];

                if(directory == NULL)
                    directory = ".";
	
		do_stat("./flnks", directory, indent, spacing);
		break;
            case 'd':
                for (index = optind; index < argc; index++)
                    directory = argv[index];

                if(directory == NULL)
                    directory = ".";
		
		do_stat("./smd", directory, indent, spacing);
		break;
            case 'g':
		for (index = optind; index < argc; index++)
                    directory = argv[index];

                if(directory == NULL)
                    directory = ".";

		do_stat("./gid", directory, indent, spacing);
                break;
            case 'i':
		 for (index = optind; index < argc; index++)
                    directory = argv[index];

                if(directory == NULL)
                    directory = ".";

                do_stat("./slnks", directory, indent, spacing);
                break;
            case 'p':
		for (index = optind; index < argc; index++)
                    directory = argv[index];

                if(directory == NULL)
                    directory = ".";

		do_stat("./perms", directory, indent, spacing);
                break;
            case 's':
                for (index = optind; index < argc; index++)
                    directory = argv[index];

                if(directory == NULL)
                    directory = ".";

		do_stat("./size", directory, indent, spacing);
                break;
            case 't':
                for (index = optind; index < argc; index++)
                    directory = argv[index];

                if(directory == NULL)
                    directory = ".";
		 
		do_stat("./sft", directory, indent, spacing);	
		break;
	    case 'u':
		for (index = optind; index < argc; index++)
                    directory = argv[index];

                if(directory == NULL)
                    directory = ".";

		do_stat("./uid", directory, indent, spacing);
                break;
            case 'l':
                for (index = optind; index < argc; index++)
                    directory = argv[index];

                if(directory == NULL)
                    directory = ".";
		
		do_stat("./sall", directory, indent, spacing);
                break;
            default:
		help_message(argv[0]);
        }

    fprintf(stderr, "\n");
    return 0;
}


// Function that will lstat the desired or default directory, fails if not directory
void do_stat(char *runner, char *directory, char *indent, char *spacing){
	struct stat sb;
	
        if (lstat(directory, &sb) == -1) {
                perror("\ndt:  Error:  Could not stat ");
                fprintf(stderr, "Value of errno: %d\n\n", errno);
        	exit(EXIT_FAILURE);
        }

	pid_runner(runner, directory, indent, spacing);
}


// Function executes the desired process based off of what option was given above
void pid_runner(char *runner, char *directory, char *indent, char *spacing){
        int statuscode = 0, pid = 0;

        pid = fork();
        if(pid == -1){
                perror("\ndt:  Error: Failed to fork ");
		fprintf(stderr, "Value of errno: %d\n\n", errno);
                exit(EXIT_FAILURE);
        }

        if(pid == 0){
                execl(runner, directory, indent, spacing, NULL);
                //only reached if execl fails
                perror("\ndt:  Error:  EXECL() failed to execute the subprogram, be sure programs were 'made' ");
		fprintf(stderr, "Value of errno: %d\n\n", errno);
		exit(EXIT_FAILURE);
       }
       else{
                wait(&statuscode);
       }

        //If executed process failed in someway
        if(statuscode != 0){
		errno = ENOEXEC;
        	perror("\ndt:  SubProgramError:  Process failed to execute or while running the subprogram ");
                fprintf(stderr, "Value of errno: %d\nVaue of statuscode: %d\n\n", errno, statuscode);
		exit(EXIT_FAILURE);
	}

        return;
}


void help_message(char *executable){
    fprintf(stderr, "\nHelp Option Selected [-h]: The following is the proper format for executing "
                    "the file:\n\n%s", executable);
    fprintf(stderr, " [-h] [-I n] [-L -d -g -i -p -s -t -u | -l] [dirname]\n\n");
    fprintf(stderr, "[-h]   : Print a help message and exit.\n"
                    "[-I n] : Change indentation to 'n' spaces for each level.\n"
                    "[-L]   : Follow symbolic links, if any.  Default will be to not follow symbolic links\n"
                    "[-d]   : Show the time of last modification.\n"
                    "[-g]   : Print the GID associated with the file.\n"
                    "[-i]   : Print the number of links to file in inode table.\n"
                    "[-p]   : Print permission bits as rwxrwxrwx.\n"
                    "[-s]   : Print the size of file in bytes.\n"
                    "[-t]   : Print information on file type.\n"
                    "[-u]   : Print the UID associated with the file.\n"
                    "[-l]   : This option will be used to print information on the file as if the options tpiugs are all specified.\n\n");

    exit(EXIT_SUCCESS);
}

