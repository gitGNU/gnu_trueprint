/*
 * Source file:
 *	openpipe.c
 *
 * Contains openpipe - a replacement for the non-POSIX function popen.
 * This returns an int rather than a FILE * to make it general purpose -
 * fdopen can be used outside to convert this to a FILE *
 */

#include "config.h"

#include <errno.h>
#include <libintl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifndef WNOHANG
# define WNOHANG 1
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#else
extern int close(int fildes);
extern int dup(int fildes);
extern pid_t fork(void);
extern int pipe(int fildes[2]);
extern pid_t waitpid(pid_t pid, int *stat_loc, int options);
#endif

#include "trueprint.h"
#include "utils.h"
#include "main.h"
#include "debug.h"

#include "openpipe.h"

/*
 * Function:
 *	openpipe
 *
 * Open a pipe.  Write an initial character through the new pipe to make sure
 * that it works (there was a bug associated with a subprocess which didn't
 * write anything to the pipe).
 */
int
openpipe(const char *command, char *mode)

{
  int fildes[2];
  int parent_pipe_end;
  int child_pipe_end;
  char *child_pipe_mode;

  if (strcmp(mode, "w") == 0)
    {
      parent_pipe_end = 1; child_pipe_end = 0;
      child_pipe_mode = "read";
    }
  else if (strcmp(mode, "r") == 0)
    {
      parent_pipe_end = 0; child_pipe_end = 1;
      child_pipe_mode = "write";
    }
  else
    abort();

  if (pipe(fildes) != 0)
    {
      perror(CMD_NAME ": Cannot create pipe");
      exit(2);
    }

  dm('f',3,"openpipe(): opened pipe with handles read=%d and write=%d\n",fildes[0],fildes[1]);

  switch (fork())
    {
    case -1:
      /* Error */
      perror(CMD_NAME ": Cannot fork");
      exit(2);
      /*NOTREACHED*/

    case 0:
      /* Child process */
      if (!((close(fildes[parent_pipe_end]) == 0)
	    && (close(child_pipe_end) == 0)
	    && (dup(fildes[child_pipe_end]) == child_pipe_end)))
	{
	  fprintf(stderr, gettext(CMD_NAME ": cannot redirect %s for child, %s\n"), child_pipe_mode, strerror(errno));
	  exit(2);
	}

      /* Write a single byte through the pipe to establish it */
      if (child_pipe_end == 1)
	{
	  write(fildes[child_pipe_end],"z",1);
	}
      else
	{
	  char buff[1];
	  read(fildes[child_pipe_end],buff,1);
	  if (*buff != 'z')
	    {
	      fprintf(stderr, gettext(CMD_NAME ": failed to open pipe properly\n"));
	      exit(2);
	    }
	}

      if (system(command) < 0)
	{
	  perror(CMD_NAME ": Cannot start pipe");
	  exit(2);
	}
      else
	exit(0);
      /*NOTREACHED*/

    default:
      /* Parent process */

      if (close(fildes[child_pipe_end]) != 0)
	{
	  fprintf(stderr, gettext(CMD_NAME ": cannot close %s end of pipe for parent, %s\n"), child_pipe_mode, strerror(errno));
	  exit(2);
	}

      /* Write a single byte through the pipe to establish it */
      if (parent_pipe_end == 1)
	{
	  write(fildes[parent_pipe_end],"z",1);
	}
      else
	{
	  char buff[1];
	  read(fildes[parent_pipe_end],buff,1);
	  if (*buff != 'z')
	    {
	      fprintf(stderr, gettext(CMD_NAME ": failed to open pipe properly\n"));
	      exit(2);
	    }
	}

      return fildes[parent_pipe_end];
    }
}

FILE *
fopenpipe(const char *command, char *mode)

{
  int handle;
  FILE *fd;

  handle = openpipe(command, mode);

  fd = fdopen(handle, mode);
  dm('f',3,"Opened pipe %d, %s, handle %d for command = %s\n",handle,mode,fd,command);
  return fd;
}

void
closepipe(int handle)

{
  int statloc;
  close(handle);
  waitpid(-1, &statloc, 0);
}

void
fclosepipe(FILE *fp)

{
  closepipe(fileno(fp));
  /* need to call fclose() to deallocate FILE buffer */
  if (fclose(fp) != EOF)
    perror(CMD_NAME ": could not close diff stream");
}
