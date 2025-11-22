cmake_minimum_required(VERSION 3.15)
project(ke C)  # Specify C language explicitly

set(CMAKE_C_STANDARD 99)
set(KE_VERSION "1.0.4")

set(CMAKE_C_FLAGS "-Wall -Wextra -pedantic -Wshadow -Werror -std=c99 -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DEFAULT_SOURCE -D_XOPEN_SOURCE")


# Add executable
add_executable(ke main.c)

# Define KE_VERSION for use in C code (e.g., #define KE_VERSION)
target_compile_definitions(ke PRIVATE KE_VERSION="ke version ${KE_VERSION}")

# Set output properties
set_target_properties(ke PROPERTIES
        FOLDER bin
        RUNTIME_OUTPUT_DIRECTORY bin
)