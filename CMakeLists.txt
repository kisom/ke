# CMake configuration for Kyle's Editor (ke)
cmake_minimum_required(VERSION 3.15)
project(ke VERSION 1.3.3 LANGUAGES CXX)

# Project metadata
set(KE_VERSION "1.3.3")

# C++17 standard requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source files
set(KE_SOURCES
    main.cc
    abuf.cc
    erow.cc
)

# Header files
set(KE_HEADERS
    ke_constants.h
    abuf.h
    erow.h
)

# Build options
option(ENABLE_ASAN "Enable AddressSanitizer for builds" OFF)

# Create executable target
add_executable(ke ${KE_SOURCES})

# Target compile features
target_compile_features(ke PRIVATE cxx_std_17)

# Target compile options
target_compile_options(ke PRIVATE
    -Wall
    -Wextra
    -pedantic
    -Wshadow
    -Werror
    -g
)

# Target compile definitions
target_compile_definitions(ke PRIVATE
    KE_VERSION="ke version ${KE_VERSION}"
    _DEFAULT_SOURCE
    _XOPEN_SOURCE
)

# AddressSanitizer support
if(ENABLE_ASAN)
    message(STATUS "ASan enabled")
    target_compile_options(ke PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(ke PRIVATE -fsanitize=address)
endif()

# Installation rules
include(GNUInstallDirs)
install(TARGETS ke RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ke.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

