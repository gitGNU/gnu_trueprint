/*
 * Include file:
 *	index.h
 */

extern void setup_index(void);
extern void add_file(char *filename, unsigned int this_file_number, long this_file_page_number);
extern void end_file(unsigned int this_file_number, long this_file_page_number);
extern void add_function(char *name, long start, long end, long page, char *filename);
extern void end_function(long page);
extern char_status get_function_name_posn(long current_char, char_status status);
extern char *get_function_name(long page);
extern char *file_name(int file_number);
extern long get_file_last_page(unsigned int this_file_number);
extern long get_file_first_page(unsigned int this_file_number);
extern void sort_function_names(void);
extern void print_index(void);
extern void print_out_file_index(void);
extern void page_has_changed(long);
extern boolean function_changed(long);
extern boolean page_changed(long);
