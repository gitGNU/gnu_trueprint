/*
 * Source file:
 *	diffs.c
 *
 * Implements the highlighting of differences between old and new versions
 * of files.
 */

#include "config.h"

#include <libintl.h>

#ifndef DIFF_CMD

#include "trueprint.h"
#include "diffs.h"

/* If there is no diff command just provide stubs */

void setup_diffs(void) { return; }
void init_diffs(char newfile[]) { return; }
void end_diffs(void) { return; }
boolean	getdelline(long current_line, char *input_line, char_status input_status[]) { return 0; }
boolean	line_inserted(long current_line) { return 0; }

#else

#include <ctype.h>
#include <errno.h>
/* Ultrix needs these sys header files this way around */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trueprint.h"
#include "utils.h"
#include "main.h"
#include "output.h"
#include "openpipe.h"
#include "options.h"
#include "debug.h"

#include "diffs.h"

static char	*diffs_string;

/*
 * Private part
 */
static unsigned	short read_values(FILE *,long *value1_ptr, long *value2_ptr);
static boolean read_long(FILE *, long *);
static void		parse_line(void);
static stream_status get_diff_char(char *input_char, char_status *status);

static FILE		*diffs_stream;
static boolean		diffs_on;
static long	diff_start_line;
static long	diff_end_line;
static long	lines_deleted;
static long	lines_added;

/*
 * Function:
 *	setup_diffs
 */

void
setup_diffs(void)
{
  diffs_string = NULL;
  diffs_stream = NULL;

  string_option("O", "diff", NULL, &diffs_string, NULL, NULL,
		OPT_MISC,
		"if <string> is a file then print diffs between <string> and input file\n"
		"    otherwise use as a prefix and print diffs");
}

/*
 * Function:
 *	init_diffs
 *
 * Initialises the diffs system:
 * checks to see if any old version was provided and returns if not;
 * calls end_diffs() if necessary;
 * starts up the diff process; 
 */
void
init_diffs(char newfile[])

{
  char	command[COMMAND_LEN];
  char	*diff_cmd;
  struct stat dummy;
  /* Check to see if any old versions given */

  if (diffs_string == NULL)
    {
      /* turn off diffs and return */
      diffs_on = FALSE;
      return;
    }

  diffs_on = TRUE;

  if ((diff_cmd = getenv("TP_DIFF_CMD")) == NULL)
    {
      diff_cmd = DIFF_CMD;
    }

  /*
   * diffs_stream is only set if still reading from
   * diff process, so need to finish the old diffs.
   */
  if (diffs_stream != NULL) end_diffs();

  /*
   * Check to see if diffs_string is a valid prefix or
   * a filename...
   */
  sprintf(command,"%s%s",diffs_string,newfile);

  if (stat(command,&dummy) == 0)
    {
      sprintf(command,"%s %s%s %s", diff_cmd,diffs_string,newfile,newfile);

    }
  else if (stat(diffs_string,&dummy) == 0)
    {
      sprintf(command, "%s %s %s", diff_cmd, diffs_string, newfile);
    }

  dm('d',3,"diffs command: %s\n",command);

  if ((diffs_stream = fopenpipe(command, "r")) == NULL)
    abort;

  parse_line();
}

/*
 * Function:
 *	read_values
 *
 * scan a line reading one integer, and reading a second if the next
 * character is a ","
 */
unsigned short
read_values(FILE *stream, long *value1_ptr, long *value2_ptr)

{
  int	next_char;
  unsigned short	retval;

  *value1_ptr = 0;
  *value2_ptr = 0;

  /* read first value - if none then return*/
  if (!read_long(stream, value1_ptr)) return 0;

  /* skip spaces and check for a comma */
  while ((next_char = getc(stream)) == ' ')
    ;
  if (next_char == ',')
    {
      /* if next char was a comma read second value */
      if (!read_long(stream, value2_ptr))
	{
	  fprintf(stderr, gettext(CMD_NAME ": internal error reading diffs\n"));
	  exit(2);
	}
      /* and skip spaces again */
      while ((next_char = getc(stream)) == ' ')
	;
      retval = 2;
    }
  else
    {
      retval = 1;
      *value2_ptr = *value1_ptr;
    }

  if (next_char != EOF)
    {
      /* replace last char in buffer for calling function */
      ungetc((int)next_char,stream);
    }

  return(retval);
}

/*
 * Function:
 *	read_long
 *
 * Reads a long from a stream
 */
boolean
read_long(FILE *stream, long *value)

{
  boolean retval = FALSE;
  int next_char;

  *value = 0;

  while (TRUE)
    {
      if ((next_char = getc(stream)) == EOF) break;
      if (!isdigit(next_char)) break;

      retval = TRUE;
      *value = (*value * 10) + (next_char - '0');
    }

  if (next_char != EOF)
    {
      ungetc(next_char, stream);
    }

  return retval;
}

