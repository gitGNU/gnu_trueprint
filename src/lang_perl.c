/*
 * Source file:
 * lang_perl.c
 *
 * Contains get_perl_char() which parses perl code.
 *
 * Pod functionality added by Daniel Wagenaar
 */


#include "config.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "trueprint.h"
#include "main.h"
#include "input.h"
#include "index.h"
#include "language.h"
#include "output.h"


#include "lang_perl.h"


/*
 * Public part
 */
char            lang_perl_defaults[] = "--i --F --f";


/*
 * Private part
 */


typedef enum {
  IP_CODE,
  IP_STRING,
  IP_COMMENT,
  IP_STRING_ESCAPE,
  IP_POD,
  IP_INPOD,
  IP_POD_END,
} perl_ip_states;


typedef enum {
  Q_LOOKING,
  Q_SAWLT,
  Q_SAWLTLT,
  Q_SAWQ,
  Q_IDENT,
  Q_STALLED,
  Q_AFTER_HEREDOC_SIGNAL,
  Q_WITHIN_HEREDOC_FINDING,
  Q_WITHIN_HEREDOC,
} perl_q_states;


typedef enum {
  SUB_INIT,
  SUB_BOL,
  SUB_S,
  SUB_SU,
  SUB_SUB,
  SUB_FOUND,
  SUB_NAME,
} perl_sub_states;


typedef unsigned char byte;
char nestable_chars[256];
static boolean perl_has_been_setup = FALSE;

static void perl_setup(void)
{
  memset(nestable_chars, 0, sizeof(nestable_chars));
  nestable_chars[(byte) '('] = ')';
  nestable_chars[(byte) '['] = ']';
  nestable_chars[(byte) '{'] = '}';
  nestable_chars[(byte) '<'] = '>';
  nestable_chars[(byte) '`'] = '\'';
  perl_has_been_setup = TRUE;
}


static int isword(int c) { return isalnum(c) || c == '_'; }


static int isidentstart(int c) { return isalpha(c) || c == '_'; }


typedef enum {
  MYCHAR_NORMAL,
  MYCHAR_FUNCTION,
  MYCHAR_STRING,
  MYCHAR_COMMENT,
} my_char_status;

static void note(char c,
                 perl_ip_states state,
                 perl_q_states q_state,
                 perl_sub_states sub_state,
                 my_char_status status,
                 char end_token);


