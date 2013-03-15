/*
 * Source file:
 *	main.c
 *
 * Contains the main program body for trueprint.  The structure of the code
 * is very simple - main() initialises various things, using handle_options()
 * to read options and set up defaults.  Then it calls print_page() until
 * the end of input without printing anything (first pass) to get page numbers
 * & etc. for the indices.  The two indices are then printed, followed by
 * the second pass when the text is printed out by once again calling
 * print_page() until end of input.
 *
 * Exit codes:
 *	0	Normal exit
 *	1	Bad parameters
 *	2	Internal error or resource problem
 */

#include "config.h"

#ifdef MSWIN
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifndef O_RDONLY
# define O_RDONLY 0
#endif

#include <time.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#else
int close(int fildes);
int dup(int fildes);
#endif

#include "trueprint.h"
#include "options.h"
#include "debug.h"
#include "diffs.h"
#include "index.h"
#include "language.h"
#include "postscript.h"
#include "output.h"
#include "openpipe.h"
#include "printers_fl.h"
#include "print_prompt.h"
#include "input.h"
#include "headers.h"
#include "utils.h"

#include "main.h"

#define VERSION "5.4"

/******************************************************************************
 * Public part
 */

unsigned int	no_of_files;
char        	*current_filename;
short		pass;
unsigned int	file_number;
long		total_pages;
int		page_width;
int		page_length;
boolean		use_environment;

/*
 * Private variables
 */
static boolean	no_print_body;
static boolean	no_print_file_index;
static boolean	no_print_function_index;
static char	*destination;
static char	*printer_destination;
static short	no_of_copies;
static char	**file_names;
static char     *output_filename;
static boolean	redirect_output;

/*
 * Functions
 */
int	main(int, char**);
static void setup_main(void);
static void print_files(void);
static void set_dest(const char *p, const char *o, char *value);
static void set_dest_default(char *value);
static void print_help(const char *p, const char *o, char *value);
static void print_version(const char *p, const char *o);

#ifdef LOGTOOL
static void write_log(void);
#else
#define write_log() 
#endif

/******************************************************************************
 * Function:
 *	main
 *
 * Analyse parameters,
 * Sort out font and page size,
 * perform first pass to sort out function names, etc.,
 * sort out output stream (either stdout or x9700|opr)
 * print the index pages,
 * perform second pass, printing out pages.
 */
int main(int argc, char **argv)

