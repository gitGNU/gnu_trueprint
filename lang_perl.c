/*
 * Source file:
 * lang_perl.c
 *
 * Contains get_perl_char() which parses perl code.
 *
 * Pod functionality added by Daniel Wagenaar
 */

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trueprint.h"
#include "main.h"
#include "input.h"
#include "index.h"
#include "language.h"
#include "output.h"

#include "lang_perl.h"

/*
 * Public part
 */
char		lang_perl_defaults[] = "--i --F --f";

/*
 * Private part
 */
typedef enum {
  IP_CODE,
  IP_STRING,
  IP_QSTRING,
  IP_RE_OPTS,
  IP_COMMENT,
  IP_COMMAND,
  IP_VAR1,
  IP_VARBODY,
  IP_HD1,
  IP_HD2,
  IP_HDSTART,
  IP_HD,
  IP_HDSTARTLINE,
  IP_HDCHECKSTRING,
  IP_POD1,
  IP_POD2,
  IP_POD,
  IP_EPOD1,
  IP_EPOD2,
  IP_EPOD3,
  IP_EPOD4,
  IP_EPOD5,
  IP_EPOD6,
} perl_ip_states;

typedef enum {
  F1_CODE,
  F1_FUNCTION,
  F1_LEADING_SPACE,
  F1_NAME,
  F1_TRAILING_SPACE,
  F1_FNTEXT
} perl_f1_states;

/*
 * get_perl_char()
 * detects comment starts and ends;
 * finds function names;
 * detects function ends;
 */
stream_status
get_perl_char(char *input_char, char_status *status)

{
  static perl_ip_states	state;
  stream_status	retval;
  static perl_f1_states	f1_state;
  static size_t	f1_index;
  static long	f1_start_char;
  static long	f1_end_char;
  static char	f1_name[SYMBOL_LEN];
  static char	here_marker[SYMBOL_LEN];
  static short	here_marker_index;
  static short	here_marker_length;
  static long	f1_page_number;
  static char	sub[]="sub";
  static boolean	escaped;
  static int prepodstate;

  *status = CHAR_NORMAL;

  if (restart_language == TRUE)
    {
      state		= IP_CODE;
      f1_state	= F1_CODE;
      prepodstate = 0;
      f1_index	= 0;
      f1_start_char	= 0;
      f1_end_char	= 0;
      f1_page_number= 0;
      escaped	= FALSE;
      braces_depth	= 0;
      restart_language	= FALSE;
    }

  retval = getnextchar(input_char);

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
	  if (*input_char=='\n') prepodstate++; else prepodstate = 0;
	  switch (*input_char)
	    {
	    case '$': state=IP_VAR1; break;
	    case '"': state=IP_STRING; break;
	    case '\'': state=IP_QSTRING; break;
	    case '`': state=IP_COMMAND; break;
	    case '#':
	      state=IP_COMMENT;
	      *status=CHAR_ITALIC;
	      break;
	    case '<': state=IP_HD1; break;
	    case '[': state=IP_RE_OPTS; break;
	    case '{':
	      if (braces_depth == 0)
		{
		  if (f1_state == F1_TRAILING_SPACE)
		    {
		      add_function(f1_name,f1_start_char,f1_end_char,f1_page_number,current_filename);
		      f1_index = 0;
		      f1_state = F1_FNTEXT;
		    }
		}
	      braces_depth++;
	      break;
	    case '}': if (((braces_depth -= 1) == 0)
			  && ((f1_state == F1_FNTEXT)))
	      {
		end_function(page_number);
		retval |= STREAM_FUNCTION_END;
		f1_state = F1_CODE;
	      }
	    break;
	    case '=': if (prepodstate>1) { state = IP_POD; *status=CHAR_ITALIC; }
	      break;
	    default:
	      ;
	    }
	  break;
	case IP_STRING:
	  if (*input_char == '"') state=IP_CODE;
	  break;
	case IP_RE_OPTS:
	  if (*input_char == ']') state=IP_CODE;
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
	      state=IP_CODE; prepodstate = 1;
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
	case IP_POD:
	  *status = CHAR_ITALIC;
	  if (*input_char == '\n') state = IP_EPOD1;
	  break;
	case IP_EPOD1:
	  *status = CHAR_ITALIC;
	  if (*input_char == '\n') state = IP_EPOD2; else state = IP_POD;
	  break;
	case IP_EPOD2:
	  *status = CHAR_ITALIC;
	  if (*input_char == '\n') state = IP_EPOD3; else state = IP_POD;
	  break;
	case IP_EPOD3:
	  *status = CHAR_ITALIC;
	  if (*input_char == '\n') state = IP_EPOD4; else state = IP_POD;
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

  if ((state == IP_CODE) && (braces_depth == 0))
    {
      switch (f1_state)
	{
	case F1_CODE:
	  if (*input_char == 's')
	    {
	      f1_index = 1;
	      f1_state = F1_FUNCTION;
	      f1_page_number = page_number;
	    }
	  break;
	case F1_FUNCTION:
	  if (*input_char == sub[f1_index++])
	    {
	      if (f1_index == strlen(sub))
		f1_state = F1_LEADING_SPACE;
	    }
	  else
	    f1_state = F1_CODE;
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
	default:
	  ;
	}
    }

  if (pass==1) *status = get_function_name_posn(char_number,*status);

  return(retval);
}

