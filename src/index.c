/*
 * Source file:
 *	index.c
 */

#include "config.h"

#if TM_IN_SYS_TIME
# include <sys/time.h>
#else
# include <time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "trueprint.h"
#include "main.h"
#include "headers.h"
#include "postscript.h"
#include "debug.h"
#include "utils.h"

#include "index.h"

/*
 * Public part
 */

/*
 * Private part
 */
#define LINE		0
#define INPUT_END	1
#define BLOCK_SIZE 1000

typedef struct {
  char	name[SYMBOL_LEN];
  long	name_start_char;
  long	name_end_char;
  long	page_number;
  long	end_page;
  boolean changed;
  char	*filename;
} function_entry;

typedef struct {
  long	starting_page;
  long	ending_page;
  char	*name;
  time_t modified;
} file_entry;

typedef struct {
  boolean changed;
} page_entry;

static void dot_fill(char string[]);
static int compare(const void *, const void *);
static short print_index_line(int print_bold);

static size_t	max_function_name_length = 0;
static size_t	max_file_name_length = 0;

static unsigned short	no_of_functions=0;
static unsigned int	function_list_size = 0;
static unsigned int	file_list_size = 0;
static unsigned int	page_list_size = 0;
static function_entry	*functions = NULL;
static function_entry	*sorted_functions = NULL;
static file_entry	*files = NULL;
static page_entry	*pages = NULL;

static boolean		current_function_changed = FALSE;

static void grow_array(void *list_ptr_ptr, unsigned int *, size_t);

/*****************************************************************************
 * Function:
 *	setup_index
 */
void
setup_index(void)
{
  max_function_name_length = 0;
  max_file_name_length = 0;

  no_of_functions=0;
  function_list_size = 0;
  file_list_size = 0;
  page_list_size = 0;
  functions = NULL;
  sorted_functions = NULL;
  files = NULL;
  pages = NULL;

  current_function_changed = FALSE;
}

/*****************************************************************************
 * Function:
 *	grow_array
 */
static void
grow_array(void *list_ptr_ptr, unsigned int *list_size_ptr, size_t list_entry_size)

{
  size_t	size_needed;

  *list_size_ptr += BLOCK_SIZE;

  size_needed = *list_size_ptr * list_entry_size;

  dm('i',3,"index.c:grow_array() Growing array by %d bytes\n", size_needed);

  if (*(void **)list_ptr_ptr == NULL)
    *(void **)list_ptr_ptr = xmalloc(size_needed);
  else
    *(void **)list_ptr_ptr = xrealloc(*(void **)list_ptr_ptr, size_needed);
}

/*****************************************************************************
 * Function:
 *	page_has_changed
 *
 * Flag this page and function as containing a change
 */
void
page_has_changed(long this_page_number)

{
  if (pass == 1) return;

  while (this_page_number >= page_list_size)
    grow_array(&pages, &page_list_size, sizeof(page_entry));

  current_function_changed = TRUE;
  pages[this_page_number].changed = TRUE;
}

/*****************************************************************************
 * Function:
 *	add_file
 *
 * Remembers the starting page and other info for the named file.
 */
void
add_file(char *filename, unsigned int this_file_number, long this_file_page_number)

{
  size_t length = strlen(filename);
  struct stat stat_buffer;

  if (this_file_number >= file_list_size)
    grow_array(&files, &file_list_size, sizeof(file_entry));

  files[this_file_number].starting_page = this_file_page_number;
  files[this_file_number].name = strdup(filename);

  if (strcmp(filename,"-") != 0) {
    if (stat(filename,&stat_buffer) == -1) {
      perror(CMD_NAME ": cannot stat file");
      exit(1);
    }

    files[this_file_number].modified = stat_buffer.st_mtime;
  } else {
    files[this_file_number].modified = 0;
  }

  if (length > max_file_name_length)
    max_file_name_length = length;
}

/*****************************************************************************
 * Function:
 *	end_file
 *
 * Remembers the ending page for the named file.
 */
