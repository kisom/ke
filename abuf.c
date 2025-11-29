#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "abuf.h"
#include "core.h"


static void
abuf_grow(abuf *buf, size_t delta)
{
	if (buf->cap - buf->size < delta) {
		ab_resize(buf, buf->cap + delta);
	}
}


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
	ab_init(buf);

	if (cap > 0) {
		ab_resize(buf, cap);
	}
}


void
ab_resize(abuf *buf, size_t cap)
{
	char	*newbuf = NULL;

	cap = cap_growth(buf->cap, cap) + 1;
	newbuf = realloc(buf->b, cap);
	assert(newbuf != NULL);
	buf->cap = cap;
	buf->b   = newbuf;
}


void
ab_appendch(abuf *buf, char c)
{
	abuf_grow(buf, 1);
	ab_append(buf, &c, 1);
}


void
ab_append(abuf *buf, const char *s, size_t len)
{
	char	*nc = NULL;

	abuf_grow(buf, len);
	nc = buf->b;

	memcpy(&nc[buf->size], s, len);
	buf->b = nc;
	buf->size += len;
}


void
ab_append_ab(abuf *buf, const abuf *other)
{
	assert(buf != NULL);
	if (other == NULL) {
		return;
	}

	ab_append(buf, other->b, other->size);
}


void
ab_prependch(abuf *buf, const char c)
{
	abuf_grow(buf, 1);

	ab_prepend(buf, &c, 1);
}


void
ab_prepend(abuf *buf, const char *s, const size_t len)
{
	char	*nc = NULL;

	abuf_grow(buf, len);
	nc = buf->b;
	assert(nc != NULL);

	memmove(nc + len, nc, buf->size);
	memcpy(nc, s, len);

	buf->b = nc;
	buf->size += len;
}


void
ab_prepend_ab(abuf *buf, const abuf *other)
{
	assert(buf != NULL);
	if (other == NULL) {
		return;
	}

	ab_prepend(buf, other->b, other->size);
}


void
ab_free(abuf *buf)
{
	free(buf->b);
	buf->b    = NULL;
	buf->size = 0;
	buf->cap  = 0;
}