{
  unsigned int		last_param_used;
  int			pipe_handle = -1;
#ifdef MSWIN
  FILE			*pipe_fhandle = NULL;
#endif

  /*
   * Need to set this here as it is used by setup_printers_fl()
   */
  use_environment = TRUE;

  /*
   * Set up various modules, including option declaration.
   * setup_options() must come first.
   */
  setup_options();
  setup_debug();
  setup_input();
  setup_diffs();
  setup_headers();
  setup_index();
  setup_language();
  setup_output();
  setup_postscript();
  setup_printers_fl();
  setup_print_prompter();
  setup_main();

  /*
   * Handle command line options.
   */
  /*
   * use_environment is itself used when setting options, so
   * initialize it here.  The default is to use environment stuff,
   * so set it to 1 (true).
   */
  use_environment = 1;

  /*
   * Note that the debug option will not be set before this line - the dm
   * function should not be called before this function call.
   */
  last_param_used = handle_options(argc, argv);

  /*
   * Next check $TP_OPTS to see if the user has customised anything...
   */
  dm('o',1,"Looking at TP_OPTS options\n");
  handle_string_options(getenv("TP_OPTS"));

  /*
   * Write the log file - this doesn't do anything if LOGTOOL
   * is not defined
   */
  write_log();

  /*
   * Set file_names to the first unused parameter - all remaining
   * parameters are source file names.
   */
  dm('P',1,"Checking parameters\n");
  if (last_param_used < (unsigned int)argc)
    {
      file_names = &(argv[last_param_used]);
      no_of_files = (unsigned int)argc - last_param_used;
      dm('P',1,"Got %d file names\n", no_of_files);
    }
  else
    {
      /*
       * No file names - use a list with just "-" for stdin
       */
      static char *stdin_filenames[] =
      { "-", 0 };
      file_names = stdin_filenames;
      no_of_files = 1;
      dm('P',1,"No filenames - reading stdin\n");
    }

  /*
   * End of the user options, so set the default options for
   * the language.
   * If language has been set then this will use it instead of filename.
   */
  dm('l',1,"Setting language for filename %s\n", file_names[0]);
  handle_string_options(language_defaults(file_names[0]));

  /*
   * Now set up the other defaults that haven't been overridden yet.
   * DEFAULT_OPTS used to be the mechanism for setting defaults -
   * now (v3.7) it has been replaced by set_option_defaults(),
   * but DEFAULT_OPTS is kept in case somebody wants to override
   * the default defaults in trueprint.h.  Note that if --ignore-environment
   * is set then the defaults from DEFAULT_OPTS are ignored; otherwise
   * make check would fail if DEFAULT_OPTS is not empty.
   */
  dm('o',1,"Setting default options\n");
  if (use_environment)
    handle_string_options(DEFAULT_OPTS);
  set_option_defaults();

  /*
   * And finally calculate the page dimensions.
   */
  PS_pagesize(destination, &page_width, &page_length);
  dm('O',1,"Page width is  %d, page length is %d\n", page_width, page_length);

  /*
   * If redirect_output is set then calculate the output filename.
   * Do it now so we can catch the error condition that redirect-output
   * is being used with stdin.
   */
   if (redirect_output)
   {
      if (strcmp(file_names[0],"-") == 0)
	{
	  fprintf(stderr, gettext(CMD_NAME ": cannot use redirect-output option with stdin\n"));
	  exit(1);
	}

      /* Grab a buffer long enough for filename plus .ps plus NULL */
      output_filename = xmalloc(strlen(file_names[0])+4);
      strcpy(output_filename,file_names[0]);

      /* Add .ps to the filename */
      {
	char *suffix;

	/* point suffix to the dot, if any, in the filename */
	suffix = strrchr(output_filename,'.');

	/* If there is no . in filename then point suffix to the string end */
	if (suffix == NULL)
	  {
	    suffix = output_filename+strlen(output_filename);
	  }

	/* Now write .ps to the end of the string */
	*(suffix++) = '.';
	*(suffix++) = 'p';
	*(suffix++) = 's';
	*(suffix++) = '\0';
      }

      dm('O',1,"main.c:main() Redirecting output to %s\n",output_filename);
    }

  /*
   * Perform first pass to get function names and locations.
   */
  dm('p', 1, "Starting first pass\n");

  pass = 0;

  init_postscript();

  print_files();

  total_pages = page_number;

  if (!got_some_input)
    {
      fprintf(stderr, gettext(CMD_NAME ": empty input, not submitting print job\n"));
      exit(1);
    }

  dm('p', 1, "Starting second pass\n");

  pass = 1;

  init_postscript();

  /*
   * Now set up output stream to print command.  Put the filehandle into
   * pipe_handle.
   */

  if ((strlen(output_filename) > 0) && (strcmp(output_filename,"-") != 0))
    {
#ifdef MSWIN
      pipe_handle = _creat(output_filename, _S_IREAD | _S_IWRITE );
#else
      pipe_handle = creat(output_filename, 0666);
#endif
      if (pipe_handle == -1)
	{
	  fprintf(stderr, gettext(CMD_NAME ": cannot open %s for writing, %s\n"),
		  output_filename, strerror(errno));
	  exit(1);
	}

      /* now dup the pipe_handle into stdout so all stdout goes to the pipe */
#ifdef MSWIN
      if (!((close(1) == 0) && (_dup(pipe_handle) == 1)))
	abort();
#else
      if (!((close(1) == 0) && (dup(pipe_handle) == 1)))
	abort();
#endif

      dm('O',1,"Sending output to %s\n",output_filename);
    }
#if defined(PRINT_CMD)
  else if (strcmp(output_filename,"-") == 0)
#else
  else
#endif
    {
      /*
       * In the case that the output filename is "-" or there isn't a print
       * command, leave stdout alone so that all output goes simply to stdout.
       */
    }
#if defined(PRINT_CMD)
  else
    {
      char print_cmd_line[200];
      char *tp_print_cmd = getenv("TP_PRINT_CMD");

      dm('P',1,"Setting up pipe for printer\n");

      if ((tp_print_cmd != NULL) && (*tp_print_cmd != '\0'))
	{
	  sprintf(print_cmd_line,"%s", getenv("TP_PRINT_CMD"));
	}
      else
	{
	  sprintf(print_cmd_line,"%s %s %s%d", PRINT_CMD, printer_destination, PRINT_CMD_COUNT_FLAG, no_of_copies);
	}

      dm('P',1,"Printer command is %s\n", print_cmd_line);

#ifdef MSWIN
      pipe_fhandle = _popen(print_cmd_line, "w");
      if (pipe_fhandle == NULL) abort();
	  
      /* now dup the pipe_handle into stdout so all stdout goes to the pipe */
      if (!((close(1) == 0) && (dup(_fileno(pipe_fhandle)) == 1)))
        abort();
#else
      /* openpipe exits on failure, so don't bother to check return code */
      pipe_handle = openpipe(print_cmd_line, "w");

      /* now dup the pipe_handle into stdout so all stdout goes to the pipe */
      if (!((close(1) == 0) && (dup(pipe_handle) == 1)))
	abort();
#endif
    }
#endif /* defined(PRINT_CMD) */

  /*
   * Print out the Postscript header
   * If use_environment is not set, use an old version id
   */
  dm('P',1,"Print out postscript header\n");
  PS_header(use_environment?VERSION:"3.6.5", !no_print_body);

  /*
   * These function calls do precisely what you think they do...
   */
  dm('i',1,"sort function names & print indices if necessary\n");
  sort_function_names();

  if ((no_print_function_index == FALSE)
      && (print_prompt(PAGE_SPECIAL, 0, "function index") == TRUE))
    print_index();

  if ((no_of_files > 1)
      && (no_print_file_index == FALSE)
      && (print_prompt(PAGE_SPECIAL, 0, "file index") == TRUE))
    print_out_file_index();

  /*
   * Now perform second pass to print out listings.
   */
  if (no_print_body == FALSE)
    {

      print_files();
    }

  /*
   * Finish up the postscript output
   */
  PS_end_output();

  fflush(stdout);
  close(1);

#ifdef MSWIN
  if (pipe_fhandle != NULL)
    _pclose(pipe_fhandle);
#else
  if (pipe_handle != -1)
    closepipe(pipe_handle);
#endif

  return(0);
}

