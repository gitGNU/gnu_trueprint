/*
 * Source file:
 *	output.c
 *
 * Contains the formatting and output functions.
 */

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trueprint.h"
#include "main.h"
#include "diffs.h"
#include "headers.h"
#include "index.h"
#include "postscript.h"
#include "language.h"
#include "print_prompt.h"
#include "debug.h"
#include "options.h"
#include "utils.h"

#include "output.h"

/******************************************************************************
 * Public part
 */
long	file_page_number;
long	page_number;

/******************************************************************************
 * Private part
 */

typedef enum {
  INSERT,
  DELETE,
  NORMAL
} diff_states;

static void	add_char(short position,char character,char_status status,char *line,char_status line_status[]);
static int	line_end(char *input_line, int last_char_printed);
static stream_status printnextline(void);
static boolean	blank_page(boolean print_page);

/*
 * Local variables
 */
static boolean	no_clever_wrap;
static short	min_line_length;
static short	tabsize;
static boolean	no_function_page_breaks;
static boolean	new_sheet_after_file;
static boolean	no_expand_page_break;
static long	line_number;
static boolean	reached_end_of_sheet;

static char *segment_ends[4][3] = {
  /*     INSERT               DELETE             NORMAL       */
  /* NORMAL */    { ") BF setfont show ",   ") CF setfont So show ",   ") CF setfont show " },
		  /* ITALIC */    { ") IF setfont Bs ",     ") IF setfont So show ",   ") IF setfont show " },
		  /* BOLD */      { ") BF setfont show ",   ") BF setfont So show ",   ") BF setfont show " },
		  /* UNDERLINE */ { ") BF setfont Ul show", ") CF setfont So Ul show ",") CF setfont Ul show " },
};

/******************************************************************************
 * Function:
 *	init_output
 */
void
init_output(void)
{
  line_number = 0;
  page_number = 0;
}

/******************************************************************************
 * Function:
 *	setup_output
 */
void
setup_output(void)
{
  boolean_option("b", "no-page-break-after-function", "page-break-after-function", TRUE, &no_function_page_breaks, NULL, NULL,
		 OPT_TEXT_FORMAT,
		 "don't print page breaks at the end of functions",
		 "print page breaks at the end of functions");

  boolean_option(NULL, "new-sheet-after-file", "no-new-sheet-after-file", TRUE, &new_sheet_after_file, NULL, NULL, OPT_TEXT_FORMAT,
		 "Print each file on a new sheet of paper",
		 "Don't print each file on a new sheet of paper");

  boolean_option("W", "no-intelligent-line-wrap", "intelligent-line-wrap", FALSE, &no_clever_wrap, NULL, NULL,
		 OPT_TEXT_FORMAT,
		 "Wrap lines at exactly the line-wrap column",
		 "Wrap lines intelligently at significant characters, such\n"
		 "    as a space");

  short_option("L", "minimum-line-length", 10, NULL, 0, 5, 4096, &min_line_length, NULL, NULL,
	       OPT_TEXT_FORMAT, 
	       "minimum line length permitted by intelligent line wrap (default 10)",
	       NULL);
  short_option("T", "tabsize", 8, NULL, 0, 1, 20, &tabsize, NULL, NULL,
	       OPT_TEXT_FORMAT,
	       "set tabsize (default 8)", NULL);

  boolean_option("E", "ignore-form-feeds", "form-feeds", FALSE, &no_expand_page_break, NULL, NULL,
		 OPT_TEXT_FORMAT,
		 "don't expand form feed characters to new page",
		 "expand form feed characters to new page");
}

/******************************************************************************
 * Function:
 *	add_char
 *
 * Simply adds a character plus status to a couple of arrays
 */
void
add_char(short position,char character,char_status status,char *line,char_status line_status[])

{
  if (position >= MAXLINELENGTH)
    fprintf(stderr, gettext(CMD_NAME ": line too long!  Are you sure this is a program listing?\n"));
  line[position] = character;
  line_status[position] = status;
}

/******************************************************************************
 * Function:
 *	getnextline
 * Gets the next full line of input and expands tabs.
 * Always returns data, even if it is a blank line.
 */
stream_status
getnextline(stream_status (*get_input_char)(char *,char_status *), boolean *blank_line, char input_line[], char_status input_status[])

