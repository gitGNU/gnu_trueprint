/*
 * Header file:
 *	openpipe.h
 */

extern int openpipe(const char *command, char *mode);
extern FILE *fopenpipe(const char *command, char *mode);
extern void closepipe(int handle);
extern void fclosepipe(FILE *fp);
