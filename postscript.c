/*
 * Source file:
 *	postscript.c
 *
 * Implements stuff for postscript printers
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trueprint.h"
#include "main.h"
#include "expand_str.h"
#include "utils.h"
#include "debug.h"
#include "options.h"
#include "index.h"
#include "printers_fl.h"

#include "postscript.h"

/*
 * Public part
 */
boolean	no_show_indent_level;
boolean no_show_line_number;

/*
 * Private part
 */

typedef enum {
  NO_LAYOUT,
  ONE_ON_ONE,
  TWO_ON_ONE,
  THREE_ON_ONE,
  FOUR_ON_ONE
} page_layouts;

static unsigned short logical_pages_on_physical_page;
static long physical_page_no;
static short	greenlines;
static boolean	no_holepunch;
static boolean	no_top_holepunch;
static int	interline_gap;
static boolean	include_headers;
static boolean	include_footers;
static short	pointsize;
static char	orientation;
static page_layouts layout;
static boolean	no_cover_sheet;
static char	*user_name;
static unsigned short printable_width;
static unsigned short printable_height;
static unsigned short virtual_width;
static unsigned short virtual_height;
static unsigned short header_box_height;
static unsigned short footer_box_height;
static unsigned short logical_pages_rotated;
static unsigned short pos_left;
static unsigned short pos_centre_left;
static unsigned short pos_centre_right;
static unsigned short pos_right;
static unsigned short pos_bottom;
static unsigned short pos_centre_bottom;
static unsigned short pos_centre_top;
static unsigned short pos_top;
static unsigned short logical_page_no;
static unsigned short left_margin;
static unsigned short right_margin;
static unsigned short top_margin;
static unsigned short bottom_margin;
static boolean left_page, right_page;
static char	no_of_sides;

static void balance_strings(char *string1, char *string2, char *string3, long page_no, boolean index_page);
static void set_layout_default(void);
static void set_layout_1(const char *p, const char *s);
static void set_layout_2(const char *p, const char *s);
static void set_layout_3(const char *p, const char *s);
static void set_layout_4(const char *p, const char *s);
static boolean PS_side_full(void);


/*
 * Function:
 *	setup_postscript()
 *
 * Declares variables and performs any initializations
 */
