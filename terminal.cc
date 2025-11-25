#include "terminal.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>

namespace ke {

static Terminal* g_terminal_instance = nullptr;

static void cleanup_terminal_on_exit() {
    if (g_terminal_instance) {
        g_terminal_instance->disable_raw_mode();
    }
}

Terminal::Terminal() : raw_mode_enabled_(false) {
    g_terminal_instance = this;
}

Terminal::~Terminal() {
    if (raw_mode_enabled_) {
        disable_raw_mode();
    }
    g_terminal_instance = nullptr;
}

void Terminal::setup() {
    if (tcgetattr(STDIN_FILENO, &entry_term_) == -1) {
        perror("can't snapshot terminal settings");
        exit(1);
    }
    atexit(cleanup_terminal_on_exit);
    enable_raw_mode();
}

void Terminal::enable_raw_mode() {
    if (raw_mode_enabled_) {
        return;
    }
    
    struct termios raw;
    
    /* Read the current terminal parameters for standard input. */
    if (tcgetattr(STDIN_FILENO, &raw) == -1) {
        perror("tcgetattr while enabling raw mode");
        exit(1);
    }
    
    /*
     * Put the terminal into raw mode.
     */
    cfmakeraw(&raw);
    
    /*
     * Set timeout for read(2).
     *
     * VMIN: what is the minimum number of bytes required for read
     * to return?
     *
     * VTIME: max time before read(2) returns in hundreds of milli-
     * seconds.
     */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    /*
     * Now write the terminal parameters to the current terminal,
     * after flushing any waiting input out.
     */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr while enabling raw mode");
        exit(1);
    }
    
    raw_mode_enabled_ = true;
}

void Terminal::disable_raw_mode() {
    if (!raw_mode_enabled_) {
        return;
    }
    
    clear_screen();
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &entry_term_) == -1) {
        perror("couldn't disable terminal raw mode");
        exit(1);
    }
    
    raw_mode_enabled_ = false;
}

int Terminal::get_window_size(int* rows, int* cols) {
    struct winsize ws;
    
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    }
    
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

void Terminal::clear_screen() {
    (void)write(STDOUT_FILENO, "\x1b[2J", 4);
    (void)write(STDOUT_FILENO, "\x1b[H", 3);
}

} // namespace ke
