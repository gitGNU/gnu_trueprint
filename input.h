/*
 * Source file:
 *	input.h
 */

extern long		char_number;
extern int		got_some_input;

extern boolean set_input_stream(int);
extern stream_status	getnextchar(char *);
extern void ungetnextchar(char, stream_status);
extern void setup_input(void);
extern void init_input(void);
