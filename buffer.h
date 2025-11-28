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

/* Access current buffer and convenient aliases for file-specific fields */
buffer		*buffer_current(void);

#define CURBUF			(buffer_current())
#define EROW			(CURBUF->row)
#define ENROWS			(CURBUF->nrows)
#define ECURX			(CURBUF->curx)
#define ECURY			(CURBUF->cury)
#define ERX			(CURBUF->rx)
#define EROWOFFS		(CURBUF->rowoffs)
#define ECOLOFFS		(CURBUF->coloffs)
#define EFILENAME		(CURBUF->filename)
#define EDIRTY			(CURBUF->dirty)
#define EMARK_SET		(CURBUF->mark_set)
#define EMARK_CURX		(CURBUF->mark_curx)
#define EMARK_CURY		(CURBUF->mark_cury)


void		 buffers_init(void);
int		 buffer_add_empty(void);
void		 buffer_save_current(void);
void		 buffer_switch(int idx);
void		 buffer_next(void);
void		 buffer_prev(void);
void		 buffer_switch_by_name(void);
void		 buffer_close_current(void);
const char	*buffer_name(buffer *b);
/* Helpers */
int		 buffer_is_unnamed_and_empty(const buffer *b);


#endif
