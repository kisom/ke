#include "killring.h"
#include <termios.h>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <ctime>

// Need access to main.cc structures and functions
extern int erow_init(struct erow *row, int len);
extern void erow_update(struct erow *row);
extern void erow_free(struct erow *row);
extern void editor_set_status(const char *fmt, ...);
extern void insertch(int16_t c);
extern void newline();
extern void move_cursor(int16_t c);
extern int cursor_at_eol();
extern void swap_int(int *a, int *b);
extern void deletech(uint8_t op);

// erow definition
struct erow {
	char *line;
	char *render;
	int size;
	int rsize;
	int cap;
};

// editor_t definition
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

void Killring::flush(editor_t* editor) {
    if (editor->killring != nullptr) {
        erow_free(editor->killring);
        free(editor->killring);
        editor->killring = nullptr;
    }
}

void Killring::yank(editor_t* editor) {
    if (editor->killring == nullptr) {
        return;
    }
    /*
     * Insert killring contents at the cursor without clearing the ring.
     * Interpret '\n' as an actual newline() rather than inserting a raw 0x0A
     * byte, so yanked content preserves lines correctly.
     */
    for (int i = 0; i < editor->killring->size; i++) {
        unsigned char ch = (unsigned char) editor->killring->line[i];
        if (ch == '\n') {
            newline();
        } else {
            insertch(ch);
        }
    }
}

void Killring::start_with_char(editor_t* editor, unsigned char ch) {
    erow* row = nullptr;
    
    if (editor->killring != nullptr) {
        erow_free(editor->killring);
        free(editor->killring);
        editor->killring = nullptr;
    }
    
    editor->killring = static_cast<erow*>(malloc(sizeof(erow)));
    assert(editor->killring != nullptr);
    assert(erow_init(editor->killring, 0) == 0);
    
    /* append one char to empty killring without affecting editor.dirty */
    row = editor->killring;
    
    row->line = static_cast<char*>(realloc(row->line, row->size + 2));
    assert(row->line != nullptr);
    row->line[row->size] = ch;
    row->size++;
    row->line[row->size] = '\0';
    erow_update(row);
}

void Killring::append_char(editor_t* editor, unsigned char ch) {
    erow* row = nullptr;
    
    if (editor->killring == nullptr) {
        start_with_char(editor, ch);
        return;
    }
    
    row = editor->killring;
    row->line = static_cast<char*>(realloc(row->line, row->size + 2));
    assert(row->line != nullptr);
    row->line[row->size] = ch;
    row->size++;
    row->line[row->size] = '\0';
    erow_update(row);
}

void Killring::prepend_char(editor_t* editor, unsigned char ch) {
    if (editor->killring == nullptr) {
        start_with_char(editor, ch);
        return;
    }
    
    erow* row = editor->killring;
    row->line = static_cast<char*>(realloc(row->line, row->size + 2));
    assert(row->line != nullptr);
    memmove(&row->line[1], &row->line[0], row->size + 1);
    row->line[0] = ch;
    row->size++;
    erow_update(row);
}

void Killring::toggle_markset(editor_t* editor) {
    if (editor->mark_set) {
        editor->mark_set = 0;
        editor_set_status("Mark cleared.");
        return;
    }
    
    editor->mark_set = 1;
    editor->mark_curx = editor->curx;
    editor->mark_cury = editor->cury;
    editor_set_status("Mark set.");
}

int Killring::cursor_after_mark(editor_t* editor) {
    if (editor->mark_cury < editor->cury) {
        return 1;
    }
    
    if (editor->mark_cury > editor->cury) {
        return 0;
    }
    
    return editor->curx >= editor->mark_curx;
}

int Killring::count_chars_from_cursor_to_mark(editor_t* editor) {
    int count = 0;
    int curx = editor->curx;
    int cury = editor->cury;
    
    int markx = editor->mark_curx;
    int marky = editor->mark_cury;
    
    if (!cursor_after_mark(editor)) {
        swap_int(&curx, &markx);
        swap_int(&cury, &marky);
    }
    
    editor->curx = markx;
    editor->cury = marky;
    
    while (editor->cury != cury) {
        while (!cursor_at_eol()) {
            move_cursor(1000 + 3); // ARROW_RIGHT
            count++;
        }
        
        move_cursor(1000 + 3); // ARROW_RIGHT
        count++;
    }
    
    while (editor->curx != curx) {
        count++;
        move_cursor(1000 + 3); // ARROW_RIGHT
    }
    
    return count;
}

void Killring::kill_region(editor_t* editor) {
    int curx = editor->curx;
    int cury = editor->cury;
    int markx = editor->mark_curx;
    int marky = editor->mark_cury;
    
    if (!editor->mark_set) {
        return;
    }
    
    /* kill the current killring */
    flush(editor);
    
    if (!cursor_after_mark(editor)) {
        swap_int(&curx, &markx);
        swap_int(&cury, &marky);
    }
    
    editor->curx = markx;
    editor->cury = marky;
    
    while (editor->cury != cury) {
        while (!cursor_at_eol()) {
            append_char(editor, editor->row[editor->cury].line[editor->curx]);
            move_cursor(1000 + 3); // ARROW_RIGHT
        }
        append_char(editor, '\n');
        move_cursor(1000 + 3); // ARROW_RIGHT
    }
    
    while (editor->curx != curx) {
        append_char(editor, editor->row[editor->cury].line[editor->curx]);
        move_cursor(1000 + 3); // ARROW_RIGHT
    }
    
    editor_set_status("Region killed.");
}

void Killring::delete_region(editor_t* editor) {
    int count = count_chars_from_cursor_to_mark(editor);
    int killed = 0;
    int curx = editor->curx;
    int cury = editor->cury;
    int markx = editor->mark_curx;
    int marky = editor->mark_cury;
    
    if (!editor->mark_set) {
        return;
    }
    
    if (!cursor_after_mark(editor)) {
        swap_int(&curx, &markx);
        swap_int(&cury, &marky);
    }
    
    editor->curx = markx;
    editor->cury = marky;
    
    while (killed < count) {
        deletech(0); // KILLRING_NO_OP
        killed++;
    }
    
    editor->kill = 1;
    editor_set_status("Region killed.");
}

} // namespace ke