void
setup_postscript(void)
{
  pointsize = -1;
  short_option("p", "point-size", 10,
	       NULL, 0,
	       MINPOINTSIZE, MAXPOINTSIZE, &pointsize, NULL, NULL,
	       OPT_TEXT_FORMAT,
	       "specify point size (default 10)", NULL);

  boolean_option("C", "no-cover-sheet", "cover-sheet", FALSE, &no_cover_sheet, NULL, NULL,
		 OPT_PRINT,
		 "don't print cover sheet",
		 "print cover sheet");

  boolean_option("I", "no-holepunch", "holepunch", TRUE, &no_holepunch, NULL, NULL,
		 OPT_PAGE_FORMAT,
		 "don't leave space for holepunch at the side of each page",
		 "leave space for holepunch at the side of each page");

  boolean_option("J", "no-top-holepunch", "top-holepunch", TRUE, &no_top_holepunch, NULL, NULL,
		 OPT_PAGE_FORMAT,
		 "don't leave space for holepunch at the top of each page",
		 "leave space for holepunch at the top of each page");

  greenlines = -1;
  short_option("G", "gray-bands", 0,
	       NULL, 0,
	       0, MAXGREENLINES, &greenlines, NULL, NULL,
	       OPT_PAGE_FURNITURE,
	       "<lines> emulate the old lineprinter paper with gray bands\n"
	       "    across each page.  The value of <lines> gives the width of\n"
	       "    the bands and the gaps between them",
	       NULL);

  interline_gap = -1;
  int_option("g", "leading", 1,
	     NULL, 0,
	     MININTERLINE_GAP, MAXINTERLINE_GAP, &interline_gap, NULL, NULL,
	     OPT_TEXT_FORMAT,
	     "specify interline gap in points (default 1)", NULL);

  boolean_option("K", "headers", "no-headers", TRUE, &include_headers, NULL, NULL,
		 OPT_PAGE_FURNITURE,
		 "include the header on each page",
		 "suppress the header on each page");

  boolean_option("k", "footers", "no-footers", TRUE, &include_footers, NULL, NULL,
		 OPT_PAGE_FURNITURE,
		 "include the footer on each page",
		 "suppress the footer on each page");

  user_name = NULL;
  string_option("U", "username", NULL, &user_name, NULL, NULL,
		OPT_MISC,
		"set username for coversheet");

  boolean_option("i", "no-braces-depth", "braces-depth", FALSE, &no_show_indent_level, NULL, NULL,
		 OPT_PAGE_FURNITURE,
		 "exclude the braces depth count",
		 "include the braces depth count");

  boolean_option("n", "no-line-numbers", "line-numbers", FALSE, &no_show_line_number, NULL, NULL,
		 OPT_PAGE_FURNITURE,
		 "exclude the line number count",
		 "include the line number count");

  choice_option("o", "portrait", "landscape",
		'p', 'l', &orientation, NULL, NULL,
		 OPT_PAGE_FORMAT,
		"print using portrait orientation",
		"print using landscape orientation");

  no_of_sides = '0';
  choice_option("S", "single-sided", "double-sided",
		'1', '2', &no_of_sides, NULL, NULL,
		OPT_PAGE_FORMAT,
		"print single-sided",
		"print double-sided");

  layout = NO_LAYOUT;
  noparm_option("1", "one-up", TRUE, &set_layout_1, &set_layout_default, OPT_PAGE_FORMAT, "print 1-on-1 (default)");
  noparm_option("2", "two-up", FALSE, &set_layout_2, NULL, OPT_PAGE_FORMAT, "print 2-on-1");
  noparm_option("3", "two-tall-up", FALSE, &set_layout_3, NULL, OPT_PAGE_FORMAT, "print 2-on-1 at 4-on-1 pointsize");
  noparm_option("4", "four-up", FALSE, &set_layout_4, NULL, OPT_PAGE_FORMAT, "print 4-on-1");
}

/*
 * Function:
 *	init_postscript()
 *
 * Gets postscript ready for either pass 0 or pass 1
 */
void
init_postscript(void)
{
  physical_page_no = 1;
  logical_page_no = 0;
  right_page = TRUE;
  left_page = FALSE;
}

void set_layout_default(void)
{
  if (layout != NO_LAYOUT) return;
  layout = ONE_ON_ONE;
}

void set_layout_1(const char *p, const char *s)
{
  if (layout != NO_LAYOUT) return;
  layout = ONE_ON_ONE;
}

void set_layout_2(const char *p, const char *s)
{
  if (layout != NO_LAYOUT) return;
  layout = TWO_ON_ONE;
}

void set_layout_3(const char *p, const char *s)
{
  if (layout != NO_LAYOUT) return;
  layout = THREE_ON_ONE;
}

void set_layout_4(const char *p, const char *s)
{
  if (layout != NO_LAYOUT) return;
  layout = FOUR_ON_ONE;
}

/*
 * Function:
 *	PS_header()
 *
 * Prints out the initial stuff for a postscript printer
 */

void
PS_header(char *version, boolean print_body)

