/*
 * Source file:
 * lang_sh.c
 *
 * Contains get_sh_char() which parses shell code.
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
#include "language.h"
#include "output.h"

#include "lang_sh.h"

/*
 * Public part
 */
char		lang_sh_defaults[] = "-i --F --f";

/*
 * Private part
 */
typedef enum {
  IP_CODE,
  IP_STRING,
  IP_QSTRING,
  IP_COMMENT,
  IP_COMMAND,
  IP_VAR1,
  IP_VARBODY,
  IP_HD1,
  IP_HD2,
  IP_HDSTART,
  IP_HD,
  IP_HDSTARTLINE,
  IP_HDCHECKSTRING
} sh_ip_states;

typedef enum {
  F1_CODE,
  F1_FUNCTION,
  F1_LEADING_SPACE,
  F1_NAME,
  F1_TRAILING_SPACE,
  F1_FNTEXT
} sh_f1_states;

typedef enum {
  FN_INITIAL_SPACE,
  FN_NAME,
  FN_TRAIL_SPACE,
  FN_OPEN_BRACKET,
  FN_CLOSE_BRACKET,
  FN_FNTEXT
} sh_fn_states;

/*
 * get_sh_char()
 * detects comment starts and ends;
 * finds function names;
 * detects function ends;
 * returns one of the following:
 * CHAR
 * COMMENT_START
 * COMMENT_END
 * FUNCTION_END
 * FILE_END
 * INPUT_END
 * FUNCTION_NAME_START
 * FUNCTION_NAME_END
 */
stream_status
get_sh_char(char *input_char, char_status *status)