static stream_status
my_get_perl_char(char *input_char, my_char_status *status)
{
  static perl_ip_states state;
  static perl_sub_states sub_state;
  static perl_q_states q_state;


  static char end_token;
  static boolean line_start = FALSE;
  static int hd_check_index = 0;
  static int hd_end_index = 0;
  static char hd_end[SYMBOL_LEN];
  static int brace_count = 0;
  static int q_count = 0;
  static boolean in_function;


  static int previous_newlines = 0;


  stream_status retval;


  *status = MYCHAR_NORMAL;


  if (restart_language == TRUE) {
    state = IP_CODE;
    sub_state = SUB_INIT;
    q_state = Q_LOOKING;
    q_count = 0;
    restart_language    = FALSE;
    brace_count = 0;
    previous_newlines = 0;
    if (!perl_has_been_setup) perl_setup();
  }


  // If previous character set line_start, increment number of
  // newlines just seen.
  if (line_start)
    previous_newlines++;
  else
    previous_newlines = 0;


  line_start = FALSE;


  retval = getnextchar(input_char);


  switch (state) {
  case IP_CODE:
    switch (*input_char) {


    case '"':
    case '\'':
      if (q_state != Q_SAWLTLT) {
        state = IP_STRING;
        end_token = *input_char;
      }
      break;


    case '\n':
      line_start = TRUE;
      break;


    case '=':
      if (previous_newlines >= 2) {
        state = IP_POD;
        *status = MYCHAR_COMMENT;
      }
      break;


    case '#':
      state = IP_COMMENT;
      *status = MYCHAR_COMMENT;
      break;


    case '{':
      brace_count++;
      break;


    case '}':
      brace_count--;
      if (brace_count == 0 && in_function) {
        end_function(page_number);
        in_function = FALSE;
        retval |= STREAM_FUNCTION_END;
      }
      if (brace_count < 0) brace_count = 0;
      break;
    }
    break;


  case IP_POD:
    *status = MYCHAR_COMMENT;
    state = IP_INPOD;
    break;


  case IP_INPOD:
    *status = MYCHAR_COMMENT;
    if (*input_char == '\n')
      line_start = TRUE;
    else if (*input_char == '=' && previous_newlines >= 2)
      state = IP_POD_END;
    break;


  case IP_POD_END:
    if (*input_char == '\n')
      state = IP_CODE;
    else
      *status = MYCHAR_COMMENT;
    break;


  case IP_COMMENT:
    if (*input_char == '\n') state = IP_CODE;
    *status = MYCHAR_COMMENT;
    break;


  case IP_STRING:
    if (*input_char == end_token) {
      state = IP_CODE;
      q_state = Q_STALLED; // Don't immediately start next string
    } else {
      *status = MYCHAR_STRING;
      if (*input_char == '\\') state = IP_STRING_ESCAPE;
    }
    break;
      
  case IP_STRING_ESCAPE:
    state = IP_STRING;
    *status = MYCHAR_STRING;
    break;
  }


  // Sub-states of IP_CODE for detecting strings. (Meaning, this state
  // machine is stalled whenever state is not IP_CODE)
  if (state == IP_CODE) {
    switch (q_state) {
    case Q_LOOKING:
      if (*input_char == 'q') {
        q_state = Q_SAWQ;
        q_count = 1;
      } else if (*input_char == '<')
        q_state = Q_SAWLT;
      else if (*input_char == '\'') {
        end_token = '\'';
        state = IP_STRING;
      } else if (*input_char == '"') {
        end_token = '"';
        state = IP_STRING;
      } else if (isword(*input_char))
        q_state = Q_IDENT;
      break;


    case Q_SAWLT:
      if (*input_char == '<') q_state = Q_SAWLTLT;
      else q_state = Q_LOOKING;
      hd_end_index = 0;
      break;


    case Q_SAWLTLT:
      if (isidentstart(*input_char))
        hd_end[hd_end_index++] = *input_char;
      else if (*input_char == '"' || *input_char == '\'')
        ; /* Do nothing */
      else if (hd_end_index > 0) {
        if (isword(*input_char))
          hd_end[hd_end_index++] = *input_char;
        else {
          state = IP_CODE;
          q_state = Q_AFTER_HEREDOC_SIGNAL;
        }
      } else
        q_state = Q_LOOKING;
      break;


    case Q_AFTER_HEREDOC_SIGNAL:
      if (*input_char == '\n') {
        q_state = Q_WITHIN_HEREDOC_FINDING;
        hd_check_index = 0;
      }
      break;


    // Currently matching heredoc terminator
    case Q_WITHIN_HEREDOC_FINDING:
      if (*input_char == '\n')
        hd_check_index = 0; // Start over search at beginning
      else if (*input_char == hd_end[hd_check_index]) {
        hd_check_index++;
        if (hd_check_index == hd_end_index) {
          // Found the complete terminator
          state = IP_CODE;
          q_state = Q_LOOKING;
        }
      } else if (hd_check_index == 0 && isspace(*input_char))
        ; /* Allow leading whitespace before heredoc terminator */
      else
        q_state = Q_WITHIN_HEREDOC; // Not found
      *status = MYCHAR_COMMENT;
      break;


    // Terminator not found at beginning of line; just consume line
    case Q_WITHIN_HEREDOC:
      if (*input_char == '\n') {
        hd_check_index = 0;
        q_state = Q_WITHIN_HEREDOC_FINDING;
      }
      *status = MYCHAR_COMMENT;
      break;


    case Q_SAWQ:
      if (q_count == 1) {
        if (*input_char == 'q') // qq(...)
          q_count++;
        else if (*input_char == 'x') // qx(...)
          q_count++;
        else if (*input_char == 'w') // qw(...)
          q_count++;
        if (q_count == 2) break;
      }
      if (isword(*input_char))
        q_state = Q_IDENT;
      else if (nestable_chars[(byte) *input_char]) {
        end_token = nestable_chars[(byte) *input_char];
        state = IP_STRING;
        q_state = Q_LOOKING; // But stalled
      } else if (isspace(*input_char)) {
        q_state = Q_LOOKING;
      } else {
        end_token = *input_char;
        state = IP_STRING;
        q_state = Q_LOOKING; // But stalled
      }
      break;


    case Q_IDENT:
      if (!isword(*input_char))
        q_state = Q_LOOKING;
      break;
    
    case Q_STALLED:
      q_state = Q_LOOKING;
      break;
    }
  }


  // Third state machine: finding subroutine declarations. This one is
  // reset to its initial state whenever anything returns to IP_CODE
  // from anything else.
  if (state != IP_CODE) {
    sub_state = SUB_INIT;
  } else {
    switch (sub_state) {
    case SUB_INIT:
      if (*input_char == '\n') sub_state = SUB_BOL;
      break;


    case SUB_BOL:
      if (*input_char == '\n')
        sub_state = SUB_BOL;
      else if (isspace(*input_char))
        /* do nothing */;
      else if (*input_char == 's')
        sub_state = SUB_S;
      else
        sub_state = SUB_INIT;
      break;


    case SUB_S:
      if (*input_char == 'u') sub_state = SUB_SU;
      else if (*input_char == '\n') sub_state = SUB_BOL;
      else sub_state = SUB_INIT;
      break;


    case SUB_SU:
      if (*input_char == 'b') sub_state = SUB_SUB;
      else if (*input_char == '\n') sub_state = SUB_BOL;
      else sub_state = SUB_INIT;
      break;


    case SUB_SUB:
      if (isspace(*input_char)) sub_state = SUB_FOUND;
      else if (*input_char == '\n') sub_state = SUB_BOL;
      else sub_state = SUB_INIT;
      break;


    case SUB_FOUND:
      if (isidentstart(*input_char)) {
        *status = MYCHAR_FUNCTION;
        sub_state = SUB_NAME;
      } else if (*input_char == '{')
        sub_state = SUB_INIT;
      else if (*input_char == '\n')
        sub_state = SUB_BOL;
      break;


    case SUB_NAME:
      if (isalnum(*input_char) || *input_char == '_') {
        *status = MYCHAR_FUNCTION;
        in_function = TRUE;
      } else if (*input_char == '\n')
        sub_state = SUB_BOL;
      else
        sub_state = SUB_INIT;
      break;
    }
  }


//    note(*input_char, state, q_state, sub_state, *status,
//         (state == IP_STRING) ? end_token : 0);


  return retval;
}


