/*
 * Source file:
 * lang_text.c
 *
 * Contains get_text_char() which "parses" straight text.
 */

#include "config.h"

#include <ctype.h>
#include <string.h>

#include "trueprint.h"
#include "input.h"
#include "lang_text.h"

/*
 * Public part
 */
char		lang_text_defaults[] = "-i -n -f -b";
char		lang_list_defaults[] = "-i --n -w0 -l66 -b -f";

/*
 * Private part
 */

/*
 * get_text_char()
 * Basically just calls getnextchar and passes up information.
 * returns:
 * CHAR
 * FILE_END
 * INPUT_END
 */
stream_status
get_text_char(char *input_char, char_status *status)

{
  stream_status retval;

  *status = CHAR_NORMAL;

  retval = getnextchar(input_char);

  if (*input_char != BACKSPACE)
    {
      char          nextchar;
      char_status   nextstatus;

      nextstatus = getnextchar(&nextchar);

      if (nextchar == BACKSPACE)
	{
	  char          nextnextchar;
	  char_status   nextnextstatus;

	  nextnextstatus = getnextchar(&nextnextchar);

	  if (nextnextchar == BACKSPACE)
	    {
	      /* Special case - start of a string of backspaces */
	      ungetnextchar(nextnextchar, nextnextstatus);
	      ungetnextchar(nextchar, nextstatus);
	    }
	  else if (nextnextchar == *input_char)
	    {
	      *status = CHAR_BOLD;
	    }
	  else if (nextnextchar == '_')
	    {
	      *status = CHAR_UNDERLINE;
	    }
	  else
	    {
	      *input_char = nextnextchar;
	    }
	}
      else /* nextchar != BACKSPACE */
	{
	  ungetnextchar(nextchar, nextstatus);
	}
    }

  return retval;
    
}

