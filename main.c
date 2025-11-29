/*
 * ke - kyle's editor
 *
 * ke started off following along with the kilo walkthrough at
 *  https://viewsourcecode.org/snaptoken/kilo/
 *
 * It is inspired heavily by mg(1) and VDE. This is a single file and
 * can be built with
 *	$(CC) -D_DEFAULT_SOURCE -D_XOPEN_SOURCE -Wall -Wextra -pedantic \
 *		-Wshadow -Werror -std=c99 -g -o ke main.c
 *
 * It builds and runs on Linux and Darwin. I can't confirm BSD compatibility.
 *
 * commit 59d3fa1dab68e8683d5f5a9341f5f42ef3308876
 * Author: Kyle Isom <kyle@imap.cc>
 * Date:   Fri Feb 7 20:46:43 2020 -0800
 *
 *   Initial import, starting with kyle's editor.
 */
#include <sys/ioctl.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>

#include "abuf.h"
#include "buffer.h"
#include "editor.h"
#include "core.h"
#include "term.h"


#ifndef KE_VERSION
#define KE_VERSION		"ke dev build"
#endif

#define ESCSEQ			"\x1b["
#define	CTRL_KEY(key)		((key)&0x1f)
#define TAB_STOP		8
#define MSG_TIMEO		3

/*
 * define the keyboard input modes
 * normal: no special mode
 * kcommand: ^k commands
 * escape: what happens when you hit escape?
 */
#define	MODE_NORMAL		0
#define	MODE_KCOMMAND		1
#define	MODE_ESCAPE		2


#define	TAB_STOP		8


#define KILLRING_NO_OP		0	/* don't touch the killring */
#define KILLRING_APPEND		1	/* append deleted chars */
#define KILLRING_PREPEND	2	/* prepend deleted chars */
#define KILLING_SET		3	/* set killring to deleted char */
#define KILLRING_FLUSH		4	/* clear the killring */


/* kill ring, marking, etc... */
void		 killring_flush(void);
void		 killring_yank(void);
void		 killring_start_with_char(unsigned char ch);
void		 killring_append_char(unsigned char ch);
void		 killring_prepend_char(unsigned char ch);
void		 toggle_markset(void);
int	    	 cursor_after_mark(void);
int	    	 count_chars_from_cursor_to_mark(void);
void		 kill_region(void);
void		 indent_region(void);
void		 delete_region(void);

/* miscellaneous */
void		 jump_to_position(size_t col, size_t row);
void		 goto_line(void);
int	    	 cursor_at_eol(void);
int 		 iswordchar(unsigned char c);
void		 find_next_word(void);
void		 delete_next_word(void);
void		 find_prev_word(void);
void		 delete_prev_word(void);
void		 delete_row(const size_t at);
void		 row_insert_ch(abuf *row, int at, int16_t c);
void		 row_delete_ch(abuf *row, int at);
void		 insertch(int16_t c);
void		 deletech(uint8_t op);
void		 open_file(const char *filename);
char		*rows_to_buffer(int *buflen);
int     	 save_file(void);
uint16_t	 is_arrow_key(int16_t c);
int16_t		 get_keypress(void);
void		 editor_find_callback(char *query, int16_t c);
void		 editor_find(void);
char		*editor_prompt(const char*, void (*cb)(char*, int16_t));
void		 editor_openfile(void);
int	    	 first_nonwhitespace(abuf *row);
void		 move_cursor_once(int16_t c, int interactive);
void		 move_cursor(int16_t c, int interactive);
void		 uarg_start(void);
void		 uarg_digit(int d);
void		 uarg_clear(void);
int	    	 uarg_get(void);
void		 newline(void);
void		 process_kcommand(int16_t c);
void		 process_normal(int16_t c);
void		 process_escape(int16_t c);
int	    	 process_keypress(void);
char		*get_cloc_code_lines(const char *filename);
int	    	 dump_pidfile(void);
void		 draw_rows(abuf *ab);
char		 status_mode_char(void);
void		 draw_status_bar(abuf *ab);
void		 draw_message_line(abuf *ab);
void		 scroll(void);
void		 display_refresh(void);
void		 loop(void);
void		 enable_debugging(void);
void		 deathknell(void);
static void	 signal_handler(int sig);
static void	 install_signal_handlers(void);


static int
path_is_dir(const char *path)
{
	struct stat st;

	if (path == NULL) {
		return 0;
	}

	if (stat(path, &st) == 0) {
		return S_ISDIR(st.st_mode);
	}

	return 0;
}


static size_t
str_lcp2(const char *a, const char *b)
{
	size_t	 i = 0;

	if (!a || !b) {
		return 0;
	}

	while (a[i] && b[i] && a[i] == b[i]) {
		i++;
	}

	return i;
}


/*
 * TODO(kyle): not proud of this, but it does work. It needs to be
 * cleaned up and the number of buffers consolidated.
 */
static void
file_open_prompt_cb(char *buf, const int16_t key)
{
	DIR		*dirp              = NULL;
	const char	*name              = NULL;
	const char	*names[128]        = {0};
	char		 ext[256]          = {0};
	char		 full[PATH_MAX]    = {0};
	char		 msg[80]           = {0};
	char		 newbuf[PATH_MAX]  = {0};
	int		 isdir[128]        = {0};
	struct dirent	*de                = NULL;
	const char	*slash             = NULL;
	char		 dirpath[PATH_MAX] = {0};
	char		 base[256]         = {0};
	int		 n                 = 0;
	size_t		 cur               = 0;
	size_t		 k                 = 0;
	size_t		 lcp               = 0;
	size_t		 to_copy           = 0;
	size_t		 dlen              = 0;
	size_t		 plen              = 0;

	if (key != TAB_KEY) {
		return;
	}

	slash = strrchr(buf, '/');
	if (slash) {
		dlen = (size_t) (slash - buf);
		if (dlen == 0) {
			/* path like "/foo" -> dir is "/" */
			strcpy(dirpath, "/");
		} else {
			if (dlen >= sizeof(dirpath)) {
				dlen = sizeof(dirpath) - 1;
			}

			memcpy(dirpath, buf, dlen);
			dirpath[dlen] = '\0';
		}

		strncpy(base, slash + 1, sizeof(base) - 1);
		base[sizeof(base) - 1] = '\0';
	} else {
		strcpy(dirpath, ".");
		strncpy(base, buf, sizeof(base) - 1);
		base[sizeof(base) - 1] = '\0';
	}

	dirp = opendir(dirpath);
	if (!dirp) {
		editor_set_status("No such dir: %s", dirpath);
		return;
	}

	plen = strlen(base);
	while ((de = readdir(dirp)) != NULL) {
		name = de->d_name;

		/* Skip . and .. */
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
			continue;
		}

		if (plen == 0 || strncmp(name, base, plen) == 0) {
			if (n < 128) {
				names[n] = strdup(name);

				/* Build full path to test dir */
				if (snprintf(full, sizeof(full),
					"%s/%s", dirpath, name) >= 0) {
					isdir[n] = path_is_dir(full);
				} else {
					isdir[n] = 0;
				}

				n++;
			}
		}
	}

	closedir(dirp);

	if (n == 0) {
		editor_set_status("No file matches '%s' in %s", base, dirpath);
		return;
	}

	lcp = strlen(names[0]);
	for (int i = 1; i < n; i++) {
		k = str_lcp2(names[0], names[i]);
		if (k < lcp) {
			lcp = k;
		}

		if (lcp == 0) {
			break;
		}
	}

	newbuf[0] = '\0';
	if (slash) {
		dlen = (size_t) (slash - buf);
		if (dlen >= sizeof(newbuf)) {
			dlen = sizeof(newbuf) - 1;
		}

		memcpy(newbuf, buf, dlen);
		newbuf[dlen] = '\0';
		strncat(newbuf, "/", sizeof(newbuf) - strlen(newbuf) - 1);
	}

	/* appending: if unique -> full name (+ '/' if dir), else current
	 * base extended to LCP */
	if (n == 1) {
		strncat(newbuf, names[0], sizeof(newbuf) - strlen(newbuf) - 1);
		if (isdir[0]) {
			strncat(newbuf, "/", sizeof(newbuf) - strlen(newbuf) - 1);
		}

		/* copy to input buffer (max 127 chars + NUL) avoiding truncation warnings */
		do {
			const char *src__ = newbuf[0] ? newbuf : names[0];
			size_t cap__      = 128u;
			size_t len__      = strnlen(src__, cap__ - 1);
			memcpy(buf, src__, len__);
			buf[len__] = '\0';
		} while (0);

		editor_set_status("Unique match: %s%s", names[0], isdir[0] ? "/" : "");
	} else {
		cur = strlen(base);
		if (lcp > cur) {
			to_copy = lcp - cur;
			if (to_copy >= sizeof(ext)) {
				to_copy = sizeof(ext) - 1;
			}

			memcpy(ext, names[0] + cur, to_copy);
			ext[to_copy] = '\0';

			strncat(newbuf, base, sizeof(newbuf) - strlen(newbuf) - 1);
			strncat(newbuf, ext, sizeof(newbuf) - strlen(newbuf) - 1);
		} else {
			strncat(newbuf, base, sizeof(newbuf) - strlen(newbuf) - 1);
		}

		/* copy to input buffer (max 127 chars + NUL) avoiding truncation warnings */
		do {
			const char *src__ = newbuf;
			size_t cap__      = 128u;
			size_t len__      = strnlen(src__, cap__ - 1);
			memcpy(buf, src__, len__);
			buf[len__] = '\0';
		} while (0);

		size_t used = 0;
		used += snprintf(msg + used, sizeof(msg) - used, "%d matches: ", n);
		for (int i = 0; i < n && used < sizeof(msg) - 1; i++) {
			used += snprintf(msg + used, sizeof(msg) - used, "%s%s%s",
			                 (i ? ", " : ""), names[i], isdir[i] ? "/" : "");
		}

		editor_set_status("%s", msg);
	}

	/* Free duplicated names */
	for (int i = 0; i < n; i++) {
		free((void *) names[i]);
	}
}


