#ifndef BUFFER_H
#define BUFFER_H

#include "abuf.h"


typedef struct buffer {
	int	 curx, cury;
	int	 rx;
	int	 nrows;
	int	 rowoffs, coloffs;
	abuf	*row;
	char	*filename;
	int	 dirty;
	int	 mark_set;
	int	 mark_curx, mark_cury;
} buffer;


void		 buffers_init(void);
int		 buffer_add_empty(void);
void		 buffer_save_current(void);
void		 buffer_switch(int idx);
void		 buffer_next(void);
void		 buffer_prev(void);
void		 buffer_switch_by_name(void);
void		 buffer_close_current(void);
const char	*buffer_name(buffer *b);


#endif
