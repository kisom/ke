#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "abuf.h"
#include "core.h"


void
ab_init(abuf *buf)
{
	assert(buf != NULL);

	buf->b    = NULL;
	buf->size = buf->cap = 0;
}


void
ab_init_cap(abuf *buf, const size_t cap)
{
	buf->b    = calloc(cap, 1);
	buf->size = 0;
	buf->cap  = cap;
}


void
ab_resize(abuf *buf, size_t cap)
{
	cap = cap_growth(buf->cap, cap);
	buf->b = realloc(buf->b, cap);
	assert(buf->b != NULL);
	buf->cap = cap;
}


void
ab_appendch(abuf *buf, char c)
{
	ab_append(buf, &c, 1);
}


void
ab_append(abuf *buf, const char *s, size_t len)
{
	char	*nc = buf->b;
	size_t	 sz = buf->size + len;

	if (sz >= buf->cap) {
		while (sz > buf->cap) {
			if (buf->cap == 0) {
				buf->cap = 1;
			} else {
				buf->cap *= 2;
			}
		}
		nc = realloc(nc, buf->cap);
		assert(nc != NULL);
	}

	memcpy(&nc[buf->size], s, len);
	buf->b = nc;
	buf->size += len;
}


void
ab_prependch(abuf *buf, const char c)
{
	ab_prepend(buf, &c, 1);
}


void
ab_prepend(abuf *buf, const char *s, const size_t len)
{
	char	*nc = realloc(buf->b, buf->size + len);
	assert(nc != NULL);

	memmove(nc + len, nc, buf->size);
	memcpy(nc, s, len);

	buf->b = nc;
	buf->size += len;
}


void
ab_free(abuf *buf)
{
	free(buf->b);
	buf->b    = NULL;
	buf->size = 0;
	buf->cap  = 0;
}