int
erow_render_to_cursor(const abuf *row, const int cx)
{
	int		 rx = 0;
	size_t		 j = 0;
	wchar_t		 wc;
	mbstate_t	 st;

	memset(&st, 0, sizeof(st));

	while (j < (size_t)cx && j < (size_t)row->size) {
		unsigned char b = (unsigned char)row->b[j];
		if (b == '\t') {
			rx += (TAB_STOP - 1) - (rx % TAB_STOP);
			rx++;
			j++;
			continue;
		}
		if (b < 0x20) {
			/* render as \xx -> width 3 */
			rx += 3;
			j++;
			continue;
		}

		if (b < 0x80) {
			rx++;
			j++;
			continue;
		}

		size_t rem = (size_t)row->size - j;
		size_t n = mbrtowc(&wc, &row->b[j], rem, &st);

		if (n == (size_t)-2) {
			/* incomplete sequence at end; treat one byte */
			rx += 1;
			j += 1;
			memset(&st, 0, sizeof(st));
		} else if (n == (size_t)-1) {
			/* invalid byte; consume one and reset state */
			rx += 1;
			j += 1;
			memset(&st, 0, sizeof(st));
		} else if (n == 0) {
			/* null character */
			rx += 0;
			j += 1;
		} else {
			int w = wcwidth(wc);
			if (w < 0)
				w = 1; /* non-printable -> treat as width 1 */
			rx += w;
			j += n;
		}
	}

	return rx;
}


int
erow_cursor_to_render(abuf *row, int rx)
{
	int		 cur_rx = 0;
	size_t		 j = 0;
	wchar_t		 wc;
	mbstate_t	 st;

	memset(&st, 0, sizeof(st));

	while (j < (size_t)row->size) {
		int w = 0;
		size_t adv = 1;

		unsigned char b = (unsigned char)row->b[j];
		if (b == '\t') {
			int add = (TAB_STOP - 1) - (cur_rx % TAB_STOP);
			w = add + 1;
			adv = 1;
			/* tabs are single byte */
		} else if (b < 0x20) {
			w = 3; /* "\\xx" */
			adv = 1;
		} else if (b < 0x80) {
			w = 1;
			adv = 1;
		} else {
			size_t rem = (size_t)row->size - j;
			size_t n = mbrtowc(&wc, &row->b[j], rem, &st);

			if (n == (size_t)-2 || n == (size_t)-1) {
				/* invalid/incomplete */
				w = 1;
				adv = 1;
				memset(&st, 0, sizeof(st));
			} else if (n == 0) {
				w = 0;
				adv = 1;
			} else {
				int ww = wcwidth(wc);
				if (ww < 0)
					ww = 1;
				w = ww;
				adv = n;
			}
		}

		if (cur_rx + w > rx) {
			break;
		}

		cur_rx += w;
		j += adv;
	}

	return (int)j;
}


int
erow_init(abuf *row, int len)
{
	ab_init_cap(row, len);

	return 0;
}


void
erow_insert(int at, char *s, int len)
{
	abuf	*row = realloc(EROW, sizeof(abuf) * (ENROWS + 1));

	assert(row != NULL);
	EROW = row;

	if ((size_t) at < ENROWS) {
		memmove(&EROW[at + 1], &EROW[at],
			sizeof(abuf) * (ENROWS - (size_t) at));
	}

	ab_init(&EROW[at]);
	ab_append(&EROW[at], s, (size_t) len);
    	ENROWS++;
}


void
killring_flush(void)
{
	if (editor.killring != NULL) {
		ab_free(editor.killring);
		free(editor.killring);
		editor.killring = NULL;
	}
}


void
killring_yank(void)
{
	if (editor.killring == NULL) {
		return;
	}
	/*
	 * Insert killring contents at the cursor without clearing the ring.
	 * Interpret '\n' as an actual newline() rather than inserting a raw 0x0A
	 * byte, so yanked content preserves lines correctly.
	 */
	for (int i = 0; i < (int)editor.killring->size; i++) {
		unsigned char ch = (unsigned char)editor.killring->b[i];
		if (ch == '\n') {
			newline();
		} else {
			insertch(ch);
		}
	}
}


void
killring_start_with_char(unsigned char ch)
{
	abuf *row = NULL;

	if (editor.killring != NULL) {
		ab_free(editor.killring);
		free(editor.killring);
		editor.killring = NULL;
	}

	editor.killring = malloc(sizeof(abuf));
	assert(editor.killring != NULL);
	assert(erow_init(editor.killring, 0) == 0);

	/* append one char to empty killring without affecting editor.dirty */
	row = editor.killring;

	row->b = realloc(row->b, row->size + 2);
	assert(row->b != NULL);
	row->b[row->size] = ch;
	row->size++;
	row->b[row->size] = '\0';
}


void
killring_append_char(unsigned char ch)
{
	abuf	*row = NULL;

	if (editor.killring == NULL) {
		killring_start_with_char(ch);
		return;
	}

	row = editor.killring;
	row->b = realloc(row->b, row->size + 2);
	assert(row->b != NULL);
	row->b[row->size] = ch;
	row->size++;
	row->b[row->size] = '\0';
}


void
killring_prepend_char(unsigned char ch)
{
	abuf	*row = NULL;

	if (editor.killring == NULL) {
		killring_start_with_char(ch);
		return;
	}

	row = editor.killring;
	row->b = realloc(row->b, row->size + 2);
	assert(row->b != NULL);
	memmove(&row->b[1], &row->b[0], row->size + 1);
	row->b[0] = ch;
	row->size++;
}


