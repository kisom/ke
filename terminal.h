#ifndef TERMINAL_HPP
#define TERMINAL_HPP

#include <termios.h>

namespace ke {

/**
 * Terminal management class for handling raw mode and terminal operations.
 */
class Terminal {
public:
    Terminal();
    ~Terminal();
    
    // Deleted copy constructor and assignment
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    
    // Setup and teardown
    void setup();
    void enable_raw_mode();
    void disable_raw_mode();
    
    // Terminal operations
    static int get_window_size(int* rows, int* cols);
    static void clear_screen();
    
    // Access to original terminal settings
    const termios& entry_term() const { return entry_term_; }
    
private:
    termios entry_term_;
    bool raw_mode_enabled_;
};

} // namespace ke

#endif // TERMINAL_HPP
