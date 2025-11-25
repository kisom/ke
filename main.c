/*
 * kyle's editor
 *
 * first version is a run-through of the kilo editor walkthrough as a
 * set of guiderails. I've made a lot of changes.
 */
#include <sys/ioctl.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>
#include <wctype.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
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



/*
 * Function and struct declarations.
 */

/* append buffer */
struct abuf {
	char *b;
	int len;
	int cap;
};

#define ABUF_INIT		{NULL, 0, 0}


/* editor row */
struct erow {
	char *line;
	char *render;

	int size;
	int rsize;

	int cap;
};


/*
 * editor is the global editor state; it should be broken out
 * to buffers and screen state, probably.
 */
struct editor_t {
	struct termios entry_term;
	int rows, cols;
	int curx, cury;
	int rx;
	int mode;
	int nrows;
	int rowoffs, coloffs;
	struct erow *row;
	struct erow *killring;
	int kill; /* are we in a contiguous delete sequence? */
	/* internal flag: don't kill in delete_row */
	int no_kill;
	char *filename;
	int dirty;
	int dirtyex;
	char msg[80];
	int mark_set;
	int mark_curx, mark_cury;
	time_t msgtm;
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
};


int		 next_power_of_2(int n);
int		 cap_growth(int cap, int sz);
void		 init_editor(void);
void		 reset_editor(void);
void		 ab_append(struct abuf *buf, const char *s, int len);
void		 ab_free(struct abuf *buf);
char		 nibble_to_hex(char c);
int		 erow_render_to_cursor(struct erow *row, int cx);
int		 erow_cursor_to_render(struct erow *row, int rx);
int		 erow_init(struct erow *row, int len);
void		 erow_update(struct erow *row);
void		 erow_insert(int at, char *s, int len);
void		 erow_free(struct erow *row);

/* kill ring, marking, etc */
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
void		 goto_line(void);
int		 cursor_at_eol(void);
void		 delete_row(int at);
void		 row_append_row(struct erow *row, char *s, int len);
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
char		*editor_prompt(char *, void (*cb)(char *, int16_t));
void		 editor_openfile(void);
void		 move_cursor(int16_t c);
void		 newline(void);
void		 process_kcommand(int16_t c);
void		 process_normal(int16_t c);
void		 process_escape(int16_t c);
int		 process_keypress(void);
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
void		 process_normal(int16_t c);


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
	editor.killring = NULL;
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

	if (editor.killring != NULL) {
		erow_free(editor.killring);
		free(editor.killring);
		editor.killring = NULL;
	}

	init_editor();
}