/******************************************************************************
 * Function:
 *	print_files
 * Print files one at a time, opening the file and setting the stream
 * for the input routines.  Sets global variables current_filename
 * and file_number.
 */
void
print_files(void)

{
  int stream;
  static int stdin_stream = -1;
  
  init_input();
  init_output();

  for (file_number = 0; file_names[file_number]; file_number++)
    {

      current_filename = file_names[file_number];

      dm('p', 3, "Looking at file %s\n", current_filename);

      if (strcmp(current_filename, "-") == 0)
	{
	  if (pass == 0)
	    {
	      /*
	       * Create a temporary file and copy stdin to it, then use it.
	       * Note that we can only use stdin once.
	       */
	      char input_buffer[BUFFER_SIZE];
	      int buffer_size;

	      if (stdin_stream != -1)
		{
		  fprintf(stderr, gettext(CMD_NAME ": cannot specify stdin twice on command line\n"));
		  exit(1);
		}

	      stdin_stream = fileno(tmpfile());
      
	      while ((buffer_size = read(fileno(stdin),input_buffer,BUFFER_SIZE)) > 0)
		{
		  if (write (stdin_stream, input_buffer, buffer_size) <= 0)
		    {
		      perror(CMD_NAME ": cannot write to tmp file");
		      exit(1);
		    }
		}
	    }

	  if (lseek(stdin_stream,0,SEEK_SET) == (off_t)-1)
	    perror(CMD_NAME ": cannot seek to start of tmp file");

	  stream = stdin_stream;

	}
      else if ((stream =
		open(file_names[file_number], O_RDONLY)) == -1)
	{
	  fprintf(stderr, gettext(CMD_NAME ": cannot open file %s, %s\n"),
			current_filename, strerror(errno));
	  exit(1);
	}

      dm('f',3,"Opened stream %d to read file %s\n",stream,file_names[file_number]);
      if (pass==0)
	{
	  dm('i',3,"Pass file %s to index module\n",current_filename);
	  add_file(current_filename, file_number, page_number+1);
	}

      /*
       * set_input_stream will return FALSE if there is something wrong
       * with the file, e.g. if it is empty.
       */
      if (set_input_stream(stream))
	{

	  dm('d',3,"Init diffs for pass %d, file %s\n", pass, current_filename);
	  init_diffs(current_filename);

	  print_file();

	  dm('d',3,"Ending diffs for pass %d, file %s\n", pass, current_filename);
	  end_diffs();
	}
      else
	{
	  fprintf(stderr, gettext(CMD_NAME ": cannot read %s - possibly an empty file\n"), current_filename);
	}

      /* Close file unless it was tmp file storing stdin */
      if (strcmp(file_names[file_number],"-") != 0)
	{

	  if (close(stream) == -1)
	    {
	      fprintf(stderr, gettext(CMD_NAME ": cannot close %s, %s\n"),
			    file_names[file_number],strerror(errno));
	      exit(1);
	    }
	}

      if (pass==0)
	{
	  dm('i',3,"End file for index module\n");
	  end_file(file_number, page_number);
	}

    }

  if (pass==1)
    {
      /*
       * Print blank pages until the end of the current (last) physical page.
       * The global file_number was left as one more than the last file number
       * by the for loop, so we need to decrement it.
       */
      file_number--;
      fill_sheet_with_blank_pages();
    }

}

