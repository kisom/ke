cmake_minimum_required(VERSION 3.15)
project(ke C)  # Specify C language explicitly

set(CMAKE_C_STANDARD 99)
set(KE_VERSION "1.0.0")

# Add executable
add_executable(ke main.c)

# Define KE_VERSION for use in C code (e.g., #define KE_VERSION)
target_compile_definitions(ke PRIVATE KE_VERSION="${KE_VERSION}")

# Set output properties
set_target_properties(ke PROPERTIES
        FOLDER bin
        RUNTIME_OUTPUT_DIRECTORY bin
)