{
  unsigned int	file_index;

  printf("%%!PS-Adobe\n");

  printf("statusdict /setduplexmode known {\n");
  if (no_of_sides == '2')
    {
      printf("  true ");
    }
  else
    {
      printf("  false ");
    }
  printf("statusdict /setduplexmode get exec\n} if\n");

  printf("/Ps	%d def\n", pointsize);
  printf("/SPs	%d def\n", (pointsize*3)/4);
  printf("/Lh	%d def\n", pointsize + interline_gap);
  printf("/Rm	%d def\n", virtual_width);
  printf("/Tm	%d def\n", virtual_height);
  printf("/Bh	%d def\n", header_box_height);
  printf("/Bf	%d def\n", footer_box_height);
  printf("/CF	/Courier findfont Ps scalefont def\n");
  printf("/IF	/Courier-Oblique findfont Ps scalefont def\n");
  printf("/BF	/Courier-Bold findfont Ps scalefont def\n");
  printf("/CFs	/Courier findfont SPs scalefont def\n");
  printf("/IFs	/Courier-Oblique findfont SPs scalefont def\n");
  printf("/BFs	/Courier-Bold findfont SPs scalefont def\n");
  printf("/HF	/Helvetica findfont Ps scalefont def\n");
  if ((no_show_line_number == FALSE) || (no_show_indent_level == FALSE))
    printf("/Li CFs setfont (1234567890) stringwidth pop def\n");
  else
    printf("/Li 0 def\n");
  printf("/Nl	{ /Vpos Vpos Lh sub def } def\n");
  printf("/Lpt	{ 0 Vpos moveto } def\n");
  printf("/Gb	{\n");
  if (greenlines != 0)
    {
      printf("		gsave /Cv Tm Bh sub def\n");
      printf("		{ Li Cv moveto Rm Li sub 0 rlineto 0 -%d Lh mul rlineto\n",greenlines);
      printf("		  Rm Li sub neg 0 rlineto 0 %d Lh mul rlineto\n",greenlines);
      printf("		  0.98 setgray fill\n");
      printf("		  /Cv Cv %d Lh mul sub def\n",greenlines*2);
      printf("		  Cv 0 lt { exit } if\n");
      printf("		} loop\n");
      printf("		grestore\n");
    }
  printf("		} def\n");
  printf("/Ip	{ Gb .5 setlinewidth\n");
  if (include_headers)
    {
      printf("		0 Tm moveto 0 Bh neg rlineto Rm 0 rlineto 0 Bh rlineto closepath\n");
    }
  else
    {
      printf("		0 Tm moveto Rm 0 rlineto\n");
    }
  printf("		gsave .98 setgray fill grestore stroke\n");
  if (include_footers)
    {
      printf("		0 0 moveto 0 Bf rlineto Rm 0 rlineto 0 Bf neg rlineto closepath\n");
    }
  else
    {
      printf("		0 0 moveto Rm 0 rlineto\n");
    }
  printf("		gsave .98 setgray fill grestore stroke } def\n");
  printf("/Cp	{ Ip .3 setlinewidth newpath\n");
  printf("		Li 0 Bf add moveto Li Tm Bh sub lineto stroke newpath\n");
  printf("		0 Bf moveto 0 Tm Bh sub lineto stroke newpath\n");
  printf("		} def\n");
  printf("/So	{ gsave dup stringwidth pop Ps 3 div 0 exch rmoveto 0 rlineto fill grestore } def\n");
  printf("/Ul	{ gsave	dup stringwidth pop 0 -1 rmoveto 0 rlineto fill grestore } def\n");
  printf("/Bs	{ gsave	dup show grestore 0.5 0.5 rmoveto show } def\n");

  /*
   * Print cover sheet
   */
  if (no_cover_sheet == FALSE)
    {
      if (user_name == NULL)
	{

	  dm('h',3, "postscript.c:PS_header() use_environment = %d\n", use_environment);
	  if (!use_environment)
	    user_name = "testuser";
	  else
	    {
	      char *u = getenv("USER");
	      if (u == 0)
		{
		  user_name = "";
		}
	      else
		{
		  user_name = strdup(u);
		}
	    }
	  if (*user_name == '\0')
	    user_name = "Unknown user";
	  dm('h',3, "postscript.c:PS_header() Username = %s\n", user_name);
	}
      printf("%%%%Page: Cover %ld\n", physical_page_no++);
      printf("70 70 moveto\n");
      printf("/Helvetica findfont 10 scalefont setfont\n");
      printf("(Trueprint %s) show\n", version);
      printf("70 725 moveto\n");
      printf("/Helvetica-Bold findfont 20 scalefont setfont\n");
      printf("(For: %s) show\n", user_name);
      printf("70 700 moveto\n");
      printf("(Printed on: %s) show\n",expand_string("%L", TRUE));
      if (print_body == TRUE)
	{
	  printf("70 675 moveto\n");
	  printf("(Last page number: %ld) show\n", total_pages);
	  for (file_index = 0; file_index < (no_of_files>20?20:no_of_files); file_index++)
	    printf("70 %d moveto (File: %s) show\n", 650 - (file_index*25), file_name(file_index));
	  if (no_of_files > 20)
	    printf("70 150 moveto (Etc....) show\n");
	}
      printf("showpage\n");
      if (no_of_sides == '2')
	{
	  printf("%%%%Page: Coverback %ld\n", physical_page_no++);
	  printf("showpage\n");
	}
    }

}

