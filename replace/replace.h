#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#else
char *gettext(const char *);
#endif

#ifndef HAVE_STRDUP
extern char *strdup(const char *);
#endif

#ifndef HAVE_STRERROR
extern char *strerror(int errnum);
#endif

#ifndef HAVE_STRTOL
extern long int strtol(const char *nptr, char **endptr, int base);
#endif

