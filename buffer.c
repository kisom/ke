/* buffer.c - multiple file buffers */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "abuf.h"
#include "buffer.h"
#include "core.h"
#include "editor.h"


#define		NO_NAME		 "[No Name]"


/* externs from other modules */
char *editor_prompt(const char *, void (*cb)(char *, int16_t));


static const char *
buf_basename(const char *path)
{
	if (path == NULL) {
		return NULL;
	}

	const char *slash = strrchr(path, '/');
	return (slash != NULL) ? (slash + 1) : path;
}


static int
buffer_find_exact_by_name(const char *name)
{
	buffer	*b = NULL;

	if (name == NULL) {
		return -1;
	}

	for (int i = 0; i < editor.bufcount; i++) {
		b                = editor.buffers[i];
		const char *full = b->filename;
		const char *base = buf_basename(full);

		if (full && strcmp(full, name) == 0) {
			return i;
		}

		if (base && strcmp(base, name) == 0) {
			return i;
		}

		if (full == NULL && strcmp(name, NO_NAME) == 0) {
			return i;
		}
	}

	return -1;
}


static int
buffer_collect_prefix_matches(const char *prefix, int *out_idx, const int max_out)
{
	buffer	*b       = NULL;
	int	 count   = 0;
	int	 matched = 0;
	size_t	 plen    = (prefix ? strlen(prefix) : 0);

	for (int i = 0; i < editor.bufcount; i++) {
		b = editor.buffers[i];

		const char *cand1 = b->filename;
		const char *cand2 = buf_basename(cand1);

		if (plen == 0) {
			matched = 1; /* everything matches empty prefix */
		} else {
			if (cand2 && strncmp(cand2, prefix, plen) == 0) {
				matched = 1;
			} else if (cand1 && strncmp(cand1, prefix, plen) == 0) {
				matched = 1;
			} else if (!cand1 && strncmp(NO_NAME, prefix, plen) == 0) {
				matched = 1;
			}
		}

		if (matched) {
			if (count < max_out) {
				out_idx[count] = i;
			}
			count++;
		}
	}

	return count;
}


static void
longest_common_prefix(char *buf, const size_t bufsz, const int *idxs, const int n)
{
	const char *first = NULL;
	const char *cand  = NULL;
	int k             = 0;
	size_t j          = 0;
	size_t cur        = 0;
	size_t lcp        = 0;
	size_t to_copy    = 0;

	if (n <= 0) {
		return;
	}

	first = buf_basename(editor.buffers[idxs[0]]->filename);
	if (first == NULL) {
		first = NO_NAME;
	}

	lcp = strnlen(first, FILENAME_MAX);
	for (k = 1; k < n; k++) {
		cand = buf_basename(editor.buffers[idxs[k]]->filename);
		if (cand == NULL) {
			cand = NO_NAME;
		}

		j = 0;
		while (j < lcp && first[j] == cand[j]) {
			j++;
		}

		lcp = j;
		if (lcp == 0) {
			break;
		}
	}

	cur = strlen(buf);
	if (lcp > cur) {
		to_copy = lcp - cur;
		if (cur + to_copy >= bufsz) {
			to_copy = bufsz - cur - 1;
		}

		strncat(buf, first + cur, to_copy);
	}
}


static void
buffer_switch_prompt_cb(char *buf, const int16_t key)
{
	char msg[80]     = {0};
	const char *name = NULL;
	const char *nm   = NULL;
	int idxs[64]     = {0};
	int n            = 0;
	size_t need      = 0;
	size_t used      = 0;

	if (key != TAB_KEY) {
		return;
	}

	n = buffer_collect_prefix_matches(buf, idxs, 64);
	if (n <= 0) {
		editor_set_status("No matches");
		return;
	}

	if (n == 1) {
		name = buf_basename(editor.buffers[idxs[0]]->filename);
		if (name == NULL) {
			name = NO_NAME;
		}

		need = strlen(name);
		if (need < 128) {
			memcpy(buf, name, need);
			buf[need] = '\0';
		}

		editor_set_status("Unique match: %s", name);
		return;
	}

	longest_common_prefix(buf, 128, idxs, n);
	msg[0] = '\0';
	used   = 0;
	used += snprintf(msg + used, sizeof(msg) - used, "%d matches: ", n);
	for (int i = 0; i < n && used < sizeof(msg) - 1; i++) {
		nm = buf_basename(editor.buffers[idxs[i]]->filename);
		if (nm == NULL) {
			nm = NO_NAME;
		}

		used += snprintf(msg + used, sizeof(msg) - used, "%s%s",
		                 nm, (i == n - 1 ? "" : ", "));
	}

	editor_set_status("%s", msg);
}


static void
buffer_bind_to_editor(const buffer *b)
{
	if (b == NULL) {
		return;
	}

	editor.curx      = b->curx;
	editor.cury      = b->cury;
	editor.rx        = b->rx;
	editor.nrows     = b->nrows;
	editor.rowoffs   = b->rowoffs;
	editor.coloffs   = b->coloffs;
	editor.row       = b->row;
	editor.filename  = b->filename;
	editor.dirty     = b->dirty;
	editor.mark_set  = b->mark_set;
	editor.mark_curx = b->mark_curx;
	editor.mark_cury = b->mark_cury;
}


