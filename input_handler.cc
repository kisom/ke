#include "input_handler.h"
#include "ke_constants.h"
#include <unistd.h>
#include <cstdio>
#include <cstdlib>

namespace ke {

// Key codes for special keys
enum KeyPress {
    TAB_KEY = 9,
    ESC_KEY = 27,
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PG_UP,
    PG_DN,
};

int16_t InputHandler::get_keypress() {
    char seq[3];
    /* read raw byte so UTF-8 bytes (>=0x80) are not sign-extended */
    unsigned char uc = 0;
    int16_t c;
    
    if (read(STDIN_FILENO, &uc, 1) == -1) {
        perror("get_keypress:read");
        exit(1);
    }
    
    c = (int16_t) uc;
    
    if (c == 0x1b) {
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return c;
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return c;
        
        if (seq[0] == '[') {
            if (seq[1] < 'A') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return c;
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1':
                            return HOME_KEY;
                        case '3':
                            return DEL_KEY;
                        case '4':
                            return END_KEY;
                        case '5':
                            return PG_UP;
                        case '6':
                            return PG_DN;
                        case '7':
                            return HOME_KEY;
                        case '8':
                            return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A':
                        return ARROW_UP;
                    case 'B':
                        return ARROW_DOWN;
                    case 'C':
                        return ARROW_RIGHT;
                    case 'D':
                        return ARROW_LEFT;
                    case 'F':
                        return END_KEY;
                    case 'H':
                        return HOME_KEY;
                    
                    default:
                        /* nada */ ;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'F':
                    return END_KEY;
                case 'H':
                    return HOME_KEY;
            }
        }
        
        return 0x1b;
    }
    
    return c;
}

bool InputHandler::is_arrow_key(int16_t c) {
    switch (c) {
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case ARROW_UP:
        case CTRL_KEY('p'):
        case CTRL_KEY('n'):
        case CTRL_KEY('f'):
        case CTRL_KEY('b'):
        case CTRL_KEY('a'):
        case CTRL_KEY('e'):
        case END_KEY:
        case HOME_KEY:
        case PG_DN:
        case PG_UP:
            return true;
    }
    
    return false;
}

} // namespace ke
