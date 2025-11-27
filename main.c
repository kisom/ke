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


#define INITIAL_CAPACITY	64


#define KILLRING_NO_OP		0	/* don't touch the killring */
#define KILLRING_APPEND		1	/* append deleted chars */
#define KILLRING_PREPEND	2	/* prepend deleted chars */
#define KILLING_SET		3	/* set killring to deleted char */
#define KILLRING_FLUSH		4	/* clear the killring */


#define calloc1(sz)		calloc(1, sz)


/* append buffer */
struct abuf {
	char	*b;
	size_t	 len;
	size_t	 cap;
};

#define ABUF_INIT		{NULL, 0, 0}


/* editor row */
struct erow {
	char	*line;
	char	*render;

	int	 size;
	int	 rsize;

	int	 cap;
	int	 dirty;
};


/*
 * editor is the global editor state; it should be broken out
 * to buffers and screen state, probably.
 */
struct editor_t {
	struct termios	 entry_term;
	int		 rows, cols;
	int		 curx, cury;
	int		 rx;
	int		 mode;
	int		 nrows;
	int		 rowoffs, coloffs;
	struct erow	*row;
	struct erow	*killring;
	int		 kill;    /* KILL CHAIN (sounds metal) */
	int		 no_kill; /* don't kill in delete_row */
	char		*filename;
	int		 dirty;
	int		 dirtyex;
	char		 msg[80];
	int		 mark_set;
	int		 mark_curx, mark_cury;
	int		 uarg, ucount; /* C-u support */
	time_t		 msgtm;
} editor = {
	.cols = 0,
	.rows = 0,
	.curx = 0,
	.cury = 0,
	.mode = 0,
	.nrows = 0,
	.rowoffs = 0,
	.coloffs = 0,
	.row = NULL,
	.killring = NULL,
	.kill = 0,
	.no_kill = 0,
	.filename = NULL,
	.dirty = 0,
	.dirtyex = 0,
	.mark_set = 0,
	.mark_curx = 0,
	.mark_cury = 0,
	.uarg = 0,
	.ucount = 0,
};


void		 init_editor(void);
void		 reset_editor(void);

/* small tools, abufs, etc */
int		 next_power_of_2(int n);
int		 cap_growth(int cap, int sz);
size_t		 kstrnlen(const char *buf, const size_t max);
void		 ab_init(struct abuf *buf);
void		 ab_appendch(struct abuf *buf, char c);
void		 ab_append(struct abuf *buf, const char *s, size_t len);
void		 ab_prependch(struct abuf *buf, char c);
void		 ab_prepend(struct abuf *buf, const char *s, size_t len);
void		 ab_free(struct abuf *buf);
char		 nibble_to_hex(char c);
void		 swap_int(int *a, int *b);

/* editor rows */
int		 erow_render_to_cursor(struct erow *row, int cx);
int		 erow_cursor_to_render(struct erow *row, int rx);
int		 erow_init(struct erow *row, int len);
void		 erow_update(struct erow *row);
void		 erow_insert(int at, char *s, int len);
void		 erow_free(struct erow *row);


/* kill ring, marking, etc... */
void		 killring_flush(void);
void		 killring_yank(void);
void		 killring_start_with_char(unsigned char ch);
void		 killring_append_char(unsigned char ch);
void		 killring_prepend_char(unsigned char ch);
void		 toggle_markset(void);
int		 cursor_after_mark(void);
int		 count_chars_from_cursor_to_mark(void);
void		 kill_region(void);
void		 indent_region(void);
void		 delete_region(void);

/* miscellaneous */
void		 kwrite(int fd, const char *buf, int len);
void		 die(const char *s);
int		 get_winsz(int *rows, int *cols);
void		 jump_to_position(int col, int row);
void		 goto_line(void);
int		 cursor_at_eol(void);
int		 iswordchar(unsigned char c);
void		 find_next_word(void);
void		 delete_next_word(void);
void		 find_prev_word(void);
void		 delete_prev_word(void);
void		 delete_row(int at);
void		 row_insert_ch(struct erow *row, int at, int16_t c);
void		 row_delete_ch(struct erow *row, int at);
void		 insertch(int16_t c);
void		 deletech(uint8_t op);
void		 open_file(const char *filename);
char		*rows_to_buffer(int *buflen);
int		 save_file(void);
uint16_t	 is_arrow_key(int16_t c);
int16_t		 get_keypress(void);
void		 display_refresh(void);
void		 editor_find_callback(char *query, int16_t c);
void		 editor_find(void);
char		*editor_prompt(char*, void (*cb)(char*, int16_t));
void		 editor_openfile(void);
int		 first_nonwhitespace(struct erow *row);
void		 move_cursor_once(int16_t c, int interactive);
void		 move_cursor(int16_t c, int interactive);
void		 uarg_start(void);
void		 uarg_digit(int d);
void		 uarg_clear(void);
int		 uarg_get(void);
void		 newline(void);
void		 process_kcommand(int16_t c);
void		 process_normal(int16_t c);
void		 process_escape(int16_t c);
int		 process_keypress(void);
char		*get_cloc_code_lines(const char *filename);
int		 dump_pidfile(void);
void		 enable_termraw(void);
void		 display_clear(struct abuf *ab);
void		 disable_termraw(void);
void		 setup_terminal(void);
void		 draw_rows(struct abuf *ab);
char		 status_mode_char(void);
void		 draw_status_bar(struct abuf *ab);
void		 draw_message_line(struct abuf *ab);
void		 scroll(void);
void		 display_refresh(void);
void		 editor_set_status(const char *fmt, ...);
void		 loop(void);
void		 enable_debugging(void);
void		 deathknell(void);
static void	 signal_handler(int sig);
static void	 install_signal_handlers(void);


#ifndef strnstr
/*
 * Find the first occurrence of find in s, where the search is limited to the
 * first slen characters of s.
 */
char
*strnstr(const char *s, const char *find, size_t slen)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if (slen-- < 1 || (sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char*)s);
}
#endif


int
next_power_of_2(int n)
{
	if (n < 2) {
		n = 2;
	}

	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;

	return n + 1;
}


int
cap_growth(int cap, int sz)
{
	if (cap == 0) {
		cap = INITIAL_CAPACITY;
	}

	while (cap <= sz) {
		cap = next_power_of_2(cap + 1);
	}

	return cap;
}


enum KeyPress {
	TAB_KEY = 9,
	ESC_KEY = 27,
	BACKSPACE = 127,
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PG_UP,
	PG_DN,
};


