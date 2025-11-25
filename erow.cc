#include "erow.h"
#include <cstring>
#include <cwchar>
#include <cassert>

#include "ke_constants.h"

namespace ke {

// Constructors
erow::erow(std::size_t initial_capacity) {
    line_.reserve(initial_capacity);
}

erow::erow(const char* text, std::size_t len) {
    line_.assign(text, len);
}

// Set line content
void erow::set_line(const char* data, std::size_t len) {
    line_.assign(data, len);
}

// Append string to the line
void erow::append_string(const char* s, std::size_t len) {
    line_.append(s, len);
}

// Insert character at position
void erow::insert_char(std::size_t at, char c) {
    if (at > line_.size()) {
        at = line_.size();
    }
    line_.insert(at, 1, c);
}

// Delete character at position
void erow::delete_char(std::size_t at) {
    if (at < line_.size()) {
        line_.erase(at, 1);
    }
}

// Helper function for nibble to hex conversion
char erow::nibble_to_hex(char c) {
    c &= 0xf;
    if (c < 10) {
        return static_cast<char>('0' + c);
    }
    return static_cast<char>('A' + (c - 10));
}

// Update the render string based on the line content
void erow::update() {
    int tabs = 0;
    int ctrl = 0;
    
    // Count tabs and control characters
    for (std::size_t j = 0; j < line_.size(); ++j) {
        if (line_[j] == '\t') {
            ++tabs;
        } else if (static_cast<unsigned char>(line_[j]) < 0x20) {
            ++ctrl;
        }
    }
    
    // Allocate render buffer
    render_.clear();
    render_.reserve(line_.size() + (tabs * (TAB_STOP - 1)) + (ctrl * 3) + 1);
    
    // Build the render string
    for (std::size_t j = 0; j < line_.size(); ++j) {
        if (line_[j] == '\t') {
            // Expand tabs to spaces
            do {
                render_.push_back(' ');
            } while ((render_.size() % TAB_STOP) != 0);
        } else if (static_cast<unsigned char>(line_[j]) < 0x20) {
            // Render control characters as \xx
            render_.push_back('\\');
            render_.push_back(nibble_to_hex(line_[j] >> 4));
            render_.push_back(nibble_to_hex(line_[j] & 0x0f));
        } else {
            // Leave UTF-8 multibyte bytes untouched
            render_.push_back(line_[j]);
        }
    }
}

// Convert cursor position to render position
int erow::render_to_cursor(int cx) const {
    int rx = 0;
    std::size_t j = 0;
    
    wchar_t wc;
    std::mbstate_t st{};
    
    while (j < static_cast<std::size_t>(cx) && j < line_.size()) {
        unsigned char b = static_cast<unsigned char>(line_[j]);
        
        if (b == '\t') {
            rx += (TAB_STOP - 1) - (rx % TAB_STOP);
            ++rx;
            ++j;
            continue;
        }
        
        if (b < 0x20) {
            // Render as \xx -> width 3
            rx += 3;
            ++j;
            continue;
        }
        
        std::size_t rem = line_.size() - j;
        std::size_t n = std::mbrtowc(&wc, &line_[j], rem, &st);
        
        if (n == static_cast<std::size_t>(-2)) {
            // Incomplete sequence at end; treat one byte
            rx += 1;
            j += 1;
            std::memset(&st, 0, sizeof(st));
        } else if (n == static_cast<std::size_t>(-1)) {
            // Invalid byte; consume one and reset state
            rx += 1;
            j += 1;
            std::memset(&st, 0, sizeof(st));
        } else if (n == 0) {
            // Null character
            rx += 0;
            j += 1;
        } else {
            int w = wcwidth(wc);
            if (w < 0) {
                w = 1; // Non-printable -> treat as width 1
            }
            rx += w;
            j += n;
        }
    }
    
    return rx;
}

// Convert render position to cursor position
int erow::cursor_to_render(int rx) const {
    int cur_rx = 0;
    std::size_t j = 0;
    
    wchar_t wc;
    std::mbstate_t st{};
    
    while (j < line_.size()) {
        unsigned char b = static_cast<unsigned char>(line_[j]);
        
        if (b == '\t') {
            int tab_width = (TAB_STOP - 1) - (cur_rx % TAB_STOP) + 1;
            if (cur_rx + tab_width > rx) {
                break;
            }
            cur_rx += tab_width;
            ++j;
            continue;
        }
        
        if (b < 0x20) {
            // Control char width is 3
            if (cur_rx + 3 > rx) {
                break;
            }
            cur_rx += 3;
            ++j;
            continue;
        }
        
        std::size_t rem = line_.size() - j;
        std::size_t n = std::mbrtowc(&wc, &line_[j], rem, &st);
        std::size_t adv = 1;
        int w = 1;
        
        if (n == static_cast<std::size_t>(-2)) {
            // Incomplete sequence
            adv = 1;
            w = 1;
            std::memset(&st, 0, sizeof(st));
        } else if (n == static_cast<std::size_t>(-1)) {
            // Invalid byte
            adv = 1;
            w = 1;
            std::memset(&st, 0, sizeof(st));
        } else if (n == 0) {
            // Null character
            adv = 1;
            w = 0;
        } else {
            adv = n;
            w = wcwidth(wc);
            if (w < 0) {
                w = 1;
            }
        }
        
        if (cur_rx + w > rx) {
            break;
        }
        
        cur_rx += w;
        j += adv;
    }
    
    return static_cast<int>(j);
}

} // namespace ke
