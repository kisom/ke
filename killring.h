#ifndef KE_KILLRING_H
#define KE_KILLRING_H


#define KILLRING_NO_OP		0	/* don't touch the killring */
#define KILLRING_APPEND		1	/* append deleted chars */
#define KILLRING_PREPEND	2	/* prepend deleted chars */
#define KILLING_SET		3	/* set killring to deleted char */
#define KILLRING_FLUSH		4	/* clear the killring */


void		 killring_flush(void);
void		 killring_yank(void);
void		 killring_start_with_char(unsigned char ch);
void		 killring_append_char(unsigned char ch);
void		 killring_prepend_char(unsigned char ch);
void		 toggle_markset(void);
int	    	 cursor_after_mark(void);
int	    	 count_chars_from_cursor_to_mark(void);
void		 kill_region(void);
void		 indent_region(void);
void		 unindent_region(void);
void		 delete_region(void);


#endif
