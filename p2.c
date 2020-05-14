/******************************************************************************
Name: Jacob Romio
RedID: 822843795
Edoras Account: cssc0082
Course: CS570 Fall 2019
Professor: John Carroll
Project 4: The Shell with enhancements
Due: 11-29-2019(Early), 12-4-2019
File Name: p2.c
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

#include "p2.h"
#include "getword.h"

/** Global flags **/
enum boolean break_flag = false, Redirect_IN_flag = false, Redirect_OUT_flag = false, Redirect_APPEND_flag = false,
             error_IN_flag = false, error_OUT_flag = false, Background_flag = false, script_flag = false,
             Std_error_flag = false, pipe_error=false, first_real_pipe_flag=false, hist_error=false,
			 Redirect_IN_this = false, Redirect_OUT_this = false; //use error_OUT_flag and Redirect_OUT_this for append too
int cur_wc=0, prev_wc=0, Redirect_IN_index=-1, Redirect_OUT_index=-1, Redirect_APPEND_index=-1, letter_count=0,
    complete=0, dup_stat=0, pipe_index=-1, line=1, history_index[MAXHISTORY][MAXITEM];
char s[STORAGE], big_one[STORAGE*MAXITEM], *newargv[MAXITEM], Redirect_IN_File[STORAGE], Redirect_OUT_File[STORAGE],
     history[MAXHISTORY][STORAGE*MAXHISTORY];

extern enum boolean pipe_flag;

void myhandler(int signum)
{
    complete = 1; /* used to catch SIGTERM signal to not get Terminated when children do */
}

int main(int argc, char *argv[])
{	
	/* declaration of locals */
	int pipefd[2];
    int input_fd, status=0;
    pid_t child_id, wait_pid, parent_pid, grandchild_id;

	if(argc>1){/* Puts 1st given argument into stdin */
		int script_fd;
		script_flag = true;
		if((script_fd=open(argv[1], O_RDONLY, S_IRUSR | S_IWUSR))<0){
            fprintf(stderr, "Failed to redirect input: file not found\n");
		    exit(1);
	    }
	    dup_stat = dup2(script_fd, STDIN_FILENO);
		if (dup_stat==-1) {
            perror("Unable to duplicate file descriptor.");
            exit(1);
	    }
	}
	setpgid(0,0);
    (void) signal(SIGTERM, myhandler);
	parent_pid = getpid();
    for(;;){

        if(script_flag==false)
		    printf("%%%d%% ", line++); /* Displays prompt */ 
		else
			line++;/* Important for history when scripting. */

	    parse();/* Primary function for taking input. Read function header below for more details. */
		
        if (break_flag==true) break; /* End loop to end program if EOF or "done" is first word. */
		if( (pipe_flag==true && pipe_error==false) || (hist_error==true)){
			pipe_flag=false; /* Case occurs if pipe is the first character in stdin OR history request not valid. */
			hist_error=false;
			continue;
		}
		if(cur_wc == prev_wc){
			line--;
			continue; /* Allows prompt to repeat if only a newline occurs. */
		}
        if(strcmp(newargv[prev_wc], "cd")==0){
	        cd_call();/* Built-in cd functionality. See cd_call() header below for more details. */
			continue;
	    }
		if(pipe_error==true){ /* Pipe error occurs if 2 or more "|" are found without escape characters. */
			fprintf(stderr, "Too many pipe characters.\n");
			flags_to_false();
			continue;
		}
	    fflush(stdout);
	    fflush(stderr);
		
        if ((child_id=fork()) == 0) {/* child (at first fork) */
	        if(Background_flag == true){/* '&' */ 
		        if((input_fd=open("/dev/null", O_RDONLY, S_IRUSR | S_IWUSR))<0){
				    perror("Failed to /dev/null for background process"); /* Makes it so input doesn't get read in while in background*/
				    exit(1);
			    }
		        dup_stat = dup2(input_fd, STDIN_FILENO);
				if (dup_stat==-1) {
                    perror("Unable to duplicate file descriptor.");
	                exit(1);
	            }
		    }
	        if(Redirect_IN_flag == true)/* '<' */
		        io_handler(O_RDONLY, STDIN_FILENO); /* Sets up redirection for IO. Read header below for more. */
		    if( (Redirect_OUT_flag == true) && (first_real_pipe_flag==false) )/* '>' */
		        io_handler( (O_WRONLY | O_CREAT), STDOUT_FILENO);
			if( (Redirect_APPEND_flag == true) && (first_real_pipe_flag==false) )/* '>>' */
			    io_handler( (O_WRONLY | O_APPEND), STDOUT_FILENO);
				
			if(first_real_pipe_flag == true){ /** NEW p4 ADDITION FOR PIPING: fork occurs creating grandchild for vertical piping **/
				pipe(pipefd);/* The grandchild does command1 so the child can wait for it to finish.
                                This allows the parent to wait for the child to finish command2 properly. */
			    if((grandchild_id=fork()) == 0){/* grandchild */
				    dup2(pipefd[1], STDOUT_FILENO);
				    close(pipefd[0]);
				    close(pipefd[1]);
					exec_call(prev_wc);
			    }
			    else{/* child (at second fork)*/
                    dup2(pipefd[0], STDIN_FILENO);
                    close(pipefd[0]);
                    close(pipefd[1]);
                    if(Redirect_OUT_flag == true)/* '>' */
                        io_handler( (O_WRONLY | O_CREAT), STDOUT_FILENO);
                    if(Redirect_APPEND_flag == true)/* '>>' */
                        io_handler( (O_WRONLY | O_APPEND), STDOUT_FILENO);
					exec_call(pipe_index);
			    }
			}
			else/* Standard child for non-piping */
				exec_call(prev_wc);
	        exit(1);
        }
	    else{/* parent */
	        if(Background_flag == false)/* Waits for child if foreground, doesn't wait if background. */
	            while( (wait_pid = wait( &status )) > 0);
			else
				printf("%s [%d]\n", newargv[prev_wc], child_id); /* Prints process name and id as specified when backgrounded */
	    }
    }//end of main for loop
     killpg(getpgrp(), SIGTERM); //Terminate any children that are still running.
	 if(script_flag==false)
         printf("p2 terminated.\n"); //MAKE SURE this printf comes AFTER killpg
	 exit(0);
}//end of main

