/*
 * Source file:
 *	lang_pike.c
 *
 * Contains get_pike_char(), which parses pike code.
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

#include "lang_pike.h"

/*
 * Public part
 */
char		lang_pike_defaults[] = "--f --F --i";

/*
 * Private part
 */
typedef enum {
  IP_CODE,
  IP_COMMENT_START,
  IP_COMMENT,
  IP_COMMENT_LINE,
  IP_COMMENT_END,
  IP_STRING,
  IP_QUOTE_STRING,
  IP_ESCAPED
} pike_ip_states;

typedef enum {
  FN_INITIAL_SPACE,
  FN_FINAL_CHARS,
  FN_NAME,
  FN_CHAINED_NAME,
  FN_TRAIL_SPACE,
  FN_OPEN_BRACKET,
  FN_DBL_OPEN_BRACKET,
  FN_CLOSE_BRACKET,
  FN_ARG,
  FN_SEMICOLON,
  FN_BODY,
  FN_MACRO,
  FN_MACRO_ESCAPED
} pike_fn_states;

int is_pike_reserved_word(char *fn_name)
{
  if(
     (strcmp(fn_name,"if")==0) || 
     (strcmp(fn_name,"class")==0) || 
     (strcmp(fn_name,"else")==0) || 
     (strcmp(fn_name,"do")==0) || 
     (strcmp(fn_name,"while")==0) || 
     (strcmp(fn_name,"for")==0) || 
     (strcmp(fn_name,"foreach")==0) || 
     (strcmp(fn_name,"catch")==0) || 
     (strcmp(fn_name,"int")==0) ||
     (strcmp(fn_name,"array")==0) ||
     (strcmp(fn_name,"object")==0) ||
     (strcmp(fn_name,"program")==0) ||
     (strcmp(fn_name,"multiset")==0) ||
     (strcmp(fn_name,"mapping")==0) ||
     (strcmp(fn_name,"string")==0) ||
     (strcmp(fn_name,"float")==0) ||
     (strcmp(fn_name,"constant")==0) ||
     (strcmp(fn_name,"switch")==0)
    )
  {
    return 1; 
  }
  return 0;
}

/*
 * get_pike_char()
 * detects comment starts and ends;
 * finds function names;
 * detects function ends;
 */
stream_status
get_pike_char(char *input_char, char_status *status)