/******************************************************************************
 * Function:
 *	setup_main
 * Simply declare options and initialize global variables
 */

void
setup_main(void)
{
  page_width = -1;
  page_length = -1;
  destination=NULL;
  printer_destination = "";
  no_of_copies = -1;

  /*
   * -d and -P have the same meaning - one is from SYSV and the other
   * is from Berkeley
   */
  string_option("d", "printer", "", NULL, &set_dest, &set_dest_default,
		OPT_OUTPUT,
		"use printer <string>");

  string_option("P", "printer", "", NULL, &set_dest, &set_dest_default,
		OPT_OUTPUT,
		"use printer <string>");

  string_option("s", "output", "", &output_filename, NULL, NULL,
		OPT_OUTPUT,
		"send output to filename <string>; use - for stdout");

  boolean_option("r", "redirect-output", "no-redirect-output",
		 FALSE,
		 &redirect_output,
		 NULL, NULL,
		 OPT_OUTPUT,
		 "redirect output to .ps file named after first filename",
		 "don't redirect output");
		 
  short_option("c", "copies", 1,
	       NULL, 0,
	       1, 200, &no_of_copies, NULL, NULL,
		OPT_OUTPUT,
	       "specify number of copies to be printed", NULL);
	
  boolean_option("F", "no-file-index", "file-index", FALSE, &no_print_file_index, NULL, NULL,
		OPT_PRINT,
		 "don't print file index", "print file index");

  boolean_option("f", "no-function-index", "function-index", FALSE, &no_print_function_index, NULL, NULL,
		OPT_PRINT,
		 "don't print function index", "print function index");
	
  optional_string_option("H", "help", &print_help, 
			 OPT_MISC,
			 "Type help information\n"
			 "    --help=all-options - list all options\n"
			 "    --help=misc-options - list miscellaneous options\n"
			 "    --help=page-furniture-options - list page furniture options\n"
			 "    --help=text-format-options - list text formatting options\n"
			 "    --help=print-options - list options that select what to print\n"
			 "    --help=page-format-options - list page format options\n"
			 "    --help=output-options - list options that affect where output goes\n"
			 "    --help=language - list languages\n"
			 "    --help=prompt - format for --print-pages string\n"
			 "    --help=debug - format for --debug string\n"
			 "    --help=header - format for header & footer strings\n"
			 "    --help=report - file format for --language=report input\n"
			 "    --help=environment - list environment vars used"
			 );

  noparm_option("V", "version", FALSE, &print_version, NULL,
		OPT_MISC,
		"Type version information");

  boolean_option("B", "no-print-body", "print-body", FALSE, &no_print_body, NULL, NULL,
		 OPT_PRINT,
		 "don't print body of text", "print body of text");

  boolean_option("N", "use-environment", "ignore-environment", TRUE, &use_environment, NULL, NULL,
		 OPT_MISC,
		 "use environment variables",
		 "don't use values from environment, such as time,\n"
		 "    $USER, etc.  This is for test purposes, to make test results\n"
		 "    more reproducible");

  int_option("w", "line-wrap", -1,
	     "no-line-wrap", 0,
	     -1, 200, &page_width, NULL, NULL,
	     OPT_TEXT_FORMAT,
	     "specify the line-wrap column.",
	     "turn off line-wrap");

  int_option("l", "page-length", -1,
	     NULL, 0,
	     5, 200, &page_length, NULL, NULL,
	     OPT_TEXT_FORMAT,
	     "specify number of lines on a page, point size is\n"
	     "    calculated appropriately",
	     NULL);
}

/******************************************************************************
 * Function:
 *	set_dest_default
 * Handler for -d option to set default
 */
