/*
 * Source file:
 *	lang_java.c
 *
 * Contains get_java_char(), which partially parses Java code.
 *        It is essentially the same as get_c_char, except that it handles
 *        java methods the same as c functions, and handles // comments.  
 *        No special indication of classes; that would require this parser to
 *        understand a lot more of the language, such as the "class" keyword.
 *        It doesn't recognize a method that's within a class within a method 
 *        within a class.
 *
 * Modified by Lezz to fit in with Trueprint 4.1.1 codebase (thanks Terry!)
 *
 * $Log: lang_java.c,v $
 * Revision 1.1  1999/08/25 01:00:05  lgiles
 * Initial revision
 *
 * Revision 1.2  1998/08/28 19:30:41  tplatt
 * modified to handle "throws" clauses in method declarations,
 * added some (commented out) debug code
 *
 * Revision 1.1  98/08/17  18:49:13  18:49:13  tplatt (Terry L. Platt)
 * Initial revision
 * 
 *
 * (created from lang_c.c Revision 1.2  98/08/17  16:55:14 )
 */

#include "config.h"

#include <ctype.h>
#include <stdio.h>   /* XXDEBUG */
#include <stdlib.h>

#include "trueprint.h"
#include "main.h"
#include "input.h"
#include "index.h"
#include "language.h"
#include "output.h"

#include "lang_java.h"

/*
 * Public part
 */
char		lang_java_defaults[] = "-fy -Fy -xy -bn";

/*
 * Private part
 */
typedef enum {
  IP_CODE,
  IP_COMMENT_START,   /* got "/" */
  IP_COMMENT,         /* got comment start */
  IP_CPP_COMMENT,     /* got "//" for Java & C++*/
  IP_COMMENT_END,     /* got comment end */
  IP_STRING,          /* got opening quote */
  IP_QUOTE_STRING,    /* got opening apostrophe */
  IP_ESCAPED          /* backslash inside STRING or QUOTE_STRING */
} java_ip_states;

typedef enum {
  FN_INITIAL_SPACE,
  FN_FINAL_CHARS,
  FN_NAME,
  FN_TRAIL_SPACE,
  FN_OPEN_BRACKET,
  FN_DBL_OPEN_BRACKET,
  FN_CLOSE_BRACKET,
  FN_SEMICOLON,
  FN_BODY,
  FN_MACRO,
  FN_MACRO_ESCAPED
} java_fn_states;

/*
 * get_java_char()
 * detects comment starts and ends;
 * finds method names;
 * detects method ends;
 */
stream_status
get_java_char(char *input_char, char_status *status)
{
  static java_ip_states	state=IP_CODE;
  static java_ip_states	saved_state;  /* for escaped char in a string */
  java_ip_states		old_state=state;
  stream_status	retval;
  static java_fn_states	fn_state=FN_INITIAL_SPACE;
  static java_fn_states	saved_fn_state;  /* for macros */
  static short	fn_name_index=0;
  static char	fn_name[SYMBOL_LEN] = "UNINITIALIZED";
  static long	start_char, end_char;
  static long	fn_page_number;
  static short    saved_braces_depth = 0;  /* for Java method extents */

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
	    if (c == '*') *status = CHAR_ITALIC;
	    ungetnextchar(c,s);
	  }
	  break;
	case '{': braces_depth += 1;
	  if ((fn_state == FN_CLOSE_BRACKET)
	      || (fn_state == FN_SEMICOLON))
	    {
				/* got method! */
	      add_function(fn_name,start_char,end_char,fn_page_number,
			   current_filename);
	      fn_name_index = 0;
	      fn_state = FN_BODY;
	      saved_braces_depth = braces_depth; /*Java*/
	    }
	  break;
	case '}': if (((braces_depth -= 1) == saved_braces_depth-1)
		      && (fn_state == FN_BODY))
	  {
	    end_function(page_number);
	    retval|=STREAM_FUNCTION_END;
	    fn_state = FN_INITIAL_SPACE;
	  }
	break;
	case '"': state=IP_STRING; break;
	case '\'': state=IP_QUOTE_STRING; break;
	}
      break;
    case IP_COMMENT_START:
      switch (*input_char)
	{
	case '/': state=IP_CPP_COMMENT; *status=CHAR_ITALIC; break; /*Java & C++*/
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
	}
      break;
    case IP_CPP_COMMENT:   /*Java & C++ */
      switch (*input_char)
	{
	case '\n': state=IP_CODE; break;
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
	}
      break;
    case IP_QUOTE_STRING:
      switch (*input_char)
	{
	case '\'': state=IP_CODE; break;
	case '\\': saved_state=state; state=IP_ESCAPED; break;
	}
      break;
    case IP_ESCAPED:
      state=saved_state;
      break;
    default:
      abort();
    }  /* end of switch (state) */
  
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
      if (isalnum(*input_char) || (*input_char == '_'))
	fn_name[fn_name_index++] = *input_char;
      else if (isspace(*input_char))
	{
	  fn_name[fn_name_index] = 0;
	  end_char = char_number-1;
	  fn_state = FN_TRAIL_SPACE;
	}
      else if (*input_char == '(')
	{
	  fn_name[fn_name_index] = 0;
	  end_char = char_number-1;
	  fn_state = FN_OPEN_BRACKET;
	}
      else
	{
	  fn_name_index = 0;
	  fn_state = FN_INITIAL_SPACE;
	}
      break;
    case FN_TRAIL_SPACE:
      if (isspace(*input_char)) break;
      else if (*input_char == '(')
	{
	  fn_name[fn_name_index] = 0;
	  fn_state = FN_OPEN_BRACKET;
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
      if (ispunct(*input_char))
	{
	  fn_name_index = 0;
	  fn_state = FN_INITIAL_SPACE;
	}
      break;
    case FN_BODY:
      break;
    default:
      abort();
    }   /* end of switch (fn_state) */
  
  if (pass==1) *status = get_function_name_posn(char_number, *status);

  return(retval);
}