{
  char	input_char;
  short	line_position=0;
  stream_status	retval = STREAM_OK;
  char_status status;

  *blank_line = TRUE;

  do
    {
      retval |= get_input_char(&input_char,&status);

      if (!isspace(input_char)) *blank_line = FALSE;

      if (input_char == '\t')
	{
	  /*
	   * if we read a tab then expand it out.
	   */
	  short tab_stop = tabsize + line_position - line_position%tabsize;
	  short cursor;

	  for (cursor=line_position;cursor < tab_stop;cursor++)
	    add_char(cursor,' ',status,input_line,input_status);
	  line_position = tab_stop;

	}
      else if (input_char == '\n')
	{
	  break;

	}
      else if (input_char == '\014')
	{
	  /*
	   * if a newpage character then convert to space
	   */
	  if (no_expand_page_break == FALSE)
	    {
	      retval |= STREAM_PAGE_END;
	      add_char(line_position++,' ',status,input_line,input_status);
	    }
	  else
	    {
	      add_char(line_position++,'_',status,input_line,input_status);
	    }

	}
      else if (iscntrl(input_char))
	{

	  /*
	   * if a control character then convert to underscore
	   */
	  add_char(line_position++,'_',status,input_line,input_status);

	}
      else
	{
	  add_char(line_position++,input_char,status,input_line,input_status);
	}

    } while (!(retval & (STREAM_FILE_END)));

  /*
   * put a null at the end of the line
   */
  input_line[line_position] = 0;

  return(retval);
}

/*
 * function:
 *	line_end
 *
 * we have a line that may need to be broken.
 * search from the end of the line backwards, looking for a suitable
 * place to break the line.
 */
int
line_end(char *input_line, int last_char_printed)

{
  int	input_line_length;
  boolean	got_end=FALSE;
  short	break_index;
  int	output_line_end;

  input_line_length = (int)strlen(input_line);
  dm('O',4,"output.c:line_end() line length is %d\n",input_line_length);

  if (page_width == 0)
    {
      /*
       * The line doesn't need to be broken
       */
      output_line_end = input_line_length - 1;
      return(output_line_end);
    }

  if ((input_line_length - last_char_printed) > page_width)
    {
      if (no_clever_wrap == TRUE) 
	{
	  /*
	   * Don't need to do anything clever - just return the
	   * point in the string which is "page_width" plus
	   * the last point printed.
	   */
	  return (last_char_printed + page_width);
	}
		 
      /*
       * the line needs to be broken.  there is an array, breaks,
       * containing valid symbols that a line can be broken on.
       * what now needs to be done is to work through the array,
       * starting with the most desirable break character, until
       * one of the break characters is found in the line and then
       * the line can be broken there.
       */
    
      for (break_index=0; break_index < BREAKSLENGTH; break_index++)
	{
	  for (output_line_end = last_char_printed + page_width;
	       output_line_end > last_char_printed + min_line_length;
	       output_line_end--)
	    if (input_line[output_line_end] == BREAKS[break_index])
	      {
		got_end=TRUE;
		break;
	      }
	  if (got_end == TRUE) break;
	}
      if (got_end == FALSE)
	output_line_end = last_char_printed+page_width;

      if (output_line_end >= input_line_length)
	output_line_end = input_line_length;
    }
  else
    /* the line doesn't need to be broken at all. */
    output_line_end = input_line_length - 1;

  return(output_line_end);
}

/*
 * function:
 *	printnextline
 *
 * prints a line of code on stdout, splitting at appropriate locations.
 * behaves very similarly for pass 0 and for pass 1, except it doesn't
 * print anything out during pass 0.
 */
stream_status
printnextline()

