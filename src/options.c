/*
 * Source file:
 *       options.c
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "getopt.h"
#endif

extern int optind;  /* undocumented variable used to reset getopt() */

#include "trueprint.h" /* needed for boolean type */
#include "utils.h" /* needed for strdup */
#include "debug.h" /* needed for dm macro */

#include "options.h"

/******************************************************************************
 * Macros
 */

/* If there are more than MAX_OPTIONS options then this must be increased */
#define MAX_OPTIONS 256

/* This must specify a bit outside the range of MAX_OPTIONS */
#define OPT_FLAG 0x1000

#define SHORT_OPTION(opt) (short_options[('A'<=opt&&opt<='Z')?(opt-'A')*2:\
                      ('a'<=opt&&opt<='z')?(opt-'a')*2+1:\
                      ('0'<=opt&&opt<='9')?opt-'0'+52:\
                      62])

#define PRINTUSAGE printf(gettext(" Usage: trueprint <options> <filename>\n   Use -H for details\n"))

/******************************************************************************
 * Typedefs
 */

typedef enum {
  NOPARM,
  OPTIONAL,
  BOOLEAN,
  CHAR,
  CHOICE,
  SHORT,
  INT,
  STRING,
  FLAG_STRING
} option_types;

typedef struct option_type{
  char		letter;
  option_types	type;
  boolean	set;
  char		need_string;
  option_class	class;
  union {
    struct {
      char *string;
      boolean default_opt;
      void (*handler)(const char *p, const char *s);
      void (*set_default)(void);
      char *help_string;
    } onoparm;
    struct {
      char *string;
      void (*handler)(const char *p, const char *s, char *value);
      char *help_string;
    } ooptional;
    struct {
      char *true_string;
      char *false_string;
      boolean *var;
      boolean default_value;
      void (*handler)(const char *p, const char *s, boolean value);
      void (*set_default)(boolean value);
      char *true_help_string;
      char *false_help_string;
    } obool;
    struct {
      char *string;
      char *var;
      char default_value;
      char *valid_set;
      void (*handler)(const char *p, const char *s, char value, char *valid_set);
      void (*set_default)(char value);
      char *help_string;
    } ochar;
    struct {
      char *var;
      char *choice1_string;
      char *choice2_string;
      char choice1; /* Also default */
      char choice2;
      void (*handler)(const char *p, const char *s, char value);
      void (*set_default)(char value);
      char *choice1_help_string;
      char *choice2_help_string;
    } ochoice;
    struct {
      char *string;
      short *var;
      short default_value;
      char *special_string;
      short special_value;
      short  min;
      short  max;
      void (*handler)(const char *p, const char *s, short value, short min, short max);
      void (*set_default)(short value);
      char *help_string;
      char *special_help_string;
    } oshrt;
    struct {
      char *string;
      int *var;
      int default_value;
      char *special_string;
      short special_value;
      int  min;
      int  max;
      void (*handler)(const char *p, const char *s, int value, int min, int max);
      void (*set_default)(int value);
      char *help_string;
      char *special_help_string;
    } oint;
    struct {
      char *string;
      char **var;
      char *default_value;
      void (*handler)(const char *p, const char *s, char *value);
      void (*set_default)(char *value);
      char *help_string;
    } ostrng;
    struct {
      char *set_string;
      char *not_set_string;
      boolean flag_var;
      char **var;
      boolean default_value;
      char *true_value;
      char *false_value;
      void (*handler)(const char *p, const char *s, boolean value, char *true_value, char *false_value);
      void (*set_default)(boolean value, char *string_value);
      char *set_help_string;
      char *not_set_help_string;
    } oflg;
  } t;
} option_type;

/******************************************************************************
 * Private functions
 */
static void set_option(int option, char *prefix, const char *option_name, char *value);
static void set_option_default(int option);

/******************************************************************************
 * Constant variables and data structures
 */

static int short_options[62];

static struct option_type option_list[MAX_OPTIONS+1];
static struct option long_options[MAX_OPTIONS+1];
static int next_option;
static int next_long_option;
static int this_option;

/******************************************************************************
 * Function:
 *	setup_options
 */
void
setup_options(void)
{
  next_option = 0;
  next_long_option = 0;
}

/******************************************************************************
 * Function:
 *	handle_string_options
 *
 * This function takes options as a single string, parses them into
 * a char[][], and calls handle_options.
 */
void
handle_string_options(char *options)