void
toggle_markset(void)
{
	if (EMARK_SET) {
		EMARK_SET = 0;
		editor_set_status("Mark cleared.");
		return;
	}

	EMARK_SET  = 1;
	EMARK_CURX = ECURX;
	EMARK_CURY = ECURY;
	editor_set_status("Mark set.");
}


int
cursor_after_mark(void)
{
	if (EMARK_CURY < ECURY) {
		return 1;
	}

	if (EMARK_CURY > ECURY) {
		return 0;
	}

	return ECURX >= EMARK_CURX;
}


int
count_chars_from_cursor_to_mark(void)
{
	size_t	 count = 0;
	size_t	 curx  = ECURX;
	size_t	 cury  = ECURY;
	size_t	 markx = EMARK_CURX;
	size_t	 marky = EMARK_CURY;

	if (!cursor_after_mark()) {
		swap_size_t(&curx, &markx);
		swap_size_t(&curx, &marky);
	}

	ECURX = markx;
	ECURY = marky;

	while (ECURY != cury) {
		while (!cursor_at_eol()) {
			move_cursor(ARROW_RIGHT, 1);
			count++;
		}

		move_cursor(ARROW_RIGHT, 1);
		count++;
	}

	while (ECURX != curx) {
		count++;
		move_cursor(ARROW_RIGHT, 1);
	}

	return count;
}


void
kill_region(void)
{
	size_t	 curx  = ECURX;
	size_t	 cury  = ECURY;
	size_t	 markx = EMARK_CURX;
	size_t	 marky = EMARK_CURY;

	if (!EMARK_SET) {
		return;
	}

	/* kill the current killring first */
	killring_flush();

	if (!cursor_after_mark()) {
		swap_size_t(&curx, &markx);
		swap_size_t(&cury, &marky);
	}

	ECURX = markx;
	ECURY = marky;

	while (ECURY != cury) {
		while (!cursor_at_eol()) {
			killring_append_char(EROW[ECURY].b[ECURX]);
			move_cursor(ARROW_RIGHT, 0);
		}
		killring_append_char('\n');
		move_cursor(ARROW_RIGHT, 0);
	}

	while (ECURX != curx) {
		killring_append_char(EROW[ECURY].b[ECURX]);
		move_cursor(ARROW_RIGHT, 0);
	}

	editor_set_status("Region killed.");
	/* clearing the mark needs to be done outside this function;	*
	 * when deleting the region, the mark needs to be set too.	*/
}


void
indent_region(void)
{
	size_t	 start_row = 0;
	size_t	 end_row   = 0;
	size_t	 i         = 0;

	if (!EMARK_SET) {
		return;
	}

	if (EMARK_CURY < ECURY) {
		start_row = EMARK_CURY;
		end_row   = ECURY;
	} else if (EMARK_CURY > ECURY) {
		start_row = ECURY;
		end_row   = EMARK_CURY;
	} else {
		start_row = end_row = ECURY;
	}

	/* Ensure bounds are valid */
	if (end_row >= ENROWS) {
		end_row = ENROWS - 1;
	}

	if (start_row >= ENROWS) {
		return;
	}

	for (i = start_row; i <= end_row; i++) {
		row_insert_ch(&EROW[i], 0, '\t');
	}

	ECURX = 0;
	EDIRTY++;
}


void
unindent_region(void)
{
	size_t	 start_row = 0;
	size_t	 end_row   = 0;
	size_t	 i         = 0;
	size_t	 del       = 0;
	abuf	*row       = NULL;

	if (!EMARK_SET) {
		editor_set_status("Mark not set.");
		return;
	}

	if (EMARK_CURY < ECURY ||
	    (EMARK_CURY == ECURY && EMARK_CURX < ECURX)) {
		start_row = EMARK_CURY;
		end_row   = ECURY;
	} else {
		start_row = ECURY;
		end_row   = EMARK_CURY;
	}

	if (start_row >= ENROWS) {
		return;
	}

	if (end_row >= ENROWS) {
		end_row = ENROWS - 1;
	}

	for (i = start_row; i <= end_row; i++) {
		row = &EROW[i];

		if (row->size == 0) {
			continue;
		}

		if (row->b[0] == '\t') {
			row_delete_ch(row, 0);
		} else if (row->b[0] == ' ') {
			del = 0;

			while (del < TAB_STOP && del < row->size &&
			       row->b[del] == ' ') {
				del++;
			}

			if (del > 0) {
				/* +1 for NUL */
				memmove(row->b, row->b + del,
					row->size - del + 1);
				row->size -= del;
			}
		}
	}

	ECURX = 0;
	ECURY = start_row;

	EDIRTY++;
	editor_set_status("Region unindented");
}


void
delete_region(void)
{
	size_t	 count  = count_chars_from_cursor_to_mark();
	size_t	 killed = 0;
	size_t	 curx   = ECURX;
	size_t	 cury   = ECURY;
	size_t	 markx  = EMARK_CURX;
	size_t	 marky  = EMARK_CURY;

	if (!EMARK_SET) {
		return;
	}

	if (!cursor_after_mark()) {
		swap_size_t(&curx, &markx);
		swap_size_t(&cury, &marky);
	}

	jump_to_position(markx, marky);

	while (killed < count) {
		move_cursor(ARROW_RIGHT, 0);
		deletech(KILLRING_NO_OP);
		killed++;
	}

	while (ECURX != markx && ECURY != marky) {
		deletech(KILLRING_NO_OP);
	}

	editor.kill = 1;
	editor_set_status("Region killed.");
}


void
jump_to_position(size_t col, size_t row)
{
	if (ENROWS <= 0) {
		ECURX = 0;
		ECURY = 0;
		display_refresh();
		return;
	}

	if (row >= ENROWS) {
		row = ENROWS - 1;
	}

	if (col > EROW[row].size) {
		col = EROW[row].size;
	}

	ECURX = col;
	ECURY = row;

	display_refresh();
}


void
goto_line(void)
{
	size_t	 lineno = 0;
	char	*query  = editor_prompt("Line: %s", NULL);

	if (query == NULL) {
		return;
	}

	lineno = strtoul(query, NULL, 10);
	if (lineno < 1 || lineno > ENROWS) {
		editor_set_status("Line number must be between 1 and %d.",
		                  ENROWS);
		free(query);
		return;
	}

	jump_to_position(0, lineno - 1);
	free(query);
}


int
cursor_at_eol(void)
{
	assert(ECURY <= ENROWS);
	assert(ECURX <= EROW[ECURY].size);

	return ECURX == EROW[ECURY].size;
}


int
iswordchar(const unsigned char c)
{
	return isalnum(c) || c == '_' || strchr("/!@#$%^&*+-=~", c) != NULL;
}


void
find_next_word(void)
{
	while (cursor_at_eol()) {
		move_cursor(ARROW_RIGHT, 1);
	}

	if (iswordchar(EROW[ECURY].b[ECURX])) {
		while (!isspace(EROW[ECURY].b[ECURX]) && !
		       cursor_at_eol()) {
			move_cursor(ARROW_RIGHT, 1);
		}

		return;
	}

	if (isspace(EROW[ECURY].b[ECURX])) {
		while (isspace(EROW[ECURY].b[ECURX])) {
			move_cursor(ARROW_RIGHT, 1);
		}

		find_next_word();
	}
}


void
delete_next_word(void)
{
	while (cursor_at_eol()) {
		move_cursor(ARROW_RIGHT, 1);
		deletech(KILLRING_APPEND);
	}

	if (iswordchar(EROW[ECURY].b[ECURX])) {
		while (!isspace(EROW[ECURY].b[ECURX]) && !
		       cursor_at_eol()) {
			move_cursor(ARROW_RIGHT, 1);
			deletech(KILLRING_APPEND);
		}

		return;
	}

	if (isspace(EROW[ECURY].b[ECURX])) {
		while (isspace(EROW[ECURY].b[ECURX])) {
			move_cursor(ARROW_RIGHT, 1);
			deletech(KILLRING_APPEND);
		}

		delete_next_word();
	}
}


