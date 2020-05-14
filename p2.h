/******************************************************************************
Name: Jacob Romio
RedID: 822843795
Edoras Account: cssc0082
Course: CS570 Fall 2019
Professor: John Carroll
Project 4: The Shell with enhancements
Due: 11-29-2019(Early), 12-4-2019
File Name: p2.h
Sources: John Carroll p1.c
         Program 2 outline in course reader page 93
	     Page 8: dup2.c for redirecting io
	     Page 10: signalhandler.c for signal handling, ie catching SIGTERM
		 
		 [added for p4]
		 pipe.c from the course reader to understand the general layout for piplining
		 
		 https://www.cs.usfca.edu/~sjengle/ucdavis/ecs150-w04/source/pipe.c which is
		 a pipe.c from uc davis that examples vertical piping which helped design
		 the layout and how execvp is used with piping
******************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "getword.h"/* See getword.h to understand how getword.c works. */
#define _GNU_SOURCE
#define MAXITEM 100 /* max number of words p2 can handle(extremely limited compared to real shell) */
#define MAXHISTORY 9 /* Should hold !1, !2, ..., !9. */

enum boolean{false, true}; /* Helps with logic clarifications in code. */

/* prototypes */
//Remark: These notes may also be found in p2.c. The redundancy is recognized for the sake of
//        convenience, whether they're needed in the moment of code inspection in p2.c or for
//        an overall summary to get an idea of the program's structure. 
void parse();
/*
 * FUNCTION: parse
 * NOTES: Syntactical analysis for "words" being read in. This deals with the !!
 *        and done built-ins, the redirection setup for IO, and setting up the
 *        background processes when a '&' occurs at the end of a line.
 */
 
enum boolean metacharacter();
/*
 * FUNCTION: metacharacter
 * NOTES: Returns a boolean true if the current letter is a metacharacter, or a boolean false if non-metacharacter.
 * OUTPUT: Boolean value for the mc_flag.
 */
 
void add_word(int c);
/*
 * FUNCTION: add_word
 * NOTES: Adds word to current line of arguments by copying it to the big_one character array, then passing a char *
          that points to that location to the newargv array.
 * INPUT: Integer c that is returned from getword() function. This c value is the length of the word.
 */

void flags_to_false();
/*
 * FUNCTION: flags_to_false
 * NOTES: Sets flags used in each main loop back to false.
 */

void io_handler(int flags, int STD_FILENO);
/*
 * FUNCTION: io_handler
 * NOTES: Deals with redirects of input & output by opening the files and putting them in stdin
 *        or stdout as specified.
 * INPUT: Redirection index of input or output for newargv location, flags used for open function,
 *        and STD_FILENO that's either STDIN_FILENO or STDOUT_FILENO to signal Input or Output.
 */
 
void cd_call();
/*
 * FUNCTION: cd_call
 * NOTES: Performs built-in change directory functionality. Only accepts 1 argument
 *        or no arguments, where 1 arg goes to that directory and 0 args goes to the
 *        home directory. More than 1 argument prints an error and does nothing.
 */
 
void io_flag_setter(char *io_stat);
/*
 * FUNCTION: io_flag_setter
 * NOTES: Sets io flags accordingly, as well as index locations in newargv, for child to
 *        handle when io_handler is called for stdin, stdout, and stderr setup. If multiple
 *        redirects are in a line, that is more than 1 input redirect or more than 1 output
 *        redirect, errors will be printed accordingly.
 * INPUT: char * to signal whether we're working with Input or Output.
 */
 
 void exec_call(int offset);
 /*
 * FUNCTION: exec_call
 * NOTES: Calls the execvp to occur. If an error flag is true, the exec call
 *        will NOT occur, and an error is sent to stderr.
 * INPUT: The offset from newargv. Helps distinguish child vs grandchild.
 */
 
