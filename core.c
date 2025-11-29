#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core.h"


#ifdef	INCLUDE_STRNSTR
/*
 * Find the first occurrence of find in s, where the search is limited to the
 * first slen characters of s.
 */
char *
strnstr(const char *s, const char *find, size_t slen)
{
	char	 c, sc;
	size_t	 len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if (slen-- < 1 || (sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char*)s);
}
#endif


void
swap_size_t(size_t *first, size_t *second)
{
	*first ^= *second;
	*second ^= *first;
	*first ^= *second;
}


int
next_power_of_2(int n)
{
	if (n < 2) {
		n = 2;
	}

	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;

	return n + 1;
}


int
cap_growth(int cap, int sz)
{
	if (cap == 0) {
		cap = INITIAL_CAPACITY;
	}

	while (cap <= sz) {
		cap = next_power_of_2(cap + 1);
	}

	return cap;
}


size_t
kstrnlen(const char *buf, const size_t max)
{
	if (buf == NULL) {
		return 0;
	}

	return strnlen(buf, max);
}


void
kwrite(const int fd, const char* buf, const int len)
{
	int wlen = 0;

	wlen = write(fd, buf, len);
	assert(wlen != -1);
	assert(wlen == len);
	if (wlen == -1) {
		abort();
	}
}


void
die(const char* s)
{
	kwrite(STDOUT_FILENO, "\x1b[2J", 4);
	kwrite(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);
	exit(1);
}

