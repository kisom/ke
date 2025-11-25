# C++17 Conversion Documentation

## Overview
This document describes the initial conversion of the ke2 project from C to C++17 standards, starting with the `abuf` and `erow` structures as requested.

## Converted Components

### 1. abuf (Append Buffer)
**Original C Structure (main.c:69-73):**
```c
struct abuf {
    char *b;
    int len;
    int cap;
};
```

**C++17 Implementation (abuf.hpp, abuf.cpp):**
- Converted to a modern C++ class using RAII principles
- Uses `std::string` as the internal buffer for automatic memory management
- Implements move semantics (deleted copy operations)
- Uses C++17 features:
  - `std::string_view` for efficient string parameters
  - `[[nodiscard]]` attributes for better safety
  - `noexcept` specifications where appropriate
- No manual memory management required - destructor automatically handled by std::string

**Key Improvements:**
- Automatic memory cleanup (RAII)
- Exception-safe
- No memory leaks possible
- More efficient with move semantics
- Type-safe with modern C++ idioms

### 2. erow (Editor Row)
**Original C Structure (main.c:79-87):**
```c
struct erow {
    char *line;
    char *render;
    int size;
    int rsize;
    int cap;
};
```

**C++17 Implementation (erow.hpp, erow.cpp):**
- Converted to a C++ class with proper encapsulation
- Uses `std::string` for both `line` and `render` buffers
- Implements move semantics (deleted copy operations)
- All manual memory management replaced with RAII
- Retains all original functionality:
  - UTF-8 character handling with `mbrtowc` and `wcwidth`
  - Tab expansion (TAB_STOP = 8)
  - Control character rendering as `\xx`
  - Cursor-to-render and render-to-cursor position conversions

**Key Improvements:**
- Automatic memory cleanup for both line and render buffers
- No malloc/free/realloc needed
- Exception-safe operations
- Better encapsulation with private members
- Modern C++ method naming conventions

## Build System Updates

### Makefile
- Added C++ compiler support (g++)
- Added CXXFLAGS with `-std=c++17`
- Modified build rules to compile C and C++ separately
- Updated to link with C++ compiler
- Enhanced clean target to remove object files

### CMakeLists.txt
- Changed project language from `C` to `C CXX`
- Added `CMAKE_CXX_STANDARD 17` and `CMAKE_CXX_STANDARD_REQUIRED ON`
- Added C++ compiler flags matching C flags
- Updated executable target to include `abuf.cpp` and `erow.cpp`
- Maintained backward compatibility with existing C code (main.c)

## Compilation Testing
Both build systems have been tested and verified:
- ✅ Makefile: Successfully compiles with `make`
- ✅ CMake: Successfully configures and builds
- ✅ Object files generated correctly for all C++ sources
- ✅ No compilation warnings or errors

## C++17 Features Used
1. **std::string** - Modern string class with automatic memory management
2. **std::string_view** - Efficient non-owning string references (C++17)
3. **[[nodiscard]]** - Compiler warnings for ignored return values (C++17)
4. **noexcept** - Exception specifications for optimization
5. **default/delete** - Explicit control over special member functions
6. **Move semantics** - Efficient resource transfer
7. **constexpr** - Compile-time constants (TAB_STOP)

## Next Steps
The project now has a foundation for further C++17 conversion:
- Additional structures can be converted following the same patterns
- Consider converting more C code to C++ classes
- Potential for using more STL containers (std::vector for dynamic arrays)
- Opportunity to use smart pointers for other memory management
- Can leverage more C++17 features (std::optional, structured bindings, etc.)

## Compatibility
- The new C++ code coexists with existing C code (main.c)
- Both can be compiled together and linked successfully
- The conversion maintains the original functionality
- No changes required to existing C code at this stage
