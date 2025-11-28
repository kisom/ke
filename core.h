#ifndef KE_CORE_H
#define KE_CORE_H

#include <stddef.h>


#define		calloc1(sz)		calloc(1, sz)
#define		INITIAL_CAPACITY	8


typedef enum key_press {
	TAB_KEY     = 9,
	ESC_KEY     = 27,
	BACKSPACE   = 127,
	ARROW_LEFT  = 1000,
	ARROW_RIGHT = 1001,
	ARROW_UP    = 1002,
	ARROW_DOWN  = 1003,
	DEL_KEY     = 1004,
	HOME_KEY    = 1005,
	END_KEY     = 1006,
	PG_UP       = 1007,
	PG_DN       = 1008,
} key_press;


#ifndef strnstr
char	*strnstr(const char *s, const char *find, size_t slen);
#define	INCLUDE_STRNSTR
#endif

char		 nibble_to_hex(char c);
void		 swap_int(int *first, int *second);
int		 next_power_of_2(int n);
int		 cap_growth(int cap, int sz);
size_t		 kstrnlen(const char *buf, size_t max);
void		 kwrite(int fd, const char *buf, int len);
void		 die(const char *s);


#endif