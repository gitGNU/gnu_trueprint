/*
 * Source file:
 *	utils.c
 */

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trueprint.h"

#include "utils.h"

void
skipspaces(char **ptr)
{
  while (**ptr && isspace(**ptr)) (*ptr)++;
}

void *
xmalloc(size_t s)
{
  void *r;

  if ((r = malloc(s)) == NULL)
    {
      fprintf(stderr, gettext(CMD_NAME ": cannot allocate memory\n"));
      exit(2);
    }

  return r;
}

void *
xrealloc(void *v, size_t s)
{
  void *r;

  if ((r = realloc(v,s)) == NULL)
    {
      fprintf(stderr, gettext(CMD_NAME ": cannot reallocate memory\n"));
      exit(2);
    }

  return r;
}

#if ! HAVE_GETTEXT
#endif
