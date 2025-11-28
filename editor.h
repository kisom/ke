#ifndef KE_EDITOR_H
#define KE_EDITOR_H

#include <termios.h>
#include <time.h>


#include "abuf.h"
#include "buffer.h"


struct editor {
	size_t		  rows, cols;
	int		  mode;
	abuf		 *killring;
	int		  kill;			/* KILL CHAIN (\m/) */
	int		  no_kill;		/* don't kill in delete_row */
	int		  dirtyex;
	char		  msg[80];
	int		  uarg, ucount;		/* C-u support */
	time_t		  msgtm;
	struct buffer	**buffers;  /* array of buffers */
	size_t		  bufcount; /* number of buffers */
	size_t		  curbuf;   /* current buffer index */
	size_t		  bufcap;   /* current buffer capacity */
};


extern struct editor	 editor;
void			 editor_set_status(const char *fmt, ...);
void			 init_editor(void);
void			 reset_editor(void);


#endif /* KE_EDITOR_H */