void
end_file(unsigned int this_file_number, long this_file_page_number)

{
  if (this_file_number >= file_list_size)
    grow_array(&files, &file_list_size, sizeof(file_entry));

  files[this_file_number].ending_page = this_file_page_number;
}

/*****************************************************************************
 * Function:
 *	get_file_last_page
 *
 * Return the last page number for a file
 */
long
get_file_last_page(unsigned int this_file_number)

{
  return files[this_file_number].ending_page;
}

/*****************************************************************************
 * Function:
 *	get_file_first_page
 *
 * Return the first page number for a file
 */
long
get_file_first_page(unsigned int this_file_number)

{
  return files[this_file_number].starting_page;
}

/*****************************************************************************
 * Function:
 *	get_file_modified_time
 *
 * Return the struct time_t containing the last modified time of the file
 */
struct tm *
get_file_modified_time(unsigned int this_file_number)

{
  if (files[this_file_number].modified == 0)
    return NULL;

  return localtime(&files[this_file_number].modified);
}

/*****************************************************************************
 * Function:
 *	add_function.
 *
 * Remembers the start and end characters, the page number and the filename
 * for the named function.
 */
void
add_function(char *name, long start, long end, long page, char *filename)

{
  size_t length = strlen(name);
  if (pass == 1) return;

  dm('i',2,"index.c:add_function() Adding %s, page %ld filename %s\n",
     name,page,filename);

  if (no_of_functions == function_list_size)
    grow_array(&functions, &function_list_size, sizeof(function_entry));

  strcpy(functions[no_of_functions].name, name);
  functions[no_of_functions].name_start_char = start;
  functions[no_of_functions].name_end_char = end;
  functions[no_of_functions].page_number = page;
  functions[no_of_functions].filename = filename;
  if (length > max_function_name_length)
    max_function_name_length = length;
  current_function_changed = FALSE;
}

/*****************************************************************************
 * Function:
 *	end_function
 *
 * Remembers the end page for the current function, and gets ready for
 * information on the next function.
 */
void
end_function(long page)

{
  if (pass == 1) return;

  dm('i',2,"index.c:end_function() Ending function on page %ld\n",page);

  functions[no_of_functions].changed = current_function_changed;
  functions[no_of_functions++].end_page = page;
}

/*****************************************************************************
 * Function:
 *	get_function_name_posn
 *
 * Looks at current_char and returns whether this character starts a
 * function name, ends a function name, or does neither.
 */
char_status
get_function_name_posn(long current_char, char_status current_status)

{
  static unsigned short	current_function=0;

  if (pass==0) return(current_status);

  /* If there were no functions simply return CHAR */
  if (no_of_functions == 0) return (current_status);

  if ((current_char >= functions[current_function].name_start_char)
      && (current_char <= functions[current_function].name_end_char))
    {
      if (current_char == functions[current_function].name_end_char)
	current_function += 1;
      dm('i',5,"index.c:get_function_name_posn() Returning TRUE for char posn %ld\n", current_char);
      return(CHAR_BOLD);
    }

  return(current_status);
}

/*****************************************************************************
 * Function:
 *	get_function_name
 *
 * Get the current function name for the specified page.
 */
char *
get_function_name(long page)

{
  unsigned short	current_function=0;
  static char	dummy_return[] = "";

  dm('i',4,"Index: Searching for function name for page %ld\n",page);

  if ((pass==0) || (no_of_functions == 0)) return(dummy_return);

  for (;;)
    {
      if (page < functions[current_function].page_number)
	return(dummy_return);
      if (page <= functions[current_function].end_page)
	{
	  dm('i',4,"Index: Function name for page %ld = %s\n",page,functions[current_function].name);
	  return(functions[current_function].name);
	}
      if (current_function >= no_of_functions-1)
	return(dummy_return);
      current_function += 1;
    }
}	

/*****************************************************************************
 * Function:
 *	get_file_name
 *
 * Get the appropriate file name
 */

char *
file_name(int file_number)

{
  return files[file_number].name;
}