{
  static char	input_line[MAXLINELENGTH];
  static char_status	input_status[MAXLINELENGTH];
  static int	last_char_printed;
  static int	input_line_length=0;
  int		output_line_end;
  static stream_status	retval=STREAM_OK;
  int		output_char_idx;
  boolean		first_line_segment=FALSE;
  short		count;
  static diff_states	diff_state=NORMAL;
  boolean blank_line;
  char_status	last_char_status;

  /*
   * See if we need to read in a new line
   */
  if (input_line_length == 0)
    {

      /*
       * we do - check to see if there might be a deleted
       * line that needs to be printed before the current
       * line.  if diff_state is delete then there is already
       * a line waiting in input_line...
       *
       * diff_state:
       * NORMAL	need another line, retval set to return of previous
       *		getnextline() or LINE
       * DELETE	line waiting in input_line, retval set to return of
       *		previous getnextline() or LINE
       * INSERT	should never happen
       */

      dm('O',4,"output.c:printnextline() Need new line\n");

      switch (diff_state)
	{
	case NORMAL:
	  dm('O',4,"output.c:printnextline() diff_state is NORMAL\n");
	  if (getdelline(line_number+1,input_line,input_status)){
	    dm('O',4,"output.c:printnextline() Found deleted line - diff_state is now DELETE\n");
	    diff_state = DELETE;
	  }
	  else
	    {
	      /*
	       * there isn't a deleted line - get
	       * the next line and check if this is
	       * an inserted line.
	       */
	      line_number += 1;
	      retval = getnextline(get_char,&blank_line,input_line,input_status);
	      if (line_inserted(line_number))
		{
		  dm('O',4,"output.c:printnextline() This line is inserted - diff_state is now INSERT\n");
		  diff_state = INSERT;
		}
	      else
		{
		  dm('O',4,"output.c:printnextline() Nothing unusual - diff_state is still NORMAL\n");
		  diff_state = NORMAL;
		}
	    }
	  break;
      
	case DELETE:
	  dm('O',4,"output.c:printnextline() diff_state is DELETE\n");
	  /*
	   * There is already a line in input_line, put there at the end
	   * of the previous call to print_page()
	   */
	  break;

	case INSERT:
	default:
	  abort();
	}
      input_line_length = (int)strlen(input_line);
      last_char_printed = -1;
      first_line_segment = TRUE;

      /*
       * At this point
       * diff_state
       * NORMAL		print the line, retval set to return of getnextline()
       * DELETE		print the deleted line, retval is set to return of
       *			previous getnextline() or LINE
       * INSERT		print the line as an inserted line, retval is set to
       *			return of getnextline()
       */

      /*
       * page_has_changed() flags this page and function as having changed
       */
      if (diff_state != NORMAL) page_has_changed(page_number);

      if (pass == 1)
	{
	  /*
	   * a new line of source file is about to be printed - so
	   * the appropriate line start, including line number and
	   * a mark to show insertions or deletions, needs to be
	   * printed.
	   */
	  if (diff_state == DELETE)
	    {
	      dm('O',4,"output.c:printnextline() Printing line with diff state DELETE\n");
	      if ((no_show_line_number == FALSE) || (no_show_indent_level == FALSE))
		PUTS("Lpt (-          ) CFs setfont show (");
	      else
		PUTS("Lpt (-) CFs setfont show (");
	    }
	  else
	    {
	      if (diff_state == INSERT)
		{
		  dm('O',4,"output.c:printnextline() Printing line with diff state INSERT\n");
		  PUTS("Lpt BFs setfont (+");
		}
	      else
		{
		  dm('O',4,"output.c:printnextline() Printing line with diff state NORMAL\n");
		  PUTS("Lpt CFs setfont ( ");
		}

	      if (blank_line)
		PUTS("          ");
	      else if ((no_show_line_number == FALSE) || (no_show_indent_level == FALSE))
		{
		  if (no_show_line_number == FALSE)
		    printf("%5ld ",line_number);
		  else
		    PUTS("      ");
		  if ((no_show_indent_level == FALSE) && (braces_depth != 0))
		    printf("%2d  ",braces_depth);
		  else
		    PUTS("    ");
		}
	      PUTS(") show (");
	    }
	}
    }
  else
    {
      if (pass == 1)
	{
	  if (diff_state == DELETE)
	    {
	      dm('O',4,"output.c:printnextline() Printing line with diff state DELETE\n");
	      PUTS("Lpt CFs setfont ( ");
	    }
	  else if (diff_state == INSERT)
	    {
	      dm('O',4,"output.c:printnextline() Printing line with diff state INSERT\n");
	      PUTS("Lpt BFs setfont ( ");
	    }
	  else
	    {
	      dm('O',4,"output.c:printnextline() Printing line with diff state NORMAL\n");
	      PUTS("Lpt CFs setfont ( ");
	    }
	  if ((no_show_line_number == FALSE) || (no_show_indent_level == FALSE))
	    PUTS("          ");
	  PUTS(") show (");
	}
    }

  /*
   * now work out where (and if) the line needs to be split...
   */
  output_line_end = line_end(input_line,last_char_printed);
    
  if (pass == 1)
    {
      output_char_idx = last_char_printed;
    
      if ((no_clever_wrap == FALSE) && !first_line_segment)
	{
	  /* want to pad the outgoing line with leading spaces */
	  int leading_spaces =
	    page_width - (output_line_end - last_char_printed);
      
	  for (count=1; count < leading_spaces; count++)
	    putchar(' ');
	}    
    
      /*
       * now print the line a character at a time...  with some clever
       * coding this could probably be speeded up by printing sections
       * of the line instead of single characters.
       */

      last_char_status = input_status[output_char_idx+1];

      while (output_char_idx < output_line_end)
	{

	  output_char_idx += 1;

	  /*
	   * Check to see if anything should be printed
	   * out before printing the character
	   */
	  if (last_char_status != input_status[output_char_idx])
	    {
	      PUTS(segment_ends[last_char_status][diff_state]);
	      PUTS("(");
	    }

	  /*
	   * Print the character....
	   */
	  switch(input_line[output_char_idx])
	    {
	    case '(':
	    case ')':
	    case '\\': putchar('\\'); break;
	    default:
	      ;
	    }
	  putchar(input_line[output_char_idx]);
	
	  last_char_status = input_status[output_char_idx];
	}
    
      PUTS(segment_ends[last_char_status][diff_state]);
      PUTS(" Nl\n");
      output_char_idx++;
    }
  last_char_printed = output_line_end;

  /*
   * Have we reached the end of the line to be printed?
   */
  if (last_char_printed == input_line_length-1)
    {
      /*
       * At this point:
       * diff_state
       * NORMAL	line printed, retval set to return of getnextline()
       * INSERT	line printed, retval is set to return of getnextline()
       * DELETE	deleted line printed, retval is set to return of
       *		previous getnextline() or STREAM_OK
       *
       * We should now check to see if there are
       * any deleted lines to be printed.  If there are then set diff_state
       * and start printing them next time round.  Otherwise return retval.
       */

      input_line_length = 0;

      if (getdelline(line_number+1,input_line,input_status))
	{
	  diff_state = DELETE;
	  dm('O',3,"output.c:printnextline() Return value is STREAM_OK\n");
	  return(STREAM_OK);
	}

      diff_state = NORMAL;

      if (retval & STREAM_FILE_END)
	{
	  /*
	   * Set everything up for the next file
	   */
	  line_number = 0;
	  retval = STREAM_OK;
	  dm('O',3,"output.c:printnextline() Return value is STREAM_FILE_END\n");
	  return (STREAM_FILE_END);
	}

      dm('O',3,"output.c:printnextline() Return value is %x\n", retval);

      return(retval);

    }
  else
    {
      dm('O',3,"output.c:printnextline() Not fully printed this line - return value is LINE\n");
      return(STREAM_OK);
    }
}

