/*
 * Source file:
 *	language.c
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trueprint.h"
#include "main.h"
#include "lang_c.h"
#include "lang_cxx.h"
#include "lang_report.h"
#include "lang_sh.h"
#include "lang_pascal.h"
#include "lang_java.h"
#include "lang_perl.h"
#include "lang_text.h"
#include "lang_verilog.h"
#include "options.h"
#include "language.h"

boolean restart_language;
short braces_depth;
char *language_list =
"arguments to --language:\n"
"  c       C\n"
"  v       Verilog\n"
"  cxx     C++\n"
"  report  trueprint report file\n"
"  sh      shell\n"
"  pascal  Pascal\n"
"  perl    Perl\n"
"  java    Java\n"
"  text    plain text\n"
"  list    compiler or assembler listing file\n"
"  pseudoc C pseudocode\n";

/*
 * Private part
 */

typedef enum {
  NO_LANGUAGE,
  C,
  CXX,
  PSEUDOC,
  REPORT,
  SHELL,
  PASCAL,
  PERL,
  LIST,
  TEXT,
  JAVA,
  VERILOG
} languages;

get_char_fn 	get_char;

static languages language;

static void set_language_opt(const char *prefix, const char *option, char *value);
static void set_language_default(char *value);
static languages filename_to_language(char *);

/******************************************************************************
 * Function: 
 *	setup_language
 */
void
setup_language(void)
{
  language = NO_LANGUAGE;

  string_option("t", "language", NULL, NULL, &set_language_opt, &set_language_default,
		OPT_MISC,
		"treat input as language.  Use --help languages for list.");
}

/******************************************************************************
 * Function:
 *	set_language_default
 */
void
set_language_default(char *value)
{
  if (value == NULL)                    language = NO_LANGUAGE;
  else if (strcmp(value,"c") == 0)      language = C;
  else if (strcmp(value,"v") == 0)      language = VERILOG;
  else if (strcmp(value,"cxx") == 0)    language = CXX;
  else if (strcmp(value,"report") == 0) language = REPORT;
  else if (strcmp(value,"sh") == 0)     language = SHELL;
  else if (strcmp(value,"pascal") == 0) language = PASCAL;
  else if (strcmp(value,"perl") == 0)   language = PERL;
  else if (strcmp(value,"java") == 0)   language = JAVA;
  else if (strcmp(value,"text") == 0)   language = TEXT;
  else if (strcmp(value,"list") == 0)   language = LIST;
  else if (strcmp(value,"pseudoc") == 0)language = PSEUDOC;
  else
    abort();
}
/******************************************************************************
 * Function:
 *	set_language_opt
 */
void set_language_opt(const char *prefix, const char *option, char *value)
{
  set_language_default(value);
}

/******************************************************************************
 * Function: 
 *	filename_to_language
 */

languages
filename_to_language(char *filename)

{
  languages retval;
  char *suffix;

  if ((suffix = strrchr(filename,'.')) == (char *)0) retval = TEXT;
  else if (strcmp(suffix,".c") == 0) retval = C;
  else if (strcmp(suffix,".v") == 0) retval = VERILOG;
  else if (strcmp(suffix,".h") == 0) retval = C;
  else if (strcmp(suffix,".cxx") == 0) retval = CXX;
  else if (strcmp(suffix,".cpp") == 0) retval = CXX;
  else if (strcmp(suffix,".cc") == 0) retval = CXX;
  else if (strcmp(suffix,".C") == 0) retval = CXX;   /* James Card */
  else if (strcmp(suffix,".hpp") == 0) retval = CXX;
  else if (strcmp(suffix,".H") == 0) retval = CXX;   /* James Card */
  else if (strcmp(suffix,".pc") == 0) retval = PSEUDOC;
  else if (strcmp(suffix,".ph") == 0) retval = PSEUDOC;
  else if (strcmp(suffix,".rep") == 0) retval = REPORT;
  else if (strcmp(suffix,".sh") == 0) retval = SHELL;
  else if (strcmp(suffix,".pas") == 0) retval = PASCAL;
  else if (strcmp(suffix,".pl") == 0) retval = PERL;
  else if (strcmp(suffix,".pm") == 0) retval = PERL;
  else if (strcmp(suffix,".java") == 0) retval = JAVA;
  else if (strcmp(suffix,".lst") == 0) retval = LIST;
  else retval = TEXT;

  return retval;
}

/******************************************************************************
 * Function: 
 *	language_defaults
 */
char *
language_defaults(char *filename)

{
  char *retval;

  switch((language!=NO_LANGUAGE) ? language : filename_to_language(filename))
    {
    case C: 		retval = lang_c_defaults;	break;
    case VERILOG:	retval = lang_verilog_defaults;	break;
    case CXX: 		retval = lang_cxx_defaults;	break;
    case PSEUDOC: 	retval = lang_pc_defaults; 	break;
    case REPORT: 	retval = lang_report_defaults; 	break;
    case SHELL: 	retval = lang_sh_defaults; 	break;
    case PASCAL: 	retval = lang_pascal_defaults; 	break;
    case PERL: 		retval = lang_perl_defaults; 	break;
    case JAVA: 		retval = lang_java_defaults; 	break;
    case TEXT:	 	retval = lang_text_defaults; 	break;
    case LIST: 		retval = lang_list_defaults; 	break;
    default: 
      abort();
    }

  return retval;
}

/******************************************************************************
 * Function: 
 *	set_get_char
 * Called for each file to set get_char appropriately.  language must be
 * set.
 */

void
set_get_char(char *filename)

{
  /*
   * restart_language is used to tell the language-specific routines
   * to reset
   */
  restart_language = TRUE;

  switch((language!=NO_LANGUAGE) ? language : filename_to_language(filename))
    {
    case C:		get_char = get_c_char;		break;
    case VERILOG:	get_char = get_verilog_char;	break;
    case CXX:		get_char = get_cxx_char;	break;
    case PSEUDOC:	get_char = get_pc_char;		break;
    case REPORT:	get_char = get_report_char;	break;
    case SHELL:		get_char = get_sh_char;		break;
    case PASCAL:	get_char = get_pascal_char;	break;
    case PERL:		get_char = get_perl_char;	break;
    case JAVA:		get_char = get_java_char;	break;
    case TEXT:		get_char = get_text_char;	break;
    case LIST:		get_char = get_text_char;	break;
    default:
      abort();
    }
}
