/*
 * Source file:
 *	input.c
 *
 * Contains the functions that look after getting input from the source files.
 * These are the functions that the functions in lang_*.c use.
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#else
int read(int fildes, void *buf, unsigned int nbyte);
#endif

#include "trueprint.h"
#include "utils.h"
#include "main.h"
#include "debug.h"

#include "input.h"

/*
 * Public part
 */
long	char_number;
int	got_some_input;

/*
 * Private part
 */

	/* Arbitrary size - should never need more than 2 */
#define UNGET_STACKSIZE 8

static char *unget_charstackptr;
static char *unget_statstackptr;
static char *unget_charstackbase;
static char *unget_statstackbase;
static unsigned short unget_stackdepth;
static int	input_stream;

/* My private version of getchar */
static stream_status buffered_read(char *input_char);

/*
 * Function:
 *	set_input_stream
 * Very C++.... returns FALSE if the file is empty.
 * Assumes that if it returns false then stream will not be used.
 */
boolean
set_input_stream(int stream)
{
  char test_char;
  stream_status test_status;

  /*
   * Reset the unget stack
   */
  unget_charstackptr = unget_charstackbase;
  unget_statstackptr = unget_statstackbase;
  unget_stackdepth = 0;

  input_stream = stream;
  if ((test_status = getnextchar(&test_char)) == STREAM_EMPTY_FILE)
    {
      return FALSE;
    }
  else
    {
      ungetnextchar(test_char, test_status);
      return TRUE;
    }
}

/*
 * Function:
 *	setup_input
 */
void
setup_input(void)

{
  unget_stackdepth = 0;
  got_some_input = 0;
  unget_charstackbase = xmalloc(UNGET_STACKSIZE);
  unget_charstackptr = unget_charstackbase;
  unget_statstackbase = xmalloc(UNGET_STACKSIZE);
  unget_statstackptr = unget_statstackbase;
}

/*
 * Function:
 *	init_input
 */
void
init_input(void)

{
  char_number = -1;
}

/*
 * Function:
 *	buffered_read
 *
 * Reads buffered input from standard input, returns next character.
 * Collapses cr/lf to cr and cr/ff to ff.
 * Can return any value from buffered_read_status.
 */
stream_status
buffered_read(char *input_char)

{
  static char	input_buffer[BUFFER_SIZE];
  static short	buffer_pointer = 0;
  static int	buffer_size=0;

  /*
   * If there's nothing in the buffer, read it in - this will
   * happen the first time this function is called and at the
   * beginning of each file.
   */
  if (buffer_size == 0)
    {
      if ((buffer_size = read(input_stream,input_buffer,BUFFER_SIZE)) < 0)
	{
	  fprintf(stderr, gettext(CMD_NAME ": cannot read file %s, %s\n"),
			current_filename, strerror(errno));
	  exit(2);
	}
      if (buffer_size == 0) return(STREAM_EMPTY_FILE);

      /* Set the global flag to indicate that there is at least some input */
      got_some_input = 1;
      buffer_pointer = 0;
    }

  *input_char = input_buffer[buffer_pointer++];

  /*
   * If buffer has been totally read then read in the next buffer-full.
   */
  if (buffer_pointer == buffer_size)
    {
      if ((buffer_size = read(input_stream,input_buffer,BUFFER_SIZE)) < 0)
	{
	  fprintf(stderr, gettext(CMD_NAME ": cannot read file %s, %s\n"),
			current_filename, strerror(errno));
	  exit(2);
	}
      if (buffer_size == 0) return(STREAM_FILE_END);

      /*
       * Reset the buffer
       */
      buffer_pointer = 0;
    }

  /*
   * At this point we have a character to be returned, and we know
   * that input_buffer[buffer_pointer] contains the next character.
   * This gives us the opportunity to collapse lf/cr to cr and cr/ff
   * to ff.
   */
  if (((*input_char == '\r') && (input_buffer[buffer_pointer] == '\n'))
      || ((*input_char == '\n') && (input_buffer[buffer_pointer] == '\f')))
    return buffered_read(input_char);
  else
    return(STREAM_OK);

}

/*
 * Function:
 *	ungetnextchar
 *
 * Conceptually puts the char back into the input stream, just like
 * ungetc().
 */

void
ungetnextchar(char input_char, stream_status status)

{
  dm('I',7,"input.c:ungetnextchar(%c,%d)\n",input_char,status);
  dm('I',8,"input.c:ungetnextchar char stack = %x, stat stack = %x, size = %d\n",
     unget_charstackptr, unget_statstackptr, unget_stackdepth);
  if (unget_stackdepth++ == UNGET_STACKSIZE)
    abort();

  *(unget_charstackptr++) = input_char;
  *(unget_statstackptr++) = (char)status;
  char_number --;
}

/*
 * Function:
 *	getnextchar
 *
 * Returns the next character from the input stream.  Detects end of file
 * and keeps char_number up-to-date.
 */
stream_status
getnextchar(char *input_char)

{
  char_number += 1;

  /*
   * Check unget stack
   */
  if (unget_stackdepth > 0)
    {
      dm('I',8,"input.c:getnextchar char stack = %x, stat stack = %x, size = %d\n",
	 unget_charstackptr, unget_statstackptr, unget_stackdepth);
      unget_stackdepth--;
      *input_char = *(--unget_charstackptr);
      dm('I',7,"input.c:getnextchar returning %c, %d from stack\n",
	 *input_char, *(unget_statstackptr-1));
      return *(--unget_statstackptr);
    }

  return buffered_read(input_char);
}