void
find_prev_word(void)
{
	if (ECURY == 0 && ECURX == 0) {
		return;
	}

	move_cursor(ARROW_LEFT, 1);

	while (cursor_at_eol() || isspace(EROW[ECURY].b[ECURX])) {
		if (ECURY == 0 && ECURX == 0) {
			return;
		}

		move_cursor(ARROW_LEFT, 1);
	}

	while (ECURX > 0 && !isspace(EROW[ECURY].b[ECURX - 1])) {
		move_cursor(ARROW_LEFT, 1);
	}
}


void
delete_prev_word(void)
{
	if (ECURY == 0 && ECURX == 0) {
		return;
	}

	deletech(KILLRING_PREPEND);

	while (ECURY > 0 || ECURX > 0) {
		if (ECURX == 0) {
			deletech(KILLRING_PREPEND);
			continue;
		}

		if (!isspace(EROW[ECURY].b[ECURX - 1])) {
			break;
		}

		deletech(KILLRING_PREPEND);
	}

	while (ECURX > 0) {
		if (isspace(EROW[ECURY].b[ECURX - 1])) {
			break;
		}
		deletech(KILLRING_PREPEND);
	}
}


void
delete_row(const size_t at)
{
    abuf	*row = NULL;

    if (at >= ENROWS) {
        return;
    }

	/*
	 * Update killring with the deleted row's contents followed by a newline
	 * unless this deletion is an internal merge triggered by deletech at
	 * start-of-line. In that case, deletech will account for the single
	 * newline itself and we must NOT also push the entire row here.
	 */
	if (!editor.no_kill) {
		row = &EROW[at];
		if (row->size > 0) {
			if (!editor.kill) {
				killring_start_with_char(
					(unsigned char) row->b[0]);
				for (int i = 1; i < (int) row->size; i++) {
					killring_append_char(
						(unsigned char) row->b[i]);
				}
			} else {
				for (int i = 0; i < (int) row->size; i++) {
					killring_append_char(
						(unsigned char) row->b[i]);
				}
			}
			killring_append_char('\n');
			editor.kill = 1;
		} else {
			if (!editor.kill) {
				killring_start_with_char('\n');
			} else {
				killring_append_char('\n');
			}
			editor.kill = 1;
		}
	}

	ab_free(&EROW[at]);
	memmove(&EROW[at],
		&EROW[at + 1],
		sizeof(abuf) * (ENROWS - at - 1));
	ENROWS--;
	EDIRTY++;
}


void
row_append_row(abuf *row, const char *s, const int len)
{
	ab_append(row, s, len);
	EDIRTY++;
}


void
row_insert_ch(abuf *row, int at, const int16_t c)
{
	/*
	 * row_insert_ch just concerns itself with how to update a row.
	 */
	if (at < 0 || at > (int)row->size) {
		at = (int)row->size;
	}
	assert(c > 0);

	ab_resize(row, row->size + 2);
	memmove(&row->b[at + 1], &row->b[at], row->size - at + 1);
	row->b[at] = c & 0xff;
	row->size++;
	row->b[row->size] = 0;
}


void
row_delete_ch(abuf *row, const int at)
{
	if (at < 0 || at >= (int) row->size) {
		return;
	}

	memmove(&row->b[at], &row->b[at + 1], row->size - at);
	row->size--;
	row->b[row->size] = 0;
	EDIRTY++;
}


void
insertch(const int16_t c)
{
	/*
	 * insert_ch doesn't need to worry about how to update a
	 * a row; it can just figure out where the cursor is
	 * at and what to do.
	 */
	if (ECURY == ENROWS) {
		erow_insert(ENROWS, "", 0);
	}

	/* Inserting ends kill ring chaining. */
	editor.kill = 0;

	row_insert_ch(&EROW[ECURY],
		      ECURX,
		      (int16_t) (c & 0xff));
	ECURX++;
	EDIRTY++;
}


void
deletech(uint8_t op)
{
	abuf		*row = NULL;
	unsigned char	 dch = 0;
	int		 prev = 0;

	if (ECURY >= ENROWS) {
		return;
	}

	if (ECURY == 0 && ECURX == 0) {
		return;
	}

	row = &EROW[ECURY];
	if (ECURX > 0) {
		dch = (unsigned char) row->b[ECURX - 1];
	} else {
		dch = '\n';
	}

	if (ECURX > 0) {
		row_delete_ch(row, ECURX - 1);
		ECURX--;
	} else {
		ECURX = (int) EROW[ECURY - 1].size;
		row_append_row(&EROW[ECURY - 1],
			       row->b,
			       (int) row->size);

		prev = editor.no_kill;
		editor.no_kill = 1;

		delete_row(ECURY);
		editor.no_kill = prev;
		ECURY--;
	}

	if (op == KILLRING_FLUSH) {
		killring_flush();
		editor.kill = 0;
		return;
	}

	if (op == KILLRING_NO_OP) {
		return;
	}

	if (!editor.kill) {
		killring_start_with_char(dch);
		editor.kill = 1;
		return;
	}

	if (op == KILLING_SET) {
		killring_start_with_char(dch);
		editor.kill = 1;
	} else if (op == KILLRING_APPEND) {
		killring_append_char(dch);
	} else if (op == KILLRING_PREPEND) {
		killring_prepend_char(dch);
	}
}


void
open_file(const char *filename)
{
	char	*line    = NULL;
	size_t	 linecap = 0;
	ssize_t	 linelen = 0;
	size_t	 i       = 0;
	FILE	*fp      = NULL;
	buffer	*cur     = NULL;

	cur = buffer_current();
	if (cur == NULL) {
		return;
	}

	/* Clear editor working set and the current buffer’s contents so we load
	 * fresh data instead of appending to any previous rows. */
	reset_editor();

	if (EROW != NULL && ENROWS > 0) {
		for (i = 0; i < ENROWS; i++) {
			ab_free(&EROW[i]);
		}
		free(EROW);
		EROW   = NULL;
		ENROWS = 0;
	}

	/* Reset cursor/scroll positions for the buffer */
	ECURX    = ECURY = 0;
	ERX      = 0;
	EROWOFFS = ECOLOFFS = 0;

	if (filename == NULL) {
		return;
	}

	EFILENAME = strdup(filename);
	assert(EFILENAME != NULL);

	EDIRTY = 0;
	fp = fopen(EFILENAME, "r");
	if (fp == NULL) {
		if (errno == ENOENT) {
			editor_set_status("[new file]");
			return;
		}
		die("fopen");
	}

	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		if (linelen != -1) {
			while (linelen > 0 && (line[linelen - 1] == '\r' ||
			                       line[linelen - 1] == '\n')) {
				linelen--;
			}

			erow_insert(ENROWS, line, (int)linelen);
		}
	}

	free(line);
	line = NULL;
	fclose(fp);
}


/*
 * convert our rows to a buffer; caller must free it.
 */
char
*rows_to_buffer(int *buflen)
{
	size_t	 len = 0;
	size_t	 j   = 0;
	char	*buf = NULL;
	char	*p   = NULL;

	for (j = 0; j < ENROWS; j++) {
		len += EROW[j].size + 1;
	}

	if (len == 0) {
		return NULL;
	}

	*buflen = len;
	buf     = malloc(len);
	assert(buf != NULL);
	p = buf;

	for (j = 0; j < ENROWS; j++) {
		memcpy(p, EROW[j].b, EROW[j].size);
		p += EROW[j].size;
		*p++ = '\n';
	}

	return buf;
}


