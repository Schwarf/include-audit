cmake_minimum_required(VERSION 3.22)
project(include_audit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(include-audit
        source/main.cpp
        source/compile_db.cpp
)

target_include_directories(include-audit PRIVATE include)
target_link_libraries(include-audit PRIVATE nlohmann_json::nlohmann_json)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(include-audit PRIVATE -Wall -Wextra -Wpedantic)
endif()
