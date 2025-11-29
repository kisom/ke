#ifndef KE_TERM_H
#define KE_TERM_H

#include "abuf.h"


/* Terminal control/setup API */
void		 enable_termraw(void);
void		 disable_termraw(void);
void		 setup_terminal(void);
void		 display_clear(abuf *ab);
void		 draw_rows(abuf *ab);
char		 status_mode_char(void);
void		 draw_status_bar(abuf *ab);
void		 draw_message_line(abuf *ab);
void		 scroll(void);
void		 display_refresh(void);

/*
 * get_winsz uses the TIOCGWINSZ to get the window size.
 *
 * there's a fallback way to do this, too, that involves moving the
 * cursor down and to the left \x1b[999C\x1b[999B. I'm going to skip
 * on this for now because it's bloaty and this works on OpenBSD and
 * Linux, at least.
 */
int  get_winsz(size_t *rows, size_t *cols);


#endif /* KE_TERM_H */