stream_status
get_perl_char(char *input_char, char_status *retstatus)
{
  my_char_status status;
  stream_status retval;
  static int function_start_page;
  static int function_start = 0;
  static char function_name[SYMBOL_LEN];
  static int function_name_index = 0;


  retval = my_get_perl_char(input_char, &status);


  if (function_start && status != MYCHAR_FUNCTION) {
    function_name[function_name_index] = '\0';
    add_function(function_name, function_start, char_number,
                 function_start_page, current_filename);
    function_start = 0;
  }


  switch (status) {
  case MYCHAR_FUNCTION:
    *retstatus = CHAR_BOLD;
    if (! function_start) {
        function_start = char_number;
        function_start_page = page_number;
        function_name_index=0;
    }
    function_name[function_name_index++] = *input_char;
    break;


  case MYCHAR_NORMAL:
    *retstatus = CHAR_NORMAL;
    break;


  case MYCHAR_STRING:
    *retstatus = CHAR_NORMAL;
//    *retstatus = CHAR_UNDERLINE;
    break;


  case MYCHAR_COMMENT:
    *retstatus = CHAR_ITALIC;
    break;
  }


  if (pass==1) *retstatus = get_function_name_posn(char_number,*retstatus);


  return retval;
} 

/******************************************************************************
 * Debugging function - include by uncommenting call above
 */	

static void note(char c,
                 perl_ip_states state,
                 perl_q_states q_state,
                 perl_sub_states sub_state,
                 my_char_status status,
                 char end_token)
{
  static int counter = 0;
  fprintf(stderr, "%05d: %02x ", counter++, (unsigned int) c);
  if (isprint(c)) fprintf(stderr, "(%c) ", c);
  switch (state) {
  case IP_CODE: fprintf(stderr, "IP_CODE "); break;
  case IP_STRING: fprintf(stderr, "IP_STRING "); break;
  case IP_COMMENT: fprintf(stderr, "IP_COMMENT "); break;
  case IP_STRING_ESCAPE: fprintf(stderr, "IP_STRING_ESCAPE "); break;
  case IP_POD: fprintf(stderr, "IP_POD "); break;
  case IP_INPOD: fprintf(stderr, "IP_INPOD "); break;
  case IP_POD_END: fprintf(stderr, "IP_POD_END "); break;
  }


  switch(q_state) {
  case Q_LOOKING: fprintf(stderr, "Q_LOOKING "); break;
  case Q_SAWLT: fprintf(stderr, "Q_SAWLT "); break;
  case Q_SAWLTLT: fprintf(stderr, "Q_SAWLTLT "); break;
  case Q_SAWQ: fprintf(stderr, "Q_SAWQ "); break;
  case Q_IDENT: fprintf(stderr, "Q_IDENT "); break;
  case Q_STALLED: fprintf(stderr, "Q_STALLED "); break;
  case Q_AFTER_HEREDOC_SIGNAL: fprintf(stderr, "Q_AFTER_HEREDOC_SIGNAL "); break;
  case Q_WITHIN_HEREDOC_FINDING: fprintf(stderr, "Q_WITHIN_HEREDOC_FINDING "); break;
  case Q_WITHIN_HEREDOC: fprintf(stderr, "Q_WITHIN_HEREDOC "); break;
  }


  switch (sub_state) {
  case SUB_INIT: fprintf(stderr, "SUB_INIT "); break;
  case SUB_BOL: fprintf(stderr, "SUB_BOL "); break;
  case SUB_S: fprintf(stderr, "SUB_S "); break;
  case SUB_SU: fprintf(stderr, "SUB_SU "); break;
  case SUB_SUB: fprintf(stderr, "SUB_SUB "); break;
  case SUB_FOUND: fprintf(stderr, "SUB_FOUND "); break;
  case SUB_NAME: fprintf(stderr, "SUB_NAME "); break;
  }


  if (end_token) {
    fprintf(stderr, "end_token=%c ", end_token);
  }


  fprintf(stderr, "=> ");


  switch (status) {
  case MYCHAR_NORMAL: fprintf(stderr, "MYCHAR_NORMAL "); break;
  case MYCHAR_FUNCTION: fprintf(stderr, "MYCHAR_FUNCTION "); break;
  case MYCHAR_STRING: fprintf(stderr, "MYCHAR_STRING "); break;
  case MYCHAR_COMMENT: fprintf(stderr, "MYCHAR_COMMENT "); break;
  }


  fprintf(stderr, "\n");
}

