/*
 * Source file:
 *	print_prompt.c
 */

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trueprint.h"
#include "main.h"
#include "index.h"
#include "utils.h"
#include "options.h"
#include "output.h"

#include "print_prompt.h"

/*
 * Public part
 */

char    *print_selection;



/*
 * Private part
 */
static boolean check_selection(long page_no, char *filename);
static boolean no_prompt_to_print;

/*
 * Function:
 *	setup_print_prompter()
 */

void
setup_print_prompter(void)
{
  string_option("A", "print-pages", NULL, &print_selection, NULL, NULL,
		OPT_PRINT,
		"specify list of pages to be printed");

  boolean_option("a", "no-prompt", "prompt", TRUE, &no_prompt_to_print, NULL, NULL,
		 OPT_PRINT,
		 "don't prompt for each page, whether it should be printed or not",
		 "prompt for each page, whether it should be printed or not");

}

/*
 * Function:
 *	print_prompt
 *
 * Returns whether or not this page should be printed.  Uses
 * -a and -A information.
 */
boolean
print_prompt(page_types type, long filepage_no, char *filename)
{
  char *fn_name;
  char prompt[MAXLINELENGTH];
  char response[INPUT_LINE_LEN];
  char *response_ptr;
  boolean retval=UNSET;
  static int yes_count=0;
  static int no_count=0;
  static boolean all_yes=FALSE, all_no=FALSE;

  if (pass == 0) return FALSE;

  if (all_yes) return TRUE;
  if (all_no) return FALSE;

  if (print_selection != NULL) return check_selection(page_number, filename);

  if (yes_count != 0)
    {
      yes_count--;
      return TRUE;
    }
  if (no_count != 0)
    {
      no_count--;
      return FALSE;
    }

  if (no_prompt_to_print==FALSE)
    {
      switch(type)
	{
	case PAGE_BODY:
	  fn_name = get_function_name(page_number);
	  if (*fn_name)
	    sprintf(prompt, "Print page %5ld, page %3ld of %s %s(%s%s)?",
			  page_number, filepage_no, filename, page_changed(page_number)?"[changed]":"", fn_name,
			  function_changed(page_number)?"[changed]":"");
	  else
	    sprintf(prompt, "Print page %5ld, page %3ld of %s %s?",
			  page_number, filepage_no, filename, page_changed(page_number)?"[changed]":"");

	  break;
	case PAGE_BLANK:
	  sprintf(prompt, "Print blank page %5ld?", page_number+1);
	  break;
	case PAGE_SPECIAL:
	  sprintf(prompt, "Print %s?", filename);
	  break;
	default:
	  abort();
	}

      while (retval == UNSET)
	{
	  fprintf(stderr, gettext("%s [n] ? for help: "), prompt);
	  fflush(stderr);
	  fgets(response,INPUT_LINE_LEN-1,stdin);

	  response_ptr = response;
	  skipspaces(&response_ptr);
	  switch (*response_ptr++)
	    {
	    case 'y':
	    case 'Y':
	      yes_count = strtol(response_ptr, &response_ptr, 10);
	      if ((yes_count == 0) && (*response_ptr == '*')) all_yes = TRUE;
	      if (yes_count == 1) yes_count = 0;
	      retval = TRUE;
	      break;
	    case 'n':
	    case 'N':
	      no_count = strtol(response_ptr, &response_ptr, 10);
	      if ((no_count == 0) && (*response_ptr == '*')) all_no = TRUE;
	      if (no_count == 1) no_count = 0;
	      /*FALLTHROUGH*/
	    case '\0':
	      retval = FALSE;
	      break;
	    case 'p':
	    case 'P':
	      print_selection = strdup(response_ptr);
	      retval = check_selection(page_number, filename);
	      break;
	    case '?':
	    default:
	      fprintf(stderr, "---------------------------------------------\n");
	      fprintf(stderr, gettext("y       print this page\n"));
	      fprintf(stderr, gettext("y<n>    print <n> pages\n"));
	      fprintf(stderr, gettext("y*      print all remaining pages\n"));
	      fprintf(stderr, gettext("n       skip this page\n"));
	      fprintf(stderr, gettext("n<n>    skip <n> pages\n"));
	      fprintf(stderr, gettext("n*      skip all remaining pages\n"));
	      fprintf(stderr, gettext("p<list> print all remaining pages that match <list>\n"));
	      fprintf(stderr, gettext("?       show this message\n\n"));
	      fprintf(stderr, gettext("format for <list>:\n"));
	      fprintf(stderr, gettext("    comma-separated list of specifiers:\n"));
	      fprintf(stderr, gettext("    <n>             print page <n>\n"));
	      fprintf(stderr, gettext("    <n>-<o>         print all pages from page <n> to <o>\n"));
	      fprintf(stderr, gettext("    <function-name> print all pages for function\n"));
	      fprintf(stderr, gettext("    f               print function index\n"));
	      fprintf(stderr, gettext("    F               print file index\n"));
	      fprintf(stderr, gettext("    c               print cross reference info\n"));
	      fprintf(stderr, gettext("e.g. p 1-3,main,5,6 will print pages 1,2,3,5,6 and all\n"));
	      fprintf(stderr, gettext("                    pages for function main.\n"));
	      fprintf(stderr, "---------------------------------------------\n");
	    }
	}
      return retval;
    }
  return TRUE;
}

/*
 * Function:
 *	check_selection
 *
 * Returns whether or not this page should be printed, using the -A
 * string.
 */
static boolean
check_selection(long page_no, char *filename)
{
  char *s_index = print_selection;
  long start_page, end_page;
  size_t name_length;
  char *fn_name = NULL;

  if (page_no != 0) fn_name = get_function_name(page_no);

  while (*s_index)
    {
      start_page = strtol(s_index, &s_index, 10);
      skipspaces(&s_index);
      if ((start_page != 0) && (*s_index == '-'))
	{
	  s_index++;
	  end_page = strtol(s_index, &s_index, 10);
	  if ((start_page <= page_no) && (page_no <= end_page)) return TRUE;
	  skipspaces(&s_index);
	}
      if ((start_page != 0) && ((*s_index == ',')||(*s_index == '\0')))
	{
	  if (start_page == page_no) return TRUE;
	}
      if ((start_page == 0) && (*s_index != '\0'))
	{
	  name_length=0;
	  while ((s_index[name_length] != ',')
		 && (s_index[name_length] != '\0')
		 && !isspace(s_index[name_length]))
	    name_length++;
	  if (name_length == 1)
	    {
	      switch (*s_index)
		{
		case 'f':  if (filename && (strcmp(filename, "function index")==0))    return TRUE; break;
		case 'F':  if (filename && (strcmp(filename, "file index")==0))        return TRUE; break;
		case 'd':  if (page_changed(page_no))                    return TRUE; break;
		case 'D':  if (function_changed(page_no))                return TRUE; break;
		default:   fprintf(stderr, gettext(CMD_NAME ": ignoring unrecognized letter %c\n"), *s_index);
		}
	    }
	  if (fn_name
	      && (strlen(fn_name) == name_length)
	      && (strncmp(get_function_name(page_no), s_index, name_length) == 0))
	    return TRUE;

	  s_index += name_length;
	}

      skipspaces(&s_index);
      if (*s_index == ',') s_index++;
      skipspaces(&s_index);
    }

  return FALSE;
}

