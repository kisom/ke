# Constants Refactoring Documentation

## Overview
This document describes the refactoring of #defines and common constants into a single centralized header file for Kyle's Editor (ke).

## Changes Made

### 1. Created `ke_constants.h`
A new header file was created to centralize all common constants and preprocessor definitions used throughout the project.

**Location**: `/Users/kyle/src/ke2/ke_constants.h`

**Contents**:
- **Version Information**: `KE_VERSION` - Editor version string
- **Terminal Sequences**: `ESCSEQ` - ANSI escape sequence prefix
- **Keyboard Macros**: `CTRL_KEY(key)` - Control key combination macro
- **Display Constants**: 
  - `TAB_STOP` (8) - Tab stop width
  - `MSG_TIMEO` (3) - Message timeout in seconds
- **Memory Management**: `INITIAL_CAPACITY` (64) - Initial buffer capacity
- **Keyboard Modes**:
  - `MODE_NORMAL` (0) - Normal editing mode
  - `MODE_KCOMMAND` (1) - Command mode (^k commands)
  - `MODE_ESCAPE` (2) - Escape key handling mode
- **Kill Ring Operations**:
  - `KILLRING_NO_OP` (0) - No operation
  - `KILLRING_APPEND` (1) - Append deleted characters
  - `KILLRING_PREPEND` (2) - Prepend deleted characters
  - `KILLING_SET` (3) - Set killring to deleted character
  - `KILLRING_FLUSH` (4) - Clear the killring
- **Legacy Initializers**: `ABUF_INIT` - C struct initializer for append buffer

### 2. Updated `main.c`
- Added `#include "ke_constants.h"` at the top with other includes
- Removed all duplicate #define statements that were moved to the constants header
- Eliminated the duplicate `TAB_STOP` definition (was defined twice on lines 36 and 50)
- Cleaned up approximately 30+ lines of redundant definitions

### 3. Updated `erow.cpp`
- Added `#include "ke_constants.h"` with extern "C" linkage for C++ compatibility
- Removed local `constexpr int TAB_STOP = 8` definition
- Now uses the centralized `TAB_STOP` constant from `ke_constants.h`

## Benefits

1. **Single Source of Truth**: All constants are now defined in one location, making them easier to maintain and modify
2. **Eliminated Duplication**: Removed duplicate definitions (e.g., TAB_STOP was defined twice in main.c)
3. **Better Organization**: Constants are grouped logically by category with clear comments
4. **Cross-Language Compatibility**: Header works with both C (main.c) and C++ (erow.cpp) code
5. **Easier Maintenance**: Future changes to constants only need to be made in one place
6. **Improved Readability**: Cleaner source files with less clutter from #define statements

## Build System Compatibility

Both build systems have been tested and verified to work correctly:

### Makefile Build
```bash
make clean
make
```
Successfully compiles all files (main.c, abuf.cpp, erow.cpp) and links the executable.

### CMake Build
```bash
cd cmake-build-debug
cmake --build . --clean-first
```
Successfully builds the project with all components.

## Header Guard
The header uses standard include guards:
```c
#ifndef KE_CONSTANTS_H
#define KE_CONSTANTS_H
...
#endif /* KE_CONSTANTS_H */
```

## C/C++ Interoperability
When including in C++ files, use extern "C" linkage:
```cpp
extern "C" {
#include "ke_constants.h"
}
```

This ensures proper linkage and prevents name mangling issues.

## Future Considerations

1. **Additional Constants**: As the project grows, any new constants should be added to `ke_constants.h` rather than being scattered across source files
2. **Namespacing**: For C++ constants, consider creating a dedicated C++ header with namespaced constants
3. **Type Safety**: For C++ code, consider using `constexpr` or `inline constexpr` variables in a C++ header for better type safety
4. **Documentation**: Keep this document updated as new constants are added or modified

## Testing

All compilation tests passed:
- ✅ Makefile build: Clean compilation with no errors or warnings
- ✅ CMake build: Clean compilation with no errors or warnings
- ✅ All source files (C and C++) successfully include and use the constants
- ✅ No duplicate definition errors
- ✅ Proper linkage between C and C++ components

## Related Files

- `ke_constants.h` - The new constants header (created)
- `main.c` - Updated to use new header (modified)
- `erow.cpp` - Updated to use new header (modified)
- `Makefile` - No changes required (working)
- `CMakeLists.txt` - No changes required (working)

## Date
November 24, 2025
