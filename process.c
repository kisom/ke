#include <errno.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <_string.h>

#include "editing.h"
#include "buffer.h"
#include "core.h"
#include "editor.h"
#include "killring.h"
#include "term.h"
#include "process.h"


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