int
save_file(void)
{
	int	 fd     = -1;
	int	 len    = 0;
	int	 status = 1;
	char	*buf    = NULL;

	if (!EDIRTY) {
		editor_set_status("No changes to save.");
		return 0;
	}

	if (EFILENAME == NULL) {
		EFILENAME = editor_prompt("Filename: %s", NULL);
		if (EFILENAME == NULL) {
			editor_set_status("Save aborted.");
			return 0;
		}
	}

	buf = rows_to_buffer(&len);
	fd = open(EFILENAME, O_RDWR | O_CREAT, 0644);
	if (fd == -1) {
		goto save_exit;
	}

	if (-1 == ftruncate(fd, len)) {
		goto save_exit;
	}

	if (len == 0) {
		status = 0;
		goto save_exit;
	}

	if ((ssize_t) len != write(fd, buf, len)) {
		goto save_exit;
	}

	status = 0;

	save_exit:
	if (fd) {
		close(fd);
	}

	if (buf) {
		free(buf);
		buf = NULL;
	}

	if (status != 0) {
		buf = strerror(errno);
		editor_set_status("Error writing %s: %s", EFILENAME, buf);
	} else {
		editor_set_status("Wrote %d bytes to %s.", len, EFILENAME);
		EDIRTY = 0;
	}

	return status;
}


uint16_t
is_arrow_key(const int16_t c)
{
	switch (c) {
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
		case ARROW_UP:
		case CTRL_KEY('p'):
		case CTRL_KEY('n'):
		case CTRL_KEY('f'):
		case CTRL_KEY('b'):
		case CTRL_KEY('a'):
		case CTRL_KEY('e'):
		case END_KEY:
		case HOME_KEY:
		case PG_DN:
		case PG_UP:
			return 1;
		default:
			return 0;
	}

	return 0;
}


int16_t
get_keypress(void)
{
	char		 seq[3] = {0};
	unsigned char	 uc     = 0;
	int16_t		 c      = 0;

	if (read(STDIN_FILENO, &uc, 1) == -1) {
		die("get_keypress:read");
	}

	c = (int16_t)uc;

	if (c == 0x1b) {
		if (read(STDIN_FILENO, &seq[0], 1) != 1) {
			return c;
		}

		if (read(STDIN_FILENO, &seq[1], 1) != 1) {
			return c;
		}

		if (seq[0] == '[') {
			if (seq[1] < 'A') {
				if (read(STDIN_FILENO, &seq[2], 1) != 1) {
					return c;
				}

				if (seq[2] == '~') {
					switch (seq[1]) {
					case '1':
						return HOME_KEY;
					case '3':
						return DEL_KEY;
					case '4':
						return END_KEY;
					case '5':
						return PG_UP;
					case '6':
						return PG_DN;
					case '7':
						return HOME_KEY;
					case '8':
						return END_KEY;
					default:
						break;
					}
				}
			} else {
				switch (seq[1]) {
				case 'A':
					return ARROW_UP;
				case 'B':
					return ARROW_DOWN;
				case 'C':
					return ARROW_RIGHT;
				case 'D':
					return ARROW_LEFT;
				case 'F':
					return END_KEY;
				case 'H':
					return HOME_KEY;
				default:
					break;
				}
			}
		} else if (seq[0] == 'O') {
			switch (seq[1]) {
			case 'F':
				return END_KEY;
			case 'H':
				return HOME_KEY;
			default:
				break;
			}
		}

		return 0x1b;
	}

	return c;
}


char
*editor_prompt(const char *prompt, void (*cb)(char*, int16_t))
{
	size_t		 bufsz = 128;
	char		*buf = malloc(bufsz);
	size_t		 buflen = 0;
	int16_t		 c;

	if (buf == NULL) {
		return NULL;
	}

	buf[0] = '\0';
	while (1) {
		editor_set_status(prompt, buf);
		display_refresh();

		while ((c = get_keypress()) <= 0);
		if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
			if (buflen != 0) {
				buf[--buflen] = '\0';
			}
		} else if (c == ESC_KEY || c == CTRL_KEY('g')) {
			editor_set_status("");
			if (cb) {
				cb(buf, c);
			}

			free(buf);
			return NULL;
		} else if (c == '\r') {
			if (buflen != 0) {
				editor_set_status("");
				if (cb) {
					cb(buf, c);
				}

				return buf;
			}
		} else if (c == TAB_KEY) {
			/* invoke completion callback without inserting a TAB */
			if (cb) {
				cb(buf, c);
			}
			/* keep buflen in sync in case callback edited buf */
			buflen = strlen(buf);
		} else if (c >= 0x20 && c < 0x7f) {
			if (buflen == bufsz - 1) {
				bufsz *= 2;
				buf = realloc(buf, bufsz);
				assert(buf != NULL);
			}

			buf[buflen++] = (char) (c & 0xff);
			buf[buflen]   = '\0';
		}

		if (cb) {
			cb(buf, c);
			/* keep buflen in sync with any changes the callback made */
			buflen = strlen(buf);
		}
	}

	free(buf);
	return NULL;
}


void
editor_find_callback(char* query, int16_t c)
{
	static ssize_t	 last_match      = -1;  /* row index of last match */
	static int	 direction       = 1;   /* 1 = forward, -1 = back */
	static char	 last_query[128] = {0}; /* last successful query */
	abuf		*row             = NULL;
	const int	 saved_cx        = ECURX;
	const int	 saved_cy	 = ECURY;
	const size_t	 qlen		 = strlen(query);
	const char	*hay	         = NULL; /* the haystack, so to speak */
	const char	*match           = NULL;
	size_t		 i               = 0;
	size_t		 limit           = 0;
	size_t		 skip            = 0;
	size_t		 haylen		 = 0;
	size_t		 start_row       = ECURY;
	size_t		 start_col       = ECURX;
	size_t		 wrapped	 = 0;
	ssize_t		 current	 = 0;

	if (c == '\r' || c == ESC_KEY || c == CTRL_KEY('g')) {
		last_match = -1;
		direction = 1;
		last_query[0] = '\0';
		return;
	}

	if (c == CTRL_KEY('s') || c == ARROW_DOWN || c == ARROW_RIGHT) {
		direction = 1;
	} else if (c == CTRL_KEY('r') || c == ARROW_UP || c == ARROW_LEFT) {
		direction = -1;
	}

	if (qlen > 0 && (qlen != strlen(last_query) || strcmp(query, last_query) != 0)) {
		last_match = -1;
		/* copy query safely into last_query */
		strncpy(last_query, query, sizeof(last_query) - 1);
		last_query[sizeof(last_query) - 1] = '\0';
	}

	if (last_match == -1) {
		last_match = ECURY;
	}

	current = last_match - direction;
	wrapped = 0;

	for (i = 0; i < ENROWS; i++) {
		current += direction;

		if ((size_t) current >= ENROWS) {
			current = 0;
			if (wrapped++) {
				break;
			}
		}

		if (current < 0) {
			current = ENROWS - 1;
			if (wrapped++) {
				break;
			}
		}

		row = &EROW[current];

		/* Determine match based on direction. For forward searches, start just
		 * after the current cursor when on the same row. For backward searches,
		 * find the last occurrence before the current cursor when on the same row,
		 * and in other rows find the last occurrence in the row. */
		if (direction == 1) {
			hay = row->b;
			haylen = row->size;
			if ((size_t) current == start_row && wrapped == 0) {
				/* start just after the current position to avoid re-finding same match */
				skip = start_col + 1;
				if (skip > haylen) {
					skip = haylen;
				}

				hay += skip;
				haylen -= (size_t) skip;
			}
			match = (qlen > 0) ?
				strnstr(hay, query, haylen) : NULL;
		} else {
			limit = row->size;
			if ((size_t) current == start_row && wrapped == 0) {
				/* Only consider text strictly before the cursor */
				if ((size_t) start_col < limit) limit = (size_t) start_col;
			}
			if (qlen > 0 && limit >= qlen) {
				const char *p = row->b;
				const char *last = NULL;
				size_t remaining = limit;
				while (remaining >= qlen) {
					const char *q = strnstr(p, query, remaining);
					if (!q) break;
					last = q;
					/* move one byte forward to look for later occurrences */
					size_t advance = (size_t) (q - p) + 1;
					p += advance;
					remaining = limit - (size_t) (p - row->b);
				}
				match = last;
			} else {
				match = NULL;
			}
		}
		if (match) {
			last_match = current;
			ECURY = current;
			/* ECURX stores the raw byte index into the row buffer. */
			ECURX = (int) (match - row->b);
			scroll();
			display_refresh();
			return;
		}
	}

	if (qlen > 0) {
		editor_set_status("Failing search: %s", query);
	}

	ECURX = saved_cx;
	ECURY = saved_cy;
	display_refresh();
}