{
  int	opt_argc = -1;
  char	arguments[100][100];
  char	*opt_argv[100];
  int	options_index = -1;
  int	argv_index = 0;
  boolean	quoted = FALSE;
  boolean ended = FALSE;

  if (options == (char *)0) return;
  if (strlen(options) == 0) return;

  while (ended == FALSE)
    {
      switch (options[++options_index])
	{
	case ' ':
	case '	':
	  if (quoted)
	    {
	      if (argv_index == 0) opt_argc += 1;
	      arguments[opt_argc][argv_index++] = options[options_index];
	      continue;
	    }
	  if (argv_index > 0)
	    {
	      arguments[opt_argc][argv_index] = '\0';
	      argv_index = 0;
	    }
	  break;

	case '\0':
	  if (argv_index > 0)
	    {
	      arguments[opt_argc][argv_index] = '\0';
	      argv_index = 0;
	    }
	  ended = TRUE;
	  break;

	case '"':
	  if (quoted) quoted = FALSE;
	  else
	    quoted = TRUE;
	  break;

	default:
	  if (argv_index == 0) opt_argc += 1;
	  arguments[opt_argc][argv_index++] = options[options_index];
	  break;
	}
    }
  opt_argc += 1;

  for (argv_index=0; argv_index < opt_argc; argv_index++)
    opt_argv[argv_index+1] = arguments[argv_index];

  opt_argv[0] = opt_argv[1];
  opt_argc += 1;

  handle_options(opt_argc, opt_argv);
  return;
}

/******************************************************************************
 * Function:
 *	handle_options
 *
 * This function handles the options passed to the program, and also
 * the defaults for each language.  Since the options passed to the
 * program gake priority and are also found first, this function
 * will only assign a value to a flag if the flag has not previously
 * been set.
 */
unsigned int
handle_options(int argc, char **argv)

{
  int option;
  int option_index;
  int short_option_index = 0;
  int long_option_index;
  char short_option_list[125]; /* 125 = (26+26+10)*2 + 1 */

  /*
   * Since this function will be called twice, we need to initialise
   * getopt_long() variables to make sure that it behaves properly during
   * the second call.
   */
  optind = 1;

  /*
   * Set up the string of single-letter options and set the last long_option
   * to all zeros - this will be done multiple times each time trueprint
   * is invoked, but it's quick so it shouldn't hurt.
   */
  for (option_index = 0; option_index < next_long_option; option_index++)
    {
      if (option_list[option_index].letter)
	{
	  short_option_list[short_option_index++] = option_list[option_index].letter;
	  if (option_list[option_index].need_string)
	    short_option_list[short_option_index++] = ':';
	}
    }

  long_options[next_long_option].name = 0;
  long_options[next_long_option].has_arg = 0;
  long_options[next_long_option].flag = 0;
  long_options[next_long_option].val = 0;

  /*
   * Loop through the options.  This call will set this_option
   * to the option number if it is looking at a long option.
   */
  while ((option = 
	  getopt_long((unsigned int)argc, argv,
		      short_option_list, long_options, &long_option_index)
	  ) != EOF)
    {
      if (option == '?')
	{
	  fprintf(stderr, gettext(CMD_NAME ": failed to parse options\n"));
	  exit(1);
	}
      else if (option != 0)
	{
	  char *option_name;
	  option_name = xmalloc(2);
	  option_name[0] = option;
	  option_name[1] = '\0';
	  option_index = SHORT_OPTION(option);
	  set_option(option_index,"-",option_name,optarg);
	}
      else
	{
	  set_option(this_option, "--", long_options[long_option_index].name,optarg);
	}
    }
  return(optind);
}

/******************************************************************************
 * Option declaration functions
 */

/******************************************************************************
 * noparm_option
 * -c
 * --s
 * Calls handler if used.
 * Uses either single-letter option or string long option or both
 */
void noparm_option(char *c, char *s,
		   boolean default_opt,
		   void (*handler)(const char *p, const char *s),
		   void (*set_default)(void),
		   option_class class,
		   char *help_string)
{
  int option_index = next_option++;

  if (s)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = s;
      long_options[long_option_index].has_arg = 0;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index;
    }

  /*
   * This checks for an internal error, so it is OK to
   * risk an out-of-bounds access
   * before checking for the bounds limit.
   */
  if ((next_option > MAX_OPTIONS) || (next_long_option > MAX_OPTIONS))
    abort();

  if (handler == NULL)
    abort();

  if (default_opt && set_default == NULL)
    abort();

  if (c)
    {
      option_list[option_index].letter = *c;
      SHORT_OPTION(*c) = option_index;
    }
  else
    option_list[option_index].letter = 0;

  option_list[option_index].type = NOPARM;
  option_list[option_index].class = class;
  option_list[option_index].need_string = FALSE;
  option_list[option_index].set = FALSE;
  option_list[option_index].t.onoparm.string = s;
  option_list[option_index].t.onoparm.default_opt = default_opt;
  option_list[option_index].t.onoparm.handler = handler;
  option_list[option_index].t.onoparm.set_default = set_default;
  option_list[option_index].t.onoparm.help_string = help_string;
}

