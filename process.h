#ifndef KE_PROCESS_H
#define KE_PROCESS_H


#include <stdint.h>
/*
 * define the keyboard input modes
 * normal: no special mode
 * kcommand: ^k commands
 * escape: what happens when you hit escape?
 */
#define	MODE_NORMAL		0
#define	MODE_KCOMMAND		1
#define	MODE_ESCAPE		2

void		 process_kcommand(int16_t c);
void		 process_normal(int16_t c);
void		 process_escape(int16_t c);
int	    	 process_keypress(void);


#endif
