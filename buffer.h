#ifndef KE_BUFFER_H
#define KE_BUFFER_H

#include "abuf.h"
#include "undo.h"


typedef struct buffer {
	size_t		 curx, cury;
	size_t		 rx;
	size_t		 nrows;
	size_t		 rowoffs, coloffs;
	abuf		*row;
	char		*filename;
	int		 dirty;
	int		 mark_set;
	size_t		 mark_curx, mark_cury;
	undo_tree	 undo;
} buffer;


buffer				*buffer_current(void);
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
int		 buffer_is_unnamed_and_empty(const buffer *b);


#endif
