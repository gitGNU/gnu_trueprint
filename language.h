/*
 * Header file:
 *	language.h
 */

typedef stream_status	(*get_char_fn)(char *, char_status *);

extern get_char_fn	get_char;
extern boolean restart_language;
extern short		braces_depth;
extern char *language_list;

extern void setup_language(void);
extern char *language_defaults(char *);
extern void set_get_char(char *);
