/*
 * Include file:
 *	print_prompter.h
 */

typedef enum {
  PAGE_SPECIAL,
  PAGE_BLANK,
  PAGE_BODY
} page_types;

extern void setup_print_prompter(void);
extern boolean print_prompt(page_types type, long filepage_no, char *file);
extern void skipspaces(char **);

