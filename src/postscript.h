/*
 * Include file:
 *	postscript.h
 */

extern boolean	no_show_indent_level;
extern boolean no_show_line_number;

extern void setup_postscript(void);
extern void init_postscript(void);
extern void PS_header(char *, boolean);
extern boolean PS_endpage(boolean print_page);
extern void PS_startpage(char *h1, char *h2, char *h3, char *f1, char *f2, char *f3, char *message, long page_no,long hdr_total_pages,boolean index_page);
extern void PS_end_output(void);
extern void PS_pagesize(char *printer, int *width_ptr, int *length_ptr);
