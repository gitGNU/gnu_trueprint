/*
 * File:
 *	options.h
 * Supports option processing.
 * Declaration functions are used to declare an option:
 * - specify either var or handler, not both.
 * - handler will be called just once.
 */

typedef enum option_class {
  OPT_MISC,		/* Miscellaneous options */
  OPT_PAGE_FURNITURE,	/* Options that affect page furniture */
  OPT_TEXT_FORMAT,	/* Options that affect text layout */
  OPT_PRINT,		/* Options that (de)select stuff to print */
  OPT_PAGE_FORMAT,	/* Options that affect overall page presentation */
  OPT_OUTPUT		/* Options that affect or control where output goes */
} option_class;

void		setup_options(void);
void		handle_string_options(char *);
unsigned int	handle_options(int, char **);
void 		set_option_defaults(void);
void            print_usage_msgs(option_class);

	/*
	 * noparm option - option with no parameter
	 * If default_opt is true then the handler will always
	 * be called.  It is up to the handler to work out if this
	 * option should be obeyed or not.  This is intended to
	 * handle the case e.g. for -1, -2, -3 or -4 where
	 * -1 is the default - the handler will remember if
	 * any other option was invoked first.  If invoked
	 * because of the default_opt flag then set_default will
	 * be used.
	 * Must specify a handler and a set_default.
	 */
void noparm_option(char *c, char *s,
		   boolean default_opt,
		   void (*handler)(const char *p, const char *s),
		   void (*set_default)(void),
		   option_class class,
		   char *help_string);

/* option that takes an optional string */
void optional_string_option(char *c, char *s,
			    void (*handler)(const char *p, const char *s, char *value),
			    option_class class,
			    char *help_string);

	/*
	 * boolean option - either y or n
	 */
void boolean_option(char *c, char *s1, char *s2, boolean default_value,
		    boolean *var,
		    void (*handler)(const char *p, const char *s, boolean value),
		    void (*set_default)(boolean value),
		    option_class class,
		    char *true_help_string,
		    char *false_help_string);

void choice_option(char *c, char *s1, char *s2,
		   char choice1, char choice2,
		   char *var,
		   void (*handler)(const char *p, const char *s, char value),
		   void (*set_default)(char value),
		   option_class class,
		   char *choice1_help_string,
		   char *choice2_help_string);

	/* char option - one of a set of characters */
void char_option(char *c, char *s, char default_value,
		 char *valid_set,
		 char *var,
		 void (*handler)(const char *p, const char *s, char value, char *var),
		 void (*set_default)(char value),
		 option_class class,
		 char *help_string);

	/* short & int options */
void short_option(char *c, char *s, short default_value,
		  char *special, short special_value,
		  short min, short max,
		  short *var,
		  void (*handler)(const char *p, const char *s, short value, short min, short max),
		  void (*set_default)(short value),
		  option_class class,
		  char *help_string,
		  char *special_help_string);

void int_option(char *c, char *s, int default_value,
		char *special, int special_value,
		int min, int max,
		int *var,
		void (*handler)(const char *p, const char *s, int value, int min, int max),
		void (*set_default)(int value),
		option_class class,
		char *help_string,
		char *special_help_string);

	/* string option */
void string_option(char *c, char *s, char *default_value,
		   char **var,
		   void (*handler)(const char *p, const char *s, char *value),
		   void (*set_default)(char *value),
		   option_class class,
		   char *help_string);

	/* flag option for setting string */
void flag_string_option(char *c, char *s1, char *s2, boolean default_value,
			char *true_value, char *false_value,
			char **var,
			void (*handler)(const char *p, const char *s, boolean value, char *true_value, char *false_value),
			void (*set_default)(boolean value, char *string),
			option_class class,
			char *set_help_string,
			char *not_set_help_string);