void
editor_find(void)
{
	/* TODO(kyle): consider making this an abuf */
	char	*query = NULL;
	int	 scx   = ECURX;
	int	 scy   = ECURY;
	int	 sco   = ECOLOFFS;
	int	 sro   = EROWOFFS;

	query = editor_prompt("Search (ESC to cancel): %s",
	                      editor_find_callback);
	if (query) {
		free(query);
		query = NULL;
	} else {
		ECURX    = scx;
		ECURY    = scy;
		ECOLOFFS = sco;
		EROWOFFS = sro;
	}

	display_refresh();
}


void
editor_openfile(void)
{
	char		*filename = NULL;
	const buffer	*cur      = NULL;
	int		 nb       = 0;

	filename = editor_prompt("Load file: %s", file_open_prompt_cb);
	if (filename == NULL) {
		return;
	}

	cur = buffer_current();
	if (editor.bufcount == 1 && buffer_is_unnamed_and_empty(cur)) {
		open_file(filename);
	} else {
		nb = buffer_add_empty();
		buffer_switch(nb);
		open_file(filename);
	}

	free(filename);
}


int
first_nonwhitespace(abuf *row)
{
	int		 pos;
	wchar_t		 wc;
	mbstate_t	 state;
	size_t		 len;

	if (row == NULL) {
		return 0;
	}

	memset(&state, 0, sizeof(state));
 pos = ECURX;
	if (pos > (int)row->size) {
		pos = row->size;
	}

	while (pos < (int)row->size) {
		if ((unsigned char)row->b[pos] < 0x80) {
			if (!isspace((unsigned char)row->b[pos])) {
				return pos;
			}
			pos++;
			continue;
		}

		len = mbrtowc(&wc, &row->b[pos], row->size - pos, &state);
		if (len == (size_t)-1 || len == (size_t)-2) {
			break;
		}

		if (len == 0) {
			break;
		}

		if (!iswspace(wc)) {
			break;
		}

		pos += len;
	}

	return pos;
}


void
move_cursor_once(const int16_t c, int interactive)
{
    abuf    *row  = NULL;
    int      reps = 0;

	row = (ECURY >= ENROWS) ? NULL : &EROW[ECURY];

	switch (c) {
		case ARROW_UP:
		case CTRL_KEY('p'):
			if (ECURY > 0) {
				ECURY--;
				row = (ECURY >= ENROWS) ? NULL : &EROW[ECURY];
				if (interactive) {
					ECURX = first_nonwhitespace(row);
				} else if (row) {
					if (ECURX > row->size) {
						ECURX = row->size;
					}
				}
			}
			break;
		case ARROW_DOWN:
		case CTRL_KEY('n'):
			if (ECURY < ENROWS - 1) {
				ECURY++;
				row = (ECURY >= ENROWS) ? NULL : &EROW[ECURY];

				if (interactive) {
					ECURX = first_nonwhitespace(row);
				} else if (row) {
					if (ECURX > row->size) {
						ECURX = row->size;
					}
				}
			}
			break;
		case ARROW_RIGHT:
		case CTRL_KEY('f'):
			if (!row) {
				break;
			}

			if (ECURX < row->size) {
				ECURX++;
				/* skip over UTF-8 continuation bytes */
				while (ECURX < row->size &&
				       ((unsigned char) row->b[ECURX] &
					0xC0) == 0x80) {
					ECURX++;
				}
			} else if (ECURX == row->size && ECURY < ENROWS - 1) {
				ECURY++;
				ECURX = 0;
			}
			break;
		case ARROW_LEFT:
		case CTRL_KEY('b'):
			if (ECURX > 0) {
				ECURX--;
				while (ECURX > 0 &&
				       ((unsigned char) row->b[ECURX] &
					0xC0) == 0x80) {
					ECURX--;
				}
			} else if (ECURY > 0) {
				ECURY--;
				ECURX = (int) EROW[ECURY].size;

				row = &EROW[ECURY];
				while (ECURX > 0 &&
				       ((unsigned char) row->b[ECURX] &
					0xC0) == 0x80) {
					ECURX--;
				}
			}
			break;
		case PG_UP:
		case PG_DN:
			if (c == PG_UP) {
				ECURY = EROWOFFS;
			} else if (c == PG_DN) {
				ECURY = EROWOFFS + editor.rows - 1;
				if (ECURY > ENROWS) {
					ECURY = ENROWS;
				}
			}

			reps = editor.rows;
			while (--reps) {
				move_cursor(c == PG_UP ? ARROW_UP : ARROW_DOWN, 1);
			}

			break;

		case HOME_KEY:
		case CTRL_KEY('a'):
			ECURX = 0;
			break;
		case END_KEY:
		case CTRL_KEY('e'):
			if (ECURY >= ENROWS) {
				break;
			}
			ECURX = (int) EROW[ECURY].size;
			break;
		default:
			break;
	}
}


void
move_cursor(const int16_t c, const int interactive)
{
	int	 n = uarg_get();

	while (n-- > 0) {
		move_cursor_once(c, interactive);
	}
}


void
newline(void)
{
	size_t	 rhs_len = 0;
	char	*tmp     = NULL;

	if (ECURY >= ENROWS) {
		erow_insert(ECURY, "", 0);
		ECURY++;
		ECURX = 0;
	} else if (ECURX == 0) {
		erow_insert(ECURY, "", 0);
		ECURY++;
		ECURX = 0;
	} else {
		/*
		 * IMPORTANT: Do not keep a pointer to EROW[ECURY] across erow_insert(),
		 * as erow_insert() may realloc the rows array and invalidate it.
		 */
		rhs_len = EROW[ECURY].size - (size_t)ECURX;
		if (rhs_len > 0) {
			tmp = malloc(rhs_len);
			assert(tmp != NULL);
			memcpy(tmp, &EROW[ECURY].b[ECURX], rhs_len);
		}

		/* Insert the right-hand side as a new row first (may realloc rows). */
		erow_insert(ECURY + 1, tmp ? tmp : "", (int)rhs_len);
		if (tmp) {
			free(tmp);
		}

		/* Now safely shrink the original row (re-fetch by index). */
		EROW[ECURY].size = ECURX;
		if (EROW[ECURY].cap <= EROW[ECURY].size) {
			ab_resize(&EROW[ECURY], EROW[ECURY].size + 1);
		}
		EROW[ECURY].b[EROW[ECURY].size] = '\0';

		ECURY++;
		ECURX = 0;
	}

	editor.kill = 0; /* BREAK THE KILL CHAIN \m/ */
	EDIRTY++;
}


void
uarg_start(void)
{
	if (editor.uarg == 0) {
		editor.ucount = 0;
	} else {
		if (editor.ucount == 0) {
			editor.ucount = 1;
		}
		editor.ucount *= 4;
	}

	editor.uarg = 1;
	editor_set_status("C-u %d", editor.ucount);
}


