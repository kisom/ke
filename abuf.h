/*
 * abuf.h - append/prepend buffer utilities
 */
#ifndef ABUF_H
#define ABUF_H

#include <stddef.h>


typedef struct abuf {
    char	*b;
    size_t	 size;
    size_t	 cap;
} abuf;

#define ABUF_INIT {NULL, 0, 0}


void		 ab_init(abuf *buf);
void		 ab_appendch(abuf *buf, char c);
void		 ab_append(abuf *buf, const char *s, size_t len);
void		 ab_prependch(abuf *buf, const char c);
void		 ab_prepend(abuf *buf, const char *s, const size_t len);
void		 ab_free(abuf *buf);


#endif
