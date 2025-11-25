#include "file_io.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <ctime>

// Need access to main.cc structures and functions
extern void erow_insert(int at, const char *s, int len);
extern void reset_editor();
extern void editor_set_status(const char *fmt, ...);
extern void die(const char *s);
extern char *editor_prompt(const char *, void (*cb)(char *, int16_t));

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

void FileIO::open_file(editor_t* editor, const char* filename) {
    char* line = nullptr;
    size_t linecap = 0;
    ssize_t linelen;
    FILE* fp = nullptr;
    
    reset_editor();
    
    editor->filename = strdup(filename);
    assert(editor->filename != nullptr);
    
    editor->dirty = 0;
    if ((fp = fopen(filename, "r")) == nullptr) {
        if (errno == ENOENT) {
            editor_set_status("[new file]");
            return;
        }
        die("fopen");
    }
    
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        if (linelen != -1) {
            while ((linelen > 0) && ((line[linelen - 1] == '\r') ||
                                     (line[linelen - 1] == '\n'))) {
                linelen--;
            }
            
            erow_insert(editor->nrows, line, linelen);
        }
    }
    
    free(line);
    line = nullptr;
    fclose(fp);
}

char* FileIO::rows_to_buffer(editor_t* editor, int* buflen) {
    int len = 0;
    int j;
    char* buf = nullptr;
    char* p = nullptr;
    
    for (j = 0; j < editor->nrows; j++) {
        /* extra byte for newline */
        len += editor->row[j].size + 1;
    }
    
    if (len == 0) {
        return nullptr;
    }
    
    *buflen = len;
    buf = static_cast<char*>(malloc(len));
    assert(buf != nullptr);
    p = buf;
    
    for (j = 0; j < editor->nrows; j++) {
        memcpy(p, editor->row[j].line, editor->row[j].size);
        p += editor->row[j].size;
        *p++ = '\n';
    }
    
    return buf;
}

int FileIO::save_file(editor_t* editor) {
    int fd = -1;
    int len;
    int status = 1; /* will be used as exit code */
    char* buf;
    
    if (!editor->dirty) {
        editor_set_status("No changes to save.");
        return 0;
    }
    
    if (editor->filename == nullptr) {
        editor->filename = editor_prompt("Filename: %s", nullptr);
        if (editor->filename == nullptr) {
            editor_set_status("Save aborted.");
            return 0;
        }
    }
    
    buf = rows_to_buffer(editor, &len);
    if ((fd = open(editor->filename, O_RDWR | O_CREAT, 0644)) == -1) {
        goto save_exit;
    }
    
    if (-1 == ftruncate(fd, len)) {
        goto save_exit;
    }
    
    if (len == 0) {
        status = 0;
        goto save_exit;
    }
    
    if ((ssize_t) len != write(fd, buf, len)) {
        goto save_exit;
    }
    
    status = 0;
    
save_exit:
    if (fd)
        close(fd);
    if (buf) {
        free(buf);
        buf = nullptr;
    }
    
    if (status != 0) {
        buf = strerror(errno);
        editor_set_status("Error writing %s: %s",
                          editor->filename,
                          buf);
    } else {
        editor_set_status("Wrote %d bytes to %s.",
                          len,
                          editor->filename);
        editor->dirty = 0;
    }
    
    return status;
}

} // namespace ke
