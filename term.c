#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "abuf.h"
#include "core.h"
#include "term.h"

#define ESCSEQ "\x1b["


static struct termios saved_entry_term;


void
enable_termraw(void)
{
	struct termios raw;

	if (tcgetattr(STDIN_FILENO, &raw) == -1) {
		die("tcgetattr while enabling raw mode");
	}

	cfmakeraw(&raw);
	raw.c_cc[VMIN]  = 0;
	raw.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
		die("tcsetattr while enabling raw mode");
	}
}


void
display_clear(abuf *ab)
{
	if (ab == NULL) {
		kwrite(STDOUT_FILENO, ESCSEQ "2J", 4);
		kwrite(STDOUT_FILENO, ESCSEQ "H", 3);
	} else {
		ab_append(ab, ESCSEQ "2J", 4);
		ab_append(ab, ESCSEQ "H", 3);
	}
}


void
disable_termraw(void)
{
	display_clear(NULL);

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_entry_term) == -1) {
		die("couldn't disable terminal raw mode");
	}
}


void
setup_terminal(void)
{
	if (tcgetattr(STDIN_FILENO, &saved_entry_term) == -1) {
		die("can't snapshot terminal settings");
	}

	enable_termraw();
}


int
get_winsz(size_t *rows, size_t *cols)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    }

    *cols = (size_t)ws.ws_col;
    *rows = (size_t)ws.ws_row;

    return 0;
}

