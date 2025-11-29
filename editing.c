#include <sys/fcntl.h>
#include <sys/syslimits.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "buffer.h"
#include "core.h"
#include "editor.h"
#include "editing.h"
#include "killring.h"
#include "term.h"


/*
 * TODO(kyle): not proud of this, but it does work. It needs to be
 * cleaned up and the number of buffers consolidated.
 */
void
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
		return NULL;
	}

	if (fgets(outbuf, sizeof(outbuf), pipe) != NULL) {
		len = strlen(outbuf);
		if (len > 0 && outbuf[len - 1] == '\n') {
			outbuf[len - 1] = '\0';
		}

		result = malloc(strlen(outbuf) + 1);
		assert(result != NULL);
		strcpy(result, outbuf);
		pclose(pipe);
		return result;
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