size_t
kstrnlen(const char *buf, const size_t max)
{
	if (buf == NULL) {
		return 0;
	}

	return strnlen(buf, max);
}


/*
 * init_editor should set up the global editor struct.
 */
void
init_editor(void)
{
	editor.cols = 0;
	editor.rows = 0;

	if (get_winsz(&editor.rows, &editor.cols) == -1) {
		die("can't get window size");
	}
	editor.rows--; /* status bar */
	editor.rows--; /* message line */

	editor.curx = editor.cury = 0;
	editor.rx = 0;

	editor.nrows = 0;
	editor.rowoffs = editor.coloffs = 0;
	editor.row = NULL;

	/* don't clear out the kill ring:
	 * killing / yanking across files is helpful, and killring
	 * is initialized to NULL at program start.
	 */
	/* editor.killring = NULL; */
	editor.kill = 0;
	editor.no_kill = 0;

	editor.msg[0] = '\0';
	editor.msgtm = 0;

	editor.dirty = 0;
	editor.mark_set = 0;
	editor.mark_cury = editor.mark_curx = 0;
}


/*
 * reset_editor presumes that editor has been initialized.
 */
void
reset_editor(void)
{
	for (int i = 0; i < editor.nrows; i++) {
		erow_free(&editor.row[i]);
	}
	free(editor.row);

	if (editor.filename != NULL) {
		free(editor.filename);
		editor.filename = NULL;
	}


	init_editor();
}


void
ab_init(struct abuf *buf)
{
	buf->b = NULL;
	buf->len = 0;
	buf->cap = 0;
}


void
ab_appendch(struct abuf *buf, char c)
{
	ab_append(buf, &c, 1);
}


void
ab_append(struct abuf *buf, const char *s, size_t len)
{
	char	*nc = buf->b;
	size_t	 sz = buf->len + len;

	if (sz >= buf->cap) {
		while (sz > buf->cap) {
			if (buf->cap == 0) {
				buf->cap = 1;
			} else {
				buf->cap *= 2;
			}
		}
		nc = realloc(nc, buf->cap);
		assert(nc != NULL);
	}

	memcpy(&nc[buf->len], s, len);
	buf->b = nc;
	buf->len += len;
}


void
ab_prependch(struct abuf *buf, const char c)
{
	ab_prepend(buf, &c, 1);
}


void
ab_prepend(struct abuf *buf, const char *s, const size_t len)
{
	char	*nc = realloc(buf->b, buf->len + len);
	assert(nc != NULL);

	memmove(nc + len, nc, buf->len);
	memcpy(nc, s, len);

	buf->b = nc;
	buf->len += len;
}


void
ab_free(struct abuf *buf)
{
	free(buf->b);
	buf->b = NULL;
	buf->len = 0;
	buf->cap = 0;
}


char
nibble_to_hex(char c)
{
	c &= 0xf;
	if (c < 10) {
		return (char)('0' + c);
	}
	return (char)('A' + (c - 10));
}


void
swap_int(int *a, int *b)
{
	*a ^= *b;
	*b ^= *a;
	*a ^= *b;
}


