/*
 * Include file:
 *	trueprint.h
 */

#define CMD_NAME "trueprint"

/*
 * TP_ADMIN_USER contains the mail address of the administrator.
 * You should change this address!
 */
#define TP_ADMIN_USER "root@localhost"

/*
 * Default settings for options, only needed if the defaults that are
 * hard-coded are not good enough.
 */
#define DEFAULT_OPTS ""

/*
 * LOGTOOL is set to the tool to be used to log invocations of trueprint.  To turn
 * off logging, comment this line out
 */
/* #define LOGTOOL ... */

/*
 * Maximum length of a command to be passed to system()
 */
#define COMMAND_LEN	2000

/*
 * Maximum length of an input or output line for diff, indexes, etc.
 */
#define INPUT_LINE_LEN  200

/*
 * Maximum length of a symbol, in the language parsing stuff
 */
#define SYMBOL_LEN      70

/*
 * Maximum line length of an output line for source code
 */
#define MAXLINELENGTH	2000

/*
 * Buffer size - input is read in in blocks this size
 */
#define BUFFER_SIZE 4096

/*
 * Postscript max and min values
 */
#define MAXPOINTSIZE 50
#define MINPOINTSIZE 5
#define MAXINTERLINE_GAP 10
#define MININTERLINE_GAP 0

/*
 * Maximum size of greenlines
 */
#define MAXGREENLINES 10

/*
 * HOLEPUNCH_WIDTH is the space, in points, left blank for holepunches.
 * 54 points is 3/4"
 */
#define HOLEPUNCH_WIDTH 54

/*
 * HOLEPUNCH_HEIGHT is the space, in points, left blank for two-hole 
 * holepunches (that punch at the top of the page).
 * 36 points is 1/2"
 */
#define HOLEPUNCH_HEIGHT 36

/*
 * Characters that we can break lines on.  BREAKSLENGTH is set to the
 * number of these characters
 */
#define BREAKS	";({,. 	"
#define BREAKSLENGTH	7

#define PUTS(x)	(void)fputs(x,stdout)

#define BACKSPACE (char)8

typedef enum {
	CHAR_NORMAL,
	CHAR_ITALIC,
        CHAR_BOLD,
	CHAR_UNDERLINE
} char_status;

typedef enum {
	STREAM_EMPTY_FILE = -1,
	STREAM_OK = 0x0,
	STREAM_PAGE_END = 0x1,
	STREAM_FUNCTION_END = 0x2,
	STREAM_FILE_END = 0x4
} stream_status;

#undef TRUE
#undef FALSE

typedef enum {
	UNSET = -1,
	FALSE = 0,
	TRUE = 1
} boolean;
