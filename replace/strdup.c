/***************************************************************************
 * STRDUP
 * - uses malloc() to grab memory and fills it with a copy of a string.
 */

#include "src/utils.h"

char *strdup(const char *from_string)
{
  char *retval;

  retval = xmalloc(strlen(from_string)+1);

  strcpy(retval,from_string);
  return retval;
}
