#ifndef KE_CORE_H
#define KE_CORE_H

#include <stddef.h>


#ifndef KE_VERSION
#define KE_VERSION		"ke dev build"
#endif


#define ESCSEQ			"\x1b["
#define	CTRL_KEY(key)		((key)&0x1f)
#define TAB_STOP		8
#define MSG_TIMEO		3

#define	TAB_STOP		8
#define	INITIAL_CAPACITY	8


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

int		 path_is_dir(const char *path);
size_t		 str_lcp2(const char *a, const char *b);
void		 swap_size_t(size_t *first, size_t *second);
int 		 next_power_of_2(int n);
int 		 cap_growth(int cap, int sz);
size_t		 kstrnlen(const char *buf, size_t max);
void		 kwrite(int fd, const char *buf, int len);
void		 die(const char *s);


#endif
