/*
 * Source file:
 *	output.h
 */

extern long file_page_number;
extern long page_number;

extern void setup_output(void);
extern void init_output(void);
extern void	print_file(void);
extern boolean	print_page(void);
extern stream_status	getnextline(stream_status (*get_input_char)(char *,char_status *), boolean *, char input_line[], char_status input_status[]);