/*
 * FUNCTION: parse
 * NOTES: Syntactical analysis for "words" being read in. This deals with the !!
 *        and done built-ins, the redirection setup for IO, and setting up the
 *        background processes when a '&' occurs at the end of a line.
 */
void parse()
{
	int c = 0, init_letter_count = letter_count, done_wc=cur_wc, loop_count=0, history_letter_count=0, hist_requested=0;
	enum boolean first_loop = true, bang_flag=false;
	
    for(;;) {
	if(bang_flag==false)
	    c = getword(s); /* Calls getword() from p1 for lexical analysis of words. */
	//(void) printf("n=%d, s=[%s]\n", c, s);  // uncomment to see what getword() is reading in
	
	if(bang_flag==true && c<0)
		break; /* Used to break out of loop when history is used for command. */
		
	/** HISTORY WRITE(for first 9 lines entered) **/
	if(line-1<10){
		strcpy(history[line-2]+history_letter_count, s);
		history_index[line-2][loop_count] = history_letter_count;
		history_letter_count += c+1;
        history[line-2][history_letter_count] = '\0'; /* Used to separate words in history character array. */
	}
	
	/** HISTORY READ **/
	if( ( (s[0]=='!' && (s[1]=='1'||s[1]=='2'||s[1]=='3'||s[1]=='4'||s[1]=='5'||s[1]=='6'||s[1]=='7'||s[1]=='8'||s[1]=='9') && s[2]=='\0' )
		&& first_loop==true) || bang_flag==true ){
		if(bang_flag==false){
			hist_requested=s[1]-'0';
			while((c=getword(s))!=0);
		}
		bang_flag=true;
		if(hist_requested>=line-1){
			fprintf(stderr, "Shell has not reached this point in history.\n");
			hist_error=true;
			break;
		}
		strcpy(s, history[hist_requested-1]+history_index[hist_requested-1][loop_count]);
		if( ( strcmp("!!",s)==0 && first_loop==true)){ /* Allows !! in history to call the history before the requested history.
		                                                  ex: If !2 is called and !2's history is !!, !1 is technically called.*/
		    hist_requested-=1;
			continue;
		}
		if(strcmp(s, "|")==0)
			pipe_flag=true;//NOT GOING TO WORK WITH ESCAPE CHARACTER
		c = history_index[hist_requested-1][loop_count+1] - history_index[hist_requested-1][loop_count];
	}
	
	if( ( strcmp("!!",s)==0 && first_loop==true && bang_flag==false) || (strcmp("#",s)==0 && script_flag==true) ){/* '!!' and '#' */
	    while((c=getword(s))!=0);/* Skips leading words after !! until newline */
		break;
	}
	
	if(pipe_flag==true && first_loop==true){
		while((c=getword(s))!=0);
		fprintf(stderr, "Unexpected pipe in syntax.\n"); /* Has an error if first character is a "real" pipe. */
		break;
	}
	
	if(strcmp(s, "!!")!=0 && first_loop==true)
		flags_to_false(); /* Resets flags for next loop. */
	
	if(first_loop==true){ /* Statement serves to help !! and history work while in the for loop */
		prev_wc = cur_wc;
		first_loop=false;
	}

	if(pipe_flag==true && first_real_pipe_flag==false) /* Checks if piping should be done. */
		first_real_pipe_flag = true;
	else if(pipe_flag==true && first_real_pipe_flag==true)
		pipe_error=true;

    if(c>0)/* adds words into big_one character array. See header for more details. */
	    add_word(c);
	if( (strcmp("<",s)==0) && (error_IN_flag==false) )/* Next 5 if's deal with IO flag setting. Read header for more details. */ 
		io_flag_setter("Input");
	if( (strcmp(">",s)==0) && (error_OUT_flag==false) )
		io_flag_setter("Output");
	if( (strcmp(">&",s)==0) && (error_OUT_flag==false) ){
		Std_error_flag = true;
		io_flag_setter("Output");
	}
	if( (strcmp(">>",s)==0) && (error_OUT_flag==false) )
		io_flag_setter("Append");
	if( (strcmp(">>&",s)==0) && (error_OUT_flag==false) ){
		Std_error_flag = true;
		io_flag_setter("Append");
	}
	if( c == -1){
	    if( strcmp("done", s)==0 && (cur_wc!=done_wc) ){//Add if "done" is NOT first word
	        c=4;
            add_word(c);
	    }
		else if(cur_wc > prev_wc)//EOF but do final exec(ie: echo Null&void will print when EOF)
			break;
	    else{//End if EOF or first word is "done"
            break_flag = true;
		    break;
	    }
	}
	if(c==0) break;
	loop_count++;
    }/********end of for loop**********/
        if( (big_one[letter_count-2]=='&') && (big_one[letter_count-3]=='\0') && (big_one[letter_count-4]!='\0' && (letter_count-init_letter_count > 2)) ){
            newargv[--cur_wc] = NULL;
            Background_flag = true; /* Makes Backgrounds work by checking final character and characters around it */
        }
}

