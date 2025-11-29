#include <sys/ioctl.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "abuf.h"
#include "core.h"
#include "buffer.h"
#include "editing.h"
#include "editor.h"
#include "process.h"
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


void
draw_rows(abuf *ab)
{
	abuf	*row              = NULL;
	char	 buf[editor.cols];
	char	 c                = 0;
	size_t	 j                = 0;
	size_t	 filerow          = 0;
	size_t	 y                = 0;
	size_t	 len              = 0;
	size_t	 padding          = 0;
	size_t	 printed          = 0;
	size_t	 rx               = 0;

	for (y = 0; y < editor.rows; y++) {
		filerow = y + EROWOFFS;
		if (filerow >= ENROWS) {
			if ((ENROWS == 0) && (y == editor.rows / 3)) {
				len = snprintf(buf,
				               sizeof(buf),
				               "%s",
				               KE_VERSION);
				padding = (editor.rows - len) / 2;

				if (padding) {
					ab_append(ab, "|", 1);
					padding--;
				}

				while (padding--)
					ab_append(ab, " ", 1);
				ab_append(ab, buf, len);
			} else {
				ab_append(ab, "|", 1);
			}
		} else {
			row = &EROW[filerow];
			j = 0;
			rx = printed = 0;

			while (j < row->size && printed < editor.cols) {
				c = row->b[j];

				if (rx < ECOLOFFS) {
					if (c == '\t') rx += (TAB_STOP - (rx % TAB_STOP));
					else if (c < 0x20) rx += 3;
					else rx++;
					j++;
					continue;
				}

				if (c == '\t') {
					int sp = TAB_STOP - (rx % TAB_STOP);
					for (int k = 0; k < sp && printed < editor.cols; k++) {
						ab_appendch(ab, ' ');
						printed++;
						rx++;
					}
				} else if (c < 0x20) {
					char seq[4];
					snprintf(seq, sizeof(seq), "\\%02x", c);
					ab_append(ab, seq, 3);
					printed += 3;
					rx += 3;
				} else {
					ab_appendch(ab, c);
					printed++;
					rx++;
				}
				j++;
			}
			len = printed;
		}
		ab_append(ab, ESCSEQ "K", 3);
		ab_append(ab, "\r\n", 2);
	}
}


char
status_mode_char(void)
{
	switch (editor.mode) {
	case MODE_NORMAL:
		return 'N';
	case MODE_KCOMMAND:
		return 'K';
	case MODE_ESCAPE:
		return 'E';
	default:
		return '?';
	}
}


void
draw_status_bar(abuf *ab)
{
	char	 status[editor.cols];
	char	 rstatus[editor.cols];
	char	 mstatus[editor.cols];
	size_t	 len                   = 0;
	size_t	 rlen                  = 0;

	len = snprintf(status,
		       sizeof(status),
		       "%c%cke: %.20s - %lu lines",
		       status_mode_char(),
		       EDIRTY ? '!' : '-',
		       EFILENAME ? EFILENAME : "[no file]",
                ENROWS);

	if (EMARK_SET) {
		snprintf(mstatus,
		         sizeof(mstatus),
		         " | M: %lu, %lu ",
		         EMARK_CURX + 1,
		         EMARK_CURY + 1);
	} else {
		snprintf(mstatus, sizeof(mstatus), " | M:clear ");
	}

	rlen = snprintf(rstatus,
	                sizeof(rstatus),
	                "L%lu/%lu C%lu %s",
	                ECURY + 1,
	                ENROWS,
	                ECURX + 1,
	                mstatus);

	ab_append(ab, ESCSEQ "7m", 4);
	ab_append(ab, status, len);
	while (len < editor.cols) {
		if (editor.cols - len == rlen) {
			ab_append(ab, rstatus, rlen);
			break;
		}
		ab_append(ab, " ", 1);
		len++;
	}

	ab_append(ab, ESCSEQ "m", 3);
	ab_append(ab, "\r\n", 2);
}


void
draw_message_line(abuf *ab)
{
	size_t	 len = strlen(editor.msg);

	ab_append(ab, ESCSEQ "K", 3);
	if (len > editor.cols) {
		len = editor.cols;
	}

	if (len && time(NULL) - editor.msgtm < MSG_TIMEO) {
		ab_append(ab, editor.msg, len);
	}
}


void
scroll(void)
{
	const abuf	*row = NULL;

	ERX = 0;
	if (ECURY < ENROWS) {
		row = &EROW[ECURY];
		ERX = erow_render_to_cursor(row, ECURX);
	}

	if (ECURY < EROWOFFS) {
		EROWOFFS = ECURY;
	}

	if (ECURY >= EROWOFFS + editor.rows) {
		EROWOFFS = ECURY - editor.rows + 1;
	}

	if (ERX < ECOLOFFS) {
		ECOLOFFS = ERX;
	}

	if (ERX >= ECOLOFFS + editor.cols) {
		ECOLOFFS = ERX - editor.cols + 1;
	}
}


void
display_refresh(void)
{
	char	 buf[32] = {0};
	abuf	 ab      = ABUF_INIT;

	scroll();

	ab_append(&ab, ESCSEQ "?25l", 6);
	ab_append(&ab, ESCSEQ "H", 3);
	display_clear(&ab);

	draw_rows(&ab);
	draw_status_bar(&ab);
	draw_message_line(&ab);

	snprintf(buf,
	         sizeof(buf),
	         ESCSEQ "%lu;%luH",
	         (ECURY - EROWOFFS) + 1,
	         (ERX - ECOLOFFS) + 1);
	ab_append(&ab, buf, kstrnlen(buf, 32));
	/* ab_append(&ab, ESCSEQ "1;2H", 7); */
	ab_append(&ab, ESCSEQ "?25h", 6);

	kwrite(STDOUT_FILENO, ab.b, (int)ab.size);
	ab_free(&ab);
}