/*****************************************************************************
 * Function:
 *	function_changed
 *
 * Has the current function changed?
 */
boolean
function_changed(long page)

{
  static unsigned short	current_function=0;

  if (pass==0) return(FALSE);
  if (no_of_functions == 0) return(FALSE);
  for (;;)
    {
      if (page < functions[current_function].page_number)
	return(FALSE);
      if (page <= functions[current_function].end_page)
	return(functions[current_function].changed);
      if (current_function >= no_of_functions-1)
	return(FALSE);
      current_function += 1;
    }
}	

/*****************************************************************************
 * Function:
 *	page_changed
 *
 * Has the current page changed?
 */
boolean
page_changed(long page)

{
  if (pass == 0) return FALSE;
  if (page > page_list_size) return FALSE;

  return pages[page].changed;
}	

/*****************************************************************************
 * Function:
 *	dot_fill
 *
 * Take a string and fill in the middle set of spaces with dots.
 */
void
dot_fill(char string[])

{
  short	string_index= 0;

  /* Find the end of the first set of spaces */
  while (string[string_index] == ' ') string_index += 1;
  /* Find the start of the second set of spaces */
  while (string[string_index] != ' ') string_index += 1;
  /* Change spaces to dots */
  while (string[string_index] == ' ')
    string[string_index++] = '.';
}

/*****************************************************************************
 * Function:
 *	compare
 *
 * Compares two function entries, used by qsort().
 * Looks at name first, then start char.
 */
int
compare(const void *p1, const void *p2)

{
  int r;
  r = strcmp(((function_entry *)p1)->name,((function_entry *)p2)->name);
  if (r == 0)
    {
      if (((function_entry *)p1)->name_start_char > ((function_entry *)p2)->name_start_char)
	r = 1;
      else
	if (((function_entry *)p1)->name_start_char < ((function_entry *)p2)->name_start_char)
	  r = -1;
	else
	  r = 0;
    }
  return r;
}

/*****************************************************************************
 * Function:
 *	sort_function_names
 *
 * Sort the array of function records into alphabetical order into the
 * array sorted_functions[].
 */
void
sort_function_names(void)

{
  unsigned short every_function;

  size_t size_needed = function_list_size * sizeof(function_entry);

  if (function_list_size == 0) return;

  sorted_functions = xmalloc(size_needed);

  for (every_function=0;every_function<no_of_functions;every_function++)
    sorted_functions[every_function] = functions[every_function];
  qsort((char *)sorted_functions,(int)no_of_functions,sizeof(function_entry),compare);
}

/*****************************************************************************
 * Function:
 *	print_index
 *
 * Print out the contents of sorted_functions in order on a page or pages.
 */
void
print_index(void)

{
  unsigned short function_idx=0;
  long index_page_number=0;
  short output_line_number;
  boolean reached_end_of_sheet;

  /*
   * If there is nothing to print, then print nothing...
   */
  if (no_of_functions == 0) return;

  /*
   * For every page...
   */
  do
    {
      index_page_number += 1;

      /*
       * ...print the header...
       */
      print_index_header(index_page_number);

      output_line_number = 0;
       
      /*
       * ...print the index a line at a time...
       */
      while ((output_line_number < page_length)
	     && (function_idx < no_of_functions))
	{

	  char	output_line[INPUT_LINE_LEN];

	  /*
	   * Every second line has dots in it...
	   */
	  if ((function_idx & 1) == 1)
	    {
	      sprintf(output_line,"          %-24s %4ld  (%s)",
			    sorted_functions[function_idx].name,
			    sorted_functions[function_idx].page_number,
			    sorted_functions[function_idx].filename);
	      dot_fill(output_line); 
	      printf("Lpt(%s) show Nl\n",output_line);
	    }
	  else
	    {
	      sprintf(output_line,"          %-24s %4ld  (%s)\n",
			    sorted_functions[function_idx].name,
			    sorted_functions[function_idx].page_number,
			    sorted_functions[function_idx].filename);
	      printf("Lpt(%s) show Nl\n",output_line);
	    }
	  output_line_number += 1;
	  function_idx += 1;
	}
	
      /*
       * ...and print the footer
       */
      reached_end_of_sheet = PS_endpage(TRUE);

    } while (function_idx < no_of_functions);

  /*
   * Print blank pages until the last page printed was the last page
   * on a physical sheet.
   */
  while (!reached_end_of_sheet)
    {
      index_page_number += 1;
      print_index_header(index_page_number);
      reached_end_of_sheet = PS_endpage(TRUE);
    }
}