/*
 * FUNCTION: metacharacter
 * NOTES: Returns a boolean true if the current letter is a metacharacter, or a boolean false if non-metacharacter.
 * OUTPUT: Boolean value for the mc_flag.
 */
enum boolean metacharacter()
{
    if(strcmp("<",s)==0||strcmp(">",s)==0||strcmp(">&",s)==0||strcmp(">>",s)==0||strcmp(">>&",s)==0||strcmp("|", s)==0)
	    return true;
    return false;
}

/*
 * FUNCTION: add_word
 * NOTES: Adds word to current line of arguments by copying it to the big_one character array, then passing a char *
          that points to that location to the newargv array.
 * INPUT: Integer c that is returned from getword() function. This c value is the length of the word.
 */
void add_word(int c)
{
    enum boolean mc_flag = metacharacter();
	if(cur_wc == MAXITEM){
        perror("Word limit has been reached. Now exiting program.");
        exit(-1);
    }
	if((Redirect_IN_this == true) && (strlen(Redirect_IN_File)==0) ){
		if(strcmp(s,"!$")==0){
			if(prev_wc-1<0){/* This if statement prevents newargv from accessing data out of bounds. */
			    fprintf(stderr, "No earlier command for '!$'.\n");
			    return;
		    }
			strcpy(Redirect_IN_File, newargv[prev_wc-1]);
		}
		else
		    strcpy(Redirect_IN_File, s);
		Redirect_IN_this = false;
	}
	else if( (Redirect_OUT_this == true) && (strlen(Redirect_OUT_File)==0) ){
		if(strcmp(s,"!$")==0){
			if(prev_wc-1<0){/* This if statement prevents newargv from accessing data out of bounds. */
			    fprintf(stderr, "No earlier command for '!$'.\n");
			    return;
		    }
			strcpy(Redirect_OUT_File, newargv[prev_wc-1]);
		}
		else
            strcpy(Redirect_OUT_File, s);
	    Redirect_OUT_this = false;
	}
	else if(strcmp(s,"!$")==0){ /* If !$ is found, replace with the last command from the last line. */
		if(prev_wc-1<0){/* This if statement prevents newargv from accessing data out of bounds. */
			fprintf(stderr, "No earlier command for '!$'.\n");
			return;
		}
		c = strlen(newargv[prev_wc-1]); /* Note: The if statment above prevents any negative newargv calls. */
        strcpy(big_one+letter_count, newargv[prev_wc-1]);
		newargv[cur_wc++] = newargv[prev_wc-1];
        letter_count += c+1;
        big_one[letter_count] = '\0'; /* Used to separate words in big_one character array. */
	}
	else{
        strcpy(big_one+letter_count, s);
        if((mc_flag==false) || (strcmp(s,"|")==0 && pipe_flag==false) )
            newargv[cur_wc++] = big_one+letter_count;
		if(pipe_flag==true && first_real_pipe_flag==true && pipe_error==false){
			newargv[cur_wc++] = NULL;
			pipe_index = cur_wc;
			pipe_flag = false;
		}
        letter_count += c+1;
        big_one[letter_count] = '\0'; /* Used to separate words in big_one character array. */
    }
}