/******************************************************************************
 * optional_string_option
 * -c
 * --s
 * --s=[string]
 * Note that only the long option has the optional string.
 * Calls handler if used.
 * Uses either single-letter option or string long option or both
 */
void optional_string_option(char *c, char *s,
			    void (*handler)(const char *p, const char *s, char *value),
			    option_class class,
			    char *help_string)
{
  int option_index = next_option++;

  if (s)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = s;
      long_options[long_option_index].has_arg = 2;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index;
    }

  /*
   * This checks for an internal error, so it is OK to
   * risk an out-of-bounds access
   * before checking for the bounds limit.
   */
  if ((next_option > MAX_OPTIONS) || (next_long_option > MAX_OPTIONS))
    abort();

  if (handler == NULL)
    abort();

  if (c)
    {
      option_list[option_index].letter = *c;
      SHORT_OPTION(*c) = option_index;
    }
  else
    option_list[option_index].letter = 0;

  option_list[option_index].type = OPTIONAL;
  option_list[option_index].class = class;
  option_list[option_index].need_string = FALSE;
  option_list[option_index].set = FALSE;
  option_list[option_index].t.ooptional.string = s;
  option_list[option_index].t.ooptional.handler = handler;
  option_list[option_index].t.ooptional.help_string = help_string;
}

/******************************************************************************
 * boolean_option
 * -c = set option to TRUE
 * --c = set option to FALSE
 * --s1 = set option to TRUE
 * --s2 = set option to FALSE
 */
void boolean_option(char *c, char *s1, char *s2,
		    boolean default_value,
		    boolean *var,
		    void (*handler)(const char *p, const char *s, boolean value),
		    void (*set_default)(boolean value),
		    option_class class,
		    char *true_help_string,
		    char *false_help_string)
{
  int option_index = next_option++;

  if (c)
    {
      int long_option_index = next_long_option++;
      option_list[option_index].letter = *c;
      SHORT_OPTION(*c) = option_index;
      long_options[long_option_index].name = c;
      long_options[long_option_index].has_arg = 0;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index | OPT_FLAG;
    }
  else
    option_list[option_index].letter = 0;

  if (s1)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = s1;
      long_options[long_option_index].has_arg = 0;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index;
    }
  if (s2)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = s2;
      long_options[long_option_index].has_arg = 0;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index | OPT_FLAG;
    }

  if ((next_option > MAX_OPTIONS) || (next_long_option > MAX_OPTIONS))
    abort();
  if ((var == NULL) && (handler == NULL)) 
    abort();
  if ((var != NULL) && (handler != NULL))
    abort();
  if ((handler == NULL) != (set_default == NULL))
    abort();

  option_list[option_index].type = BOOLEAN;
  option_list[option_index].class = class;
  option_list[option_index].need_string = FALSE;
  option_list[option_index].set = FALSE;
  option_list[option_index].t.obool.true_string = s1;
  option_list[option_index].t.obool.false_string = s2;
  option_list[option_index].t.obool.var = var;
  option_list[option_index].t.obool.default_value = default_value;
  option_list[option_index].t.obool.handler = handler;
  option_list[option_index].t.obool.set_default = set_default;
  option_list[option_index].t.obool.true_help_string = true_help_string;
  option_list[option_index].t.obool.false_help_string = false_help_string;
}

/******************************************************************************
 * choice_option
 * -c <letter>
 * --s1
 * --s2
 */
void choice_option(char *c, char *s1, char *s2,
		   char choice1, char choice2,
		   char *var,
		   void (*handler)(const char *p, const char *s, char value),
		   void (*set_default)(char value),
		   option_class class,
		   char *choice1_help_string,
		   char *choice2_help_string)
{
  int option_index = next_option++;

  if (c)
    {
      option_list[option_index].letter = *c;
      SHORT_OPTION(*c) = option_index;
    }
  else
    option_list[option_index].letter = 0;

  if (s1)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = s1;
      long_options[long_option_index].has_arg = 0;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index;
    }

  if (s2)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = s2;
      long_options[long_option_index].has_arg = 0;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index | OPT_FLAG;
    }

  if ((next_option > MAX_OPTIONS) || (next_long_option > MAX_OPTIONS))
    abort();
  if ((var == NULL) && (handler == NULL))
    abort();
  if ((var != NULL) && (handler != NULL))
    abort();
  if ((handler == NULL) != (set_default == NULL))
    abort();

  option_list[option_index].type = CHOICE;
  option_list[option_index].class = class;
  option_list[option_index].need_string = TRUE;
  option_list[option_index].set = FALSE;
  option_list[option_index].t.ochoice.var = var;
  option_list[option_index].t.ochoice.choice1_string = s1;
  option_list[option_index].t.ochoice.choice2_string = s2;
  option_list[option_index].t.ochoice.choice1 = choice1;
  option_list[option_index].t.ochoice.choice2 = choice2;
  option_list[option_index].t.ochoice.handler = handler;
  option_list[option_index].t.ochoice.set_default = set_default;
  option_list[option_index].t.ochoice.choice1_help_string = choice1_help_string;
  option_list[option_index].t.ochoice.choice2_help_string = choice2_help_string;
}