void set_dest_default(char *value)
{
  set_dest(NULL,NULL,value);
}

/******************************************************************************
 * Function:
 *	set_dest
 * Handler for -d option
 */
void set_dest(const char *p, const char *o, char *value)
{
#ifdef PRINT_CMD
  if (destination) return;
  if (*value == '\0')
    {
      destination = getenv("PRINTER");
    }
  else
    {
      destination = strdup(value);
    }

  dm('O',1,"Setting destination to %s\n",destination);

  if (destination && (*destination != '\0'))
    {
      printer_destination = xmalloc(strlen(destination)+strlen(PRINT_CMD_DEST_FLAG)+2);
      sprintf(printer_destination, "%s %s", PRINT_CMD_DEST_FLAG, destination);
    }
  else
    {
      printer_destination = "";
    }
#else
  return;
#endif
}

/******************************************************************************
 * Function:
 *	print_help
 * Handler for -H option
 */
void print_help(const char *p, const char *o, char *value)
{
  if (value)
    {
      printf(gettext("Help for %s:\n"),value);
      if (strcmp(value,"all-options") == 0)
	{
	  print_usage_msgs(OPT_MISC);
	  print_usage_msgs(OPT_PAGE_FURNITURE);
	  print_usage_msgs(OPT_TEXT_FORMAT);
	  print_usage_msgs(OPT_PRINT);
	  print_usage_msgs(OPT_PAGE_FORMAT);
	  print_usage_msgs(OPT_OUTPUT);
	}
      else if (strcmp(value,"misc-options") == 0)
	{
	  print_usage_msgs(OPT_MISC);
	}
      else if (strcmp(value,"page-furniture-options") == 0)
	{
	  print_usage_msgs(OPT_PAGE_FURNITURE);
	}
      else if (strcmp(value,"text-format-options") == 0)
	{
	  print_usage_msgs(OPT_TEXT_FORMAT);
	}
      else if (strcmp(value,"print-options") == 0)
	{
	  print_usage_msgs(OPT_PRINT);
	}
      else if (strcmp(value,"page-format-options") == 0)
	{
	  print_usage_msgs(OPT_PAGE_FORMAT);
	}
      else if (strcmp(value,"output-options") == 0)
	{
	  print_usage_msgs(OPT_OUTPUT);
	}
      else if (strcmp(value,"language") == 0)
	{
	  printf("%s",gettext(language_list));
	}
      else if (strcmp(value,"environment") == 0)
	{
	  printf("%s",
		 gettext(
			 "  Environment variables used by trueprint:\n"
			 "    TP_DIFF_CMD:  command used to generate diffs, devault is diff\n"
			 "    USER:         username for headers, footers, coversheet, etc.\n"
			 "    TP_OPTS:      default trueprint options, overridden by command line opts\n"
			 "    TP_PRINT_CMD: command used to print output, default is lp or lpr\n"
			 "    PRINTER:      name of printer to send output to\n"
			 ));
	}
      else if (strcmp(value,"prompt") == 0)
	{
	  printf("%s",
		 gettext(
			 "--print-pages <pagelist>\n"
			 " Format for <pagelist>:\n"
			 "      <pagelist> is a comma separated list of specifiers.  A specifier can be\n"
			 "      a single page number, a range of page numbers, a function name, or a\n"
			 "      special letter:\n"
			 "      A range of pages is specified by the lower number, then a dash, then the\n"
			 "      upper number\n"
			 "      A special letter can be d, D, f, F or c for\n"
			 "	changed pages, changed functions, function index, file index or cross\n"
			 "      reference information respectively - note that the page will only be printed\n"
			 "      if it would have been printed without the -A option\n"
			 "      For example: trueprint --print-pages 1-5,main,f main.c\n"
			 "           will print out pages 1-5, all pages for function main, and the function index\n"));
	}
      else if (strcmp(value,"header") == 0)
	{
	  printf("%s",
		 gettext(
			 " Format for <string>:\n"
			 "	<string> is used to specify header and footer contents, and can contain the\n"
			 "	following sequences:\n"
			 "	%% a percent character\n"
			 "	%m month of year\n"
			 "	%d day of month\n"
			 "	%y year e.g. 1993\n"
			 "	%D date mm/dd/yy\n"
			 "	%L long date e.g. Fri Oct 8 11:49:51 1993\n"
			 "	%c file modification date mm/dd/yy\n"
			 "	%C file modification date in long format\n"
			 "	%H hour\n"
			 "	%M minute\n"
			 "	%S second\n"
			 "	%T time HH:MM:SS\n"
			 "	%j day of year ddd\n"
			 "	%w day of week (Sunday = 0)\n"
			 "	%a abbreviated weekday\n"
			 "	%h abbreviated month\n"
			 "	%r time in am/pm notation\n"
			 "	%p page number in current file\n"
			 "	%P overall page number\n"
			 "	%f number of pages in current file\n"
			 "	%F final overall page number\n"
			 "	%n current filename\n"
			 "	%N current functionname\n"
			 "	%l login name\n"));
	}
      else if (strcmp(value,"debug") == 0)
	{
	  printf("%s",
		 gettext(
			 " Format for <debug-string>:	series of char/digit pairs where char\n"
			 "	specifies information, digit specifies level of detail, in\n"
			 "	general 1=stuff that happens once, 2=per-pass, 3=per-file,\n"
			 "	4=per-page, 8=per-line - but this is not fully implemented...\n"
			 "	Current chars are: o=options, i=index, O=output, P=parameters\n"
			 "	l=language, p=pass, h=headers/footers, I=input,\n"
			 "	d=diffs, f=file/stream handling, D=destination/printer\n"
			 "	@=all of the above\n"));
	}
      else if (strcmp(value,"report") == 0)
	{
	  printf("%s",
		 gettext(
			 " Format for files when using --language=report:\n"
			 "   Strings between ^B amd ^E are printed in bold and\n"
			 "   are indexed as function names.\n"
			 "   Strings between ^C and ^C are printed in italics\n"
		 ));
	}
      else
	{
	  printf(gettext(CMD_NAME ": Unrecognized help option:%s\nUse --help for valid options\n"),value);
	}
    }
  else
    {
      printf("%s",
	     gettext(
		     "Usage: trueprint <options> <filenames>\n"
		     "For details see:\n"
		     "    --help=all-options - list all options\n"
		     "    --help=misc-options - list miscellaneous options\n"
		     "    --help=page-furniture-options - list page furniture options\n"
		     "    --help=text-format-options - list text formatting options\n"
		     "    --help=print-options - list options that select what to print\n"
		     "    --help=page-format-options - list page format options\n"
		     "    --help=output-options - list options that affect where output goes\n"
		     "    --help=language - list languages\n"
		     "    --help=prompt - format for --print-pages string\n"
		     "    --help=debug - format for --debug string\n"
		     "    --help=header - format for header & footer strings\n"
		     "    --help=report - file format for --language=report input\n"
		     "    --help=environment - list environment vars used\n"
		     ));
    }

  printf(gettext("Report bugs to bug-trueprint@gnu.org\n"));
  exit(0);
}

