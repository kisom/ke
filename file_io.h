#ifndef FILE_IO_HPP
#define FILE_IO_HPP

// Forward declaration
struct editor_t;

namespace ke {

/**
 * File I/O class for reading and writing files.
 */
class FileIO {
public:
    FileIO() = default;
    ~FileIO() = default;
    
    // Deleted copy constructor and assignment
    FileIO(const FileIO&) = delete;
    FileIO& operator=(const FileIO&) = delete;
    
    // File operations
    static void open_file(editor_t* editor, const char* filename);
    static int save_file(editor_t* editor);
    static char* rows_to_buffer(editor_t* editor, int* buflen);
};

} // namespace ke

#endif // FILE_IO_HPP
