/* Author:      Tim Kuelker
 * Date:        September 9, 2019
 * Course:      CMPSCI 4760 - Operating Systems
 * Description: This program starts off by reading the first line of a file,
 * the file can be defined or will default to input.dat.  The first line will be the
 * number of children that need to be launched to process each line of the
 * file individually.  When a child process starts, it will read the line given
 * to it, and it will then try to find a subset of numbers that equal the first
 * number on the line.  The set will exclude the first number on the line, and
 * the rest of the line will be the set.  All data, whether a subset was found
 * to sum to the first number or not, will be written to the output file, defaulted
 * to output.dat.  Pass in command line option -h for information on how to properly
 * execute the program.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>


void help_message(char *);
bool is_number(char []);
void kill_handler(int );
void searched_too_long_kill_handler(int );
unsigned int kill_alarm(int );
bool is_subset_sum(int [], int , int );
void clear_output_file(FILE *, char *);
void generateSubsets(int [], int , int , FILE *);
void subset_sum(int [], int [], int , int , int , int , int , FILE *);
void print_subset(int [], int , int, FILE *);


jmp_buf main_killer;
jmp_buf child_killer;


int main(int argc, char *argv[]) {
    char * in_file = " ", *out_file = " ";
    int c, sum, timer = 10;
    char line[81] = {0};
    int numbs[50] = {0};
    char *token;
    int pid = 0, status_code = 0, num_children = 0;
    FILE *input, *output;

    char error_string[100];

    /* If signal was caught, jump UP THE CALL STACK to here
     * setjmp can only jump up the call stack. */
    if(setjmp(main_killer) == 1){
        errno = ETIME;

        snprintf(error_string, 100, "\n\n%s: Error: PROGRAM TIMED OUT - exceeded maximum runtime of %d second(s) ", argv[0], timer);
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n\n", errno);

        if (input != NULL)
            fclose(input);
        if (output != NULL){
            fprintf(output, "Program terminated due to the overall timer exceeding %d second(s).\n", timer);
            fclose(output);
        }
        kill(pid,SIGKILL);

        exit(EXIT_SUCCESS);
    }

    if(setjmp(child_killer) == 1){
        errno = ETIME;

        snprintf(error_string, 100, "\n\n%s: Error: CHILD %d TERMINATED - subset was not found within 1 second ", argv[0], getpid());
        perror(error_string);
        fprintf(stderr, "\tVale of errno: %d\n", errno);

        fprintf(output, "%d:  No valid subset found after 1 second.\n\n", getpid());
        exit(0);
    }

    /* Signals waiting to be caught  */
    signal(SIGALRM, kill_handler);

    /* Getopt allows passed in variables to be checked and handled a lot easier */
    while ((c = getopt (argc, argv, "hi:o:t:")) != -1)
        switch (c) {
            case 'h':
                help_message(argv[0]);
                break;
            case 'i':
                in_file = optarg;
                break;
            case 'o':
                out_file = optarg;
                break;
            case 't':
                if(!is_number(optarg)){
                    errno = EINVAL;
                    perror("\nThe argument passed in was not an integer ");
                    fprintf(stderr, "Value of errno: %d\n\n\tTERMINATING PROGRAM\n\n", errno);
                    return 1;
                }
                timer = atoi(optarg);
                break;
            case '?':
                errno = EINVAL;
                if (optopt == 'i'){
                    snprintf(error_string, 100, "\n%s: Error: Option [-i] requires an argument ", argv[0]);
                    perror(error_string);
                    fprintf (stderr, "Value of errno: %d\n\n", errno);
                }
                else if (optopt == 'o'){
                    snprintf(error_string, 100, "\n%s: Error: Option [-o] requires an argument ", argv[0]);
                    perror(error_string);
                    fprintf(stderr, "Value of errno: %d\n\n", errno);
                }
                else if(optopt == 't'){
                    snprintf(error_string, 100, "\n%s: Error: Option [-t] requires an argument ", argv[0]);
                    perror(error_string);
                    fprintf(stderr, "Value of errno: %d\n\n", errno);
                }
                else {
                    snprintf(error_string, 100, "\n%s: Error: Unknown option character ", argv[0]);
                    perror(error_string);
                    fprintf (stderr, "\nUnknown Character `\\x%x'.\nValue of errno: %d\n\n", optopt, errno);
                }
                return 1;
            default:
                help_message(argv[0]);
        }

    /* Function that will start the timed alarm interrupt  */
    kill_alarm(timer);


    /* If the file names were not changed, set defaults */
    if(strcmp(in_file, " ") == 0)
        in_file = "input.dat";
    if(strcmp(out_file, " ") == 0)
        out_file = "output.dat";

    /* Clearing the output file so that all data gathered will go into blank file  */
    clear_output_file(output, out_file);

    input = fopen(in_file, "r");

    /* Checks whether the file exists */
    if (input == NULL){
        errno = ENOENT;
        perror("\nThe file that was attempted to open does not exist ");
        fprintf(stderr, "File name inputted: %s\nValue of errno: %d\n\n", in_file, errno);
        return 1;
    }

    /* Stores first line and launches that many child processes  */
    num_children = atoi(fgets(line, 4, input));

    /* Array to hold data of all children PID's */
    int pid_holder [num_children];

    output = fopen(out_file, "a");

    fprintf(stderr, "\nStarting to calculate the Subset-Sum problem, launching %d children!\n", num_children);

    int x;
    for(x = 0; x < num_children; x += 1){
        /* Forking off a child process */
        pid = fork();

        /* Reading file before doing work in child process  */
        fgets(line, sizeof(line), input);

        if(pid == -1){
            perror("\ndt:  Error: Failed to fork ");
            fprintf(stderr, "Value of errno: %d\n\n", errno);
            exit(EXIT_FAILURE);
        }
        if(pid == 0){
            signal(SIGALRM, searched_too_long_kill_handler);

            /* strtok is allowing the program to split the line into tokens, using the delimiter " ". */
            token = strtok(line, " ");
            int k;

            /* Trying to find subset that equals sum */
            sum = atoi(token);

            /* Putting rest of tokens into numbs array */
            k = 0;
            while((token = strtok(NULL, " ")) != NULL){
                numbs[k] = atoi(token);
                k += 1;
            }

            /* Getting the amount of numbers in numbs array */
            for( k = 0; k < sizeof(numbs); k += 1){
                if(numbs[k] <= 0)
                    break;
            }

            /* Since numbs would be an excessively big array due to not knowing how big each line will be,
             * this is an attempt to make the array smaller and trim any zeros or negatives. */
            int work_array[k];
            int m;
            for(m = 0; m < k; m+=1){
                work_array[m] = numbs[m];
            }

            int n = sizeof(work_array)/sizeof(work_array[0]);

            /* Starting kill_alarm, giving 1 second to find subset */
            kill_alarm(1);
            if(is_subset_sum(numbs, n, sum)) {
                /*  Resetting kill_alarm since subset found, now printing */
                kill_alarm(0);
                fprintf(stderr,"\n%d: found a subset containing the sum %d\n", getpid(), sum);
                generateSubsets(work_array, n, sum, output);
            }
            else{
                fprintf(stderr,"\n%d: could not find a subset containing the sum %d\n", getpid(), sum);
                fprintf(output,"%d: No subset of numbers summed to %d.\n", getpid(), sum);
            }


            fprintf(output, "\n");

            exit(0);
        }

        else{
            wait(&status_code);
        }

        /* If child process failed in someway */
        if(status_code != 0){

            snprintf(error_string, 100, "\n\n%s: Error: CHILD PROCESS FAILED ", argv[0]);
            perror(error_string);
            fprintf(stderr, "\tVale of errno: %d\n\n", errno);

            fclose(input);
            if (output != NULL){
                fprintf(output, "%d: child: %d failed with status_code: %d\n", getppid(), getpid(), status_code);
                fclose(output);
            }
        }
        pid_holder[x] = pid;
    }

    for(x = 0; x < num_children; x += 1){
        fprintf(output,"%d: launched child %d\n", getppid(), pid_holder[x]);
    }

    fclose(input);
    fclose(output);
    fprintf(stderr,"\n\nAll children finished running, check the file [ %s ] for program details.\n\n", out_file);
    return 0;
}


