/*
 * Source file:
 *	debug.c
 */

#include "config.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "trueprint.h"
#include "utils.h"
#include "options.h"

#include "debug.h"

/*
 * Private part
 */
static char    *debug_string;

static int match(char, int);

/******************************************************************************
 * Function:
 *	setup_debug
 */
void
setup_debug(void)
{
  debug_string = "";

  string_option("D", "debug", "", &debug_string, NULL, NULL,
		OPT_MISC,
		"set debug options to <string>");
}

/******************************************************************************
 * Function:
 *	dm (debug_message)
 */

void
dm(char class, int level, char *message, ...)

{
  va_list ap;

  if (debug_string == NULL) return;

  if (!match(class,level)) return;

  va_start(ap, message);

  vfprintf(stderr, message, ap);
}

/******************************************************************************
 * Function:
 *	match
 */
int
match(char class, int level)

{
  char *s_index = debug_string;
  char this_class;
  int this_level;

  while (*s_index)
    {
      skipspaces(&s_index);
      this_class = *(s_index++);
      skipspaces(&s_index);
      this_level = strtol(s_index, &s_index, 10);
      if ((this_class == '@') || (this_class == class))
	{
	  if (this_level >= level) return 1;
	  return 0;
	}
    }

  return 0;
}