/******************************************************************************
 * char_option
 * -c <letter>
 * --s <letter>
 */
void char_option(char *c, char *s,
		 char default_value,
		 char *valid_set,
		 char *var,
		 void (*handler)(const char *p, const char *s, char value, char *var),
		 void (*set_default)(char value),
		 option_class class,
		 char *help_string)
{
  int option_index = next_option++;

  if (c)
    {
      option_list[option_index].letter = *c;
      SHORT_OPTION(*c) = option_index;
    }
  else
    option_list[option_index].letter = 0;

  if (s)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = s;
      long_options[long_option_index].has_arg = 1;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index;
    }

  if ((next_option > MAX_OPTIONS) || (next_long_option > MAX_OPTIONS))
    abort();
  if ((var == NULL) && (handler == NULL))
    abort();
  if ((var != NULL) && (handler != NULL))
    abort();
  if ((handler == NULL) != (set_default == NULL))
    abort();

  option_list[option_index].type = CHAR;
  option_list[option_index].class = class;
  option_list[option_index].need_string = TRUE;
  option_list[option_index].set = FALSE;
  option_list[option_index].t.ochar.string = s;
  option_list[option_index].t.ochar.var = var;
  option_list[option_index].t.ochar.default_value = default_value;
  option_list[option_index].t.ochar.valid_set = valid_set;
  option_list[option_index].t.ochar.handler = handler;
  option_list[option_index].t.ochar.set_default = set_default;
  option_list[option_index].t.ochar.help_string = help_string;
}

/******************************************************************************
 * short_option
 * -c <short>
 * --s <short>
 */
void short_option(char *c, char *s, short default_value,
		  char *special_string, short special_value,
		  short min, short max, 
		  short *var, 
		  void (*handler)(const char *p, const char *s, short value, short min, short max),
		  void (*set_default)(short value),
		  option_class class,
		  char *help_string,
		  char *special_help_string)
{
  int option_index = next_option++;

  if (c)
    {
      option_list[option_index].letter = *c;
      SHORT_OPTION(*c) = option_index;
    }
  else
    option_list[option_index].letter = 0;

  if (s)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = s;
      long_options[long_option_index].has_arg = 1;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index;
    }

  if (special_string)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = special_string;
      long_options[long_option_index].has_arg = 0;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index | OPT_FLAG;
    }

  if ((next_option > MAX_OPTIONS) || (next_long_option > MAX_OPTIONS))
    abort();
  if ((var == NULL) && (handler == NULL))
    abort();
  if ((var != NULL) && (handler != NULL))
    abort();
  if ((handler == NULL) != (set_default == NULL))
    abort();

  option_list[option_index].type = SHORT;
  option_list[option_index].class = class;
  option_list[option_index].need_string = TRUE;
  option_list[option_index].set = FALSE;
  option_list[option_index].t.oshrt.string = s;
  option_list[option_index].t.oshrt.var = var;
  option_list[option_index].t.oshrt.min = min;
  option_list[option_index].t.oshrt.max = max; 
  option_list[option_index].t.oshrt.default_value = default_value;
  option_list[option_index].t.oshrt.handler = handler;
  option_list[option_index].t.oshrt.set_default = set_default;
  option_list[option_index].t.oshrt.help_string = help_string;
  option_list[option_index].t.oshrt.special_string = special_string;
  option_list[option_index].t.oshrt.special_value = special_value;
  option_list[option_index].t.oshrt.special_help_string = special_help_string;
}

/******************************************************************************
 * int_option
 * -c <short>
 * --s <short>
 */

void int_option(char *c, char *s, int default_value,
		char *special_string, int special_value,
		int min, int max,
		int *var, 
		void (*handler)(const char *p, const char *s, int value, int min, int max),
		void (*set_default)(int value),
		option_class class,
		char *help_string,
		char *special_help_string)
{
  int option_index = next_option++;

  if (c)
    {
      option_list[option_index].letter = *c;
      SHORT_OPTION(*c) = option_index;
    }
  else
    option_list[option_index].letter = 0;

  if (s)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = s;
      long_options[long_option_index].has_arg = 1;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index;
    }

  if (special_string)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = special_string;
      long_options[long_option_index].has_arg = 0;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index | OPT_FLAG;
    }

  if ((next_option > MAX_OPTIONS) || (next_long_option > MAX_OPTIONS))
    abort();
  if ((var == NULL) && (handler == NULL))
    abort();
  if ((var != NULL) && (handler != NULL))
    abort();
  if ((handler == NULL) != (set_default == NULL))
    abort();

  option_list[option_index].type = INT;
  option_list[option_index].class = class;
  option_list[option_index].need_string = TRUE;
  option_list[option_index].set = FALSE;
  option_list[option_index].t.oint.string = s;
  option_list[option_index].t.oint.var = var;
  option_list[option_index].t.oint.min = min;
  option_list[option_index].t.oint.max = max; 
  option_list[option_index].t.oint.default_value = default_value;
  option_list[option_index].t.oint.handler = handler;
  option_list[option_index].t.oint.set_default = set_default;
  option_list[option_index].t.oint.help_string = help_string;
  option_list[option_index].t.oint.special_string = special_string;
  option_list[option_index].t.oint.special_value = special_value;
  option_list[option_index].t.oint.special_help_string = special_help_string;
}