/*
 * FUNCTION: io_handler
 * NOTES: Deals with redirects of input & output by opening the files and putting them in stdin
 *        or stdout as specified.
 * INPUT: Redirection index of input or output for newargv location, flags used for open function,
 *        and STD_FILENO that's either STDIN_FILENO or STDOUT_FILENO to signal Input or Output.
 */
void io_handler(int flags, int STD_FILENO)
{
    int fd;
	if( (flags == (O_WRONLY | O_CREAT)) && (access(Redirect_OUT_File, F_OK)!=-1) ){
		fprintf(stderr, "Output file name already exists. Failed to overwrite.\n");
		close(fd);
		exit(1);
	}
	if( (flags == (O_WRONLY | O_APPEND)) && (access(Redirect_OUT_File, F_OK)==-1) ){
		fprintf(stderr, "Output file name doesn't exist. Failed to append.\n");
		close(fd);
		exit(1);
	}
	if(error_OUT_flag == true){ /* Needed for a output AND append case to prevent creating an empty file. */
		fprintf(stderr, "Failed to create output.\n");
		close(fd);
		exit(1);
	}
	if(STD_FILENO == STDIN_FILENO){
		if((fd=open(Redirect_IN_File, flags, S_IRUSR | S_IWUSR))<0){
            fprintf(stderr, "Failed to redirect input: file not found\n");
		    close(fd);
		    exit(1);
		}
	}
	else if(STD_FILENO == STDOUT_FILENO){
		if((fd=open(Redirect_OUT_File, flags, S_IRUSR | S_IWUSR))<0){
            fprintf(stderr, "Failed to redirect output\n");
		    close(fd);
		    exit(1);
		}
	}
	dup_stat = dup2(fd, STD_FILENO);
	if( (Std_error_flag==true) && (STD_FILENO==STDOUT_FILENO) )
        dup_stat = dup2(fd, STDERR_FILENO);
	if (dup_stat==-1) {
        perror("Unable to duplicate file descriptor.");
        exit(1);
    }
	close(fd);
}

