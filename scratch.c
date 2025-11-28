/*
 * scratch.c - ideas in progress
 */

#include "buffer.h"
#include "abuf.h"
#include <ctype.h>
#include <string.h>

#define REFLOW_MARGIN		72

void
reflow_region(void)
{
 int		 start_row, end_row, i, col, wlen, this_len;
 abuf		*row;
 struct abuf	 buf = ABUF_INIT;
 struct abuf	 out = ABUF_INIT;
	int		 in_paragraph = 0;
	int		 indent_len = 0;
	char		 indent[REFLOW_MARGIN + 1];
	char		 word[REFLOW_MARGIN + 1];
	char		*e = NULL;
	char		*p = NULL;
	char		*s = NULL;

 if (EMARK_SET) {
        if (EMARK_CURY < ECURY ||
            (EMARK_CURY == ECURY &&
                EMARK_CURX < ECURX)) {
            start_row = EMARK_CURY;
            end_row = ECURY;
        } else {
            start_row = ECURY;
            end_row = EMARK_CURY;
        }
    } else {
        start_row = end_row = ECURY;
        while (start_row > 0 && EROW[start_row - 1].size > 0) {
            start_row--;
        }

        while (end_row < ENROWS - 1 &&
            EROW[end_row + 1].size > 0) {
            end_row++;
        }
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
			if (in_paragraph) {
				ab_append(&buf, "\n", 1);
				in_paragraph = 0;
			}

			ab_append(&buf, "\n", 1);
			continue;
		}

        if (!in_paragraph) {
            indent_len = 0;
            while (indent_len < (int)row->size &&
                (row->b[indent_len] == ' ' ||
                    row->b[indent_len] == '\t')) {
                indent[indent_len] = row->b[indent_len], indent_len++;
            }

            indent[indent_len] = '\0';
            in_paragraph = 1;
        }

        ab_append(&buf, row->b + indent_len, row->size - indent_len);
        ab_append(&buf, " ", 1);
    }

	if (in_paragraph) {
		ab_append(&buf, "\n", 1);
	}


	p = buf.b;
	col = 0;

	while (p != NULL && *p != '\0') {
		while (*p && isspace((unsigned char)*p) &&
			*p != '\n') {
			p++;
		}

		if (*p == '\0') {
			break;
		}

		wlen = 0;
		while (*p && !isspace((unsigned char)*p)) {
			if (wlen < REFLOW_MARGIN)
				word[wlen++] = *p;
			p++;
		}
		word[wlen] = '\0';

		if (*p == '\n' && (p[1] == '\n' || p[1] == '\0')) {
			ab_append(&out, "\n", 1); /* flush */
			col = 0;
			p++; /* consume the extra \n */
			continue;
		}

		this_len = wlen;
		if (col > 0) {
			this_len++; /* space before word */
		}

		if (col == 0) {
			ab_append(&out, indent, indent_len);
			col = indent_len;
		}

		if (col + this_len > REFLOW_MARGIN && col > 0) {
			ab_append(&out, "\n", 1);
			ab_append(&out, indent, indent_len);
			col = indent_len;
		}

		if (col > 0) {
			ab_append(&out, " ", 1);
			col++;
		}

		ab_append(&out, word, wlen);
		col += wlen;
	}

	if (col > 0) {
		ab_append(&out, "\n", 1);
	}

	/* the old switcharoo */
	buf = out;
	ab_free(&out);


 for (i = end_row; i >= start_row; i--) {
        delete_row(i);
    }

 s = buf.b;
 while ((e = strchr(s, '\n'))) {
     erow_insert(start_row++, s, e - s);
     s = e + 1;
 }

	ab_free(&buf);

    EDIRTY++;
    editor_set_status("Region reflowed to %d columns", REFLOW_MARGIN);
}


static inline
void clamp_curx_to_row(void)
{
	abuf	*row  = NULL;
	int	 maxx = 0;

	if (ECURY >= ENROWS) {
		return;
	}

	row = &EROW[ECURY];
	if (ECURX < 0) {
		ECURX = 0;
	}

	maxx = (int) row->size;
	if (ECURX > maxx) {
		ECURX = maxx;
	}
}

static inline
void set_cursor(int col, int row)
{
	if (row < 0) {
		row = 0;
	}

	if (row > ENROWS) {
		row = ENROWS;
	}
	
	ECURY = row;
	ECURX = col;
	
	clamp_curx_to_row();
}