/*
 * Function:
 *	PS_side_full
 * Returns TRUE if this is the last logical page on a side.
 */
boolean
PS_side_full(void)
{
  boolean retval = FALSE;

  if (logical_page_no != 0)
    {
      retval = ((logical_page_no % logical_pages_on_physical_page) == 0);
    }

  dm('O',2, "postscript.c:PS_side_full() returning %d\n", retval);

  return retval;
}

/*
 * Function:
 *	PS_endpage
 * decides whether to print out `showpage' or not and updates various
 * other bits & pieces.
 * Returns true if this is the last page on a sheet.
 */
boolean
PS_endpage(boolean print_page)
{
  static boolean page_has_printing = FALSE;
  boolean reached_end_of_sheet;

  /*
   * If this logical page is to be printed then the overall physical page
   * needs to be printed out.
   */
  if (print_page)
    {
      page_has_printing = TRUE;
    }

  /*
   * Remember if we have reached the end of a sheet
   */
  reached_end_of_sheet = (((no_of_sides == '2')
			   && left_page
			   && ((logical_page_no % logical_pages_on_physical_page) == 0))
			  || ((no_of_sides == '1')
			   && ((logical_page_no % logical_pages_on_physical_page) == 0))
			  );

  /*
   * If the previous side is now full, then we're moving on to a new
   * physical page and we may need to change left/right page info.  We
   * also need to print out either a showpage or delete the output from postscript
   */
  if (PS_side_full())
    {

      physical_page_no++;

      if (no_of_sides == '2')
	{
	  left_page = !left_page;
	  right_page = !right_page;
	}

      if (pass == 1)
	{
	  if (page_has_printing)
	    {
	      printf("showpage\n");
	    }
	  else
	    {
	      printf("erasepage initgraphics\n");
	    }
	}
      page_has_printing = FALSE;
    }

  return reached_end_of_sheet;
}

/*
 * Function:
 *	PS_startpage
 * prints out parameters in the header and footer boxes, and print other
 * page-furniture.  Maintains logical_page_no.
 *
 * print_headers indicates whether this page should be printed or not.
 * Actually the whole page is always printed - however by the end of the
 * sheet if print_headers was false for every logical page on that
 * sheet then the output is thrown away.
 */
void
PS_startpage(char *head1, char *head2, char *head3,
	     char *foot1, char *foot2, char *foot3,
	     char *msg_string,
             long page_no,long hdr_total_pages,
	     boolean index_page)