void
uarg_digit(int d)
{
	if (editor.uarg == 0) {
		editor.uarg   = 1;
		editor.ucount = 0;
	}

	editor.ucount = editor.ucount * 10 + d;
	editor_set_status("C-u %d", editor.ucount);
}


void
uarg_clear(void)
{
	editor.uarg   = 0;
	editor.ucount = 0;
}


int
uarg_get(void)
{
	int	 n = editor.ucount > 0 ? editor.ucount : 1;

	uarg_clear();

	return n;
}


void
process_kcommand(const int16_t c)
{
	char	*buf   = NULL;
	size_t	 len   = 0;
	int	 jumpx = 0;
	int	 jumpy = 0;
	int	 reps  = 0;

	switch (c) {
		case BACKSPACE:
			while (ECURX > 0) {
				process_normal(BACKSPACE);
			}
			break;
		case '=':
			if (EMARK_SET) {
				indent_region();
			} else {
				editor_set_status("Mark not set.");
			}
			break;
		case '-':
			if (EMARK_SET) {
				unindent_region();
			} else {
				editor_set_status("Mark not set.");
			}
			break;
		case CTRL_KEY('\\'):
			/* sometimes it's nice to dump core */
			disable_termraw();
			abort();
		case '@':
			if (!dump_pidfile()) {
				break;
			}
			/* FALLTHRU */
		case '!':
			/* useful for debugging */
			editor_set_status("PID: %ld", (long) getpid());
			break;
		case ' ':
			toggle_markset();
			break;
		case CTRL_KEY(' '):
			jumpx = EMARK_CURX;
			jumpy = EMARK_CURY;
			EMARK_CURX = ECURX;
			EMARK_CURY = ECURY;

			jump_to_position(jumpx, jumpy);
			editor_set_status("Jumped to mark");
			break;
		case 'c':
			buffer_close_current();
			break;
		case 'd':
			if (ECURX == 0 && cursor_at_eol()) {
				delete_row(ECURY);
				return;
			}

			reps = uarg_get();
			while (reps--) {
				while ((EROW[ECURY].size - ECURX) > 0) {
					process_normal(DEL_KEY);
				}
				if (reps) {
					newline();
				}
			}

			break;
		case DEL_KEY:
		case CTRL_KEY('d'):
			reps = uarg_get();

			while (reps--) {
				delete_row(ECURY);
			}
			break;
		case 'e':
		case CTRL_KEY('e'):
			if (EDIRTY && editor.dirtyex) {
				editor_set_status(
					"File not saved - C-k e again to open a new file anyways.");
				editor.dirtyex = 0;
				return;
			}
			editor_openfile();
			break;
		case 'f':
			if (editor.killring == NULL || editor.killring->size == 0) {
				editor_set_status("The kill ring is empty.");
				break;
			}

			len = editor.killring ? editor.killring->size : 0;
			killring_flush();
			editor_set_status("Kill ring cleared (%lu characters)", len);
			break;
		case 'n':
			buffer_next();
			break;
		case 'p':
			buffer_prev();
			break;
		case 'b':
			buffer_switch_by_name();
			break;
		case 'g':
			goto_line();
			break;
		case 'j':
			if (!EMARK_SET) {
				editor_set_status("Mark not set.");
				break;
			}

			jumpx = EMARK_CURX;
			jumpy = EMARK_CURY;
			EMARK_CURX = ECURX;
			EMARK_CURY = ECURY;

			jump_to_position(jumpx, jumpy);
			editor_set_status("Jumped to mark; mark is now the previous location.");
			break;
		case 'l':
			buf = get_cloc_code_lines(EFILENAME);

			editor_set_status("Lines of code: %s", buf);
			free(buf);
			break;
		case 'm':
			/* todo: fix the process failed: success issue */
			if (system("make") != 0) {
				editor_set_status(
					"process failed: %s",
					strerror(errno));
			} else {
				editor_set_status("make: ok");
			}
			break;
		case 'q':
			if (EDIRTY && editor.dirtyex) {
				editor_set_status(
					"File not saved - C-k q again to quit.");
				editor.dirtyex = 0;
				return;
			}
			exit(0);
		case CTRL_KEY('q'):
			exit(0);
		case CTRL_KEY('r'):
			if (EDIRTY && editor.dirtyex) {
				editor_set_status("File not saved - C-k C-r again to reload.");
				editor.dirtyex = 0;
				return;
			}

			jumpx = ECURX;
			jumpy = ECURY;
			buf = strdup(EFILENAME);

			reset_editor();
			open_file(buf);
			display_refresh();
			free(buf);

			jump_to_position(jumpx, jumpy);
			editor_set_status("file reloaded");
			break;
		case CTRL_KEY('s'):
		case 's':
			save_file();
			break;
		case CTRL_KEY('x'):
		case 'x':
			exit(save_file());
		case 'u':
			reps = uarg_get();

			while (reps--) {}
			editor_set_status("Undo not implemented.");
			break;
		case 'U':
			reps = uarg_get();

			while (reps--) {}
			editor_set_status("Redo not implemented.");
			break;
		case 'y':
			reps = uarg_get();

			while (reps--) {
				killring_yank();
			}
			break;
		case ESC_KEY:
		case CTRL_KEY('g'):
			break;
		default:
			if (isprint(c)) {
				editor_set_status("unknown kcommand '%c'", c);
				break;
			}

			editor_set_status("unknown kcommand: %04x", c);
			return;
	}

	editor.dirtyex = 1;
}


void
process_normal(int16_t c)
{
	size_t	 cols = 0;
	size_t	 rows = 0;
	int	 reps = 0;

	/* C-u handling – must be the very first thing */
	if (c == CTRL_KEY('u')) {
		uarg_start();
		return;
	}

	/* digits after a C-u are part of the argument */
	if (editor.uarg && c >= '0' && c <= '9') {
		uarg_digit(c - '0');
		return;
	}

	if (is_arrow_key(c)) {
		/* moving the cursor breaks a delete sequence */
		editor.kill = 0;
		move_cursor(c, 1);
		return;
	}

	switch (c) {
	case '\r':
		newline();
		break;
	case CTRL_KEY('k'):
		editor.mode = MODE_KCOMMAND;
		return;
	case BACKSPACE:
	case CTRL_KEY('h'):
	case CTRL_KEY('d'):
	case DEL_KEY:
		if (c == DEL_KEY || c == CTRL_KEY('d')) {
			reps = uarg_get();
			while (reps-- > 0) {
				move_cursor(ARROW_RIGHT, 1);
				deletech(KILLRING_APPEND);
			}
		} else {
			reps = uarg_get();
			while (reps-- > 0) {
				deletech(KILLRING_PREPEND);
			}
		}
		break;
	case CTRL_KEY('a'):	/* beginning of line */
	case HOME_KEY:
		move_cursor(CTRL_KEY('a'), 1);
		break;
	case CTRL_KEY('e'):	/* end of line */
	case END_KEY:
		move_cursor(CTRL_KEY('e'), 1);
		break;
	case CTRL_KEY('g'):
		break;
	case CTRL_KEY('l'):
		if (get_winsz(&rows, &cols) == 0) {
			editor.rows = rows;
			editor.cols = cols;
		} else {
			editor_set_status("Couldn't update window size.");
		}
		display_refresh();
		break;
	case CTRL_KEY('s'):
		editor_find();
		break;
	case CTRL_KEY('w'):
		kill_region();
		delete_region();
		toggle_markset();
		break;
	case CTRL_KEY('y'):
		reps = uarg_get();

		while (reps-- > 0) {
			killring_yank();
		}
		break;
	case ESC_KEY:
		editor.mode = MODE_ESCAPE;
		break;
	default:
		if (c == TAB_KEY) {
			reps = uarg_get();

			while (reps-- > 0) {
				insertch(c);
			}
		} else if (c >= 0x20 && c != 0x7f) {
			reps = uarg_get();

			while (reps-- > 0) {
				insertch(c);
			}
		}
		break;
	}

	editor.dirtyex = 1;
}


