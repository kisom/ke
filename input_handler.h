#ifndef INPUT_HANDLER_HPP
#define INPUT_HANDLER_HPP

#include <cstdint>

namespace ke {

/**
 * Input handler class for reading and processing keyboard input.
 */
class InputHandler {
public:
    InputHandler() = default;
    ~InputHandler() = default;
    
    // Deleted copy constructor and assignment
    InputHandler(const InputHandler&) = delete;
    InputHandler& operator=(const InputHandler&) = delete;
    
    // Read a keypress from stdin
    static int16_t get_keypress();
    
    // Check if a key code is an arrow/navigation key
    static bool is_arrow_key(int16_t c);
};

} // namespace ke

#endif // INPUT_HANDLER_HPP