void help_message(char *executable){
    fprintf(stderr, "\nHelp Option Selected [-h]: The following is the proper format for executing "
                    "the file:\n\n%s", executable);
    fprintf(stderr, " [-h] [-i n] [-o n] [-t n]\n\n");
    fprintf(stderr, "[-h]   : Print a help message and exit.\n"
                    "[-i n] : This option will be used to specify the input file to read the data from. "
                    "(default: input.dat)\n"
                    "[-o n] : This option will be used to specify the output file to write the data to. "
                    "(default: output.dat)\n"
                    "[-t n] : This option will allow the user to set up a timed interrupt.  "
                    "Program will run no-longer than 'n' seconds. (default: 10 seconds)\n\n"
                    "Default: If no options were passed in the defaults will go as followed:\n"
                    "\tInput File: input.dat\n\tOutput File: output.dat\n\tProgram Timeout: 10 seconds\n\n");

    exit(EXIT_SUCCESS);
}


bool is_number(char number[]){
    int i = 0;

    /* checking for negative numbers */
    if (number[0] == '-')
        i = 1;
    for (; number[i] != 0; i++){
        /* if (number[i] > '9' || number[i] < '0') */
        if (!isdigit(number[i]))
            return false;
    }
    return true;
}


/* handles signal */
void kill_handler(int dummy){ longjmp(main_killer,1); }