int
erow_render_to_cursor(struct erow *row, int cx)
{
	int		 rx = 0;
	size_t		 j = 0;
	wchar_t		 wc;
	mbstate_t	 st;

	memset(&st, 0, sizeof(st));

	while (j < (size_t)cx && j < (size_t)row->size) {
		unsigned char b = (unsigned char)row->line[j];
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
		size_t n = mbrtowc(&wc, &row->line[j], rem, &st);

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
erow_cursor_to_render(struct erow *row, int rx)
{
	int		 cur_rx = 0;
	size_t		 j = 0;
	wchar_t		 wc;
	mbstate_t	 st;

	memset(&st, 0, sizeof(st));

	while (j < (size_t)row->size) {
		int w = 0;
		size_t adv = 1;

		unsigned char b = (unsigned char)row->line[j];
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
			size_t n = mbrtowc(&wc, &row->line[j], rem, &st);

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
erow_init(struct erow *row, int len)
{
	row->size = len;
	row->rsize = 0;
	row->render = NULL;
	row->line = NULL;
	row->cap = cap_growth(0, len) + 1; /* extra byte for NUL end */
	row->dirty = 1;

	row->line = malloc(row->cap);
	assert(row->line != NULL);
	if (row->line == NULL) {
		return -1;
	}

	row->line[len] = '\0';
	return 0;
}


void
erow_update(struct erow *row)
{
	int i = 0, j;
	int tabs = 0;
	int ctrl = 0;

	/*
	 * TODO(kyle): I'm not thrilled with this double-render.
	 */
	for (j = 0; j < row->size; j++) {
		if (row->line[j] == '\t') {
			tabs++;
		} else if ((unsigned char)row->line[j] < 0x20) {
			/* treat only ASCII control characters as non-printable */
			ctrl++;
		}
	}

	if (row->rsize || row->render != NULL) {
		free(row->render);
		row->rsize = 0;
	}
	row->render = NULL;
	row->render = malloc(
		row->size + (tabs * (TAB_STOP - 1)) + (ctrl * 3) + 1);
	assert(row->render != NULL);

	for (j = 0; j < row->size; j++) {
		if (row->line[j] == '\t') {
			do {
				row->render[i++] = ' ';
			} while ((i % TAB_STOP) != 0) ;
		} else if ((unsigned char)row->line[j] < 0x20) {
			row->render[i++] = '\\';
			row->render[i++] = nibble_to_hex(row->line[j] >> 4);
			row->render[i++] = nibble_to_hex(row->line[j] & 0x0f);
		} else {
			/* leave UTF-8 multibyte bytes untouched so terminal can render */
			row->render[i++] = row->line[j];
		}
	}

	row->render[i] = '\0';
	row->rsize = i;
}


void
erow_insert(int at, char *s, int len)
{
	struct erow	row;

	if (at < 0 || at > editor.nrows) {
		return;
	}

	assert(erow_init(&row, len) == 0);
	memcpy(row.line, s, len);
	row.line[len] = 0;

	editor.row = realloc(editor.row,
	                     sizeof(struct erow) * (editor.nrows + 1));
	assert(editor.row != NULL);

	if (at < editor.nrows) {
		memmove(&editor.row[at + 1],
		        &editor.row[at],
		        sizeof(struct erow) * (editor.nrows - at));
	}

	editor.row[at] = row;
	erow_update(&editor.row[at]);
	editor.nrows++;
}


void
erow_free(struct erow *row)
{
	free(row->render);
	free(row->line);
	row->render = NULL;
	row->line = NULL;
}


void
killring_flush(void)
{
	if (editor.killring != NULL) {
		erow_free(editor.killring);
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
	for (int i = 0; i < editor.killring->size; i++) {
		unsigned char ch = (unsigned char)editor.killring->line[i];
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
	struct erow *row = NULL;

	if (editor.killring != NULL) {
		erow_free(editor.killring);
		free(editor.killring);
		editor.killring = NULL;
	}

	editor.killring = malloc(sizeof(struct erow));
	assert(editor.killring != NULL);
	assert(erow_init(editor.killring, 0) == 0);

	/* append one char to empty killring without affecting editor.dirty */
	row = editor.killring;

	row->line = realloc(row->line, row->size + 2);
	assert(row->line != NULL);
	row->line[row->size] = ch;
	row->size++;
	row->line[row->size] = '\0';
	row->dirty = 1;
}


void
killring_append_char(unsigned char ch)
{
	struct erow *row = NULL;

	if (editor.killring == NULL) {
		killring_start_with_char(ch);
		return;
	}

	row = editor.killring;
	row->line = realloc(row->line, row->size + 2);
	assert(row->line != NULL);
	row->line[row->size] = ch;
	row->size++;
	row->line[row->size] = '\0';
	row->dirty = 1;
}


void
killring_prepend_char(unsigned char ch)
{
	if (editor.killring == NULL) {
		killring_start_with_char(ch);
		return;
	}

	struct erow *row = editor.killring;
	row->line = realloc(row->line, row->size + 2);
	assert(row->line != NULL);
	memmove(&row->line[1], &row->line[0], row->size + 1);
	row->line[0] = ch;
	row->size++;
	row->dirty = 1;
}


void
toggle_markset(void)
{
	if (editor.mark_set) {
		editor.mark_set = 0;
		editor_set_status("Mark cleared.");
		return;
	}

	editor.mark_set = 1;
	editor.mark_curx = editor.curx;
	editor.mark_cury = editor.cury;
	editor_set_status("Mark set.");
}


int
cursor_after_mark(void)
{
	if (editor.mark_cury < editor.cury) {
		return 1;
	}

	if (editor.mark_cury > editor.cury) {
		return 0;
	}

	return editor.curx >= editor.mark_curx;
}


int
count_chars_from_cursor_to_mark(void)
{
	int count = 0;
	int curx = editor.curx;
	int cury = editor.cury;

	int markx = editor.mark_curx;
	int marky = editor.mark_cury;

	if (!cursor_after_mark()) {
		swap_int(&curx, &markx);
		swap_int(&curx, &marky);
	}

	editor.curx = markx;
	editor.cury = marky;

	while (editor.cury != cury) {
		while (!cursor_at_eol()) {
			move_cursor(ARROW_RIGHT, 1);
			count++;
		}

		move_cursor(ARROW_RIGHT, 1);
		count++;
	}

	while (editor.curx != curx) {
		count++;
		move_cursor(ARROW_RIGHT, 1);
	}

	return count;
}


void
kill_region(void)
{
	int curx = editor.curx;
	int cury = editor.cury;
	int markx = editor.mark_curx;
	int marky = editor.mark_cury;

	if (!editor.mark_set) {
		return;
	}

	/* kill the current killring */
	killring_flush();

	if (!cursor_after_mark()) {
		swap_int(&curx, &markx);
		swap_int(&cury, &marky);
	}

	editor.curx = markx;
	editor.cury = marky;

	while (editor.cury != cury) {
		while (!cursor_at_eol()) {
			killring_append_char(editor.row[editor.cury].line[editor.curx]);
			move_cursor(ARROW_RIGHT, 0);
		}
		killring_append_char('\n');
		move_cursor(ARROW_RIGHT, 0);
	}

	while (editor.curx != curx) {
		killring_append_char(editor.row[editor.cury].line[editor.curx]);
		move_cursor(ARROW_RIGHT, 0);
	}

	editor_set_status("Region killed.");
	/* clearing the mark needs to be done outside this function;	*
         * when deleteing the region, the mark needs to be set too.	*/
}


void
indent_region(void)
{
	int start_row, end_row;
	int i;

	if (!editor.mark_set) {
		return;
	}

	if (editor.mark_cury < editor.cury) {
		start_row = editor.mark_cury;
		end_row = editor.cury;
	} else if (editor.mark_cury > editor.cury) {
		start_row = editor.cury;
		end_row = editor.mark_cury;
	} else {
		start_row = end_row = editor.cury;
	}

	/* Ensure bounds are valid */
	if (start_row < 0) {
		start_row = 0;
	}

	if (end_row >= editor.nrows) {
		end_row = editor.nrows - 1;
	}

	if (start_row >= editor.nrows || end_row < 0) {
		return;
	}

	/* Prepend a tab character to every row in the region */
	for (i = start_row; i <= end_row; i++) {
		row_insert_ch(&editor.row[i], 0, '\t');
	}

	/* Move cursor to beginning of the line it was on */
	editor.curx = 0;
	editor.dirty++;
}


void
unindent_region(void)
{
	int		 start_row, end_row, i, del;
	struct erow	*row;

	if (!editor.mark_set) {
		editor_set_status("Mark not set.");
		return;
	}

	if (editor.mark_cury < editor.cury ||
	    (editor.mark_cury == editor.cury && editor.mark_curx < editor.curx)) {
		start_row = editor.mark_cury;
		end_row   = editor.cury;
	    } else {
	    	start_row = editor.cury;
	    	end_row   = editor.mark_cury;
	    }

	if (start_row >= editor.nrows) {
		return;
	}

	if (end_row >= editor.nrows) {
		end_row = editor.nrows - 1;
	}

	/* actually unindent every line in the region */
	for (i = start_row; i <= end_row; i++) {
		row = &editor.row[i];

		if (row->size == 0) {
			continue;
		}

		if (row->line[0] == '\t') {
			row_delete_ch(row, 0);
		} else if (row->line[0] == ' ') {
			del = 0;

			while (del < TAB_STOP && del < row->size &&
				row->line[del] == ' ') {
				del++;
			}

			if (del > 0) {
				memmove(row->line, row->line + del, row->size - del + 1); /* +1 for NUL */
				row->size -= del;
				row->dirty = 1;
			}
		}
	}

	editor.curx = 0;
	editor.cury = start_row;

	editor.dirty++;
	editor_set_status("Region unindented");
}


/* call after kill_region */
void
delete_region(void)
{
	int count = count_chars_from_cursor_to_mark();
	int killed = 0;
	int curx = editor.curx;
	int cury = editor.cury;
	int markx = editor.mark_curx;
	int marky = editor.mark_cury;


	if (!editor.mark_set) {
		return;
	}

	if (!cursor_after_mark()) {
		swap_int(&curx, &markx);
		swap_int(&cury, &marky);
	}

	jump_to_position(markx, marky);

	while (killed < count) {
		move_cursor(ARROW_RIGHT, 0);
		deletech(KILLRING_NO_OP);
		killed++;
	}

	while (editor.curx != markx && editor.cury != marky) {
		deletech(KILLRING_NO_OP);
	}

	editor.kill = 1;
	editor_set_status("Region killed.");
}


void
kwrite(const int fd, const char* buf, const int len)
{
	int wlen = 0;

	wlen = write(fd, buf, len);
	assert(wlen != -1);
	assert(wlen == len);
	if (wlen == -1) {
		abort();
	}
}


void
die(const char* s)
{
	kwrite(STDOUT_FILENO, "\x1b[2J", 4);
	kwrite(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);
	exit(1);
}


/*
 * get_winsz uses the TIOCGWINSZ to get the window size.
 *
 * there's a fallback way to do this, too, that involves moving the
 * cursor down and to the left \x1b[999C\x1b[999B. I'm going to skip
 * on this for now because it's bloaty and this works on OpenBSD and
 * Linux, at least.
 */
int
get_winsz(int *rows, int *cols)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 ||
		ws.ws_col == 0) {
		return -1;
	}

	*cols = ws.ws_col;
	*rows = ws.ws_row;
	return 0;
}


void
jump_to_position(int col, int row)
{
	/* clamp position */
	if (row < 0) {
		row = 0;
	} else if (row > editor.nrows) {
		row = editor.nrows - 1;
	}

	if (col < 0) {
		col = 0;
	} else if (col > editor.row[row].size) {
		col = editor.row[row].size;
	}

	editor.curx = col;
	editor.cury = row;

	scroll();
	display_refresh();
}


void
goto_line(void)
{
	int lineno = 0;
	char *query = editor_prompt("Line: %s", NULL);

	if (query == NULL) {
		return;
	}

	lineno = atoi(query);
	if (lineno < 1 || lineno >= editor.nrows) {
		editor_set_status("Line number must be between 1 and %d.",
		                  editor.nrows);
		free(query);
		return;
	}

	jump_to_position(0, lineno - 1);
	free(query);
}


int
cursor_at_eol(void)
{
	assert(editor.curx >= 0);
	assert(editor.cury >= 0);
	assert(editor.cury <= editor.nrows);
	assert(editor.curx <= editor.row[editor.cury].size);

	return editor.curx == editor.row[editor.cury].size;
}


int
iswordchar(unsigned char c)
{
	return isalnum(c) || c == '_' || strchr("/!@#$%^&*+-=~", c) != NULL;
}


void
find_next_word(void)
{
	while (cursor_at_eol()) {
		move_cursor(ARROW_RIGHT, 1);
	}

	if (iswordchar(editor.row[editor.cury].line[editor.curx])) {
		while (!isspace(editor.row[editor.cury].line[editor.curx]) && !
			cursor_at_eol()) {
			move_cursor(ARROW_RIGHT, 1);
		}

		return;
	}

	if (isspace(editor.row[editor.cury].line[editor.curx])) {
		while (isspace(editor.row[editor.cury].line[editor.curx])) {
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

	if (iswordchar(editor.row[editor.cury].line[editor.curx])) {
		while (!isspace(editor.row[editor.cury].line[editor.curx]) && !
			cursor_at_eol()) {
			move_cursor(ARROW_RIGHT, 1);
			deletech(KILLRING_APPEND);
		}

		return;
	}

	if (isspace(editor.row[editor.cury].line[editor.curx])) {
		while (isspace(editor.row[editor.cury].line[editor.curx])) {
			move_cursor(ARROW_RIGHT, 1);
			deletech(KILLRING_APPEND);
		}

		delete_next_word();
	}
}


void
find_prev_word(void)
{
	if (editor.cury == 0 && editor.curx == 0) {
		return;
	}

	move_cursor(ARROW_LEFT, 1);

	while (cursor_at_eol() || isspace(
		editor.row[editor.cury].line[editor.curx])) {
		if (editor.cury == 0 && editor.curx == 0) {
			return;
		}

		move_cursor(ARROW_LEFT, 1);
	}

	while (editor.curx > 0 && !isspace(
		editor.row[editor.cury].line[editor.curx - 1])) {
		move_cursor(ARROW_LEFT, 1);
	}
}


void
delete_prev_word(void)
{
	if (editor.cury == 0 && editor.curx == 0) {
		return;
	}

	deletech(KILLRING_PREPEND);

	while (editor.cury > 0 || editor.curx > 0) {
		if (editor.curx == 0) {
			deletech(KILLRING_PREPEND);
			continue;
		}

		if (!isspace(editor.row[editor.cury].line[editor.curx - 1])) {
			break;
		}

		deletech(KILLRING_PREPEND);
	}

	while (editor.curx > 0) {
		if (isspace(editor.row[editor.cury].line[editor.curx - 1])) {
			break;
		}
		deletech(KILLRING_PREPEND);
	}
}


void
delete_row(int at)
{
	if (at < 0 || at >= editor.nrows) {
		return;
	}

	/*
	 * Update killring with the deleted row's contents followed by a newline
	 * unless this deletion is an internal merge triggered by deletech at
	 * start-of-line. In that case, deletech will account for the single
	 * newline itself and we must NOT also push the entire row here.
	 */
	if (!editor.no_kill) {
		struct erow *r = &editor.row[at];
		/* Start or continue the kill sequence based on editor.killing */
		if (r->size > 0) {
			/* push raw bytes of the line */
			if (!editor.kill) {
				killring_start_with_char(
					(unsigned char)r->line[0]);
				for (int i = 1; i < r->size; i++) {
					killring_append_char(
						(unsigned char)r->line[i]);
				}
			} else {
				for (int i = 0; i < r->size; i++) {
					killring_append_char(
						(unsigned char)r->line[i]);
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

	erow_free(&editor.row[at]);
	memmove(&editor.row[at],
	        &editor.row[at + 1],
	        sizeof(struct erow) * (editor.nrows - at - 1));
	editor.nrows--;
	editor.dirty++;
}


void
row_append_row(struct erow *row, char *s, int len)
{
	row->line = realloc(row->line, row->size + len + 1);
	assert(row->line != NULL);
	memcpy(&row->line[row->size], s, len);
	row->size += len;
	row->line[row->size] = '\0';
	row->dirty = 1;
	editor.dirty++;
}


void
row_insert_ch(struct erow *row, int at, int16_t c)
{
	/*
	 * row_insert_ch just concerns itself with how to update a row.
	 */
	if ((at < 0) || (at > row->size)) {
		at = row->size;
	}
	assert(c > 0);

	row->line = realloc(row->line, row->size + 2);
	assert(row->line != NULL);
	memmove(&row->line[at + 1], &row->line[at], row->size - at + 1);
	row->size++;
	row->line[at] = c & 0xff;

	row->dirty = 1;
}


void
row_delete_ch(struct erow *row, int at)
{
	if (at < 0 || at >= row->size) {
		return;
	}

	memmove(&row->line[at], &row->line[at + 1], row->size - at);
	row->size--;
	row->dirty = 1;
	editor.dirty++;
}


void
insertch(int16_t c)
{
	/*
	 * insert_ch doesn't need to worry about how to update a
	 * a row; it can just figure out where the cursor is
	 * at and what to do.
	 */
	if (editor.cury == editor.nrows) {
		erow_insert(editor.nrows, "", 0);
	}

	/* Inserting ends kill ring chaining. */
	editor.kill = 0;

	/* Ensure we pass a non-negative byte value to avoid assert(c > 0). */
	row_insert_ch(&editor.row[editor.cury],
	              editor.curx,
	              (int16_t)(c & 0xff));
	editor.curx++;
	editor.dirty++;
}


void
deletech(uint8_t op)
{
	struct erow	*row = NULL;
	unsigned char	 dch = 0;
	int		 prev = 0;

	if (editor.cury >= editor.nrows) {
		return;
	}

	if (editor.cury == 0 && editor.curx == 0) {
		return;
	}

	row = &editor.row[editor.cury];
	if (editor.curx > 0) {
		dch = (unsigned char)row->line[editor.curx - 1];
	} else {
		dch = '\n';
	}

	if (editor.curx > 0) {
		row_delete_ch(row, editor.curx - 1);
		editor.curx--;
	} else {
		editor.curx = editor.row[editor.cury - 1].size;
		row_append_row(&editor.row[editor.cury - 1],
		               row->line,
		               row->size);

		prev = editor.no_kill;
		editor.no_kill = 1;

		delete_row(editor.cury);
		editor.no_kill = prev;
		editor.cury--;
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
	char	*line = NULL;
	size_t	 linecap = 0;
	ssize_t	 linelen;
	FILE	*fp = NULL;

	reset_editor();

	if (filename == NULL) {
		return;
	}

	editor.filename = strdup(filename);
	assert(editor.filename != NULL);

	editor.dirty = 0;
	if ((fp = fopen(editor.filename, "r")) == NULL) {
		if (errno == ENOENT) {
			editor_set_status("[new file]");
			return;
		}
		die("fopen");
	}

	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		if (linelen != -1) {
			while ((linelen > 0) && ((line[linelen - 1] == '\r') ||
				(line[linelen - 1] == '\n'))) {
				linelen--;
			}

			erow_insert(editor.nrows, line, linelen);
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
	int len = 0;
	int j;
	char *buf = NULL;
	char *p = NULL;

	for (j = 0; j < editor.nrows; j++) {
		/* extra byte for newline */
		len += editor.row[j].size + 1;
	}

	if (len == 0) {
		return NULL;
	}

	*buflen = len;
	buf = malloc(len);
	assert(buf != NULL);
	p = buf;

	for (j = 0; j < editor.nrows; j++) {
		memcpy(p, editor.row[j].line, editor.row[j].size);
		p += editor.row[j].size;
		*p++ = '\n';
	}

	return buf;
}


int
save_file(void)
{
	int fd = -1;
	int len;
	int status = 1; /* will be used as exit code */
	char *buf;

	if (!editor.dirty) {
		editor_set_status("No changes to save.");
		return 0;
	}

	if (editor.filename == NULL) {
		editor.filename = editor_prompt("Filename: %s", NULL);
		if (editor.filename == NULL) {
			editor_set_status("Save aborted.");
			return 0;
		}
	}

	buf = rows_to_buffer(&len);
	if ((fd = open(editor.filename, O_RDWR | O_CREAT, 0644)) == -1) {
		goto save_exit;
	}

	if (-1 == ftruncate(fd, len)) {
		goto save_exit;
	}

	if (len == 0) {
		status = 0;
		goto save_exit;
	}

	if ((ssize_t)len != write(fd, buf, len)) {
		goto save_exit;
	}

	status = 0;

save_exit:
	if (fd)
		close(fd);
	if (buf) {
		free(buf);
		buf = NULL;
	}

	if (status != 0) {
		buf = strerror(errno);
		editor_set_status("Error writing %s: %s",
		                  editor.filename,
		                  buf);
	} else {
		editor_set_status("Wrote %d bytes to %s.",
		                  len,
		                  editor.filename);
		editor.dirty = 0;
	}

	return status;
}


uint16_t
is_arrow_key(int16_t c)
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
	};

	return 0;
}


int16_t
get_keypress(void)
{
	char seq[3];
	/* read raw byte so UTF-8 bytes (>=0x80) are not sign-extended */
	unsigned char uc = 0;
	int16_t c;

	if (read(STDIN_FILENO, &uc, 1) == -1) {
		die("get_keypress:read");
	}

	c = (int16_t)uc;

	if (c == 0x1b) {
		if (read(STDIN_FILENO, &seq[0], 1) != 1)
			return c;
		if (read(STDIN_FILENO, &seq[1], 1) != 1)
			return c;

		if (seq[0] == '[') {
			if (seq[1] < 'A') {
				if (read(STDIN_FILENO, &seq[2], 1) != 1)
					return c;
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
					/* nada */ ;
				}
			}
		} else if (seq[0] == 'O') {
			switch (seq[1]) {
			case 'F':
				return END_KEY;
			case 'H':
				return HOME_KEY;
			}
		}

		return 0x1b;
	}

	return c;
}


char
*editor_prompt(char *prompt, void (*cb)(char*, int16_t))
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
		} else if ((c == TAB_KEY) || (c >= 0x20 && c < 0x7f)) {
			if (buflen == bufsz - 1) {
				bufsz *= 2;
				buf = realloc(buf, bufsz);

				assert(buf != NULL);
			}

			buf[buflen++] = (char)(c & 0xff);
			buf[buflen] = '\0';
		}

		if (cb) {
			cb(buf, c);
		}
	}

	free(buf);
	return NULL;
}


void
editor_find_callback(char* query, int16_t c)
{
	static int	 last_match = -1;       /* row index of last match */
	static int	 direction = 1;         /* 1 = forward, -1 = backward */
	static char	 last_query[128] = {0}; /* remember last successful query */
	struct erow	*row;
	int		 saved_cx = editor.curx;
	int		 saved_cy = editor.cury;
	size_t		 qlen = strlen(query);
	char		*match;
	int		 i;

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
		strcpy(last_query, query);
	}

	int start_row = editor.cury;
	int start_col = editor.curx;

	if (last_match == -1) {
		if (direction == 1) {
			start_col += 1;
		}
		last_match = editor.cury;
	}

	int current = last_match;
	int wrapped = 0;

	for (i = 0; i < editor.nrows; i++) {
		current += direction;

		if (current >= editor.nrows) {
			current = 0;
			if (wrapped++) break;
		}
		if (current < 0) {
			current = editor.nrows - 1;
			if (wrapped++) break;
		}

		row = &editor.row[current];

		/* Skip rendering search on raw bytes — use line[] but respect render offsets */
		row->dirty = 1;

		char* search_start = row->render;
		if (current == start_row && direction == 1 && last_match == -1) {
			/* On first search forward: skip text before cursor */
			int skip = erow_render_to_cursor(row, start_col);
			search_start += skip;
		}

		match = strnstr(search_start, query, row->rsize - (search_start - row->render));
		if (match) {
			last_match = current;
			editor.cury = current;
			editor.curx = erow_cursor_to_render(row, match - row->render);
			if (current == start_row && direction == 1 && last_match == -1) {
				editor.curx += start_col;  /* adjust if we skipped prefix */
			}
			scroll();
			display_refresh();
			return;
		}
	}

	/* No match found */
	if (qlen > 0) {
		editor_set_status("Failing search: %s", query);
	}
	editor.curx = saved_cx;
	editor.cury = saved_cy;
	display_refresh();
}


void
editor_find(void)
{
	/* TODO(kyle): consider making this an abuf */
	char *query;
	int scx = editor.curx;
	int scy = editor.cury;
	int sco = editor.coloffs;
	int sro = editor.rowoffs;

	query = editor_prompt("Search (ESC to cancel): %s",
	                      editor_find_callback);
	if (query) {
		free(query);
		query = NULL;
	} else {
		editor.curx = scx;
		editor.cury = scy;
		editor.coloffs = sco;
		editor.rowoffs = sro;
	}

	display_refresh();
}


void
editor_openfile(void)
{
	char *filename;

	/* TODO(kyle): combine with dirutils for tab-completion */
	filename = editor_prompt("Load file: %s", NULL);
	if (filename == NULL) {
		return;
	}

	open_file(filename);
	free(filename);
}


int
first_nonwhitespace(struct erow *row)
{
	int		 pos;
	wchar_t		 wc;
	mbstate_t	 state;
	size_t		 len;

	if (row == NULL) {
		return 0;
	}

	memset(&state, 0, sizeof(state));
	pos = editor.curx;
	if (pos > row->size) {
		pos = row->size;
	}

	while (pos < row->size) {
		if ((unsigned char)row->line[pos] < 0x80) {
			if (!isspace((unsigned char)row->line[pos])) {
				return pos;
			}
			pos++;
			continue;
		}

		len = mbrtowc(&wc, &row->line[pos], row->size - pos, &state);
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
move_cursor_once(int16_t c, int interactive)
{
	struct erow	*row;
	int		 reps = 0;

	row = (editor.cury >= editor.nrows) ? NULL : &editor.row[editor.cury];

	switch (c) {
	case ARROW_UP:
	case CTRL_KEY('p'):
		if (editor.cury > 0) {
			editor.cury--;
			row = (editor.cury >= editor.nrows)
				      ? NULL
				      : &editor.row[editor.cury];
			if (interactive) {
				editor.curx = first_nonwhitespace(row);
			}
		}
		break;
	case ARROW_DOWN:
	case CTRL_KEY('n'):
		if (editor.cury < editor.nrows - 1) {
			editor.cury++;
			row = (editor.cury >= editor.nrows)
				      ? NULL
				      : &editor.row[editor.cury];

			if (interactive) {
				editor.curx = first_nonwhitespace(row);
			}
		}
		break;
	case ARROW_RIGHT:
	case CTRL_KEY('f'):
		if (!row) {
			break;
		}

		if (editor.curx < row->size) {
			editor.curx++;
			/* skip over UTF-8 continuation bytes */
			while (editor.curx < row->size &&
				((unsigned char)row->line[editor.curx] &
					0xC0) == 0x80) {
				editor.curx++;
			}
		} else if (editor.curx == row->size && editor.cury < editor.nrows - 1) {
			editor.cury++;
			editor.curx = 0;
		}
		break;
	case ARROW_LEFT:
	case CTRL_KEY('b'):
		if (editor.curx > 0) {
			editor.curx--;
			while (editor.curx > 0 &&
				((unsigned char)row->line[editor.curx] &
					0xC0) == 0x80) {
				editor.curx--;
			}
		} else if (editor.cury > 0) {
			editor.cury--;
			editor.curx = editor.row[editor.cury].size;

			row = &editor.row[editor.cury];
			while (editor.curx > 0 &&
				((unsigned char)row->line[editor.curx] &
					0xC0) == 0x80) {
				editor.curx--;
			}
		}
		break;
	case PG_UP:
	case PG_DN:
		if (c == PG_UP) {
			editor.cury = editor.rowoffs;
		} else if (c == PG_DN) {
			editor.cury = editor.rowoffs + editor.rows - 1;
			if (editor.cury > editor.nrows) {
				editor.cury = editor.nrows;
			}
		}

		reps = editor.rows;
		while (--reps) {
			move_cursor(c == PG_UP ? ARROW_UP : ARROW_DOWN, 1);
		}

		break;

	case HOME_KEY:
	case CTRL_KEY('a'):
		editor.curx = 0;
		break;
	case END_KEY:
	case CTRL_KEY('e'):
		if (editor.cury >= editor.nrows) {
			break;
		}
		editor.curx = editor.row[editor.cury].size;
		break;
	default:
		break;
	}
}


void
move_cursor(int16_t c, int interactive)
{
	int	 n = uarg_get();

	while (n-- > 0) {
		move_cursor_once(c, interactive);
	}
}


void
newline(void)
{
	struct erow *row = NULL;

	if (editor.cury >= editor.nrows) {
		erow_insert(editor.cury, "", 0);
		editor.cury++;
		editor.curx = 0;
	} else if (editor.curx == 0) {
		erow_insert(editor.cury, "", 0);
		editor.cury++;
		editor.curx = 0;
	} else {
		row = &editor.row[editor.cury];
		erow_insert(editor.cury + 1,
		            &row->line[editor.curx],
		            row->size - editor.curx);
		row = &editor.row[editor.cury];
		row->size = editor.curx;
		row->line[row->size] = '\0';
		row->dirty = 1;
		editor.cury++;
		editor.curx = 0;
	}

	/* BREAK THE KILL CHAIN \m/ */
	editor.kill = 0;
}


void
uarg_start(void)
{
	if (editor.uarg == 0) {
		editor.ucount = 0;
	}else {
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
	int		n = editor.ucount > 0 ? editor.ucount : 1;

	uarg_clear();

	return n;
}


void
process_kcommand(int16_t c)
{
	char	*buf   = NULL;
	int	 len   = 0;
	int	 jumpx = 0;
	int	 jumpy = 0;
	int	 reps  = 0;

	switch (c) {
	case BACKSPACE:
		while (editor.curx > 0) {
			process_normal(BACKSPACE);
		}
		break;
	case '=':
		if (editor.mark_set) {
			indent_region();
		} else {
			editor_set_status("Mark not set.");
		}
		break;
	case '-':
		if (editor.mark_set) {
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
		editor_set_status("PID: %ld", (long)getpid());
		break;
	case ' ':
		toggle_markset();
		break;
	case CTRL_KEY(' '):
		jumpx = editor.mark_curx;
		jumpy = editor.mark_cury;
		editor.mark_curx = editor.curx;
		editor.mark_cury = editor.cury;

		jump_to_position(jumpx, jumpy);
		editor_set_status("Jumped to mark");
		break;
	case 'c':
		len = editor.killring->size;
		killring_flush();
		editor_set_status("Kill ring cleared (%d characters)", len);
		break;
	case 'd':
		if (editor.curx == 0 && cursor_at_eol()) {
			delete_row(editor.cury);
			return;
		}

		reps = uarg_get();
		while (reps--) {
			while ((editor.row[editor.cury].size -
				editor.curx) > 0) {
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
			delete_row(editor.cury);
		}
		break;
	case 'e':
	case CTRL_KEY('e'):
		if (editor.dirty && editor.dirtyex) {
			editor_set_status(
				"File not saved - C-k e again to open a new file anyways.");
			editor.dirtyex = 0;
			return;
		}
		editor_openfile();
		break;
	case 'f':
		editor_find();
		break;
	case 'g':
		goto_line();
		break;
	case 'j':
		if (!editor.mark_set) {
			editor_set_status("Mark not set.");
			break;
		}

		jumpx = editor.mark_curx;
		jumpy = editor.mark_cury;
		editor.mark_curx = editor.curx;
		editor.mark_cury = editor.cury;

		jump_to_position(jumpx, jumpy);
		editor_set_status("Jumped to mark; mark is now the previous location.");
		break;
	case 'l':
		buf = get_cloc_code_lines(editor.filename);

		editor_set_status("Lines of code: %s", buf);
		free(buf);
		break;
	case 'm':
		if (system("make") != 0) {
			editor_set_status(
				"process failed: %s",
				strerror(errno));
		} else {
			editor_set_status("make: ok");
		}
		break;
	case 'q':
		if (editor.dirty && editor.dirtyex) {
			editor_set_status(
				"File not saved - C-k q again to quit.");
			editor.dirtyex = 0;
			return;
		}
		exit(0);
	case CTRL_KEY('q'):
		exit(0);
	case CTRL_KEY('r'):
		if (editor.dirty && editor.dirtyex) {
			editor_set_status("File not saved - C-k C-r again to reload.");
			editor.dirtyex = 0;
			return;
		}

		jumpx = editor.curx;
		jumpy = editor.cury;
		buf = strdup(editor.filename);

		reset_editor();
		open_file(buf);
		display_refresh();

		jump_to_position(jumpx, jumpy);
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

		while (reps--);
		editor_set_status("Undo not implemented.");
		break;
	case 'U':
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
	int	reps = 0;

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
process_escape(int16_t c)
{
	editor_set_status("hi");

	switch (c) {
	case '>':
		editor.cury = editor.nrows;
		editor.curx = 0;
		break;
	case '<':
		editor.cury = 0;
		editor.curx = 0;
		break;
	case 'b':
		find_prev_word();
		break;
	case 'd':
		delete_next_word();
		break;
	case 'f':
		find_next_word();
		break;
	case 'm':
		toggle_markset();
		break;
	case 'w':
		if (!editor.mark_set) {
			editor_set_status("mark isn't set");
			break;
		}
		kill_region();
		toggle_markset();
		break;
	case BACKSPACE:
		delete_prev_word();
		break;
	case ESC_KEY:
	case CTRL_KEY('g'):
		break; /* escape out of escape-mode */
	default:
		editor_set_status("unknown ESC key: %04x", c);
	}
}


int
process_keypress(void)
{
	int16_t c = get_keypress();


	/* we didn't actually read a key */
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
	char command[512];
	char buffer[256];
	char *result = NULL;
	FILE* pipe = NULL;
	size_t len = 0;

	if (editor.filename == NULL) {
		snprintf(command, sizeof(command),
			 "buffer has no associated file.");
		result = malloc((kstrnlen(command, sizeof(command))) + 1);
		assert(result != NULL);
		strcpy(result, command);
		return result;
	}

	if (editor.dirty) {
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
		snprintf(command, sizeof(command), "Error getting LOC: %s", strerror(errno));
		result = (char*)malloc(sizeof(buffer) + 1);
		return NULL;
	}

	if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
		len = strlen(buffer);
		if (len > 0 && buffer[len - 1] == '\n') {
			buffer[len - 1] = '\0';
		}

		result = malloc(strlen(buffer) + 1);
		assert(result != NULL);
		if (result) {
			strcpy(result, buffer);
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


/*
 * A text editor needs the terminal to be in raw mode; but the default
 * is to be in canonical (cooked) mode, which is a buffered input mode.
 */
void
enable_termraw(void)
{
	struct termios raw;

	/* Read the current terminal parameters for standard input. */
	if (tcgetattr(STDIN_FILENO, &raw) == -1) {
		die("tcgetattr while enabling raw mode");
	}

	/*
	 * Put the terminal into raw mode.
	 */
	cfmakeraw(&raw);

	/*
	 * Set timeout for read(2).
	 *
	 * VMIN: what is the minimum number of bytes required for read
	 * to return?
	 *
	 * VTIME: max time before read(2) returns in hundreds of milli-
	 * seconds.
	 */
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	/*
	 * Now write the terminal parameters to the current terminal,
	 * after flushing any waiting input out.
	 */
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
		die("tcsetattr while enabling raw mode");
	}
}


void
display_clear(struct abuf *ab)
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

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &editor.entry_term) == -1) {
		die("couldn't disable terminal raw mode");
	}
}


void
setup_terminal(void)
{
	if (tcgetattr(STDIN_FILENO, &editor.entry_term) == -1) {
		die("can't snapshot terminal settings");
	}

	enable_termraw();
}


void
draw_rows(struct abuf *ab)
{
	assert(editor.cols >= 0);

	struct erow	*row;
	char		 buf[editor.cols];
	int		 len, filerow, padding;
	int		 y;

	for (y = 0; y < editor.rows; y++) {
		filerow = y + editor.rowoffs;
		if (filerow >= editor.nrows) {
			if ((editor.nrows == 0) && (y == editor.rows / 3)) {
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
			row = &editor.row[filerow];
			if (row->dirty) {
				erow_update(row);
				row->dirty = 0;
			}

			len = row->rsize - editor.coloffs;
			if (len < 0) {
				len = 0;
			}

			if (len > editor.cols) {
				len = editor.cols;
			}
			ab_append(ab,
			          editor.row[filerow].render + editor.coloffs,
			          len);
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
draw_status_bar(struct abuf *ab)
{
	char status[editor.cols];
	char rstatus[editor.cols];
	char mstatus[editor.cols];

	int len, rlen;

	len = snprintf(status,
	               sizeof(status),
	               "%c%cke: %.20s - %d lines",
	               status_mode_char(),
	               editor.dirty ? '!' : '-',
	               editor.filename ? editor.filename : "[no file]",
	               editor.nrows);

	if (editor.mark_set) {
		snprintf(mstatus,
		         sizeof(mstatus),
		         " | M: %d, %d ",
		         editor.mark_curx + 1,
		         editor.mark_cury + 1);
	} else {
		snprintf(mstatus, sizeof(mstatus), " | M:clear ");
	}

	rlen = snprintf(rstatus,
	                sizeof(rstatus),
	                "L%d/%d C%d %s",
	                editor.cury + 1,
	                editor.nrows,
	                editor.curx + 1,
	                mstatus);

	ab_append(ab, ESCSEQ "7m", 4);
	ab_append(ab, status, len);
	while (len < editor.cols) {
		if ((editor.cols - len) == rlen) {
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
draw_message_line(struct abuf *ab)
{
	int len = strlen(editor.msg);

	ab_append(ab, ESCSEQ "K", 3);
	if (len > editor.cols) {
		len = editor.cols;
	}

	if (len && ((time(NULL) - editor.msgtm) < MSG_TIMEO)) {
		ab_append(ab, editor.msg, len);
	}
}


void
scroll(void)
{
	struct erow	*row = NULL;
	editor.rx = 0;
	if (editor.cury < editor.nrows) {
		row = &editor.row[editor.cury];
		if (row->dirty == 1) {
			erow_update(row);
		}

		editor.rx = erow_render_to_cursor(row, editor.curx);
	}

	if (editor.cury < editor.rowoffs) {
		editor.rowoffs = editor.cury;
	}

	if (editor.cury >= editor.rowoffs + editor.rows) {
		editor.rowoffs = editor.cury - editor.rows + 1;
	}

	if (editor.rx < editor.coloffs) {
		editor.coloffs = editor.rx;
	}

	if (editor.rx >= editor.coloffs + editor.cols) {
		editor.coloffs = editor.rx - editor.cols + 1;
	}
}


void
display_refresh(void)
{
	char buf[32];
	struct abuf ab = ABUF_INIT;

	scroll();

	ab_append(&ab, ESCSEQ "?25l", 6);
	ab_append(&ab, ESCSEQ "H", 3);
	display_clear(&ab);

	draw_rows(&ab);
	draw_status_bar(&ab);
	draw_message_line(&ab);

	snprintf(buf,
	         sizeof(buf),
	         ESCSEQ "%d;%dH",
	         (editor.cury - editor.rowoffs) + 1,
	         (editor.rx - editor.coloffs) + 1);
	ab_append(&ab, buf, kstrnlen(buf, 32));
	/* ab_append(&ab, ESCSEQ "1;2H", 7); */
	ab_append(&ab, ESCSEQ "?25h", 6);

	kwrite(STDOUT_FILENO, ab.b, ab.len);
	ab_free(&ab);
}


void
editor_set_status(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(editor.msg, sizeof(editor.msg), fmt, ap);
	va_end(ap);

	editor.msgtm = time(NULL);
}


int
kbhit(void)
{
	int	 bytes_waiting;
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
	int up = 1; /* update on the first runthrough */

	while (1) {
		if (up)
			display_refresh();

		/*
		 * ke should only refresh the display if it has received keyboard
		 * input; if it has, drain all the inputs. This is useful for
		 * handling pastes without massive screen flicker.
		 *
		 */
		if ((up = process_keypress()) != 0) {
			while (kbhit()) {
				process_keypress();
			}
		}
	}
}


void
enable_debugging(void)
{
	time_t now;

	dump_pidfile();

	now = time(&now);
	printf("time: %s\n", ctime(&now));
	fprintf(stderr, "Debug log started %s\n", ctime(&now));
	fflush(stderr);
}


void
deathknell(void)
{
	fflush(stderr);

	if (editor.killring != NULL) {
		erow_free(editor.killring);
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
	int opt;
	int debug = 0;

	install_signal_handlers();

	while ((opt = getopt(argc, argv, "df:")) != -1) {
		switch (opt) {
		case 'd':
			debug = 1;
			break;
		default:
			fprintf(stderr, "Usage: ke [-d] [-f logfile] [path]\n");
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

	if (argc > 0) {
		open_file(argv[0]);
	}

	editor_set_status("C-k q to exit / C-k d to dump core");

	display_clear(NULL);
	loop();

	return 0;
}
