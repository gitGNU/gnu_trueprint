/*
 * Source file:
 *	diffs.h
 */

extern void	setup_diffs(void);
extern void	init_diffs(char newfile[]);
extern void	end_diffs(void);
extern boolean	getdelline(long current_line, char *input_line, char_status input_status[]);
extern boolean	line_inserted(long current_line);
