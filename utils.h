void skipspaces(char **);
/* strdup may be defined by string.h, but it is also defined here */
extern char *strdup(const char *);

/* strerror may be defined by string.h, but it is also defined here */
extern char *strerror(int errnum);

/* strtol may be defined by string.h, but it is also defined here */
extern long int strtol(const char *nptr, char **endptr, int base);

extern void *xmalloc(size_t);

extern void *xrealloc(void *, size_t);

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#else
char *gettext(const char *);
#endif