/*
 * Function:
 *	parse_line
 *
 * Reads a line from the diff stream, reading start and end line numbers
 * for both the old and new files and the change type (d, a or c).
 */
void
parse_line(void)

{
  long delete_start;
  long delete_end;
  int	diff_type;

  /*
   * Read the first two values - if there aren't any then we've
   * reached the end of the file.  These values refer to the
   * first file so we're only interested in them for deleting lines.
   */
  if (read_values(diffs_stream, &delete_start, &delete_end) == 0)
    {
      end_diffs();
      diffs_on = FALSE;
      return;
    }

  /*
   * Calculate the number of lines deleted.
   * If delete start and delete end are the same then 1 line has been
   * deleted... i.e. one more than the difference.
   */
  lines_deleted = delete_end - delete_start + 1;

  diff_type = getc(diffs_stream);	/* a=add, d=del, c=change */

  switch (diff_type)
    {
    case 'a':
    case 'd':
    case 'c':
      break;
    default:
      fprintf(stderr, gettext(CMD_NAME ": warning, bad diffs stream format char %c!\n"), diff_type);
      end_diffs();
      diffs_on = FALSE;
      return;
    }
	  
  /* Same as before, only for new version of file */
  if (read_values(diffs_stream, &diff_start_line, &diff_end_line) == 0)
    {
      fprintf(stderr, gettext(CMD_NAME ": warning, bad diffs stream format!\n"));
      end_diffs();
      diffs_on = FALSE;
      return;
    }
  lines_added = diff_end_line - diff_start_line + 1;

  if (diff_type == 'a') lines_deleted = 0;
  if (diff_type == 'd')
    {
      lines_added = 0;
      /*
       * deleted lines are actually *after* the specified line,
       * so we need to pretend they're one line later...
       */
      diff_start_line += 1;
      diff_end_line += 1;
    }
  /* Skip to the end of the line */
  while (getc(diffs_stream) != '\n')
    ;
}

/*
 * Function:
 *	end_diffs
 *
 * Finish off the diffs system:
 * - close the diffs stream (i.e. kill off the diff process);
 */

void
end_diffs(void)

{
  if (!diffs_on) return;

  dm('d',3,"diffs: closing diffs\n");

  fclosepipe(diffs_stream);
  diffs_stream = NULL;
}

/*
 * Function:
 *	get_diff_char
 *
 * Simple function to be passed to getnextline() (which expands tabs, etc)
 * in getdelline(), so we don't need to duplicate the functionality of
 * getnextline().
 */
stream_status
get_diff_char(char *input_char, char_status *status)

{
  *status = CHAR_NORMAL;
  *input_char = (char)getc(diffs_stream);
  return(STREAM_OK);
}

/*
 * Function:
 *	getdelline
 *
 * Returns:
 *	TRUE if a(nother) deleted line occurs after current line, returns
 *		line in parameters.
 *	FALSE if no deleted line between current line and next line.
 */

boolean
getdelline(long current_line, char *input_line, char_status input_status[])

{
  boolean blank_line;

  /*
   * Check whether there is a deleted line waiting...
   */
  if ((current_line != diff_start_line)
      || !diffs_on
      || (lines_deleted == 0))
    return(FALSE);

  /*
   * First skip the first two characters of the line -
   * they should be "< "
   */
  if ((getc(diffs_stream)!='<') || (getc(diffs_stream)!=' '))
    {
      fprintf(stderr, gettext(CMD_NAME ": diffs stream in unexpected format\n"));
      exit(2);
    }
  /* Read in the next line from diff stream */
  getnextline(get_diff_char,&blank_line,input_line,input_status);

  lines_deleted -= 1;
  if (lines_deleted == 0)
    {
      /*
       * If there are lines added then this is a "change" and
       * the added lines will be dealt with by line_inserted().
       * Otherwise we need to find the next difference with
       * parse_line().
       */
      if (lines_added == 0)
	parse_line();
      else
	/* else skip over line of ---- */
	while (getc(diffs_stream) != '\n')
	  ;
    }
  return(TRUE);
}

/*
 * Function:
 *	line_inserted
 *
 * Returns:
 *	INSERT if line_number has been inserted
 *	NORMAL otherwise
 */

boolean
line_inserted(long current_line)

{
  boolean	retval = FALSE;
  if (!diffs_on) return(FALSE);
  if ((current_line >= diff_start_line)
      && (current_line <= diff_end_line))
    {
      /* skip over line */
      while (getc(diffs_stream) != '\n')
	;
      retval = TRUE;
    }
  /* If reached the end of the insertion... */
  if (current_line >= diff_end_line) parse_line();
  return(retval);
}

#endif