/******************************************************************************
 * string_option
 * -c <string>
 * --s <string>
 */
void string_option(char *c, char *s,
		   char *default_value,
		   char **var,
		   void (*handler)(const char *p, const char *s, char *value),
		   void (*set_default)(char *value),
		   option_class class,
		   char *help_string)
{
  int option_index = next_option++;

  if (c)
    {
      option_list[option_index].letter = *c;
      SHORT_OPTION(*c) = option_index;
    }
  else
    option_list[option_index].letter = 0;

  if (s)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = s;
      long_options[long_option_index].has_arg = 1;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index;
    }

  if ((var == NULL) && (handler == NULL))
    abort();
  if ((var != NULL) && (handler != NULL))
    abort();
  if ((handler == NULL) != (set_default == NULL))
    abort();

  option_list[option_index].type = STRING;
  option_list[option_index].class = class;
  option_list[option_index].need_string = TRUE;
  option_list[option_index].set = FALSE;
  option_list[option_index].t.ostrng.string = s;
  option_list[option_index].t.ostrng.var = var;
  option_list[option_index].t.ostrng.default_value = default_value;
  option_list[option_index].t.ostrng.handler = handler;
  option_list[option_index].t.ostrng.set_default = set_default;
  option_list[option_index].t.ostrng.help_string = help_string;
}

/******************************************************************************
 * flag_string_option
 * -c <string>
 * --c
 * -s1 <string>
 * --s2
 */
void flag_string_option(char *c, char *s1, char *s2,
			boolean default_value,
			char *true_value, char *false_value,
			char **var,
			void (*handler)(const char *p, const char *s, boolean value, char *true_value, char *false_value),
			void (*set_default)(boolean value, char *string_value),
			option_class class,
			char *set_help_string,
			char *not_set_help_string)
{
  int option_index = next_option++;

  if (c)
    {
      option_list[option_index].letter = *c;
      SHORT_OPTION(*c) = option_index;
    }
  else
    option_list[option_index].letter = 0;

  if (s1)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = s1;
      long_options[long_option_index].has_arg = 1;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index;
    }

  if (s2)
    {
      int long_option_index = next_long_option++;
      long_options[long_option_index].name = s2;
      long_options[long_option_index].has_arg = 0;
      long_options[long_option_index].flag = &this_option;
      long_options[long_option_index].val = option_index | OPT_FLAG;
    }

  if ((next_option > MAX_OPTIONS) || (next_long_option > MAX_OPTIONS))
    abort();
  if ((var == NULL) && (handler == NULL))
    abort();
  if ((var != NULL) && (handler != NULL))
    abort();
  if ((handler == NULL) != (set_default == NULL))
    abort();

  option_list[option_index].type = FLAG_STRING;
  option_list[option_index].class = class;
  option_list[option_index].need_string = FALSE;
  option_list[option_index].set = FALSE;
  option_list[option_index].t.oflg.var = var;
  option_list[option_index].t.oflg.set_string = s1;
  option_list[option_index].t.oflg.not_set_string = s2;
  option_list[option_index].t.oflg.default_value = default_value;
  option_list[option_index].t.oflg.true_value = true_value;
  option_list[option_index].t.oflg.false_value = false_value;
  option_list[option_index].t.oflg.handler = handler;
  option_list[option_index].t.oflg.set_default = set_default;
  option_list[option_index].t.oflg.set_help_string = set_help_string;
  option_list[option_index].t.oflg.not_set_help_string = not_set_help_string;
}

/******************************************************************************
 * set_option - sets the option
 */