/*
 * FUNCTION: cd_call
 * NOTES: Performs built-in change directory functionality. Only accepts 1 argument
 *        or no arguments, where 1 arg goes to that directory and 0 args goes to the
 *        home directory. More than 1 argument prints an error and does nothing.
 */
void cd_call()
{
    if(newargv[prev_wc+2]!=NULL)
		fprintf(stderr, "Too many arguments for cd. Please try again.\n");
	else if(newargv[prev_wc+1]!=NULL){
		if(access(newargv[prev_wc+1], F_OK)!=-1)
            chdir(newargv[prev_wc+1]);
		else
			perror("");
	}
	else
	    chdir( getenv("HOME") );
}

/*
 * FUNCTION: io_flag_setter
 * NOTES: Sets io flags accordingly, as well as index locations in newargv, for child to
 *        handle when io_handler is called for stdin, stdout, and stderr setup. If multiple
 *        redirects are in a line, that is more than 1 input redirect or more than 1 output
 *        redirect, errors will be printed accordingly.
 * INPUT: char * to signal whether we're working with Input or Output.
 */
void io_flag_setter(char *io_stat)
{
	if(strcmp(io_stat, "Input")==0){/* Input */
		if(Redirect_IN_flag == false){
            Redirect_IN_flag = true;
			Redirect_IN_this = true;
        }
        else{
            fprintf(stderr, "Ambiguous Input. Please try again.\n");
            error_IN_flag = true;
            Redirect_IN_flag = false;
        }
	}
	else if(strcmp(io_stat, "Output")==0){/* Output */
		if((Redirect_OUT_flag == false) && (Redirect_APPEND_flag==false)){
            Redirect_OUT_flag = true;
			Redirect_OUT_this = true;
        }
        else{
            fprintf(stderr, "Ambiguous Output. Please try again.\n");
            error_OUT_flag = true;
            Redirect_OUT_flag = false;
        }
	}
	else{/* Append */
		if((Redirect_APPEND_flag == false) && (Redirect_OUT_flag==false)){
            Redirect_APPEND_flag = true;
			Redirect_OUT_this = true;
        }
        else{
            fprintf(stderr, "Ambiguous Append. Please try again.\n");
            error_OUT_flag = true;
            Redirect_APPEND_flag = false;
        }
	}
}

/*
 * FUNCTION: flags_to_false
 * NOTES: Sets flags used in each main loop back to false 
 *        and clears Redirect_IN_File and Redirect_OUT_File.
 */
void flags_to_false()
{
    Background_flag = false;
    error_IN_flag = false;
    error_OUT_flag = false;
	Std_error_flag = false;
	pipe_flag = false;
	pipe_error = false;
	first_real_pipe_flag=false;
    strcpy(Redirect_IN_File, "");
	strcpy(Redirect_OUT_File, "");
    Redirect_IN_flag = false;
    Redirect_OUT_flag = false;
	Redirect_APPEND_flag = false;
	Redirect_IN_this = false;
	Redirect_OUT_this = false;
}

/*
 * FUNCTION: exec_call
 * NOTES: Calls the execvp to occur. If an error flag is true, the exec call
 *        will NOT occur, and an error is sent to stderr.
 * INPUT: The offset from newargv. Helps distinguish child vs grandchild.
 */
void exec_call(int offset)
{
    int execvp_val=0;
	
	if( (error_IN_flag == false) && (error_OUT_flag == false) ) /* Do execvp if no error flag is true*/
        execvp_val = execvp(newargv[offset], newargv+offset);
        /* newargv is passed in from where offset indicates.
           With this design, only ONE newargv is needed for the whole program.   */
	if (execvp_val==-1) {
        perror("");
        exit(9);
    }
}