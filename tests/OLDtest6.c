/*
 * Source file:
 *	openpipe.c
 *
 * Contains openpipe - a replacement for the non-POSIX function popen.
 * This returns an int rather than a FILE * to make it general purpose.
 * NOTE THAT THE VARIABLE cmd_name IS NOT DEFINED IN THIS FILE
 */

#define _POSIX_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/****************************************************************************
 * Function:
 *	openpipe
 *
 * Opens a pipe to or from a command.  This is _not_ a direct replacement for
 * popen because:
 * - in the case of failure it prints an error message and calls exit,
 *   instead of just returning NULL;
 * - it returns an int instead of a FILE *;
 * - it is called openpipe instead of popen
 */

int
openpipe(const char *command, char *mode)

{
  int fildes[2];
  int parent_pipe_end;
  int child_pipe_end;
  char *child_pipe_mode;

  if (strcmp(mode, "w") == 0) {
    parent_pipe_end = 1; child_pipe_end = 0;
    child_pipe_mode = "read";
  } else if (strcmp(mode, "r") == 0) {
    parent_pipe_end = 0; child_pipe_end = 1;
    child_pipe_mode = "write";
  } else {
    (void)fprintf(stderr, "%s: Internal error: popen() called with bad mode %s", cmd_name, mode);
    exit(2);
  }

  if (pipe(fildes) != 0) {
    (void)fprintf(stderr, "%s: Cannot create pipe, %s\n", cmd_name, strerror(errno));
    exit(2);
  }

  switch (fork()) {
  case -1:
    /* Error */
    (void)fprintf(stderr, "%s: Cannot fork, %s\n", cmd_name, strerror(errno));
    exit(2);
    /*NOTREACHED*/

  case 0:
    /* Child process */
    if (!((close(fildes[parent_pipe_end]) == 0) &&
	 (close(child_pipe_end) == 0) &&
	 (dup(fildes[child_pipe_end]) == child_pipe_end))) {
      (void)fprintf(stderr,"%s: Cannot redirect %s for child, %s\n", cmd_name, child_pipe_mode, strerror(errno));
      exit(2);
    }

    if (system(command) < 0) {
      (void)fprintf(stderr,"%s: Cannot start pipe, %s\n",cmd_name, strerror(errno));
      exit(2);
    } else exit(0);
    /*NOTREACHED*/

  default:
    /* Parent process */

    if (close(fildes[child_pipe_end]) != 0) {
      (void)fprintf(stderr,"%s: Cannot close %s end of pipe for parent, %s\n",cmd_name, child_pipe_mode, strerror(errno));
      exit(2);
    }

    return fildes[parent_pipe_end];
  }
}

FILE *
fopenpipe(const char *command, char *mode)

{
  int handle;

  handle = openpipe(command, mode);

  return fdopen(handle, mode);
}

void
closepipe(int handle)

{
  int statloc;

  (void)close(handle);
  (void)waitpid(-1, &statloc, WNOHANG);
}

void
fclosepipe(FILE *fp)

{
  closepipe(fileno(fp));
}
