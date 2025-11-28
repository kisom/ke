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
	.rows = 0,
	.curx = 0,
	.cury = 0,
	.rx = 0,
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
	.msgtm = 0,
	.buffers = NULL,
	.bufcount = 0,
	.curbuf = -1,
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

	editor.curx = editor.cury = 0;
	editor.rx   = 0;

	editor.nrows   = 0;
	editor.rowoffs = editor.coloffs = 0;
	editor.row     = NULL;

	/* don't clear out the kill ring:
	 * killing / yanking across files is helpful, and killring
	 * is initialized to NULL at program start.
	 */
	editor.kill    = 0;
	editor.no_kill = 0;

	editor.msg[0] = '\0';
	editor.msgtm  = 0;

	editor.dirty     = 0;
	editor.mark_set  = 0;
	editor.mark_cury = editor.mark_curx = 0;

	/* initialize buffer system on first init */
	if (editor.buffers == NULL && editor.bufcount == 0) {
		buffers_init();
	}
}


void
reset_editor(void)
{
	/* Clear current working set. Notably, does not reset terminal
	 * or buffers list. */
	for (int i = 0; i < editor.nrows; i++) {
		ab_free(&editor.row[i]);
	}
	free(editor.row);

	editor.row     = NULL;
	editor.nrows   = 0;
	editor.rowoffs = editor.coloffs = 0;
	editor.curx    = editor.cury    = 0;
	editor.rx      = 0;

	if (editor.filename != NULL) {
		free(editor.filename);
		editor.filename = NULL;
	}

	editor.dirty     = 0;
	editor.mark_set  = 0;
	editor.mark_cury = editor.mark_curx = 0;
}
