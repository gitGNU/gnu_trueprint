/*
 * Source file:
 *	printers_fl.c
 *
 * Reads in the printers file and returns values based on printer name
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trueprint.h"
#include "main.h"
#include "utils.h"
#include "debug.h"

#include "printers_fl.h"

/*
 * Private part
 */

typedef struct printer_type {
  char *name;
  short sides;
  int left, right,top,bottom;
  struct printer_type *next;
} printer_type;

typedef struct printer_record {
  char *names;
  char *type;
  struct printer_record *next;
} printer_record;

static struct printer_type *parse_printer_type(char *line);
static struct printer_record *parse_printer_record(char *line);
static char *printers_filename;

/******************************************************************************
 * Function:
 *	setup_printersile_fl
 * If environment variable PRINTERS_FILE is set then use that, otherwise
 * use the configured value for PRINTERS_FILE.
 */
void
setup_printers_fl(void)
{
  printers_filename = NULL;

  if (use_environment)
    printers_filename = getenv("TP_PRINTERS_FILE");

  if ((printers_filename == NULL) || (strlen(printers_filename) == 0))
    printers_filename = PRINTERS_FILE;
}

/******************************************************************************
 * Function:
 *	printer_stats
 *
 * Looks up printer in printers_filename.
 */
void
printer_stats(char *printer, short *sides, 
	      unsigned short *lm, unsigned short *rm, 
	      unsigned short *tm, unsigned short *bm)
{
  char buffer[1024];
  char *env_printer;
  FILE *printers_file;
  printer_record *printers_list = NULL;
  printer_type *types_list = NULL;
  printer_record *tmp_printer = NULL;
  printer_type *tmp_type = NULL;
  printer_record *this_printer = NULL;
  printer_type *this_type = NULL;

  /*
   * Set up defaults
   */
  *lm	= 15;
  *rm	= 590;
  *tm	= 776;
  *bm	= 30;
  *sides = 1;

  /*
   * If this is a test and nothing should be taken from outside trueprint,
   * then simply use the default settings
   */
  if (!use_environment)
    {
      env_printer = "testprinter";
      dm('D',3,"Using testprinter entry\n");
      return;
    }

  /*
   * If there is no printer name use the defaults
   */
  if ((printer == NULL) || (*printer == '\0'))
    {
      if (((env_printer = getenv("PRINTER")) == NULL) || (*env_printer == '\0'))
	{
	  dm('D',3, CMD_NAME ": $PRINTER null or not defined: using default printer properties\n");
	  return;
	}
      else
	{
	  printer = env_printer;
	}
    }

  /*
   * Open the printers file, and return defaults if we can't
   */
  if ((printers_file = fopen(printers_filename, "r")) == NULL)
    {
      fprintf(stderr, gettext(CMD_NAME ": warning: cannot open %s, %s\n"), printers_filename, strerror(errno));
      return;
    }

  /*
   * Read in the printer file information
   */
  while (fgets(buffer, 1024, printers_file))
    {
      if (strlen(buffer) == 0)
	{
	  /* Ignore empty lines */
	}
      else if (*buffer == '#')
	{
	  /* Ignore comment lines */
	}
      else if (strncmp(buffer, "type",4) == 0)
	{
	  tmp_type = parse_printer_type(buffer);
	  tmp_type->next = types_list;
	  types_list = tmp_type;
	}
      else if (strncmp(buffer, "printer", 7) == 0)
	{
	  tmp_printer = parse_printer_record(buffer);
	  tmp_printer->next = printers_list;
	  printers_list = tmp_printer;
	}
    }

  /*
   * Find printer
   */
  for (tmp_printer = printers_list; tmp_printer; tmp_printer = tmp_printer->next)
    {
      char *s = strtok(tmp_printer->names,",");

      while (s)
	{
	  dm('D',5,"Comparing printer names %s and %s\n",printer,s);
	  if (strcmp(printer,s) == 0)
	    {
	      this_printer = tmp_printer;
	      break;
	    }
	  s = strtok(NULL,",");
	}
      if (this_printer) break;
    }

  /*
   * If printer not found, return defaults
   */
  if (!this_printer)
    {
      fprintf(stderr, gettext(CMD_NAME ": warning: cannot find printer %s in %s\n"), printer, printers_filename);
      fprintf(stderr, gettext(CMD_NAME ": send mail to %s if you want to have it added\n"), TP_ADMIN_USER);
      return;
    }

  /*
   * Find type
   */
  for (tmp_type = types_list; tmp_type; tmp_type = tmp_type->next)
    {
      if (strcmp(this_printer->type,tmp_type->name) == 0)
	{
	  this_type = tmp_type;
	  break;
	}
    }

  /*
   * If printer type not found, return defaults
   */
  if (!this_type)
    {
      fprintf(stderr, gettext(CMD_NAME ": warning: cannot find printer type %s in %s\n"), this_printer->type, printers_filename);
      fprintf(stderr, gettext(CMD_NAME ": you should notify %s\n"), TP_ADMIN_USER);
      return;
    }

  if (fclose(printers_file) == EOF)
    {
      perror(CMD_NAME ": Cannot close printers file");
      exit(2);
    }

  dm('D',3,"printers_file.c: Printer type %s\n", this_type->name);

  *lm	= this_type->left;
  *rm	= this_type->right;
  *tm	= this_type->top;
  *bm	= this_type->bottom;
  *sides = this_type->sides;
}

/******************************************************************************
 * Function:
 *	parse_printer_type
 *
 */
struct printer_type *parse_printer_type(char *line)
{
  struct printer_type *r = xmalloc(sizeof(printer_type));

  /* Ignore "type" field */
  strtok(line, ":");
  r->name    = strdup(strtok(NULL, ":"));
  r->sides = (short) atoi(strtok(NULL, ":"));
  r->left = (unsigned short) atoi(strtok(NULL, ":"));
  r->right = (unsigned short) atoi(strtok(NULL, ":"));
  r->top = (unsigned short) atoi(strtok(NULL, ":"));
  r->bottom = (unsigned short) atoi(strtok(NULL, ":"));
  r->next = NULL;

  if ((r->sides != 1) && (r->sides != 2))
    {
      fprintf(stderr, gettext(CMD_NAME ": printers file %s: type %s: second field must be 1 or 2, but is %d\n"), printers_filename, r->name, r->sides);
      exit(2);
    }
    
  dm('D',3,"Read type %s entry: %d:%d:%d:%d:%d\n",
     r->name,r->sides,r->left,r->right,r->top,r->bottom);

  return r;
}

/******************************************************************************
 * Function:
 *	parse_printer_record
 *
 */
struct printer_record *parse_printer_record(char *line)
{
  struct printer_record *r = xmalloc(sizeof(printer_record));

  /* Ignore "printer" field */
  strtok(line, ":");
  r->names    = strdup(strtok(NULL, ":"));
  r->type     = strdup(strtok(NULL, ":"));

  /* Overwrite newline at end of last field */
  r->type[strlen(r->type)-1] = '\0';

  r->next     = NULL;

  dm('D',3,"Read printer entry: %s:%s\n",
     r->names,r->type);

  return r;
}




