/*
 * Source file:
 *	expand_str.c
 */

#include "config.h"

#if TM_IN_SYS_TIME
# include <sys/time.h>
#else
# include <time.h>
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trueprint.h"
#include "main.h"
#include "index.h"
#include "debug.h"
#include "output.h"
#include "utils.h"

#include "expand_str.h"

/*
 * Public part
 */

/*
 * Private part
 */
static char *expand_character(char,boolean);
static void add_string(char **, char *, size_t *);
static void add_character(char **, char, size_t *);

/*****************************************************************************
 * Function:
 *	expand_string
 */
char *
expand_string(char *original_string, boolean index_page)

{
  static char *output_buffer=NULL;
  static size_t buffer_size = 100;
  char *current_char;

  if (original_string)
    dm('h', 5, "expand_string.c:expand_string() Expanding string %s\n", original_string);
  else
    dm('h', 5, "expand_string.c:expand_string() Expanding null string\n");

  /*
   * Initialize output buffer
   */
  if (output_buffer == NULL)
    output_buffer = xmalloc(100);

  *output_buffer = '\0';

  if (original_string)
    {
      /*
       * Run through input string, expanding characters as necessary
       */
      for (current_char = original_string; *current_char; current_char++)
	{
	  if (*current_char != '%')
	    {
	      add_character(&output_buffer, *current_char, &buffer_size);
	      continue;
	    }
	  add_string(&output_buffer, expand_character(*++current_char,index_page), &buffer_size);
	}
    }
  else
    {
      *output_buffer = '\0';
    }

  dm('h', 5, "expand_string.c:expand_string Returning string %s\n", output_buffer);

  return output_buffer;
}

/*****************************************************************************
 * Function:
 *	expand_character
 */
char *
expand_character(char character, boolean index_page)

{
  static char *output_buffer=NULL;
  static struct tm *t=NULL;
  static char *daynames[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  static char *monnames[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  /*
   * Initialize time buffer
   */
  if (t == NULL)
    {
      time_t now;
      if (!use_environment)
	now = 387774000;
      else
	now = time((time_t *)NULL);
      t = localtime(&now);
    }

  /*
   * Initialize output buffer
   */
  if (output_buffer == NULL)
    output_buffer = xmalloc(100);

  *output_buffer = '\0';

  switch (character)
    {
    case '%':		/* percent character */
      return "%";
    case 'm':		/* month of year */
      sprintf(output_buffer, "%02d", t->tm_mon + 1);
      return output_buffer;
    case 'd':		/* day of month */
      sprintf(output_buffer, "%02d", t->tm_mday);
      return output_buffer;
    case 'y':		/* year - four digits */
      sprintf(output_buffer, "%4d", t->tm_year + 1900);
      return output_buffer;
    case 'D':		/* date as mm/dd/yy */
      sprintf(output_buffer, "%02d/%02d/%02d", t->tm_mon+1, t->tm_mday, t->tm_year);
      return output_buffer;
    case 'L':		/* date as %a %h %d %T %y */
      sprintf(output_buffer, "%s %s %02d %02d:%02d:%02d %4d",
		    daynames[t->tm_wday], monnames[t->tm_mon], t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, t->tm_year + 1900);
      return output_buffer;
    case 'H':		/* hour - 00 to 23 */
      sprintf(output_buffer, "%02d", t->tm_hour);
      return output_buffer;
    case 'M':		/* minute - 00 to 59 */
      sprintf(output_buffer, "%02d", t->tm_min);
      return output_buffer;
    case 'S':		/* second - 00 to 59 */
      sprintf(output_buffer, "%02d", t->tm_sec);
      return output_buffer;
    case 'T':		/* time as HH:MM:SS */
      sprintf(output_buffer, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
      return output_buffer;
    case 'j':		/* day of year - 001 to 366 */
      sprintf(output_buffer, "%3d", t->tm_yday);
      return output_buffer;
    case 'w':		/* day of week - Sunday = 0 */
      sprintf(output_buffer, "%d", t->tm_wday);
      return output_buffer;
    case 'a':		/* abbreviated weekday - Sun to Sat */
      return daynames[t->tm_wday];
    case 'h':		/* abbreviated month - Jan to Dec */
      return monnames[t->tm_mon];
    case 'r':		/* time in AM/PM notation */
      if (t->tm_hour > 12)
	sprintf(output_buffer, "%02d:%02dpm", t->tm_hour-12, t->tm_min);
      else
	sprintf(output_buffer, "%02d:%02dam", t->tm_hour, t->tm_min);
      return output_buffer;

    case 'p':		/* page number in current file */
      if (index_page) return "";
      sprintf(output_buffer, "%ld", file_page_number);
      return output_buffer;
    case 'P':		/* page number overall */
      if (index_page) return "";
      sprintf(output_buffer, "%ld", page_number);
      return output_buffer;
    case 'f':		/* total page numbers in current file */
      if (index_page) return "";
      sprintf(output_buffer, "%ld", get_file_last_page(file_number) - get_file_first_page(file_number) + 1);
      return output_buffer;
    case 'F':		/* final page number overall */
      if (index_page) return "";
      sprintf(output_buffer, "%ld", total_pages);
      return output_buffer;
    case 'n':		/* current filename */
      if (index_page) return "";
      return file_name(file_number);
    case 'N':             /* current function name */
      if (index_page) return "";
      return get_function_name(page_number);
    case 'l':		/* login name */
      if (!use_environment) return "testuser";
      {
	char *u = getenv("USER");
	if (u == 0)
	  return "";
	else
	  return u;
      }
    default:
      return "?";
    }
}

static void
add_string(char **buffer_ptr, char *string, size_t *bufflen_ptr)

{
  size_t required_length;

  required_length = strlen(*buffer_ptr)+strlen(string)+1;

  if (required_length > *bufflen_ptr)
    {
      *buffer_ptr = xrealloc(*buffer_ptr, required_length);
      *bufflen_ptr = required_length;
    }
  strcat(*buffer_ptr, string);
  return;
}

static void
add_character(char **buffer_ptr, char character, size_t *bufflen_ptr)

{
  size_t required_length;

  required_length = strlen(*buffer_ptr)+2;

  if (required_length > *bufflen_ptr)
    {
      *buffer_ptr = xrealloc(*buffer_ptr, required_length);
      *bufflen_ptr = required_length;
    }
  (*buffer_ptr)[required_length-1] = '\0';
  (*buffer_ptr)[required_length-2] = character;
  return;
}
