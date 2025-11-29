#ifndef KE_EDITING_H
#define KE_EDITING_H

#include <stdint.h>

#include "abuf.h"


/* miscellaneous */
void		 file_open_prompt_cb(char *buf, int16_t key);
int		 erow_render_to_cursor(const abuf *row, int cx);
int		 erow_cursor_to_render(abuf *row, int rx);
int		 erow_init(abuf *row, int len);
void		 erow_insert(int at, char *s, int len);
void		 jump_to_position(size_t col, size_t row);
void		 goto_line(void);
int	    	 cursor_at_eol(void);
int 		 iswordchar(unsigned char c);
void		 find_next_word(void);
void		 delete_next_word(void);
void		 find_prev_word(void);
void		 delete_prev_word(void);
void		 delete_row(size_t at);
void		 row_append_row(abuf *row, const char *s, int len);
void		 row_insert_ch(abuf *row, int at, int16_t c);
void		 row_delete_ch(abuf *row, int at);
void		 insertch(int16_t c);
void		 deletech(uint8_t op);
void		 open_file(const char *filename);
char		*rows_to_buffer(int *buflen);
int     	 save_file(void);
uint16_t	 is_arrow_key(int16_t c);
int16_t		 get_keypress(void);
char		*editor_prompt(const char*, void (*cb)(char*, int16_t));
void		 editor_find_callback(char *query, int16_t c);
void		 editor_find(void);
void		 editor_openfile(void);
int	    	 first_nonwhitespace(abuf *row);
void		 move_cursor_once(int16_t c, int interactive);
void		 move_cursor(int16_t c, int interactive);
void		 uarg_start(void);
void		 uarg_digit(int d);
void		 uarg_clear(void);
int	    	 uarg_get(void);
void		 newline(void);
char		*get_cloc_code_lines(const char *filename);
int	    	 dump_pidfile(void);


#endif
