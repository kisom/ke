cmake_minimum_required(VERSION 3.15)
project(ke C)  # Specify C language explicitly

set(CMAKE_C_STANDARD 99)
set(KE_VERSION "2.1.0")

set(CMAKE_C_FLAGS "-Wall -Wextra -pedantic -Wshadow -Werror -std=c99 -g -Werror=stringop-truncation")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DEFAULT_SOURCE -D_XOPEN_SOURCE")

# Optionally enable AddressSanitizer (ASan)
option(ENABLE_ASAN "Enable AddressSanitizer for builds" OFF)

if (ENABLE_ASAN)
    message(STATUS "ASan enabled")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    # Ensure the sanitizer is linked too (especially important on some platforms)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

include(GNUInstallDirs)

# Add executable
add_executable(ke
        abuf.c
        term.c
        buffer.c
        editor.c
        core.c
        core.h
        main.c
)
target_compile_definitions(ke PRIVATE KE_VERSION="ke version ${KE_VERSION}")
install(TARGETS ke RUNTIME DESTINATION bin)
install(FILES ke.1 TYPE MAN)

install(TARGETS ke RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ke.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

