/*
 * Source file:
 *	lang_verilog.c
 *
 * Contains get_verilog_char(), which parses Verilog code.
 */

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "trueprint.h"
#include "main.h"
#include "input.h"
#include "index.h"
#include "language.h"
#include "output.h"

#include "lang_verilog.h"

/*
 * Public part
 */
char		lang_verilog_defaults[] = "-f --F --i";

/*
 * Private part
 */
typedef enum {
  IP_CODE,
  IP_COMMENT_START,
  IP_COMMENT,
  IP_COMMENT_LINE,
  IP_COMMENT_END
} verilog_ip_states;

/*
 * get_verilog_char()
 * detects comment starts and ends;
 * finds function names;
 * detects function ends;
 */
stream_status
get_verilog_char(char *input_char, char_status *status)

{
  static verilog_ip_states	state=IP_CODE;
  static verilog_ip_states	saved_state;
  /* verilog_ip_states		old_state=state; */
  stream_status		retval;
  /* static char		fn_name[SYMBOL_LEN] = "UNINITIALIZED"; */
  /* static short		fn_name_index; */
  static long		start_char, end_char;
  static long		fn_page_number;

  *status = CHAR_NORMAL;

  if (restart_language == TRUE)
    {
      state		= IP_CODE;
      saved_state	= IP_CODE;
      start_char	= 0;
      end_char	= 0;
      fn_page_number= 0;
      braces_depth	= 0;
      restart_language	= FALSE;
    }

  retval = getnextchar(input_char);

  switch (state)
    {
    case IP_CODE:
      switch (*input_char)
	{
	case '/':
	  state = IP_COMMENT_START;
	  {
	    stream_status s;
	    char          c;
	    s = getnextchar(&c);
	    if ((c == '*') || (c == '/')) *status = CHAR_ITALIC;
	    ungetnextchar(c,s);
	  }
	  break;
	case '{':
	  braces_depth += 1;
	  break;
	case '}':
	  braces_depth -= 1;
	  break;
	default:
	  ;
	}
      break;
    case IP_COMMENT_START:
      switch (*input_char)
	{
	case '/': state=IP_COMMENT_LINE; *status=CHAR_ITALIC; break;
	case '*': state=IP_COMMENT; *status=CHAR_ITALIC; break;
	case '{': state=IP_CODE; braces_depth+=1; break;
	case '}': if ((braces_depth -= 1) == 0) retval|=STREAM_FUNCTION_END;
	  break;
	default: state=IP_CODE; break;
	}
      break;
    case IP_COMMENT:
      *status = CHAR_ITALIC;
      switch (*input_char)
	{
	case '*': state=IP_COMMENT_END; break;
	default:
	  ;
	}
      break;
    case IP_COMMENT_LINE:
      *status = CHAR_ITALIC;
      switch (*input_char)
	{
	case '\n': state=IP_CODE; break;
	default:
	  ;
	}
      break;
    case IP_COMMENT_END:
      *status = CHAR_ITALIC;
      switch (*input_char)
	{
	case '/': state=IP_CODE; break;
	case '*': break;
	default: state=IP_COMMENT; break;
	}
      break;
    default:
      abort();
    }

  /* The whole switch on fn_state is removed - it can be copied back from
     lang_cxx.c if necessary. */


  if (pass==1) *status = get_function_name_posn(char_number,*status);

  return(retval);
}