void
ab_append(struct abuf *buf, const char *s, int len)
{
	char *nc = buf->b;
	int sz = buf->len + len;

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
	buf->len += len; /* DANGER: overflow */
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
		return (char) ('0' + c);
	}
	return (char) ('A' + (c - 10));
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
	int rx = 0;
	size_t j = 0;

	wchar_t wc;
	mbstate_t st;

	memset(&st, 0, sizeof(st));

	while (j < (size_t) cx && j < (size_t) row->size) {
		unsigned char b = (unsigned char) row->line[j];
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

		size_t rem = (size_t) row->size - j;
		size_t n = mbrtowc(&wc, &row->line[j], rem, &st);

		if (n == (size_t) -2) {
			/* incomplete sequence at end; treat one byte */
			rx += 1;
			j += 1;
			memset(&st, 0, sizeof(st));
		} else if (n == (size_t) -1) {
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
	int cur_rx = 0;
	size_t j = 0;

	wchar_t wc;
	mbstate_t st;

	memset(&st, 0, sizeof(st));

	while (j < (size_t) row->size) {
		int w = 0;
		size_t adv = 1;

		unsigned char b = (unsigned char) row->line[j];
		if (b == '\t') {
			int add = (TAB_STOP - 1) - (cur_rx % TAB_STOP);
			w = add + 1;
			adv = 1;
			/* tabs are single byte */
		} else if (b < 0x20) {
			w = 3; /* "\\xx" */
			adv = 1;
		} else {
			size_t rem = (size_t) row->size - j;
			size_t n = mbrtowc(&wc, &row->line[j], rem, &st);

			if (n == (size_t) -2 || n == (size_t) -1) {
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

	return (int) j;
}


int
erow_init(struct erow *row, int len)
{
	row->size = len;
	row->rsize = 0;
	row->render = NULL;
	row->line = NULL;
	row->cap = cap_growth(0, len)+1; /* extra byte for NUL end */

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
		} else if ((unsigned char) row->line[j] < 0x20) {
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
		} else if ((unsigned char) row->line[j] < 0x20) {
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
	struct erow row;

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
		unsigned char ch = (unsigned char) editor.killring->line[i];
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
	erow_update(row);
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
	erow_update(row);
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
	erow_update(row);
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
			move_cursor(ARROW_RIGHT);
			count++;
		}

		move_cursor(ARROW_RIGHT);
		count++;
	}

	while (editor.curx != curx) {
		count++;
		move_cursor(ARROW_RIGHT);
	}

	return count;
}


void
kill_region(void)
{
	int	curx	= editor.curx;
	int	cury	= editor.cury;
	int	markx	= editor.mark_curx;
	int	marky	= editor.mark_cury;

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
			move_cursor(ARROW_RIGHT);
		}
		killring_append_char('\n');
		move_cursor(ARROW_RIGHT);
	}

	while (editor.curx != curx) {
		killring_append_char(editor.row[editor.cury].line[editor.curx]);
		move_cursor(ARROW_RIGHT);
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


/* call after kill_region */
void
delete_region(void)
{
	int	count	= count_chars_from_cursor_to_mark();
	int	killed	= 0;
	int	curx	= editor.curx;
	int	cury	= editor.cury;
	int	markx	= editor.mark_curx;
	int	marky	= editor.mark_cury;


	if (!editor.mark_set) {
		return;
	}

	if (!cursor_after_mark()) {
		swap_int(&curx, &markx);
		swap_int(&cury, &marky);
	}

	editor.curx = markx;
	editor.cury = marky;

	while (killed < count) {
		move_cursor(ARROW_RIGHT);
		deletech(KILLRING_NO_OP);
		killed++;
	}

	editor.kill = 1;
	editor_set_status("Region killed.");
}


void
kwrite(const int fd, const char *buf, const int len)
{
	int	wlen = 0;

	wlen = write(fd, buf, len);
	assert(wlen != -1);
	assert(wlen == len);
	if (wlen == -1) {
		abort();
	}
}


void
die(const char *s)
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

	editor.cury = lineno - 1;
	editor.rowoffs = editor.cury - (editor.rows / 2);
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


void
find_next_word(void)
{
	while (cursor_at_eol()) {
		move_cursor(ARROW_RIGHT);
	}

	if (isalnum(editor.row[editor.cury].line[editor.curx])) {
		while (!isspace(editor.row[editor.cury].line[editor.curx]) && !
		       cursor_at_eol()) {
			move_cursor(ARROW_RIGHT);
		}

		return;
	}

	if (isspace(editor.row[editor.cury].line[editor.curx])) {
		while (isspace(editor.row[editor.cury].line[editor.curx])) {
			move_cursor(ARROW_RIGHT);
		}

		find_next_word();
	}
}


void
delete_next_word(void)
{
	while (cursor_at_eol()) {
		move_cursor(ARROW_RIGHT);
		deletech(KILLRING_APPEND);
	}

	if (isalnum(editor.row[editor.cury].line[editor.curx])) {
		while (!isspace(editor.row[editor.cury].line[editor.curx]) && !
		       cursor_at_eol()) {
			move_cursor(ARROW_RIGHT);
			deletech(KILLRING_APPEND);
		}

		return;
	}

	if (isspace(editor.row[editor.cury].line[editor.curx])) {
		while (isspace(editor.row[editor.cury].line[editor.curx])) {
			move_cursor(ARROW_RIGHT);
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

	move_cursor(ARROW_LEFT);

	while (cursor_at_eol() || isspace(
		       editor.row[editor.cury].line[editor.curx])) {
		if (editor.cury == 0 && editor.curx == 0) {
			return;
		}

		move_cursor(ARROW_LEFT);
	}

	while (editor.curx > 0 && !isspace(
		       editor.row[editor.cury].line[editor.curx - 1])) {
		move_cursor(ARROW_LEFT);
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
					(unsigned char) r->line[0]);
				for (int i = 1; i < r->size; i++) {
					killring_append_char(
						(unsigned char) r->line[i]);
				}
			} else {
				for (int i = 0; i < r->size; i++) {
					killring_append_char(
						(unsigned char) r->line[i]);
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
	erow_update(row);
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

	erow_update(row);
}


void
row_delete_ch(struct erow *row, int at)
{
	if (at < 0 || at >= row->size) {
		return;
	}

	memmove(&row->line[at], &row->line[at + 1], row->size - at);
	row->size--;
	erow_update(row);
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

	/* Any insertion breaks a delete sequence for killring chaining. */
	editor.kill = 0;
	/* Ensure we pass a non-negative byte value to avoid assert(c > 0). */
	row_insert_ch(&editor.row[editor.cury],
	              editor.curx,
	              (int16_t) (c & 0xff));
	editor.curx++;
	editor.dirty++;
}


void
deletech(uint8_t op)
{
	struct erow *row = NULL;
	unsigned char dch = 0;

	if (editor.cury >= editor.nrows) {
		return;
	}

	if (editor.cury == 0 && editor.curx == 0) {
		return;
	}

	row = &editor.row[editor.cury];

	/* determine which character is being deleted for killring purposes */
	if (editor.curx > 0) {
		dch = (unsigned char) row->line[editor.curx - 1];
	} else {
		dch = '\n';
	}

	/* update buffer */
	if (editor.curx > 0) {
		row_delete_ch(row, editor.curx - 1);
		editor.curx--;
	} else {
		editor.curx = editor.row[editor.cury - 1].size;
		row_append_row(&editor.row[editor.cury - 1],
		               row->line,
		               row->size);
		int prev = editor.no_kill;
		editor.no_kill = 1;
		/* prevent killring update on internal row delete */
		delete_row(editor.cury);
		editor.no_kill = prev;
		editor.cury--;
	}

	/* update killring if requested */
	if (op == KILLRING_FLUSH) {
		killring_flush();
		editor.kill = 0;
		return;
	}

	if (op == KILLRING_NO_OP) {
		/* Do not modify killring or chaining state. */
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
	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	FILE *fp = NULL;

	reset_editor();

	editor.filename = strdup(filename);
	assert(editor.filename != NULL);

	editor.dirty = 0;
	if ((fp = fopen(filename, "r")) == NULL) {
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
char *
rows_to_buffer(int *buflen)
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

	if ((ssize_t) len != write(fd, buf, len)) {
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

	c = (int16_t) uc;

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


char *
editor_prompt(char *prompt, void (*cb)(char *, int16_t))
{
	size_t bufsz = 128;
	char *buf = malloc(bufsz);
	size_t buflen = 0;
	int16_t c;

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

			buf[buflen++] = (char) (c & 0xff);
			buf[buflen] = '\0';
		}

		if (cb) {
			cb(buf, c);
		}
	}

	free(buf);
}


void
editor_find_callback(char *query, int16_t c)
{
	static int lmatch = -1;
	static int dir = 1;
	int i, current;
	char *match;
	struct erow *row;

	if (c == '\r' || c == ESC_KEY) {
		/* reset search */
		lmatch = -1;
		dir = 1;
		return;
	} else if (c == ARROW_RIGHT || c == ARROW_DOWN || c == CTRL_KEY('s')) {
		dir = 1;
	} else if (c == ARROW_LEFT || c == ARROW_UP) {
		dir = -1;
	} else {
		lmatch = -1;
		dir = 1;
	}

	if (lmatch == -1) {
		dir = 1;
	}
	current = lmatch;

	for (i = 0; i < editor.nrows; i++) {
		current += dir;
		if (current == -1) {
			current = editor.nrows - 1;
		} else if (current == editor.nrows) {
			current = 0;
		}

		row = &editor.row[current];
		match = strstr(row->render, query);
		if (match) {
			lmatch = current;
			editor.cury = current;
			editor.curx = erow_render_to_cursor(row,
				match - row->render);
			if (editor.curx > row->size) {
				editor.curx = row->size;
			}
			/*
			 * after this, scroll will put the match line at
			 * the top of the screen.
			 */
			editor.rowoffs = editor.nrows;
			break;
		}
	}

	display_refresh();
}


void
editor_find(void)
{
	char *query;
	int scx = editor.curx;
	int scy = editor.cury;
	int sco = editor.coloffs;
	int sro = editor.rowoffs;

	query = editor_prompt("Search (ESC to cancel): %s",
	                      editor_find_callback);
	if (query) {
		free(query);
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
}


int
first_nonwhitespace(struct erow *row)
{
	int pos;
	wchar_t wc;
	mbstate_t state;
	size_t len;

	if (row == NULL) {
		return 0;
	}

	memset(&state, 0, sizeof(state));
	pos = 0;
	while (pos < row->size) {
		len = mbrtowc(&wc, &row->line[pos], row->size - pos, &state);
		if (len == (size_t) -1 || len == (size_t) -2) {
			/* Invalid or incomplete sequence, stop here */
			break;
		}
		if (len == 0) {
			/* Null character, stop here */
			break;
		}
		if (!iswspace(wc)) {
			/* Found non-whitespace character */
			break;
		}
		pos += len;
	}

	return pos;
}


void
move_cursor(int16_t c)
{
	struct erow *row;
	int reps;

	row = (editor.cury >= editor.nrows) ? NULL : &editor.row[editor.cury];

	switch (c) {
	case ARROW_UP:
	case CTRL_KEY('p'):
		if (editor.cury > 0) {
			editor.cury--;
			row = (editor.cury >= editor.nrows)
				      ? NULL
				      : &editor.row[editor.cury];
			editor.curx = first_nonwhitespace(row);
		}
		break;
	case ARROW_DOWN:
	case CTRL_KEY('n'):
		if (editor.cury < editor.nrows) {
			editor.cury++;
			row = (editor.cury >= editor.nrows)
				      ? NULL
				      : &editor.row[editor.cury];
			editor.curx = first_nonwhitespace(row);
		}
		break;
	case ARROW_RIGHT:
	case CTRL_KEY('f'):
		if (row && editor.curx < row->size) {
			editor.curx++;
			/* skip over UTF-8 continuation bytes */
			while (row && editor.curx < row->size &&
			       ((unsigned char) row->line[editor.curx] &
			        0xC0) == 0x80) {
				editor.curx++;
			}
		} else if (row && editor.curx == row->size) {
			editor.cury++;
			row = (editor.cury >= editor.nrows)
				      ? NULL
				      : &editor.row[editor.cury];
			editor.curx = first_nonwhitespace(row);
		}
		break;
	case ARROW_LEFT:
	case CTRL_KEY('b'):
		if (editor.curx > 0) {
			editor.curx--;
			/* move to the start byte if we landed on a continuation */
			while (editor.curx > 0 &&
			       ((unsigned char) row->line[editor.curx] &
			        0xC0) == 0x80) {
				editor.curx--;
			}
		} else if (editor.cury > 0) {
			editor.cury--;
			editor.curx = editor.row[editor.cury].size;
			/* ensure at a codepoint boundary at end of previous line */
			row = &editor.row[editor.cury];
			while (editor.curx > 0 &&
			       ((unsigned char) row->line[editor.curx] &
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
			move_cursor(c == PG_UP ? ARROW_UP : ARROW_DOWN);
		}

		break;

	case HOME_KEY:
	case CTRL_KEY('a'):
		editor.curx = 0;
		break;
	case END_KEY:
	case CTRL_KEY('e'):
		if (editor.nrows == 0) {
			break;
		}
		editor.curx = editor.row[editor.cury].size;
		break;
	default:
		break;
	}


	row = (editor.cury >= editor.nrows) ? NULL : &editor.row[editor.cury];
	reps = row ? row->size : 0;
	if (editor.curx > reps) {
		editor.curx = reps;
	}
}


void
newline(void)
{
	struct erow *row = NULL;

	if (editor.cury >= editor.nrows) {
		/* At or past end of file, insert empty line */
		erow_insert(editor.cury, "", 0);
	} else if (editor.curx == 0) {
		erow_insert(editor.cury, "", 0);
	} else {
		row = &editor.row[editor.cury];
		erow_insert(editor.cury + 1,
		            &row->line[editor.curx],
		            row->size - editor.curx);
		row = &editor.row[editor.cury];
		row->size = editor.curx;
		row->line[row->size] = '\0';
		erow_update(row);
	}

	editor.cury++;
	editor.curx = 0;
	/* Any insertion breaks a delete sequence for killring chaining. */
	editor.kill = 0;
}


char *
get_cloc_code_lines(const char* filename)
{
	char		 command[512];
	char		 buffer[256];
	char		*result = NULL;
	FILE		*pipe = NULL;
	size_t		 len = 0;

	if (editor.filename == NULL) {
		snprintf(command, sizeof(command),
			"buffer has no associated file.");
		result = malloc((strnlen(command, sizeof(command))) + 1);
		assert(result != NULL);
		strcpy(result, command);
		return result;
	}

	if (editor.dirty) {
		snprintf(command, sizeof(command),
			"buffer must be saved first.");
		result = malloc((strnlen(command, sizeof(command))) + 1);
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
		result = (char *)malloc(sizeof(buffer) + 1);
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


void
process_kcommand(int16_t c)
{
	char	*buf = NULL;

	switch (c) {
		case ' ':
			toggle_markset();
			break;
		case 'q':
		case CTRL_KEY('q'):
			if (editor.dirty && editor.dirtyex) {
				editor_set_status(
					"File not saved - C-k q again to quit.");
				editor.dirtyex = 0;
				return;
			}
			exit(0);
		case CTRL_KEY('s'):
		case 's':
			save_file();
			break;
		case CTRL_KEY('x'):
		case 'x':
			exit(save_file());
		case DEL_KEY:
		case CTRL_KEY('d'):
			delete_row(editor.cury);
			break;
		case 'd':
			if (editor.curx == 0 && cursor_at_eol()) {
				delete_row(editor.cury);
				return;
			}

			while ((editor.row[editor.cury].size - editor.curx) >
			       0) {
				process_normal(DEL_KEY);
			}

			break;
		case 'g':
		case CTRL_KEY('g'):
			goto_line();
			break;
		case BACKSPACE:
			while (editor.curx > 0) {
				process_normal(BACKSPACE);
			}
			break;
		case CTRL_KEY('\\'):
			/* sometimes it's nice to dump core */
			disable_termraw();
			abort();
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
		case 'u':
			editor_set_status("undo: todo");
			break;
		case 'y':
			killring_yank();
			break;
		default:
			editor_set_status("unknown kcommand: %04x", c);
			return;
	}

	editor.dirtyex = 1;
	return;
}


void
process_normal(int16_t c)
{
	if (is_arrow_key(c)) {
		/* moving the cursor breaks a delete sequence */
		editor.kill = 0;
		move_cursor(c);
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
				move_cursor(ARROW_RIGHT);
				deletech(KILLRING_APPEND);
			} else {
				deletech(KILLRING_PREPEND);
			}
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
			killring_yank();
			break;
		case ESC_KEY:
			editor.mode = MODE_ESCAPE;
			break;
 	default:
 		if (c == TAB_KEY) {
 			if (editor.mark_set) {
 				indent_region();
 			} else {
 				insertch(c);
 			}
 		} else if (c >= 0x20 && c != 0x7f) {
 			insertch(c);
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
	atexit(disable_termraw);
	enable_termraw();
}


void
draw_rows(struct abuf *ab)
{
	assert(editor.cols >= 0);

	char buf[editor.cols];
	int buflen, filerow, padding;
	int y;

	for (y = 0; y < editor.rows; y++) {
		filerow = y + editor.rowoffs;
		if (filerow >= editor.nrows) {
			if ((editor.nrows == 0) && (y == editor.rows / 3)) {
				buflen = snprintf(buf,
				                  sizeof(buf),
				                  "%s",
				                  KE_VERSION);
				padding = (editor.rows - buflen) / 2;

				if (padding) {
					ab_append(ab, "|", 1);
					padding--;
				}

				while (padding--)
					ab_append(ab, " ", 1);
				ab_append(ab, buf, buflen);
			} else {
				ab_append(ab, "|", 1);
			}
		} else {
			erow_update(&editor.row[filerow]);
			buflen = editor.row[filerow].rsize - editor.coloffs;
			if (buflen < 0) {
				buflen = 0;
			}

			if (buflen > editor.cols) {
				buflen = editor.cols;
			}
			ab_append(ab,
			          editor.row[filerow].render + editor.coloffs,
			          buflen);
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
	editor.rx = 0;
	if (editor.cury < editor.nrows) {
		editor.rx = erow_render_to_cursor(
			&editor.row[editor.cury],
			editor.curx);
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
	display_clear(&ab);

	draw_rows(&ab);
	draw_status_bar(&ab);
	draw_message_line(&ab);

	snprintf(buf,
	         sizeof(buf),
	         ESCSEQ "%d;%dH",
	         (editor.cury - editor.rowoffs) + 1,
	         (editor.rx - editor.coloffs) + 1);
	ab_append(&ab, buf, strnlen(buf, 32));
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
			while (process_keypress()) ;
		}
	}
}


int
main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	setup_terminal();
	init_editor();

	if (argc > 1) {
		open_file(argv[1]);
	}

	editor_set_status("C-k q to exit / C-k d to dump core");

	display_clear(NULL);
	loop();

	return 0;
}
