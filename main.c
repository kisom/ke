/*
 * ke - kyle's editor
 *
 * ke started off following along with the kilo walkthrough at
 *  https://viewsourcecode.org/snaptoken/kilo/
 *
 * It is inspired heavily by mg(1) and VDE. This is a single file and
 * can be built with
 *	$(CC) -D_DEFAULT_SOURCE -D_XOPEN_SOURCE -Wall -Wextra -pedantic \
 *		-Wshadow -Werror -std=c99 -g -o ke main.c
 *
 * It builds and runs on Linux and Darwin. I can't confirm BSD compatibility.
 *
 * commit 59d3fa1dab68e8683d5f5a9341f5f42ef3308876
 * Author: Kyle Isom <kyle@imap.cc>
 * Date:   Fri Feb 7 20:46:43 2020 -0800
 *
 *   Initial import, starting with kyle's editor.
 */
#include <sys/ioctl.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <sys/stat.h>

#include "abuf.h"
#include "buffer.h"
#include "editor.h"
#include "editing.h"
#include "process.h"
#include "term.h"


void		 loop(void);
void		 enable_debugging(void);
void		 deathknell(void);
static void	 signal_handler(int sig);
static void	 install_signal_handlers(void);


int
kbhit(void)
{
	int	 bytes_waiting = 0;

	ioctl(STDIN_FILENO, FIONREAD, &bytes_waiting);
	if (bytes_waiting < 0) {
		editor_set_status("kbhit: FIONREAD failed: %s", strerror(errno));

		/* if FIONREAD fails, we need to assume we should read. this
		 * will default to a much slower input sequence, but it'll work.
		 */
		return 1;
	}
	return bytes_waiting > 0;
}


void
loop(void)
{
	int	up = 1; /* update on the first runthrough */

	while (1) {
		if (up) {
			display_refresh();
		}

		/*
		 * ke should only refresh the display if it has received keyboard
		 * input; if it has, drain all the inputs. This is useful for
		 * handling pastes without massive screen flicker.
		 *
		 */
		up = process_keypress();
		if (up  != 0) {
			while (kbhit()) {
				process_keypress();
			}
		}
	}
}


void
enable_debugging(void)
{
	dump_pidfile();
}


void
deathknell(void)
{
	fflush(stderr);

	if (editor.killring != NULL) {
		ab_free(editor.killring);
		free(editor.killring);
		editor.killring = NULL;
	}

	reset_editor();
	disable_termraw();
}


static void
signal_handler(int sig)
{
	signal(sig, SIG_DFL);

	fprintf(stderr, "caught signal %d\n", sig);

	deathknell();

	raise(sig);
	_exit(127 + sig);
}


static void
install_signal_handlers(void)
{
	/* Block all these signals while inside any of them */
	const int fatal_signals[] = {
		SIGABRT, SIGFPE, SIGILL, SIGSEGV,
	#ifdef SIGBUS
		SIGBUS,
	#endif
	#ifdef SIGQUIT
		SIGQUIT,
	#endif
	#ifdef SIGSYS
		SIGSYS,
	#endif
		-1                          /* sentinel */
	    };
	int	i = 0;

	for (i = 0; fatal_signals[i] != -1; i++) {
		signal(fatal_signals[i], signal_handler);
	}

	atexit(deathknell);
}


int
main(int argc, char *argv[])
{
	const char	*arg          = NULL;
	const char	*path         = NULL;
	int		 i            = 0;
	int		 v            = 0;
	int		 nb           = 0;
	int		 opt          = 0;
	int		 debug        = 0;
	int		 pending_line = 0;   /* line number for the next filename */
	int		 first_loaded = 0;   /* has a filed been loaded already? */

	install_signal_handlers();

	while ((opt = getopt(argc, argv, "df:")) != -1) {
		if (opt == 'd') {
			debug = 1;
		} else {
			fprintf(stderr, "Usage: ke [-d] [-f logfile] [ +N ] [file ...]\n");
			exit(EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;

	setlocale(LC_ALL, "");
	if (debug) {
		enable_debugging();
	}

	setup_terminal();
	init_editor();

	/* start processing file names. if an arg starts with a '+',
	 * interpret it as the line to jump to.
	 */
	for (i = 0; i < argc; i++) {
		arg = argv[i];
		if (arg[0] == '+') {
			path = arg + 1;

			v = 0;
			if (*path != '\0') {
				v = atoi(path);
				if (v < 1) v = 0;
			}

			pending_line = v;
			continue;
		}

		if (!first_loaded) {
			open_file(arg);
			if (pending_line > 0) {
				jump_to_position(0, pending_line - 1);
				pending_line = 0;
			}

			first_loaded = 1;
		} else {
			nb = buffer_add_empty();
			buffer_switch(nb);
			open_file(arg);
			if (pending_line > 0) {
				jump_to_position(0, pending_line - 1);
				pending_line = 0;
			}
		}
	}

	editor_set_status("C-k q to exit / C-k d to dump core");

	display_clear(NULL);
	loop();

	return 0;
}
