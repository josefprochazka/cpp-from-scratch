# Visual Studio umí CMake projekt otevřít přímo: File > Open > Folder...
cmake_minimum_required(VERSION 3.21)

project(CppFromScratch LANGUAGES CXX)

# Standard C++20 (std::span, std::bit_cast, <bit>, ranges, concepts, std::format ...)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cpp-from-scratch
    src/main.cpp
    src/util.cpp
    src/memview.cpp
    src/objects.cpp
    src/vector_move.cpp
    src/arena.cpp
    src/bits.cpp
    src/tasks.cpp
    src/raii.cpp
    src/lambdas.cpp
)

# Druhý, samostatný program: fronta (FIFO) jako kruhový buffer.
# Ve Visual Studiu ho vybereš v "Select Startup Item" jako fronta.exe.
add_executable(fronta
    fronta/main.cpp
    fronta/queue_view.cpp
    src/util.cpp          # print, read_line, read_int sdílí s hlavním programem
)
target_include_directories(fronta PRIVATE src)

foreach(target cpp-from-scratch fronta)
    if(MSVC)
        # /W4             = vysoká úroveň varování
        # /utf-8          = zdrojáky i výstup v UTF-8 (kvůli češtině)
        # /permissive-    = striktně podle standardu, bez "rozšíření" MSVC
        # /Zc:__cplusplus = makro __cplusplus hlásí skutečnou verzi C++ (jinak 199711L)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
    endif()
endforeach()