/******************************************************************************
 * Function:
 *	print_version
 * Handler for -V option
 */
void print_version(const char *p, const char *o)
{
  printf("GNU Trueprint %s\n", VERSION);
  printf(gettext(
		 "Copyright (C) 2013 Free Software Foundation, Inc.\n"
		 "GNU Trueprint comes with NO WARRANTY,\n"
		 "to the extent permitted by law.\n"
		 "You may redistribute copies of GNU Trueprint\n"
		 "under the terms of the GNU General Public License.\n"
		 "For more information about these matters,\n"
		 "see the file named COPYING.\n"
		 ));

  exit(0);
}

#ifdef LOGTOOL
/******************************************************************************
 * Function:
 *	write_log
 */

void
write_log(void)

{
  char *log_cmd;
  size_t log_cmd_length=COMMAND_LEN;
  char *tp_opts;
  int i;

  if (use_environment != TRUE) return;

  log_cmd = xmalloc(log_cmd_length);

  log_cmd[0] = '\0';
  tp_opts = getenv("TP_OPTS");
	    
  sprintf(log_cmd,"%s %s", LOGTOOL, "trueprint");
  if (tp_opts != NULL) 
    sprintf(log_cmd, "%s %s%s%s", log_cmd, "TPOPTS=\"<",tp_opts,">\"");
		
  for (i=1; i<argc; i++)
    {
      if ( (strlen(log_cmd) + 6 + strlen(argv[i])) > log_cmd_length )
	{
	  log_cmd_length += COMMAND_LEN;
	  log_cmd = xmalloc(log_cmd_length)
	}

      sprintf(log_cmd, "%s \" <%s>\"", log_cmd, argv[i]);
    }
  system(log_cmd);
}
#endif