void set_option(int index, char *prefix, const char *option_name, char *value) {
  boolean flag_set = ((index & OPT_FLAG) != 0);
  struct option_type *op;

  if (flag_set)
    op = &option_list[index-OPT_FLAG];
  else
    op = &option_list[index];

  if (value)
    dm('o',3,"Trying to set option %s%s to %s\n", prefix, option_name, value);
  else
    dm('o',3,"Trying to set option %s%s\n", prefix, option_name);

  if (op->set)
    {
      dm('o',3,"Option %s%s already set\n", prefix, option_name);
      return;
    }

  switch (op->type)
    {

    case NOPARM:
      (*(op->t.onoparm.handler))(prefix, option_name);
      break;

    case OPTIONAL:
      (*(op->t.ooptional.handler))(prefix, option_name, value);
      break;

    case BOOLEAN:
      if (op->t.obool.var)
	{
	  *(op->t.obool.var) = (!flag_set);
	}
      else
	{
	  (*(op->t.obool.handler))(prefix, option_name, !flag_set);
	}
      break;

    case CHAR:
      if (strlen(value) != 1)
	{
	  fprintf(stderr, gettext(CMD_NAME ": must have one character parameter for %s%s flag, but got '%s'\n"),prefix,option_name,value);
	  exit(1);
	}
      /* TODO: Need to check that c is in valid_set */
      if (op->t.ochar.var)
	{
	  *(op->t.ochar.var) = *value;
	}
      else
	{
	  (*(op->t.ochar.handler))(prefix, option_name, *value, op->t.ochar.valid_set);
	}
      break;

    case CHOICE:
      if (strlen(prefix) == 1)
	{
	  /* This is a short option */
	  if (strlen(value) != 1)
	    {
	      fprintf(stderr, gettext(CMD_NAME ": must have one character parameter for %s%s flag, but got '%s'\n"),prefix,option_name,value);
	      exit(1);
	    }
	  
	  if ((*value != op->t.ochoice.choice1) && (*value != op->t.ochoice.choice2))
	    {
	      fprintf(stderr, gettext(CMD_NAME ": option %s%s can only take %c or %c, not %c\n"),prefix,option_name,op->t.ochoice.choice1,op->t.ochoice.choice2,*value);
	      exit(1);
	    }

	  if (op->t.ochoice.var)
	    {
	      *(op->t.ochoice.var) = *value;
	    }
	  else
	    {
	      (*(op->t.ochoice.handler))(prefix, option_name, *value);
	    }
	}
      else
	{
	  /* This is a long option */
	  char chosen_value;
	  if (strcmp(option_name,op->t.ochoice.choice1_string) == 0)
	    {
	      /* First option selected */
	      chosen_value = op->t.ochoice.choice1;
	    }
	  else
	    {
	      /* Second option selected */
	      chosen_value = op->t.ochoice.choice2;
	    }

	  if (op->t.ochoice.var)
	    {
	      *(op->t.ochoice.var) = chosen_value;
	    }
	  else
	    {
	      (*(op->t.ochoice.handler))(prefix, option_name, chosen_value);
	    }
	}

      break;

    case SHORT:
      if (flag_set)
	{
	  if (op->t.oshrt.var)
	    {
	      *(op->t.oshrt.var) = op->t.oshrt.special_value;
	    }
	  else
	    {
	      (*(op->t.oshrt.handler))(prefix, option_name, op->t.oshrt.special_value, op->t.oshrt.min, op->t.oshrt.max);
	    }
	}
      else
	{
	  int intvalue = atoi(value);
	  if ((intvalue > op->t.oshrt.max) || (intvalue < op->t.oshrt.min))
	    {
	      fprintf(stderr, gettext(CMD_NAME ": option %s%s not between %d and %d\n"),prefix,option_name,op->t.oshrt.min,op->t.oshrt.max);
	      exit(1);
	    }
      
	  if (op->t.oshrt.var)
	    {
	      *(op->t.oshrt.var) = (short)intvalue;
	    }
	  else
	    {
	      (*(op->t.oshrt.handler))(prefix, option_name, (short)intvalue, op->t.oshrt.min, op->t.oshrt.max);
	    }
	}
      break;

    case INT:
      if (flag_set)
	{
	  if (op->t.oint.var)
	    {
	      *(op->t.oint.var) = op->t.oint.special_value;
	    }
	  else
	    {
	      (*(op->t.oint.handler))(prefix, option_name, op->t.oint.special_value, op->t.oint.min, op->t.oint.max);
	    }
	}
      else
	{
	  int intvalue = atoi(value);
	  if ((intvalue > op->t.oint.max) || (intvalue < op->t.oint.min))
	    {
	      fprintf(stderr, gettext(CMD_NAME ": option %s%s not between %d and %d\n"),prefix,option_name,op->t.oint.min,op->t.oint.max);
	      exit(1);
	    }
	  if (op->t.oint.var)
	    {
	      *(op->t.oint.var) = intvalue;
	    }
	  else
	    {
	      (*(op->t.oint.handler))(prefix, option_name, intvalue, op->t.oint.min, op->t.oint.max);
	    }
	}
      break;

    case STRING:
      if (op->t.ostrng.var)
	{
	  *(op->t.ostrng.var) = strdup(value);
	}
      else
	{
	  (*(op->t.ostrng.handler))(prefix, option_name, value);
	}
      break;

    case FLAG_STRING:
      if (op->t.oflg.var)
	{
	  if (flag_set)
	    {
	      if (op->t.oflg.true_value)
		{
		  *(op->t.oflg.var) = strdup(op->t.oflg.true_value);
		}
	      else
		{
		  *(op->t.oflg.var) = NULL;
		}
	    }
	  else
	    {
	      if (op->t.oflg.false_value)
		{
		  *(op->t.oflg.var) = strdup(op->t.oflg.false_value);
		}
	      else
		{
		  *(op->t.oflg.var) = NULL;
		}
	    }
	}
      else
	{
	  (*(op->t.oflg.handler))(prefix, option_name, !flag_set, op->t.oflg.true_value, op->t.oflg.false_value);
	}
      break;

    default:
      abort();
    }

  op->set = TRUE;

  dm('o',3,"Succeeded - %s%s has not been set before\n",prefix,option_name);
}

