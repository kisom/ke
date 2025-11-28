#ifndef EDITOR_H
#define EDITOR_H

#include <termios.h>
#include <time.h>


#include "abuf.h"
#include "buffer.h"


/* TODO(kyle): remove the "per-buffer" fields completely from the editor. */

struct editor {
	int		  rows, cols;
	int		  curx, cury;		/* per-buffer */
	int		  rx;			/* per-buffer */
	int		  mode;
	int		  nrows;		/* per-buffer */
	int		  rowoffs, coloffs;	/* per-buffer */
	abuf		 *row;			/* per-buffer */
	abuf		 *killring;
	int		  kill;			/* KILL CHAIN (\m/) */
	int		  no_kill;		/* don't kill in delete_row */
	char		 *filename;		/* per-buffer */
	int		  dirty;		/* per-buffer */
	int		  dirtyex;
	char		  msg[80];
	int		  mark_set;		/* per-buffer */
	int		  mark_curx, mark_cury;	/* per-buffer */
	int		  uarg, ucount;		/* C-u support */
	time_t		  msgtm;

	/* Multi-buffer support */
	struct buffer	**buffers; /* array of buffers */
	int		  bufcount; /* number of buffers */
	int		  curbuf; /* current buffer index */
};


extern struct editor	 editor;
void			 editor_set_status(const char *fmt, ...);
void			 init_editor(void);
void			 reset_editor(void);


#endif /* EDITOR_H */
