cmake_minimum_required(VERSION 3.22)
project(include_audit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(include-audit
        source/main.cpp
)

target_include_directories(include-audit PRIVATE include)

# Helpful warnings (GCC/Clang)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(include-audit PRIVATE -Wall -Wextra -Wpedantic)
endif()