/******************************************************************************
 * Function:
 *	set_option_default
 */
void set_option_default(int index)
{
  option_type *op = &option_list[index];

  dm('o',3,"Trying to set option %d to default value\n", index);
  
  if (op->set)
    {
      dm('o',3,"Option %d already set\n", index);
      return;
    }

  switch (op->type)
    {

    case NOPARM:
      if (op->t.onoparm.default_opt)
	(*(op->t.onoparm.set_default))();
      break;

    case OPTIONAL:
      break;

    case BOOLEAN:
      if (op->t.obool.var)
	{
	  *(op->t.obool.var) = op->t.obool.default_value;
	}
      else
	{
	  (*(op->t.obool.set_default))(op->t.obool.default_value);
	}
      break;

    case CHAR:
      if (op->t.ochar.var)
	{
	  *(op->t.ochar.var) = op->t.ochar.default_value;
	}
      else
	{
	  (*(op->t.ochar.set_default))(op->t.ochar.default_value);
	}
      break;

    case CHOICE:
      if (op->t.ochoice.var)
	{
	  *(op->t.ochoice.var) = op->t.ochoice.choice1;
	}
      else
	{
	  (*(op->t.ochoice.set_default))(op->t.ochoice.choice1);
	}
      break;

    case SHORT:
      if (op->t.oshrt.var)
	{
	  *(op->t.oshrt.var) = op->t.oshrt.default_value;
	}
      else
	{
	  (*(op->t.oshrt.set_default))(op->t.oshrt.default_value);
	}
      break;

    case INT:
      if (op->t.oint.var)
	{
	  *(op->t.oint.var) = op->t.oint.default_value;
	}
      else
	{
	  (*(op->t.oint.set_default))(op->t.oint.default_value);
	}
      break;

    case STRING:
      if (op->t.ostrng.var)
	{
	  if (op->t.ostrng.default_value)
	    {
	      *(op->t.ostrng.var) = strdup(op->t.ostrng.default_value);
	    }
	  else
	    {
	      *(op->t.ostrng.var) = NULL;
	    }
	}
      else
	{
	  if (op->t.ostrng.default_value)
	    {
	      (*(op->t.ostrng.set_default))(strdup(op->t.ostrng.default_value));
	    }
	  else
	    {
	      (*(op->t.ostrng.set_default))(NULL);
	    }
	}
      break;

    case FLAG_STRING:
      if (op->t.oflg.var)
	{
	  if (op->t.oflg.default_value)
	    {
	      if (op->t.oflg.true_value)
		{
		  *(op->t.oflg.var) = strdup(op->t.oflg.true_value);
		}
	      else
		{
		  *(op->t.oflg.var) = NULL;
		}
	    }
	  else
	    {
	      if (op->t.oflg.false_value)
		{
		  *(op->t.oflg.var) = strdup(op->t.oflg.false_value);
		}
	      else
		{
		  *(op->t.oflg.var) = NULL;
		}
	    }
	}
      else
	{
	  if (op->t.oflg.default_value)
	    {
	      if (op->t.oflg.true_value)
		{
		  (*(op->t.oflg.set_default))(1, strdup(op->t.oflg.true_value));
		}
	      else
		{
		  (*(op->t.oflg.set_default))(1, NULL);
		}
	    }
	  else
	    {
	      if (op->t.oflg.false_value)
		{
		  (*(op->t.oflg.set_default))(0, strdup(op->t.oflg.false_value));
		}
	      else
		{
		  (*(op->t.oflg.set_default))(0, NULL);
		}
	    }
	}
      break;

    default:
      abort();
    }

  dm('o',3,"Succeeded - option %d has not been set before\n",index);
}

/******************************************************************************
 * Function:
 *	print_usage_msgs
 */
