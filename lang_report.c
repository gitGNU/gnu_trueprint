/*
 * Source file:
 * lang_report.c
 *
 * Contains get_report_char() which parses text files in a special
 * format.  ^B marks the beginning of a 'function name', ^E marks the end,
 * and ^C is the comment delimiter
 */

#include "config.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "trueprint.h"
#include "main.h"
#include "input.h"
#include "index.h"
#include "output.h"

#include "lang_report.h"

/*
 * Public part
 */
char		lang_report_defaults[] = "-i --F --f --b";

/*
 * Private part
 */
typedef enum {
  IP_TEXT,
  IP_COMMENT,
  IP_TAG
} report_ip_states;

/*
 * get_report_char()
 * detects comment starts and ends;
 * finds function names;
 */
stream_status
get_report_char(char *input_char, char_status *status)

{
  static report_ip_states	state=IP_TEXT;
  stream_status	retval;
  static size_t	tag_index=0;
  static long	tag_start_char=0;
  static char	tag_name[SYMBOL_LEN]="UNINITIALIZED";
  static long	tag_page_number=0;

  *status = CHAR_NORMAL;

  retval = getnextchar(input_char);

  switch (state)
    {
    case IP_TEXT:
      switch (*input_char)
	{
	case 2:			/* ^B */
	  retval = getnextchar(input_char);
	  tag_index = 0;
	  tag_name[tag_index++] = *input_char;
	  tag_start_char = char_number;
	  tag_page_number = page_number;
	  state = IP_TAG;
	  break;
	case 3:			/* ^C */
	  *status = CHAR_ITALIC;
	  retval = getnextchar(input_char);
	  state=IP_COMMENT;
	  break;
	default:
	  ;
	}
      break;
    case IP_COMMENT:
      *status = CHAR_ITALIC;
      switch (*input_char)
	{
	case 3:			/* ^C */
	  retval = getnextchar(input_char);
	  state=IP_TEXT;
	  break;
	default:
	  ;
	}
      break;
    case IP_TAG:
      switch (*input_char)
	{
	case 5:			/* ^E */
	  tag_name[tag_index++] = '\0';
	  add_function(tag_name,tag_start_char,char_number-1,tag_page_number,current_filename);
	  end_function(page_number);
	  retval = getnextchar(input_char);
	  state=IP_TEXT;
	  break;
	default:
	  tag_name[tag_index++] = *input_char;
	  break;
	}
      break;
    default:
      ;
    }

  if (pass==1) *status = get_function_name_posn(char_number,*status);

  return(retval);
}

