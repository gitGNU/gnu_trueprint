/*
 * Source file:
 *	utils.c
 */

#include "config.h"

#include <ctype.h>
#include <libintl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trueprint.h"

#include "utils.h"

#if ! HAVE_STRDUP

/***************************************************************************
 * STRDUP
 * - uses malloc() to grab memory and fills it with a copy of a string.
 */

char *strdup(const char *from_string)
{
  char *retval;

  retval = xmalloc(strlen(from_string)+1)

  strcpy(retval,from_string);
  return retval;
}

#endif

#if ! HAVE_STRERROR

/***********************************************************************
 * strerror
 * Returns an informative message on system call failure.  Normally
 * supplied by the system.
 */
char *strerror(int errnum)
{
  static char *b = 0;

  if (!b)
    b = xmalloc(15);

  sprintf(b,"errno = %d",errnum);

  return b;
}

#endif

#if ! HAVE_STRTOL

/*
 * Stub for strtol - only works with base 10
 */
long int strtol(const char *nptr, char **endptr, int base)
{
  long int retval = 0;

  if (base != 10)
    abort();

  *endptr = nptr;

  while (('0' <= *endptr) && (*endptr <= '9'))
    {
      retval = retval * 10 + (*endptr - '0');
      (*endptr)++;
    }
  return retval;
}

#endif

extern long int strtol(const char *nptr, char **endptr, int base);
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

