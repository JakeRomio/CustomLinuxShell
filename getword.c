/******************************************************************************
Name: Jacob Romio
RedID: 822843795
Edoras Account: cssc0082
Course: CS570 Fall 2019
Professor: John Carroll
Project 1: Lexical Analyzer
Due: 9-16-2019
File Name: getword.c
Source: I wrote this code myself.
******************************************************************************/

#include "p2.h"
#include "getword.h"

int mult_meta(int num_of_letters, char *w);

enum boolean pipe_flag = false; /* Used to distinguish "|" vs "\|" */

/*
 * FUNCTION: getword
 * NOTES: Accepts stdin characters and stores them into a character array
 *        buffer until a space, newline, or EOF occurs and returns either
 *        a -1, 0, or integer of word size.
 *        If escape_flag is TRUE we ignore the properties if
 *        whether or not the current letter is a metacharacter, ie it
 *        behaves like a non-metacharacter.
 *        
 *        P4 UPDATE: Don't count # as a metacharacter.
 * INPUT: Pointer to the beginning of the character array buffer.
 * OUTPUT: Integer that is either a -1, 0, or number of letters in a word.
 */
int getword(char *w)
{
    int num_of_letters = 0;
    int cur_letter;
    enum boolean escape_flag = false; //flag to determine if '\' was called
    while((cur_letter = getchar()) == ' '); //Loop skips spaces.
    while( (cur_letter != EOF) && (cur_letter != '\n') &&    // Main loop of the function:
	   (cur_letter != ' ' || escape_flag==true ) &&      // Continues to pull characters from
	   ((STORAGE-1)!=num_of_letters))                    // stdin until EOF, newline, space,
    {                                                        // or STORAGE limit.
        if(cur_letter != '\\')
	{
	    w[num_of_letters++] = cur_letter;
	    if(cur_letter == '>' && escape_flag == false)// If metacharacter is '>', see if there's more to append
		num_of_letters = mult_meta(num_of_letters, w);
		if( (cur_letter=='|') && (escape_flag==false) ) /** NEW p4 ADDITION FOR PIPING **/
			pipe_flag = true;
	    if((cur_letter=='<'||cur_letter=='>'||cur_letter=='|'||cur_letter=='&') && escape_flag == false)//EDITED TO NOT INCLUDE '#' in p4
	        break;// break out of the loop if a metacharacter is found
	    cur_letter = getchar();
	    escape_flag = false;
        }
	else if((cur_letter == '\\') && (escape_flag==true))//Lets you have "\\" to output n=1 s=[\]
	{
            w[num_of_letters++] = cur_letter;
	    cur_letter = getchar();
	    escape_flag = false;
	}
	else//reached if a '\' character is entered as an escape character
	{
	    cur_letter = getchar();
	    escape_flag = true;
	}
	if((cur_letter=='<'||cur_letter=='>'||cur_letter=='|'||cur_letter=='#'||cur_letter=='&') && escape_flag == false)
	{   
	    ungetc(cur_letter, stdin);// Used for cases such as "a>", where the '>' would disappear without ungetc.
	    break;// break out of the loop if a metacharacter is found
	}
    }
    w[num_of_letters] = '\0'; //Adds null terminater to finish word.

    if( (num_of_letters != 0 && (cur_letter == '\n')) || (STORAGE-1==num_of_letters) )
        ungetc(cur_letter, stdin);/*Case occurs when a newline appears
                                    immediately after a word finishes.
				    This is so newline is still recognized
				    in the input stream to produce the
				    correct output when a newline is found.*/
 
    if((cur_letter == EOF && num_of_letters==0) || strcmp(w, "done")==0)
        return -1;//Case occurs if EOF or "done" is found to end program.

    return num_of_letters;
}

/*
 * FUNCTION: mult_meta
 * NOTES: Checks if metacharacter has additional letters
 *        and appends them.
 * INPUT: Number of current letters and char pointer where
 *        character buffer is.
 * OUTPUT: New number of letters after appending.
 */
int mult_meta(int num_of_letters, char *w)
{
    int cur_letter = getchar();
    if(cur_letter == '>')
    {
        w[num_of_letters++] = cur_letter;
        cur_letter = getchar();
        if(cur_letter == '&')
            w[num_of_letters++] = cur_letter;
        else
            ungetc(cur_letter, stdin);// ungetc replaces letter in the case it's not what we're looking for.
    }                                 // In other words, it functions as a "look ahead".
    else if(cur_letter == '&')
        w[num_of_letters++] = cur_letter;
    else
        ungetc(cur_letter, stdin);
    return num_of_letters;
}