{
  short	message_pointsize;
  char	*message;
  unsigned short gap;

  logical_page_no++;

  dm('O',2, "postscript.c:PS_startpage(), logical page = %d, physical page = %d\n", logical_page_no, physical_page_no);

  if (pass == 0) return;

  dm('h',4,"Printing page %s/%s/%s %s/%s/%s + %s, page %d, total %d, index %d\n",
     head1, head2, head3, foot1, foot2, foot3, msg_string, page_no, hdr_total_pages, index_page);

  dm('h',4,"postscript.c:PS_startpage layout %d no_of_sides %c page_no %d\n", layout, no_of_sides, page_no);

  message = expand_string(msg_string, index_page);

  /*
   * Work out if we need to indent for holepunch or not
   */
  if (!no_holepunch && right_page)
    gap = HOLEPUNCH_WIDTH - pos_left;
  else
    gap = 0;

  /*
   * Next translate and rotate as appropriate for 1-on-1, 2-on-1, 3-on-1 or 4-on-1
   * Also print the page header if this is the first logical page on a physical
   * page.
   */

  switch (layout)
    {
    case ONE_ON_ONE:
      printf("%%%%Page: %d %ld\n",logical_page_no, physical_page_no);
      if (orientation == 'p') printf("%d %d translate\n", 	      pos_left+gap,  pos_bottom);
      if (orientation == 'l') printf("%d %d translate 90 rotate\n", pos_right+gap, pos_bottom);
      break;
    case TWO_ON_ONE:
      switch (logical_page_no & 1)
	{
	case 1:
	  printf("%%%%Page: %d %ld\n",logical_page_no, physical_page_no);
	  printf("gsave\n");
	  if (orientation == 'p')
	    printf("%d %d translate .64 .64 scale 90 rotate\n", pos_right+gap, pos_bottom);
	  else
	    printf("%d %d translate .64 .64 scale\n",           pos_left+gap,  pos_centre_top);
	  break;
	case 0:
	  printf("grestore\n");
	  if (orientation == 'p')
	    printf("%d %d translate .64 .64 scale 90 rotate\n", pos_right+gap, pos_centre_top);
	  else
	    printf("%d %d translate .64 .64 scale\n",           pos_left+gap,  pos_bottom);
	}
      break;
    case THREE_ON_ONE:
      switch (logical_page_no & 1)
	{
	case 1:
	  printf("%%%%Page: %d %ld\n",logical_page_no, physical_page_no);
	  printf("gsave\n");
	  if (orientation == 'p')
	    printf("%d %d translate .5 .5 scale\n",           pos_left+gap,        pos_bottom);
	  else
	    printf("%d %d translate .5 .5 scale 90 rotate\n", pos_centre_left+gap, pos_bottom);
	  break;
	case 0:
	  printf("grestore\n");
	  if (orientation == 'p')
	    printf("%d %d translate .5 .5 scale\n",           pos_centre_right+gap, pos_bottom);
	  else
	    printf("%d %d translate .5 .5 scale 90 rotate\n", pos_right+gap,        pos_bottom);
	}
      break;
    case FOUR_ON_ONE:
      switch (logical_page_no & 3)
	{
	case 1:
	  printf("%%%%Page: %d %ld\n",logical_page_no, physical_page_no);
	  printf("gsave\n");
	  if (orientation == 'p')
	    printf("%d %d translate .5 .5 scale\n",           pos_left+gap,        pos_centre_top);
	  else
	    printf("%d %d translate .5 .5 scale 90 rotate\n", pos_centre_left+gap, pos_bottom);
	  break;
	case 2:
	  printf("grestore gsave\n");
	  if (orientation == 'p')
	    printf("%d %d translate .5 .5 scale\n",           pos_left+gap,  pos_bottom);
	  else
	    printf("%d %d translate .5 .5 scale 90 rotate\n", pos_right+gap, pos_bottom);
	  break;
	case 3:
	  printf("grestore gsave\n");
	  if (orientation == 'p')
	    printf("%d %d translate .5 .5 scale\n",           pos_centre_right+gap, pos_centre_top);
	  else
	    printf("%d %d translate .5 .5 scale 90 rotate\n", pos_centre_left+gap,  pos_centre_top);
	  break;
	case 0:
	  printf("grestore\n");
	  if (orientation == 'p')
	    printf("%d %d translate .5 .5 scale\n",           pos_centre_right+gap, pos_bottom);
	  else
	    printf("%d %d translate .5 .5 scale 90 rotate\n", pos_right+gap,        pos_centre_top);
	}
      break;
    case NO_LAYOUT:
      abort();
    }

  /*
   * Use the appropriate postscript macro to print the page frame, including header
   * and footer boxes and vertical lines
   */
  if (!index_page && ((no_show_line_number == FALSE) || (no_show_indent_level == FALSE)))
    printf("Cp ");
  else
    printf("Ip ");

  /*
   * Next print out the message string
   */
  if ((message != NULL) && (strlen(message) != 0))
    {
      message_pointsize = printable_width / strlen(message);
      printf("gsave\n");
      printf(".9 setgray\n");
      printf("Rm 2 div Tm 2 div translate ");
      if (orientation == 'l')
	printf("30 rotate\n");
      else
	printf("60 rotate\n");
      printf("/Courier-Bold findfont %d scalefont setfont ",
		   message_pointsize);
      printf("-216 0 moveto (%s) show\n", message);
      printf("grestore\n");
    }

  if (include_headers)
    {
      /*
       * Set up the font and Ypos to print the header strings
       */
      printf("HF setfont /Ypos Tm Bh sub Ps 2 div add def\n");
    
      balance_strings(head1, head2, head3, page_no, index_page);
    }

  if (include_footers)
    {
      /*
       * Set up the font and Ypos to print the footer strings
       */
      printf("HF setfont /Ypos Ps 2 div def\n");

      balance_strings(foot1, foot2, foot3, page_no, index_page);
    }

  if (index_page)
    {
      printf("CF setfont ");
    }

  printf("/Vpos Tm Bh sub Ps sub def\n");
}

