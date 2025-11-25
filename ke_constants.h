/*
 * ke_constants.h
 * 
 * Common constants and defines for Kyle's Editor (ke)
 * Refactored from main.c and other source files for centralized maintenance.
 */

#ifndef KE_CONSTANTS_H
#define KE_CONSTANTS_H

/* Version information */
#ifndef KE_VERSION
#define KE_VERSION		"ke dev build"
#endif

/* Terminal escape sequences */
#define ESCSEQ			"\x1b["

/* Keyboard and control key macros */
#define CTRL_KEY(key)		((key)&0x1f)

/* Display and rendering constants */
#define TAB_STOP		8
#define MSG_TIMEO		3

/* Memory management constants */
#define INITIAL_CAPACITY	64

/* Keyboard input modes */
#define MODE_NORMAL		0
#define MODE_KCOMMAND		1
#define MODE_ESCAPE		2

/* Kill ring operations */
#define KILLRING_NO_OP		0	/* don't touch the killring */
#define KILLRING_APPEND		1	/* append deleted chars */
#define KILLRING_PREPEND	2	/* prepend deleted chars */
#define KILLING_SET		3	/* set killring to deleted char */
#define KILLRING_FLUSH		4	/* clear the killring */

/* Legacy C struct initializers (for compatibility with main.c) */
#define ABUF_INIT		{NULL, 0, 0}

#endif /* KE_CONSTANTS_H */