static void
buffer_extract_from_editor(buffer *b)
{
	if (b == NULL) {
		return;
	}

	b->curx      = editor.curx;
	b->cury      = editor.cury;
	b->rx        = editor.rx;
	b->nrows     = editor.nrows;
	b->rowoffs   = editor.rowoffs;
	b->coloffs   = editor.coloffs;
	b->row       = editor.row;
	b->filename  = editor.filename;
	b->dirty     = editor.dirty;
	b->mark_set  = editor.mark_set;
	b->mark_curx = editor.mark_curx;
	b->mark_cury = editor.mark_cury;
}

const char *
buffer_name(buffer *b)
{
	if (b && b->filename) {
		return buf_basename(b->filename);
	}

	return NO_NAME;
}


void
buffers_init(void)
{
	int	 idx = 0;

	editor.buffers  = NULL;
	editor.bufcount = 0;
	editor.curbuf   = -1;

	idx = buffer_add_empty();
	buffer_switch(idx);
}


int
buffer_add_empty(void)
{
	buffer	 *buf    = NULL;
	buffer **newlist = realloc(editor.buffers, sizeof(buffer *) * (editor.bufcount + 1));
	int	 idx     = 0;

	assert(newlist != NULL);
	editor.buffers = newlist;

	buf = calloc(1, sizeof(buffer));
	assert(buf != NULL);

	buf->curx      = 0;
	buf->cury      = 0;
	buf->rx        = 0;
	buf->nrows     = 0;
	buf->rowoffs   = 0;
	buf->coloffs   = 0;
	buf->row       = NULL;
	buf->filename  = NULL;
	buf->dirty     = 0;
	buf->mark_set  = 0;
	buf->mark_curx = 0;
	buf->mark_cury = 0;

	editor.buffers[editor.bufcount] = buf;
	idx                             = editor.bufcount;
	editor.bufcount++;
	return idx;
}


void
buffer_save_current(void)
{
	buffer	*b = NULL;

	if (editor.curbuf < 0 || editor.curbuf >= editor.bufcount) {
		return;
	}

	b = editor.buffers[editor.curbuf];
	buffer_extract_from_editor(b);
}


buffer *
buffer_current(void)
{
	if (editor.curbuf < 0 || editor.curbuf >= editor.bufcount) {
		return NULL;
	}
	return editor.buffers[editor.curbuf];
}


int
buffer_is_unnamed_and_empty(const buffer *b)
{
	if (b == NULL) {
		return 0;
	}

	if (b->filename != NULL) {
		return 0;
	}

	if (b->dirty) {
		return 0;
	}

	if (b->nrows != 0) {
		return 0;
	}

	if (b->row != NULL) {
		return 0;
	}

	return 1;
}


void
buffer_switch(const int idx)
{
	buffer	*b = NULL;

	if (idx < 0 || idx >= editor.bufcount) {
		return;
	}

	if (editor.curbuf == idx) {
		return;
	}

	if (editor.curbuf >= 0) {
		buffer_save_current();
	}

	b = editor.buffers[idx];
	buffer_bind_to_editor(b);
	editor.curbuf  = idx;
	editor.dirtyex = 1;
	editor_set_status("Switched to buffer %d: %s", editor.curbuf, buffer_name(b));
}


void
buffer_next(void)
{
	int idx = 0;

	if (editor.bufcount <= 1) {
		return;
	}

	idx = (editor.curbuf + 1) % editor.bufcount;
	buffer_switch(idx);
}


void
buffer_prev(void)
{
	int	 idx = 0;

	if (editor.bufcount <= 1) {
		return;
	}

	idx = (editor.curbuf - 1 + editor.bufcount) % editor.bufcount;
	buffer_switch(idx);
}


void
buffer_close_current(void)
{
	buffer	*b   = NULL;
	int	 closing = 0;
	int	 target  = 0;
	int	 nb      = 0;

	if (editor.curbuf < 0 || editor.curbuf >= editor.bufcount) {
		editor_set_status("No buffer to close.");
		return;
	}

	closing = editor.curbuf;

	target = -1;
	if (editor.bufcount > 1) {
		target = (closing - 1 >= 0) ? (closing - 1) : (closing + 1);
		buffer_switch(target);
	} else {
		nb = buffer_add_empty();
		buffer_switch(nb);
	}

	b = editor.buffers[closing];
	if (b) {
		if (b->row) {
			for (int i = 0; i < b->nrows; i++) {
				ab_free(&b->row[i]);
			}
			free(b->row);
		}

		if (b->filename) {
			free(b->filename);
		}
		free(b);
	}

	memmove(&editor.buffers[closing], &editor.buffers[closing + 1],
	        sizeof(buffer *) * (editor.bufcount - closing - 1));

	editor.bufcount--;
	if (editor.bufcount == 0) {
		editor.curbuf = -1;
	} else {
		if (editor.curbuf > closing) {
			editor.curbuf--;
		}
	}

	editor.dirtyex = 1;
	editor_set_status("Closed buffer. Now on %s",
	                  buffer_name(editor.buffers[editor.curbuf]));
}


void
buffer_switch_by_name(void)
{
	int	 idxs[64] = {0};
	char	*name   = NULL;
	int	 idx      = 0;
	int	 n        = 0;

	if (editor.bufcount <= 1) {
		editor_set_status("No other buffers.");
		return;
	}

	name = editor_prompt("Switch buffer (name, TAB to complete): %s", buffer_switch_prompt_cb);
	if (name == NULL) {
		return;
	}

	idx = buffer_find_exact_by_name(name);
	if (idx < 0) {
		n = buffer_collect_prefix_matches(name, idxs, 64);
		if (n == 1) {
			idx = idxs[0];
		}
	}

	if (idx >= 0) {
		buffer_switch(idx);
	} else {
		editor_set_status("No such buffer: %s", name);
	}

	free(name);
}