/*
 * Function:
 *	PS_end_output()
 * 
 */
void
PS_end_output(void)
{
  printf("%%%%Trailer\n");
  printf("%%%%EOF\n");
}

/*
 * Function:
 *	balance_strings()
 */
void
balance_strings(char *string1, char *string2, char *string3, long page_no, boolean index_page)
{
  char	*s1, *s2, *s3;

  if (right_page)
    {
      s1 = strdup(expand_string(string1, index_page));
      s2 = strdup(expand_string(string2, index_page));
      s3 = strdup(expand_string(string3, index_page));
    }
  else
    {
      s1 = strdup(expand_string(string3, index_page));
      s2 = strdup(expand_string(string2, index_page));
      s3 = strdup(expand_string(string1, index_page));
    }
  /* start  end1  start2  mid2  end2  start3  end3   */
  /*      s1               s2               s3       */
  printf("Ps Ypos moveto\n");
  printf("(%s) show ",s1);		/* print first string at extreme left hand edge */
  printf("currentpoint pop dup ");		/* . end1 end1 */
  printf("Rm Ps sub ");				/* . end1 end1 end3 */
  printf("(%s) stringwidth pop ", s3);	/* . end1 end1 end3 length3 */
  printf("sub dup Ypos moveto\n");		/* . end1 end1 start3 */
  printf("(%s) show\n",s3);
  printf("exch sub 2 div add ");			/* . mid2 */
  printf("(%s) stringwidth pop ",s2);	/* . mid2 length2 */
  printf("2 div sub Ypos moveto\n");		/* . */
  printf("(%s) show\n",s2);
}

/*
 * Function:
 * 	PS_pagesize()
 * sets up all variables corresponding to page size & related measurements, using
 * margins from printer_stats().
 * Specifically if length is unspecified then it is calculated from pointsize,
 * else pointsize is calculated from length.
 * Then width is calculated from pointsize.
 */
void
PS_pagesize(char *printer, int *width_ptr, int *length_ptr)