{
  static sh_ip_states	state=IP_CODE;
  stream_status	retval;
  static sh_f1_states	f1_state=F1_CODE;
  static sh_fn_states	fn_state=FN_INITIAL_SPACE;
  static size_t	f1_index=0;
  static short	fn_index=0;
  static long	f1_start_char=0;
  static long	fn_start_char=0;
  static long	f1_end_char=0;
  static long	fn_end_char=0;
  static char	f1_name[SYMBOL_LEN]="UNINITIALIZED";
  static char	fn_name[SYMBOL_LEN]="UNINITIALIZED";
  static char	here_marker[SYMBOL_LEN];
  static short	here_marker_index;
  static short	here_marker_length;
  static long	f1_page_number=0;
  static long	fn_page_number=0;
  static char	function[]="function";
  static boolean	escaped=FALSE;

  *status = CHAR_NORMAL;

  if (restart_language == TRUE)
    {
      state		= IP_CODE;
      f1_state	= F1_CODE;
      fn_state	= FN_INITIAL_SPACE;
      f1_index	= 0;
      fn_index	= 0;
      f1_start_char	= 0;
      fn_start_char	= 0;
      f1_end_char	= 0;
      fn_end_char	= 0;
      f1_page_number= 0;
      fn_page_number= 0;
      escaped	= FALSE;
      braces_depth	= 0;
      restart_language	= FALSE;
    }

  retval = getnextchar(input_char);

  dm('L',9,"lang_sh.c:state=%d,f1_state=%d,fn_state=%d,input=%c\n",state,f1_state,fn_state,*input_char);
  /* First a short switch to sort out escaped stuff... */
  switch (state)
    {
    case IP_CODE:
    case IP_STRING:
    case IP_QSTRING:
    case IP_COMMAND:
    case IP_VARBODY:
      if (escaped)
	{
	  escaped=FALSE;
	  break;
	}
      /* otherwise drop through */
    case IP_HD1:
    case IP_HD2:
    case IP_VAR1:
    case IP_HDSTART:
      if ((*input_char == '\\') && !escaped)
	{
	  escaped = TRUE;
	  break;
	}
      /* otherwise drop through */
    default:
      switch (state)
	{
	case IP_CODE:
	  switch (*input_char)
	    {
	    case '$': state=IP_VAR1; break;
	    case '"': state=IP_STRING; break;
	    case '\'': state=IP_QSTRING; break;
	    case '`': state=IP_COMMAND; break;
	    case '#':
	      *status = CHAR_ITALIC;
	      state=IP_COMMENT;
	      break;
	    case '<': state=IP_HD1; break;
	    case '{':
	      if (braces_depth == 0)
		{
		  if (f1_state == F1_TRAILING_SPACE)
		    {
		      add_function(f1_name,f1_start_char,f1_end_char,f1_page_number,current_filename);
		      f1_index = 0;
		      f1_state = F1_FNTEXT;
		    }
		  else if (fn_state == FN_CLOSE_BRACKET)
		    {
		      add_function(fn_name, fn_start_char, fn_end_char,fn_page_number,current_filename);
		      fn_index = 0;
		      fn_state = FN_FNTEXT;
		    }
		}
	      braces_depth++;
	      break;
	    case '}': if (((braces_depth -= 1) == 0)
			  && ((f1_state == F1_FNTEXT)
			      || (fn_state == FN_FNTEXT)))
	      {
		end_function(page_number);
		retval |= STREAM_FUNCTION_END;
		f1_state = F1_CODE;
		fn_state = FN_INITIAL_SPACE;
					
	      }
	    break;
	    default:
	      ;
	    }
	  break;
	case IP_STRING:
	  if (*input_char == '"') state=IP_CODE;
	  break;
	case IP_QSTRING:
	  if (*input_char == '\'') state=IP_CODE;
	  break;
	case IP_COMMAND:
	  if (*input_char == '`') state=IP_CODE;
	  break;
	case IP_COMMENT:
	  *status = CHAR_ITALIC;
	  if (*input_char == '\n')
	    {
	      state=IP_CODE;
	    }
	  break;
	case IP_VAR1:
	  if (escaped)
	    {
	      state=IP_CODE;
	      escaped=FALSE;
	      break;
	    }
	  if (*input_char == '{') state=IP_VARBODY;
	  else state=IP_CODE;
	  break;
	case IP_VARBODY:
	  if (*input_char == '}') state=IP_CODE;
	  break;
	case IP_HD1:
	  if (escaped)
	    {
	      state=IP_CODE;
	      escaped=FALSE;
	      break;
	    }
	  if (*input_char == '<') state=IP_HD2;
	  else state=IP_CODE;
	  break;
	case IP_HD2:
	  if ((escaped || ((*input_char != '-') && (!isspace(*input_char))))
	      && (*input_char != '"') && (*input_char != '\''))
	    {
	      escaped = FALSE;
	      here_marker[here_marker_index=0] = *input_char;
	      state = IP_HDSTART;
	      break;
	    }
	  if (*input_char == '-') break;
	  if (isspace(*input_char) || (*input_char == '"')
	      || (*input_char == '\'')) break;
	  state=IP_CODE;
	  break;
	case IP_HDSTART:
	  if ((escaped || !isspace(*input_char))
	      && (*input_char != '"') && (*input_char != '\''))
	    {
	      escaped = FALSE;
	      here_marker[++here_marker_index] = *input_char;
	      break;
	    }
	  here_marker_length = here_marker_index+1;
	  here_marker_index = 0;
	  state=IP_HD;
	  break;
	case IP_HD:
	  if (*input_char == '\n') state = IP_HDSTARTLINE;
	  break;
	case IP_HDSTARTLINE:
	  if ((*input_char == '\n') || (isspace(*input_char)))
	    break;
	  state=IP_HDCHECKSTRING;
	  /* else fall through */
	case IP_HDCHECKSTRING:
	  if ((*input_char == '"') || (*input_char == '\''))
	    break;
	  if (*input_char != here_marker[here_marker_index++])
	    {
	      if ((here_marker_index == here_marker_length+1)
		  && (*input_char == '\n'))
		{
		  state=IP_CODE;
		  break;
		}
	      state=IP_HD;
	      here_marker_index=0;
	    }
	  break;
	default:
	  abort();
	}
    }

  if (state == IP_CODE)
    {
      switch (f1_state)
	{
	case F1_CODE:
	  if (*input_char == 'f')
	    {
	      f1_index = 1;
	      f1_state = F1_FUNCTION;
	      f1_page_number = page_number;
	    }
	  break;
	case F1_FUNCTION:
	  if (*input_char == function[f1_index++])
	    {
	      if (f1_index == strlen(function))
		f1_state = F1_LEADING_SPACE;
	    }
	  else f1_state = F1_CODE;
	  break;
	case F1_LEADING_SPACE:
	  if (isspace(*input_char)) break;
	  if (isalpha(*input_char))
	    {
	      f1_page_number = page_number;
	      f1_name[f1_index=0] = *input_char;
	      f1_start_char = char_number;
	      f1_state=F1_NAME;
	      break;
	    }
	  f1_state = F1_CODE;
	  break;
	case F1_NAME:
	  if (isalnum(*input_char) || (*input_char == '_'))
	    {
	      f1_name[++f1_index] = *input_char;
	    }
	  else
	    {
	      f1_name[++f1_index] = '\0';
	      f1_end_char = char_number - 1;
	      f1_state = F1_TRAILING_SPACE;
	    }
	  break;
	case F1_TRAILING_SPACE:
	  if (!isspace(*input_char)) f1_state = F1_FNTEXT;
	  break;
	default:
	  ;
	}
    }

  if (state == IP_CODE)
    {
      switch (fn_state)
	{
	case FN_INITIAL_SPACE:
	  if (isalpha(*input_char))
	    {
	      fn_page_number = page_number;
	      fn_name[fn_index++] = *input_char;
	      fn_start_char = char_number;
	      fn_state = FN_NAME;
	    }
	  break;
	case FN_NAME:
	  if (isalnum(*input_char) || (*input_char == '_'))
	    fn_name[fn_index++] = *input_char;
	  else if (isspace(*input_char))
	    {
	      fn_name[fn_index] = '\0';
	      fn_end_char = char_number - 1;
	      fn_state = FN_TRAIL_SPACE;
	    }
	  else if (*input_char == '(')
	    {
	      fn_name[fn_index] = '\0';
	      fn_end_char = char_number - 1;
	      fn_state = FN_OPEN_BRACKET;
	    }
	  else
	    {
	      fn_index = 0;
	      fn_state = FN_INITIAL_SPACE;
	    }
	  break;
	case FN_TRAIL_SPACE:
	  if (isspace(*input_char)) break;
	  else if (*input_char == '(') fn_state = FN_OPEN_BRACKET;
	  else if (isalpha(*input_char))
	    {
	      fn_index=0;
	      fn_page_number = page_number;
	      fn_name[fn_index++] = *input_char;
	      fn_start_char = char_number;
	      fn_state = FN_NAME;
	    }
	  else
	    {
	      fn_index = 0;
	      fn_state = FN_INITIAL_SPACE;
	    }
	  break;
	case FN_OPEN_BRACKET:
	  if (*input_char == ')') fn_state = FN_CLOSE_BRACKET;
	  break;
	case FN_CLOSE_BRACKET:
	  if (isspace(*input_char)) break;
	  fn_index=0;
	  fn_page_number = page_number;
	  fn_name[fn_index++] = *input_char;
	  fn_start_char = char_number;
	  fn_state = FN_NAME;
	  break;
	default:
	  ;
	}
    }

  if (pass==1) *status = get_function_name_posn(char_number,*status);

  return(retval);
}

