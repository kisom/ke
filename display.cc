#include "display.h"
#include "ke_constants.h"
#include "abuf.h"
#include "erow.h"
#include <termios.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cassert>

// Need access to main.cc structures and functions
extern void erow_update(struct erow *row);
extern int erow_render_to_cursor(struct erow *row, int cx);

// erow definition (needed for accessing row fields)
struct erow {
	char *line;
	char *render;
	int size;
	int rsize;
	int cap;
};

// editor_t definition (needed for display operations)
struct editor_t {
	struct termios entry_term;
	int rows, cols;
	int curx, cury;
	int rx;
	int mode;
	int nrows;
	int rowoffs, coloffs;
	struct erow *row;
	struct erow *killring;
	int kill;
	int no_kill;
	char *filename;
	int dirty;
	int dirtyex;
	char msg[80];
	int mark_set;
	int mark_curx, mark_cury;
	time_t msgtm;
};

namespace ke {

void Display::clear(ke::abuf* ab) {
    if (ab == nullptr) {
        (void)write(STDOUT_FILENO, ESCSEQ "2J", 4);
        (void)write(STDOUT_FILENO, ESCSEQ "H", 3);
    } else {
        ab->append(ESCSEQ "2J", 4);
        ab->append(ESCSEQ "H", 3);
    }
}

void Display::draw_rows(editor_t* editor, ke::abuf* ab) {
    assert(editor->cols >= 0);
    
    char* buf = new char[editor->cols];
    int buflen, filerow, padding;
    int y;
    
    for (y = 0; y < editor->rows; y++) {
        filerow = y + editor->rowoffs;
        if (filerow >= editor->nrows) {
            if ((editor->nrows == 0) && (y == editor->rows / 3)) {
                buflen = snprintf(buf,
                                  editor->cols,
                                  "%s",
                                  KE_VERSION);
                padding = (editor->rows - buflen) / 2;
                
                if (padding) {
                    ab->append("|", 1);
                    padding--;
                }
                
                while (padding--)
                    ab->append(" ", 1);
                ab->append(buf, buflen);
            } else {
                ab->append("|", 1);
            }
        } else {
            erow_update(&editor->row[filerow]);
            buflen = editor->row[filerow].rsize - editor->coloffs;
            if (buflen < 0) {
                buflen = 0;
            }
            
            if (buflen > editor->cols) {
                buflen = editor->cols;
            }
            ab->append(editor->row[filerow].render + editor->coloffs,
                       buflen);
        }
        ab->append(ESCSEQ "K", 3);
        ab->append("\r\n", 2);
    }
    delete[] buf;
}

char Display::status_mode_char(int mode) {
    switch (mode) {
        case MODE_NORMAL:
            return 'N';
        case MODE_KCOMMAND:
            return 'K';
        case MODE_ESCAPE:
            return 'E';
        default:
            return '?';
    }
}

void Display::draw_status_bar(editor_t* editor, ke::abuf* ab) {
    char* status = new char[editor->cols];
    char* rstatus = new char[editor->cols];
    char* mstatus = new char[editor->cols];
    
    int len, rlen;
    
    len = snprintf(status,
                   editor->cols,
                   "%c%cke: %.20s - %d lines",
                   status_mode_char(editor->mode),
                   editor->dirty ? '!' : '-',
                   editor->filename ? editor->filename : "[no file]",
                   editor->nrows);
    
    if (editor->mark_set) {
        snprintf(mstatus,
                 editor->cols,
                 " | M: %d, %d ",
                 editor->mark_curx + 1,
                 editor->mark_cury + 1);
    } else {
        snprintf(mstatus, editor->cols, " | M:clear ");
    }
    
    rlen = snprintf(rstatus,
                    editor->cols,
                    "L%d/%d C%d %s",
                    editor->cury + 1,
                    editor->nrows,
                    editor->curx + 1,
                    mstatus);
    
    if (len > editor->cols) {
        len = editor->cols;
    }
    
    ab->append(ESCSEQ "7m", 4);
    ab->append(status, len);
    while (len < editor->cols) {
        if (editor->cols - len == rlen) {
            ab->append(rstatus, rlen);
            break;
        } else {
            ab->append(" ", 1);
            len++;
        }
        len++;
    }
    ab->append(ESCSEQ "m", 3);
    ab->append("\r\n", 2);
    delete[] status;
    delete[] rstatus;
    delete[] mstatus;
}

void Display::draw_message_line(editor_t* editor, ke::abuf* ab) {
    int len = strlen(editor->msg);
    
    ab->append(ESCSEQ "K", 3);
    if (len > editor->cols) {
        len = editor->cols;
    }
    
    if (len && ((time(nullptr) - editor->msgtm) < MSG_TIMEO)) {
        ab->append(editor->msg, len);
    }
}

void Display::scroll(editor_t* editor) {
    editor->rx = 0;
    if (editor->cury < editor->nrows) {
        editor->rx = erow_render_to_cursor(
            &editor->row[editor->cury],
            editor->curx);
    }
    
    if (editor->cury < editor->rowoffs) {
        editor->rowoffs = editor->cury;
    }
    
    if (editor->cury >= editor->rowoffs + editor->rows) {
        editor->rowoffs = editor->cury - editor->rows + 1;
    }
    
    if (editor->rx < editor->coloffs) {
        editor->coloffs = editor->rx;
    }
    
    if (editor->rx >= editor->coloffs + editor->cols) {
        editor->coloffs = editor->rx - editor->cols + 1;
    }
}

void Display::refresh(editor_t* editor) {
    char buf[32];
    ke::abuf ab;
    
    scroll(editor);
    
    ab.append(ESCSEQ "?25l", 6);
    clear(&ab);
    
    draw_rows(editor, &ab);
    draw_status_bar(editor, &ab);
    draw_message_line(editor, &ab);
    
    snprintf(buf,
             sizeof(buf),
             ESCSEQ "%d;%dH",
             (editor->cury - editor->rowoffs) + 1,
             (editor->rx - editor->coloffs) + 1);
    ab.append(buf, strnlen(buf, 32));
    ab.append(ESCSEQ "?25h", 6);
    
    (void)write(STDOUT_FILENO, ab.data(), ab.size());
}

} // namespace ke