{
  float margin_height;
  short tmp_sides;

  printer_stats(printer, &tmp_sides, &left_margin, &right_margin, &top_margin, &bottom_margin);

  if (no_of_sides == '0')
    {
      if (tmp_sides == 1)
	no_of_sides = '1';
      else
	no_of_sides = '2';
    }

  dm('O',3,"postscript.c:PS_pagesize left %d right %d top %d bottom %d\n", left_margin, right_margin, top_margin, bottom_margin);
  dm('O',3,"postscript.c:PS_pagesize pointsize %d\n", pointsize);

  if (HOLEPUNCH_WIDTH < left_margin) no_holepunch = TRUE;

  if (!no_holepunch)
    printable_width = right_margin - HOLEPUNCH_WIDTH;
  else
    printable_width = right_margin - left_margin;

  if (!no_top_holepunch)
    printable_height = top_margin - bottom_margin - HOLEPUNCH_HEIGHT;
  else
    printable_height = top_margin - bottom_margin;

  if ((*length_ptr == -1) && (pointsize == 0))
    abort();

  if (layout == ONE_ON_ONE)
    {
      logical_pages_on_physical_page = 1;
      if (orientation == 'p')
	{
	  logical_pages_rotated = FALSE;
	  virtual_width = printable_width;
	  virtual_height = printable_height;
	}
      else
	{
	  logical_pages_rotated = TRUE;
	  virtual_width = printable_height;
	  virtual_height = printable_width;
	}
    }
  else if (layout == TWO_ON_ONE)
    {
      logical_pages_on_physical_page = 2;
      if (orientation == 'p')
	{
	  logical_pages_rotated = TRUE;
	  virtual_width = printable_height/2 * 1.56 - 3;
	  virtual_height = printable_width * 1.56;
	}
      else
	{
	  logical_pages_rotated = FALSE;
	  virtual_width = printable_width * 1.56;
	  virtual_height = printable_height/2 * 1.56 - 3;
	}
    }
  else if (layout == THREE_ON_ONE)
    {
      logical_pages_on_physical_page = 2;
      if (orientation == 'p')
	{
	  logical_pages_rotated = FALSE;
	  virtual_width = printable_width - 4;
	  virtual_height = printable_height * 2;
	}
      else
	{
	  logical_pages_rotated = TRUE;
	  virtual_width = printable_height * 2;
	  virtual_height = printable_width - 4;
	}
    }
  else
    {
      logical_pages_on_physical_page = 4;
      if (orientation == 'p')
	{
	  logical_pages_rotated = FALSE;
	  virtual_width = printable_width - 4;
	  virtual_height = printable_height - 4;
	}
      else
	{
	  logical_pages_rotated = TRUE;
	  virtual_width = printable_height - 4;
	  virtual_height = printable_width - 4;
	}
    }

  pos_left          = left_margin;	
  pos_centre_left   = left_margin + printable_width/2 - 2;
  pos_centre_right  = left_margin + printable_width/2 + 2;
  pos_right         = left_margin + printable_width;
  pos_bottom        = bottom_margin;
  pos_centre_bottom = bottom_margin + printable_height/2 - 2;
  pos_centre_top    = bottom_margin + printable_height/2 + 2;
  pos_top           = bottom_margin + printable_height;

  dm('O',3,"postscript.c:PS_pagesize virtual width %d, virtual height %d\n", virtual_width, virtual_height);
  dm('O',3,"postscript.c:PS_pagesize specified width %d, specified length %d\n", *width_ptr, *length_ptr);

  if (!include_headers)
    header_box_height = 0;
  else
    header_box_height = 1.5 * pointsize;

  if (!include_footers)
    footer_box_height = 0;
  else
    footer_box_height = 1.5 * pointsize;

  margin_height = header_box_height + footer_box_height;

  if (*length_ptr == -1) 
    *length_ptr = (virtual_height - margin_height) / (pointsize + interline_gap);
  else
    pointsize = (short)(virtual_height - (interline_gap * *length_ptr))/(3 + *length_ptr);

  dm('O',3,"virtual_width = %d, pointsize = %d %d\n", virtual_width, pointsize, 0.6*pointsize);
  if (*width_ptr == -1)
    *width_ptr = 1.66 * virtual_width / pointsize;
  dm('O',3,"postscript.c:PS_pagesize width to be used %d, length to be used %d\n", *width_ptr, *length_ptr);

  if (((no_show_line_number == FALSE) || (no_show_indent_level == FALSE)) && (*width_ptr >= 8))
    (*width_ptr) -= 8;
	
  dm('O',3,"postscript.c:PS_pagesize width to be used %d, length to be used %d\n", *width_ptr, *length_ptr);
}	
