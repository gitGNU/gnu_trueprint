/*
 * Source file:
 *	headers.c
 *
 * Prints page headers.  All functions are just front-ends to PS_startpage().
 */

#include "config.h"

#include <stdio.h>

#include "trueprint.h"
#include "postscript.h"
#include "options.h"
#include "headers.h"

static char	*top_left_string;
static char	*top_centre_string;
static char	*top_right_string;
static char	*bottom_left_string;
static char	*bottom_centre_string;
static char	*bottom_right_string;
static char	*message_string;

/*
 * Function:
 *	setup_headers
 */
void
setup_headers(void)
{
  top_left_string = NULL;
  string_option("X", "left-header", "%L", &top_left_string, NULL, NULL,
		OPT_PAGE_FURNITURE,
		"specify string for left side of header");

  bottom_left_string = NULL;
  string_option("x", "left-footer", "%L", &bottom_left_string, NULL,  NULL,
		OPT_PAGE_FURNITURE,
		"specify string for left side of footer");

  top_centre_string = NULL;
  string_option("Y", "center-header", "%N", &top_centre_string, NULL, NULL,
		OPT_PAGE_FURNITURE,
		"specify string for center of header");
  
  bottom_centre_string = NULL;
  string_option("y", "center-footer", "%n %p", &bottom_centre_string, NULL,  NULL,
		OPT_PAGE_FURNITURE,
		"specify string for center of footer");
  
  top_right_string = NULL;
  string_option("Z", "right-header", "Page %P of %F", &top_right_string, NULL,  NULL,
		OPT_PAGE_FURNITURE,
		"specify string for right side of header");
  
  bottom_right_string = NULL;
  string_option("z", "right-footer", "Page %P of %F", &bottom_right_string, NULL, NULL,
		OPT_PAGE_FURNITURE,
		"specify string for right side of footer");
  
  message_string = NULL;
  string_option("m", "message", NULL, &message_string, NULL, NULL,
		OPT_PAGE_FURNITURE,
		"message to be printed over page");
}

/*
 * Function:
 *	print_text_header
 */
void
print_text_header(long page_number, long total_pages)
{
  PS_startpage(top_left_string, top_centre_string, top_right_string,
	       bottom_left_string, bottom_centre_string, bottom_right_string,
	       message_string, page_number, total_pages, FALSE);
}

/*
 * Function:
 *	print_file_header
 */
void
print_file_header(long page_no)
{
  char page_no_string[10];

  sprintf(page_no_string, "Page %ld", page_no);

  PS_startpage("%L", "File Index", page_no_string,
	       "%L", "File Index", page_no_string,
	       message_string, page_no, 0, TRUE);
}

/*
 * Function:
 *	print_index_header
 */
void
print_index_header(long page_no)
{
  char page_no_string[10];

  sprintf(page_no_string, "Page %ld", page_no);

  PS_startpage("%L", "Function Index", page_no_string,
	       "%L", "Function Index", page_no_string,
	       message_string, page_no, 0, TRUE);
}







