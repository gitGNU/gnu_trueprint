/*
 * Source file:
 *	lang_pascal.c
 *
 * Contains get_pascal_char(), which parses C code.
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

#include "lang_pascal.h"

/*
 * Public part
 */
char		lang_pascal_defaults[] = "--f --F --i";

/*
 * Private part
 */

typedef enum {
  PAS_SPACE,
  PAS_COMMENT_START,
  PAS_COMMENT_END,
  PAS_COMMENT
} pascal_states;

typedef enum {
  FN_CODE,
  FN_PROCEDURE,
  FN_FUNCTION,
  FN_BEGIN,
  FN_END,
  FN_SPACE,
  FN_NAME
} pascal_fn_states;

static char	procedure_string[] = "procedure";
static char	function_string[]  = "function";
static char	begin_string[]     = "begin";
static char	end_string[]       = "end";

/*
 * get_pascal_char()
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
get_pascal_char(char *input_char, char_status *status)

{
  stream_status	retval;
  static pascal_states	state = PAS_SPACE;
  static pascal_fn_states	fn_state = FN_CODE;
  static long		start_char;
  static char		fn_name[SYMBOL_LEN];
  static long		fn_page;
  static size_t		token_index;
  static short		function_depth = 0;

  *status = CHAR_NORMAL;

  if (restart_language == TRUE)
    {
      state = PAS_SPACE;
      fn_state = FN_CODE;
      function_depth = 0;
      braces_depth	= 0;
      restart_language	= FALSE;
    }

  retval = getnextchar(input_char);

  switch (state)
    {
    case PAS_SPACE:
      if (isspace(*input_char)) break;
      if (*input_char == '{')
	{
	  state = PAS_COMMENT;
	  *status = CHAR_ITALIC;
	}
      else if (*input_char == '(')
	{
	  stream_status s;
	  char c;
	  s = getnextchar(&c);
	  if (c == '*') *status = CHAR_ITALIC;
	  ungetnextchar(c,s);
	  state=PAS_COMMENT_START;
	}
      break;
    case PAS_COMMENT_START:
      if (*input_char == '*')
	{
	  state = PAS_COMMENT;
	  *status = CHAR_ITALIC;
	}
      else
	state = PAS_SPACE;
      break;
    case PAS_COMMENT_END:
      *status = CHAR_ITALIC;
      if (*input_char == ')') state = PAS_SPACE;
      break;
    case PAS_COMMENT:
      *status = CHAR_ITALIC;
      if (*input_char == '}')
	{
	  state = PAS_SPACE;
	}
      else if (*input_char == '*')
	{
	  state = PAS_COMMENT_END;
	}
      break;
    default:
      abort();
    }

  if ((state == PAS_SPACE) || (state == PAS_COMMENT_START))
    {
      switch (fn_state)
	{
	case FN_CODE:
	  if (*input_char == 'b'){fn_state=FN_BEGIN;token_index=1;break;}
	  if (*input_char == 'e'){fn_state=FN_END;token_index=1;break;}
	  if (*input_char == 'f'){fn_state=FN_FUNCTION;token_index=1;break;}
	  if (*input_char == 'p'){fn_state=FN_PROCEDURE;token_index=1;break;}
	  break;
	case FN_PROCEDURE:
	  if (*input_char == procedure_string[token_index])
	    {
	      if (++token_index == strlen(procedure_string))
		{ token_index = 0; fn_state = FN_SPACE; }
	    }
	  else
	    fn_state = FN_CODE;
	  break;
	case FN_FUNCTION:
	  if (*input_char == function_string[token_index])
	    {
	      if (++token_index == strlen(function_string))
		{ token_index = 0; fn_state = FN_SPACE; }
	    }
	  else
	    fn_state = FN_CODE;
	  break;
	case FN_BEGIN:
	  if (*input_char == begin_string[token_index])
	    {
	      if (++token_index == strlen(begin_string))
		{
		  token_index = 0;
		  fn_state = FN_CODE;
		  braces_depth++;
		}
	    }
	  else
	    fn_state = FN_CODE;
	  break;
	case FN_END:
	  if (*input_char == end_string[token_index])
	    {
	      if (++token_index == strlen(end_string))
		{
		  token_index = 0;
		  fn_state = FN_CODE;
		  if (braces_depth != 0)
		    if (--braces_depth == function_depth)
		      {
			retval|=STREAM_FUNCTION_END;
			end_function(page_number);
			function_depth--;
		      }
		}
	    }
	  else
	    fn_state = FN_CODE;
	  break;
	case FN_SPACE:
	  if (!isspace(*input_char))
	    {
	      if (isalpha(*input_char))
		{
		  token_index = 0;
		  fn_name[token_index++] = *input_char;
		  start_char = char_number;
		  fn_page = page_number;
		  fn_state = FN_NAME;
		}
	      else
		fn_state = FN_CODE;
	    }
	  break;
	case FN_NAME:
	  if ((isalnum(*input_char)||*input_char == '_') && (token_index < SYMBOL_LEN-1))
	    fn_name[token_index++] = *input_char;
	  else
	    {
	      fn_name[token_index] = '\0';
	      add_function(fn_name,start_char,char_number-1,fn_page,current_filename);
	      fn_state = FN_CODE;
	      function_depth = braces_depth;
	    }
	  break;
	default:
	  abort();
	}
    }

  if (pass==1) *status = get_function_name_posn(char_number,*status);

  return(retval);
}
