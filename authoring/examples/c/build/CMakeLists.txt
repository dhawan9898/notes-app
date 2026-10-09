cmake_minimum_required(VERSION 3.16)
project(demo C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(util STATIC src/util.c)
target_include_directories(util PUBLIC include)
target_compile_options(util PRIVATE -Wall -Wextra -Wpedantic)

add_executable(demo src/main.c)
target_link_libraries(demo PRIVATE util)
target_compile_options(demo PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME runs COMMAND demo hello)