/* handles signal */
void searched_too_long_kill_handler(int dummy){ longjmp(child_killer,1); }


/* Starts a timer that will release an alarm signal at the desired time */
unsigned int kill_alarm (int seconds){
    struct itimerval old, new;
    new.it_interval.tv_usec = 0;
    new.it_interval.tv_sec = 0;
    new.it_value.tv_usec = 0;
    new.it_value.tv_sec = (long int) seconds;
    if (setitimer (ITIMER_REAL, &new, &old) < 0){
        return 0;
    }
    else{
        return (unsigned int) old.it_value.tv_sec;
    }
}


/* Returns true if there is a subset of set[] with sun equal to given sum */
bool is_subset_sum(int set[], int n, int sum){
    /* Base Cases */
    if (sum == 0)
        return true;
    if (n == 0)
        return false;

    /* If last element is greater than sum, then ignore it */
    if (set[n-1] > sum)
        return is_subset_sum(set, n-1, sum);
    /* else, check if sum can be obtained by any of the following
     * (a) including the last element
     * (b) excluding the last element */
    return is_subset_sum(set, n-1, sum) ||
           is_subset_sum(set, n-1, sum-set[n-1]);
}


/* Function will clear any data from outfile */
void clear_output_file(FILE *output, char *out_file){
    output = fopen(out_file, "w");
    fclose(output);
}



/* The following functions use backtracking to find then print the all
 * of the subsets that equal the sum.  The functions were found at:
 *
 *        https://geeksforgeeks.org/subset-sum-backtracking-4
 *
 * I modified them a bit so that all data is written to an output file,
 * and I came across an error where empty arrays were being passed to
 * print_subset().  Added extra check to ensure an empty array will
 * not be passed to it. */


/* prints subset found */
void print_subset(int A[], int size, int target_sum, FILE * output) {
    int i, summer = 0;

    /* Checking subset to be sure equals target_sum */
    for(i = 0; i < size; i += 1)
        summer += A[i];

    /* If subset does not equal target_sum, return */
    if(summer != target_sum)
	return;

    fprintf(output, "%d: ", getpid());
    for(i = 0; i < size; i += 1) {
        if(A[i] == 0)
            continue;
        if(i == 0){
            fprintf(output, "%d", A[i]);
            continue;
        }
        fprintf(output, " + %d", A[i]);
    }
    fprintf(output, " = %d\n", target_sum);
}


/* inputs
 * s             - set vector
 * t             - tuple vector
 * s_size        - set size
 * t_size        - tuple size so far
 * sum           - sum so far
 * ite           - nodes count
 * target_sum    - sum to be found */
void subset_sum(int s[], int t[], int s_size, int t_size, int sum, int ite, int target_sum, FILE *output){
    int i;

    if(target_sum == sum){
        /* We found sum */
        print_subset(t, t_size, target_sum, output);

        /* Exclude previous added item and consider next candidate */
        subset_sum(s, t, s_size, t_size - 1, sum - s[ite], ite + 1, target_sum, output);

        return;
    }
    else{
        /* generate nodes along the breadth */
        for(i = ite; i < s_size; i += 1){
            t[t_size] = s[i];

            /* consider next level node (along depth) */
            subset_sum(s, t, s_size, t_size + 1, sum + s[i], i + 1, target_sum, output);
        }
    }
}


/* Wrapper that prints subsets that sum to target_sum */
void generateSubsets(int s[], int size, int target_sum, FILE *output){
    int *tuple_vector = (int *)malloc(size * sizeof(int));

    subset_sum(s, tuple_vector, size, 0, 0, 0, target_sum, output);

    free(tuple_vector);
}