void
process_escape(const int16_t c)
{
	int	 reps = 0;

	editor_set_status("hi");

	switch (c) {
		case '>':
			ECURY = ENROWS;
			ECURX = 0;
			break;
		case '<':
			ECURY = 0;
			ECURX = 0;
			break;
		case 'b':
			reps = uarg_get();

			while (reps--) {
				find_prev_word();
			}
			break;
		case 'd':
			reps = uarg_get();

			while (reps--) {
				delete_next_word();
			}
			break;
		case 'f':
			reps = uarg_get();

			while (reps--) {
				find_next_word();
			}
			break;
		case 'm':
			toggle_markset();
			break;
		case 'w':
			if (!EMARK_SET) {
				editor_set_status("mark isn't set");
				break;
			}
			kill_region();
			toggle_markset();
			break;
		case BACKSPACE:
			reps = uarg_get();

			while (reps--) {
				delete_prev_word();
			}
			break;
		case ESC_KEY:
		case CTRL_KEY('g'):
			break; /* escape from escape-mode the movie */
		default:
			editor_set_status("unknown ESC key: %04x", c);
	}

	uarg_clear();
}


int
process_keypress(void)
{
	const int16_t	 c = get_keypress();

	if (c <= 0) {
		return 0;
	}

	switch (editor.mode) {
	case MODE_KCOMMAND:
		process_kcommand(c);
		editor.mode = MODE_NORMAL;
		break;
	case MODE_NORMAL:
		process_normal(c);
		break;
	case MODE_ESCAPE:
		process_escape(c);
		editor.mode = MODE_NORMAL;
		break;
	default:
		editor_set_status("we're in the %d-D space now cap'n",
		                  editor.mode);
		editor.mode = MODE_NORMAL;
	}

	return 1;
}


char
*get_cloc_code_lines(const char *filename)
{
	char	 command[512] = {0};
	char	 outbuf[256]  = {0};
	char	*result       = NULL;
	FILE	*pipe         = NULL;
	size_t	 len          = 0;

	if (filename == NULL) {
		snprintf(command, sizeof(command),
			 "buffer has no associated file.");
		result = malloc((kstrnlen(command, sizeof(command))) + 1);
		assert(result != NULL);
		strcpy(result, command);
		return result;
	}

	if (EDIRTY) {
		snprintf(command, sizeof(command),
			 "buffer must be saved first.");
		result = malloc((kstrnlen(command, sizeof(command))) + 1);
		assert(result != NULL);
		strcpy(result, command);
		return result;
	}

	snprintf(command,
		 sizeof(command),
		 "cloc --quiet %s | tail -2 | head -1 | awk '{print $5}'",
		 filename);

	pipe = popen(command, "r");
	if (!pipe) {
		snprintf(command, sizeof(command),
			 "Error getting LOC: %s", strerror(errno));
		result = (char *) malloc(sizeof(outbuf) + 1);
		return NULL;
	}

	if (fgets(outbuf, sizeof(outbuf), pipe) != NULL) {
		len = strlen(outbuf);
		if (len > 0 && outbuf[len - 1] == '\n') {
			outbuf[len - 1] = '\0';
		}

		result = malloc(strlen(outbuf) + 1);
		assert(result != NULL);
		if (result) {
			strcpy(result, outbuf);
			pclose(pipe);
			return result;
		}
	}

	pclose(pipe);
	char *zero = malloc(2);
	if (zero) {
		strcpy(zero, "0");
		return zero;
	}
	return NULL;
}


int
dump_pidfile(void)
{
	FILE* pid_file = NULL;

	if ((pid_file = fopen("ke.pid", "w")) == NULL) {
		editor_set_status("Failed to dump PID file: %s", strerror(errno));
		return 0;
	}

	fprintf(pid_file, "%ld", (long)getpid());
	fclose(pid_file);

	return 1;
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


int
kbhit(void)
{
	int	 bytes_waiting = 0;

	ioctl(STDIN_FILENO, FIONREAD, &bytes_waiting);
	if (bytes_waiting < 0) {
		editor_set_status("kbhit: FIONREAD failed: %s", strerror(errno));

		/* if FIONREAD fails, we need to assume we should read. this
		 * will default to a much slower input sequence, but it'll work.
		 */
		return 1;
	}
	return bytes_waiting > 0;
}


void
loop(void)
{
	int	up = 1; /* update on the first runthrough */

	while (1) {
		if (up) {
			display_refresh();
		}

		/*
		 * ke should only refresh the display if it has received keyboard
		 * input; if it has, drain all the inputs. This is useful for
		 * handling pastes without massive screen flicker.
		 *
		 */
		up = process_keypress();
		if (up  != 0) {
			while (kbhit()) {
				process_keypress();
			}
		}
	}
}


void
enable_debugging(void)
{
	dump_pidfile();
}


void
deathknell(void)
{
	fflush(stderr);

	if (editor.killring != NULL) {
		ab_free(editor.killring);
		free(editor.killring);
		editor.killring = NULL;
	}

	reset_editor();
	disable_termraw();
}


static void
signal_handler(int sig)
{
	signal(sig, SIG_DFL);

	fprintf(stderr, "caught signal %d\n", sig);

	deathknell();

	raise(sig);
	_exit(127 + sig);
}


static void
install_signal_handlers(void)
{
	/* Block all these signals while inside any of them */
	const int fatal_signals[] = {
		SIGABRT, SIGFPE, SIGILL, SIGSEGV,
	#ifdef SIGBUS
		SIGBUS,
	#endif
	#ifdef SIGQUIT
		SIGQUIT,
	#endif
	#ifdef SIGSYS
		SIGSYS,
	#endif
		-1                          /* sentinel */
	    };
	int	i = 0;

	for (i = 0; fatal_signals[i] != -1; i++) {
		signal(fatal_signals[i], signal_handler);
	}

	atexit(deathknell);
}


int
main(int argc, char *argv[])
{
	const char	*arg          = NULL;
	const char	*path         = NULL;
	int		 i            = 0;
	int		 v            = 0;
	int		 nb           = 0;
	int		 opt          = 0;
	int		 debug        = 0;
	int		 pending_line = 0;   /* line number for the next filename */
	int		 first_loaded = 0;   /* has a filed been loaded already? */

	install_signal_handlers();

	while ((opt = getopt(argc, argv, "df:")) != -1) {
		if (opt == 'd') {
			debug = 1;
		} else {
			fprintf(stderr, "Usage: ke [-d] [-f logfile] [ +N ] [file ...]\n");
			exit(EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;

	setlocale(LC_ALL, "");
	if (debug) {
		enable_debugging();
	}

	setup_terminal();
	init_editor();

	/* start processing file names. if an arg starts with a '+',
	 * interpret it as the line to jump to.
	 */
	for (i = 0; i < argc; i++) {
		arg = argv[i];
		if (arg[0] == '+') {
			path = arg + 1;

			v = 0;
			if (*path != '\0') {
				v = atoi(path);
				if (v < 1) v = 0;
			}

			pending_line = v;
			continue;
		}

		if (!first_loaded) {
			open_file(arg);
			if (pending_line > 0) {
				jump_to_position(0, pending_line - 1);
				pending_line = 0;
			}

			first_loaded = 1;
		} else {
			nb = buffer_add_empty();
			buffer_switch(nb);
			open_file(arg);
			if (pending_line > 0) {
				jump_to_position(0, pending_line - 1);
				pending_line = 0;
			}
		}
	}

	editor_set_status("C-k q to exit / C-k d to dump core");

	display_clear(NULL);
	loop();

	return 0;
}