void print_usage_msgs(option_class class)
{
  int option_index;

  switch (class){
  case OPT_MISC:
    printf(gettext("Miscellaneous options:\n"));
    break;
  case OPT_PAGE_FURNITURE:
    printf(gettext("Page furniture options:\n"));
    break;
  case OPT_TEXT_FORMAT:
    printf(gettext("Text formatting options:\n"));
    break;
  case OPT_PRINT:
    printf(gettext("Print selection options:\n"));
    break;
  case OPT_PAGE_FORMAT:
    printf(gettext("Page format options:\n"));
    break;
  case OPT_OUTPUT:
    printf(gettext("Output options:\n"));
    break;
  }

  for (option_index=0; option_index < next_option; option_index++)
    {
      option_type *op = &option_list[option_index];
      if (class == op->class)
	{
	  switch (op->type)
	    {
	    case NOPARM:
	      if (op->letter)
		printf("-%c  ",op->letter);
	      if (op->t.onoparm.string)
		printf("--%s",op->t.onoparm.string);
	      printf("\n    %s\n",gettext(op->t.onoparm.help_string));
	      break;

	    case OPTIONAL:
	      if (op->letter)
		printf("-%c  ",op->letter);
	      if (op->t.ooptional.string)
		printf("--%s[=<string>]",op->t.ooptional.string);
	      printf("\n    %s\n",gettext(op->t.ooptional.help_string));
	      break;
	    
	    case BOOLEAN:
	      if (op->letter)
		printf("-%c  ",op->letter);
	      if (op->t.obool.true_string)
		printf("--%s",op->t.obool.true_string);
	      printf("\n    %s\n",gettext(op->t.obool.true_help_string));
	      if (op->letter)
		printf("--%c  ",op->letter);
	      if (op->t.obool.false_string)
		printf("--%s",op->t.obool.false_string);
	      printf("\n    %s\n",gettext(op->t.obool.false_help_string));
	      break;

	    case CHAR:
	      if (op->letter)
		printf("-%c <char>  ",op->letter);
	      if (op->t.ochar.string)
		printf("--%s <char>",op->t.ochar.string);
	      printf("\n    %s\n",gettext(op->t.ochar.help_string));
	      break;

	    case CHOICE:
	      if (op->letter)
		printf("-%c %c  ",op->letter,op->t.ochoice.choice1);
	      if (op->t.ochoice.choice1_string)
		printf("--%s",op->t.ochoice.choice1_string);
	      printf("\n    %s\n",gettext(op->t.ochoice.choice1_help_string));
	      if (op->letter)
		printf("-%c %c  ",op->letter,op->t.ochoice.choice2);
	      if (op->t.ochoice.choice2_string)
		printf("--%s",op->t.ochoice.choice2_string);
	      printf("\n    %s\n",gettext(op->t.ochoice.choice2_help_string));
	      break;

	    case SHORT:
	      if (op->letter)
		printf("-%c <number>  ",op->letter);
	      if (op->t.oshrt.string)
		printf("--%s=<number>",op->t.oshrt.string);
	      printf("\n    %s\n",gettext(op->t.oshrt.help_string));
	      if (op->t.oshrt.special_string)
		printf("--%s\n    %s\n", op->t.oshrt.special_string, gettext(op->t.oshrt.special_help_string));
	      break;

	    case INT:
	      if (op->letter)
		printf("-%c <number>  ",op->letter);
	      if (op->t.oint.string)
		printf("--%s=<number>",op->t.oint.string);
	      printf("\n    %s\n",gettext(op->t.oint.help_string));
	      if (op->t.oint.special_string)
		printf("--%s\n    %s\n", op->t.oint.special_string, gettext(op->t.oint.special_help_string));
	      break;

	    case STRING:
	      if (op->letter)
		printf("-%c <string>  ",op->letter);
	      if (op->t.ostrng.string)
		printf("--%s=<string>",op->t.ostrng.string);
	      printf("\n    %s\n",gettext(op->t.ostrng.help_string));
	      break;

	    case FLAG_STRING:
	      if (op->letter)
		printf("-%c <string>  ",op->letter);
	      if (op->t.oflg.set_string)
		printf("--%s=<string>",op->t.oflg.set_string);
	      printf("\n    %s\n",gettext(op->t.oflg.set_help_string));
	      if (op->letter)
		printf("--%c  ",op->letter);
	      if (op->t.oflg.set_string)
		printf("--%s",op->t.oflg.not_set_string);
	      printf("\n    %s\n",gettext(op->t.oflg.not_set_help_string));
	    }
	}	
    }
}

/******************************************************************************
 * Function:
 *	set_option_defaults
 */
void set_option_defaults(void)
{
  int option_index;

  for (option_index=0; option_index < next_option; option_index++)
    {
      set_option_default(option_index);
    }
}