/*
   [OVERVIEW]
       The p2 program is a simple command line interpreter for UNIX systems that displays a prompt with "%n% " where n is
   the integer representing the current command line you are on. It takes in user input to perform basic actions a
   typical shell could handle including functionalities such as file redirection with '<', '>', '>&', '>>', and '>>&'
   characters, backgrounding processes with '&' as the last character, repeating the last commands with the word "!!"
   as the first word, changing directories with "cd" as the first word, executing the first word found by creating
   child processes and calling execvp, executing both a child and a grandchild process if '|' is found, commenting
   out text with '#' when inputing a script, repeating the last word in the previous line with !$, and calling history
   of the first 9 commands with !1, !2, !3, !4, !5, !6, !7, !8, !9. The "first word" is the first word typed in by the
   user that is NOT associated with file redirection. For example, "<input >output executable" will detect "executable"
   as the first word, thus order does not matter. Order DOES matter however when putting a process in the background,
   where '&' MUST be the last character to be put in the background.
   Also, multiple redirections will be seen as an error, such as typing "<input1 <input2 first".
   "first >output1 >>&output2" is also seen as an error since the output is ambiguious. Along with output, if a
   file already exists, it will ALWAYS refuse to be overwritten. If more than one '|' is found that are not "escaped",
   it will be detected as an error.
       If an argument is provided with p2, that argument will be redirected into stdin of the parent process like a
   typical shell would do; in other words, "p2 < myfile" will have the SAME execution as "p2 myfile", BUT "p2 myfile"
   will behave as a script and not print out any prompts or declare that "p2 terminated".
   
   
   
   [IMPORTANT VARIABLES]
   cur_wc: Represents the current word count and is incremented each time add_word() is called.
   
   prev_wc: Represents the previous word count from the last time parse was called and is used to help determine
            where newargv should start when passed into execvp().
			
   Redirect_IN_this and Redirect_OUT_this: Each signal when the next word is a file to read in or out.
   
   letter_count: Adds up all of the c values +1 returned from getword(). 1 is added to these c values to account
                 for the null terminator that gets added at the end of newly added words for placement purposes.
				 In that sense, "letter_count" is not a "true" letter count, since it counts these null terminators.
				 
   pipe_index: Used similarly to prev_wc where it functions as an offset from newargv, but is used to help determine
	            where newargv should start for command2 of the pipe process.
				
   history_index: Used to function as an offset for the history data structure for a given history read. For example,
	              if !4 was called, history_index[3] would be called and history_index[3][loop_count](loop_count is a simple
				  incrementing integer for parse loop number) would be added to the history data structure to find
				  lexcially analyzed tokens.
				  (ex: in this case, we'd call history[3]+history_index[3][loop_count] for !4)
   
   
   
   [DATA STRUCTURES]
   The main data structures used are the char big_one[STORAGE*MAXITEM] and the char *newargv[MAXITEM] variables.
   big_one holds every word and meta character that getword() recognizes, and newargv acts as an array of char
   pointers that only point to words(ie NOT metacharacters). This newargv is then passed into execvp() for the
   children produced to use as arguments. In this shell design, there's only ONE big_one and ONE newargv. In
   other words, the same newargv is passed into ALL children using the prev_wc integer variable.
   
   A history data structure was also implemented using using a 2D char array, where it is 9 by STORAGE*MAXHISTORY
   which I figured should be good enough for the first 9 commands. Each word read in and stored to s is then copied
   to the History 2D array for the first 9 commands.
   
   
   
   [INTERESTING DESIGN CHOICES]
   "echo hi> &" is treated as echo call for 'hi' and creates a file called '&' to put 'hi' into it. This occurs since
   the program prioritizes redirection over background checking, therefore '&' is treated as a redirect out and not
   flagged as a background process.
   
   When something like "echo hi > a >> b" is called, two error messages occur indicating "Ambiguous append. Please try again."
   and "Failed to create output.". These errors occur if an append(>>) and a redirect out(>) occur since this is designed
   to be ambiguious. Because of this, no output is created thus the second error message occurs.
   
   For backgrounded pipe processes, the name of the first command is displayed with the pid. This was chosen since every execvp
   call produces at least 1 command, and displaying the first command will be consistent with every backgrounded process.
   
   Pipes in history are all seen as "real" pipes, that is they're NOT read as "escaped" pipes. This was decided since most pipe
   characters are probably read in as "real" pipes; unless the user frequently types "escaped" pipes into file names this should
   be okay.
   
   When something like !99 is called(far out of our history's range), it's treated as a "word" rather than a history request. To clarify,
   only something with the structure of ![1,2,...,9] would be considered a history call, everything else similar in structure such as !0
   are seen as the first "word" as any other command.
   
   
   
   [CHANGES TO getword.c]
   Added a global enum boolean variable called "pipe_flag" used to signal when a pipe is a "real" pipe or an "escaped" pipe.
   # is NOT seen as a metacharacter to getword; the distinction of # is looked at in the parse function of p2.c.

   Useful manpages to consider for system calls:
       dup2, execpv, getenv, chdir, exit, fork, open, perror, wait, stat,
	   fflush, access, sigaction, signal, setpgid, getpgrp, killpg, pipe
	   
	   
	   
   [KNOWN ISSUES]
       >Backgrounding DOES NOT WORK WITH HISTORY.
	   
	   >When background processes are present, only 1 foreground process can
        be executed before waiting for the other background processes to finish.

       >Issues when redirecting '&' character since it's recognized as a redirect out,
	    as noted in "INTERESTING DESIGN CHOICES".
*/