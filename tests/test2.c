/* @(#)getopt.c */

#define _POSIX_SOURCE

#include <stdio.h>
#include <string.h>

#include "trueprint.h"
#include "main.h"

/*
 * get option letter from argument vector
 */
int		optind = 1;		/* index into parent argv vector */
char		*optarg;		/* argument associated with option */

static int	optopt;			/* character checked for validity */

int
getopt(int nargc, char **nargv, char *ostr)
{
	register char	*oli;		/* option letter list index */
	static char	*place = "";	/* option letter processing */

	if(!*place) {			/* update scanning pointer */
		if(optind >= nargc || *(place = nargv[optind]) != '-' || !*++place) {
		  place = "";
		  return(EOF);
		}
		if (*place == '-') {	/* found "--" */
		  ++optind;
		  place = "";
		  return EOF;
		}
	}				/* option letter okay? */
	if ((optopt = (int)*place++) == (int)':' || !(oli = strchr(ostr,optopt))) {
		if(!*place) ++optind;
		(void)fprintf(stderr, "%s: illegal option -- %c\n", cmd_name, optopt);
		return '?';
	}
	if (*++oli != ':') {		/* don't need argument */
		optarg = NULL;
		if (!*place) ++optind;
	} else {				/* need an argument */
		if (*place) {			/* no white space */
			optarg = place;
		} else if (nargc <= ++optind) {	/* no arg */
			place = "";
			(void)fprintf(stderr, "%s: option requires an argument -- %c\n", cmd_name, optopt);
			optopt = '?';
		} else {
			optarg = nargv[optind];	/* white space */
		}
		place = "";
		++optind;
	}
	return optopt;			/* dump back option letter */
}