/******************************************************************************
 * function:
 *	blank_page
 * prints out a blank page (well, what did you expect!).
 */
boolean
blank_page(boolean output_page)

{
  file_page_number += 1;
  page_number += 1;
  
  print_text_header(page_number, total_pages);
  return PS_endpage(output_page);
}

/******************************************************************************
 * function:
 *	print_page
 *
 * prints out a single page of a file.
 */
boolean
print_page(void)

{
  short	page_line_number = 0;
  stream_status	retval=STREAM_OK;

  file_page_number += 1;
  page_number += 1;

  dm('O',2,"output.c:print_page() Printing file %d page %d, filepage %d, pass %d\n", file_number, page_number, file_page_number, pass);


  /*
   * Print a page header
   */
  print_text_header(page_number, total_pages);

  /*
   * now print out enough lines to fill the page or until we've reached
   * a page break (which is caused by end of input, file, function or
   * a newpage character).
   */

  while (page_line_number < page_length)
    {
      dm('O',3,"output.c:print_page() Printing line %d\n", page_line_number);
      retval = printnextline();
      page_line_number += 1;
      if ((retval & (STREAM_PAGE_END|STREAM_FILE_END)) 
	  || ((retval & STREAM_FUNCTION_END) && (no_function_page_breaks == FALSE)))
	break;
    }

  /*
   * And finally end the page
   */
  reached_end_of_sheet = PS_endpage(print_prompt(PAGE_BODY, file_page_number, file_name(file_number)));

  dm('O',2,"output.c:print_page() retval %x\n", retval);
  return (!(retval & STREAM_FILE_END));
}

/*
 * function:
 *	print_file
 *
 * prints out a page at a time, calling blank_page to ensure an even number
 * of pages at the end of the file.
 */
void
print_file(void)

{
  file_page_number = 0;

  /*
   * set get_char appropriately
   */
  set_get_char(current_filename);

  while (print_page());

  /*
   * we want to ensure that each file starts on an odd page (i.e. a
   * new sheet of paper, and also that the whole output has an even
   * number of pages in total, so print any blank pages necessary.
   */

  if (new_sheet_after_file)
    {
      fill_sheet_with_blank_pages();
    }
}

/*
 * Print blank pages until the last page printed was the last page
 * on a physical sheet.
 */
void
fill_sheet_with_blank_pages(void)
{
  while (!reached_end_of_sheet)
    {
      dm('O',3,"output.c:print_page() Printing a blank page\n");
      reached_end_of_sheet = blank_page(print_prompt(PAGE_BLANK, 0, NULL));
    }
}
