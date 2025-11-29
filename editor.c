/* editor.c - editor-wide state and functions */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "abuf.h"
#include "buffer.h"
#include "core.h"
#include "editor.h"
#include "term.h"

/*
 * Global editor instance
 */
struct editor editor = {
	.cols = 0,
	.rows     = 0,
	.mode     = 0,
	.killring = NULL,
	.kill     = 0,
	.no_kill  = 0,
	.dirtyex  = 0,
	.uarg     = 0,
	.ucount   = 0,
	.msgtm    = 0,
	.buffers  = NULL,
	.bufcount = 0,
	.curbuf   = 0,
	.bufcap   = 0,
};


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
init_editor(void)
{
	editor.cols = 0;
	editor.rows = 0;

	if (get_winsz(&editor.rows, &editor.cols) == -1) {
		die("can't get window size");
	}
	editor.rows--; /* status bar */
	editor.rows--; /* message line */

	/* don't clear out the kill ring:
	 * killing / yanking across files is helpful, and killring
	 * is initialized to NULL at program start.
	 */
	editor.kill = 0;
	editor.no_kill = 0;

	editor.msg[0] = '\0';
	editor.msgtm = 0;

	/* initialize buffer system on first init */
	if (editor.buffers == NULL && editor.bufcount == 0) {
		editor.bufcap = 0;
		buffers_init();
	}
}


void
reset_editor(void)
{
	buffer *b = buffer_current();
	if (b == NULL) {
		return;
	}

	if (b->row) {
		for (size_t i = 0; i < b->nrows; i++) {
			ab_free(&b->row[i]);
		}
		free(b->row);
	}

	b->row = NULL;
	b->nrows = 0;
	b->rowoffs = 0;
	b->coloffs = 0;
	b->rx = 0;
	b->curx = 0;
	b->cury = 0;
	if (b->filename) {
		free(b->filename);
		b->filename = NULL;
	}
	b->dirty = 0;
	b->mark_set = 0;
	b->mark_curx = 0;
	b->mark_cury = 0;
}

