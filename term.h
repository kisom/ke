#ifndef TERM_H
#define TERM_H

#include "abuf.h"

/* Terminal control/setup API */
void enable_termraw(void);
void disable_termraw(void);
void setup_terminal(void);
void display_clear(abuf *ab);

/*
 * get_winsz uses the TIOCGWINSZ to get the window size.
 *
 * there's a fallback way to do this, too, that involves moving the
 * cursor down and to the left \x1b[999C\x1b[999B. I'm going to skip
 * on this for now because it's bloaty and this works on OpenBSD and
 * Linux, at least.
 */
int  get_winsz(int *rows, int *cols);

#endif /* TERM_H */
