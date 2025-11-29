#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "abuf.h"
#include "core.h"
#include "editing.h"
#include "editor.h"
#include "killring.h"


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
