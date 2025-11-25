#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <cstddef>

// Forward declarations
struct editor_t;

namespace ke {
    class abuf;  // Forward declaration of ke::abuf

/**
 * Display class for screen rendering and refresh operations.
 */
class Display {
public:
    Display() = default;
    ~Display() = default;
    
    // Deleted copy constructor and assignment
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;
    
    // Main display operations
    static void refresh(editor_t* editor);
    static void clear(ke::abuf* ab);
    
    // Drawing operations
    static void draw_rows(editor_t* editor, ke::abuf* ab);
    static void draw_status_bar(editor_t* editor, ke::abuf* ab);
    static void draw_message_line(editor_t* editor, ke::abuf* ab);
    
    // Scrolling
    static void scroll(editor_t* editor);
    
private:
    static char status_mode_char(int mode);
};

} // namespace ke

#endif // DISPLAY_HPP
