/***********************************************************************
 * strerror
 * Returns an informative message on system call failure.  Normally
 * supplied by the system.
 */

#include "src/utils.h"

char *strerror(int errnum)
{
  static char *b = 0;

  if (!b)
    b = xmalloc(15);

  sprintf(b,"errno = %d",errnum);

  return b;
}

