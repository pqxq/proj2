cmake_minimum_required(VERSION 3.25)

project(Task5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Пошук бібліотеки Intel TBB для системних політик <execution> під GCC/Clang
find_package(TBB QUIET)

add_executable(task5
    main.cpp
    task5.cpp
    task5.h
)

if(TBB_FOUND)
    target_link_libraries(task5 PRIVATE TBB::tbb)
endif()

target_compile_options(task5 PRIVATE -Wall -Wextra -Wpedantic -O3 -march=native)