{
  static pike_ip_states	state=IP_CODE;
  static pike_ip_states	saved_state;
  pike_ip_states		old_state=state;
  stream_status		retval;
  static pike_fn_states	fn_state;
  static pike_fn_states	saved_fn_state;
  static short		fn_name_index;
  static char		fn_name[SYMBOL_LEN] = "UNINITIALIZED";
  static long		start_char, end_char;
  static long		fn_page_number;

  *status = CHAR_NORMAL;

  if (restart_language == TRUE)
    {
      state		= IP_CODE;
      saved_state	= IP_CODE;
      fn_state	= FN_INITIAL_SPACE;
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
	case '{': braces_depth += 1;
	  if (
                (fn_state == FN_CLOSE_BRACKET) ||
	          (fn_state == FN_CHAINED_NAME) ||
	        (fn_state == FN_BODY)
	       || (fn_state == FN_SEMICOLON)
             )
	    {
              if(!is_pike_reserved_word(fn_name))
              {
	        /* got function! */
	        add_function(fn_name,start_char,end_char,fn_page_number,current_filename);
	        fn_state = FN_BODY;
	      }
	      fn_name_index = 0;
            }
	  break;
	/*case '}': if (((braces_depth -= 1) == 0) && (fn_state == FN_BODY))*/
	case '}': 
          braces_depth -= 1;
          if (fn_state == FN_BODY)
	  {
	    end_function(page_number);
	    retval|=STREAM_FUNCTION_END;
	    fn_state = FN_INITIAL_SPACE;
	  }
	break;
	case '"': state=IP_STRING; break;
	case '\'': state=IP_QUOTE_STRING; break;
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
    case IP_STRING:
      switch (*input_char)
	{
	case '"': state=IP_CODE; break;
	case '\\': saved_state=state; state=IP_ESCAPED; break;
	default:
	  ;
	}
      break;
    case IP_QUOTE_STRING:
      switch (*input_char)
	{
	case '\'': state=IP_CODE; break;
	case '\\': saved_state=state; state=IP_ESCAPED; break;
	default:
	  ;
	}
      break;
    case IP_ESCAPED:
      state=saved_state;
      break;
    default:
      abort();
    }

  if ((state == IP_CODE) && (old_state != IP_COMMENT_END)) switch (fn_state)
    {
    case FN_INITIAL_SPACE:
      if (isalpha(*input_char))
	{
	  fn_page_number = page_number;
	  fn_name[fn_name_index++] = *input_char;
	  start_char = char_number;
	  fn_state = FN_NAME;
	}
      if (*input_char == '#')
	{
	  saved_fn_state = fn_state;
	  fn_state = FN_MACRO;
	}
      break;
    case FN_MACRO:
      if (*input_char == '\\') fn_state = FN_MACRO_ESCAPED;
      if (*input_char == '\n') fn_state = saved_fn_state;
      break;
    case FN_MACRO_ESCAPED:
      fn_state = FN_MACRO;
      break;
    case FN_NAME:
      if (isalnum(*input_char) || (*input_char == '_')
	  || (*input_char == ':') || (*input_char == '~'))
	fn_name[fn_name_index++] = *input_char;
      else if (isspace(*input_char))
	{
	  fn_name[fn_name_index] = 0;
	  end_char = char_number-1;
              if(!is_pike_reserved_word(fn_name))
              {
	        fn_state = FN_TRAIL_SPACE;
              }
              else
              {
                // if we don't reset this, then the returntype will
                // become part of the function name...
	        fn_name_index = 0;
	        fn_state = FN_INITIAL_SPACE;
              }
	}
      else if (*input_char == '(')
	{
	      fn_name[fn_name_index] = 0;
	      end_char = char_number-1;
              if(!is_pike_reserved_word(fn_name))
              {
	        fn_state = FN_OPEN_BRACKET;
              }
              else
              {
	        fn_name_index = 0;
	        fn_state = FN_INITIAL_SPACE;
              }
	}
      else
	{
	  fn_name_index = 0;
	  fn_state = FN_INITIAL_SPACE;
	}
      break;
    case FN_CHAINED_NAME:
      /*
       * This state is a bit of a cheat - we stay in this state
       * until the other FSM finds a {.  This may find more
       * methods than are strictly there, but it'll work in all
       * normal cases.
       */
      break;
    case FN_TRAIL_SPACE:
      if (isspace(*input_char)) break;
      else if (*input_char == '(')
	{
	  fn_name[fn_name_index] = 0;
              if(!is_pike_reserved_word(fn_name))
              {
	        fn_state = FN_OPEN_BRACKET;
              }
	}
      else	if (isalpha(*input_char))
	{
	  fn_name_index = 0;
	  fn_name[fn_name_index++] = *input_char;
	  fn_page_number = page_number;
	  start_char = char_number;
	  fn_state = FN_NAME;
	}
      else
	{
	  fn_name_index = 0;
	  fn_state = FN_INITIAL_SPACE;
	}
      break;
    case FN_OPEN_BRACKET:
      if (*input_char == '(') fn_state = FN_DBL_OPEN_BRACKET;
      if (*input_char == ')') fn_state = FN_CLOSE_BRACKET;
      break;
    case FN_DBL_OPEN_BRACKET:
      if (*input_char == ')') fn_state = FN_OPEN_BRACKET;
      break;
    case FN_CLOSE_BRACKET:
      if (isspace(*input_char))
	{
	  break;
	}
      else if (*input_char == ':')
	{
	  fn_state = FN_CHAINED_NAME;
	}
      else if (isalpha(*input_char) || (*input_char == '['))
	{
	  /* check for [ in case of bp listings! */
	  fn_state = FN_ARG;
	}
      else
	{
	  fn_name_index = 0;
	  fn_state = FN_INITIAL_SPACE;
	}
      break;
    case FN_ARG:
      if (*input_char == ';') fn_state = FN_SEMICOLON;
      /* Treat ] as a semicolon in case of bp listings */
      if (*input_char == ']') fn_state = FN_SEMICOLON;
      break;
    case FN_SEMICOLON:
      if (isspace(*input_char)) break;
      if (*status == CHAR_ITALIC) break;
      /* Take care of arg ending in ...[]; */
      if (*input_char != ';') fn_state = FN_ARG;
      break;
    case FN_BODY:
      break;
    default:
      abort();
    }


  if (pass==1) *status = get_function_name_posn(char_number,*status);

  return(retval);
}