/*****************************************************************************
 * Function:
 *	print_index_line
 *
 * Prints out a line of the file index, which is either a filename or
 * a function name within the current file.
 */
short
print_index_line(int print_bold)

{
  static unsigned int	current_file = 0;
  static unsigned short	function_idx = 0;
  static boolean          first_call = TRUE;

  /*
   * If we haven't started printing out functions for a file
   * then we have to print out the filename first and we
   * print it in bold.  This will only happen on the first
   * call to print_index_line().
   */
  if (first_call)
    {
      printf("Lpt(    %-24s %5ld) BF setfont show CF setfont Nl\n",
		   files[current_file].name,
		   files[current_file].starting_page);
      first_call = FALSE;
      return(LINE);
    }

  /*
   * OK, so we need to print out a function name.  Find the next
   * function name belonging to the current file in the sorted
   * array of functions.
   */
  while (function_idx != no_of_functions)
    {
      if (strcmp(files[current_file].name, sorted_functions[function_idx].filename) != 0)
	{
	  /* if no match and not reached end continue with the next function name */
	  function_idx += 1;
	  continue;
	}
      else
	break;
    }
    
  /*
   * If this comparison is true then we didn't find any more functions for this file.
   */
  if (function_idx == no_of_functions)
    {
      /* If reached end of last file return */
      if (++current_file >= no_of_files) return(INPUT_END);
      /*
       * else reached end of this file - print the filename of the next file in
       * bold and set up for starting printing function names for this title
       */
      printf("Lpt(    %-24s %5ld) BF setfont show CF setfont Nl\n",
		   files[current_file].name,
		   files[current_file].starting_page);
      function_idx = 0;
      return(LINE);
    }

  /*
   * If "print_bold" then fill in with dots while printing out
   * function details.
   */
  if (print_bold)
    {
      char	string[INPUT_LINE_LEN];

      sprintf(string,"          %-24s %4ld",
		    sorted_functions[function_idx].name,
		    sorted_functions[function_idx].page_number);
      dot_fill(string);

      printf("Lpt(%s) show Nl\n",string);
    }
  else
    {
      printf("Lpt(          %-24s %4ld) show Nl\n",
		   sorted_functions[function_idx].name,
		   sorted_functions[function_idx].page_number);
    }
  function_idx += 1;
  return(LINE);
}

/*****************************************************************************
 * Function:
 *	print_out_file_index
 *
 * Print out the contents of sorted_functions in order per file.
 */
void
print_out_file_index(void)

{
  long index_page_number=0;
  short output_line_number;
  short retval = LINE;
  boolean reached_end_of_sheet;

  /*
   * For every page...
   */
  do
    {
      index_page_number += 1;

      /*
       * ...print the header...
       */
      print_file_header(index_page_number);

      output_line_number = 0;

      /*
       * ...print all the lines, every second one with dots...
       */
      while (output_line_number < page_length)
	{
	  PUTS("          ");
	  if ((retval = print_index_line(output_line_number &1))
	      == INPUT_END) break;
	  output_line_number += 1;
	}

      /*
       * ...and print the footer...
       */
      reached_end_of_sheet = PS_endpage(TRUE);

    } while (retval == LINE);

  /*
   * Print blank pages until the last page printed was the last page
   * on a physical sheet.
   */
  while (!reached_end_of_sheet)
    {
      index_page_number += 1;
      print_file_header(index_page_number);
      reached_end_of_sheet = PS_endpage(TRUE);
    }
}
