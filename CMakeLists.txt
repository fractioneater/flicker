cmake_minimum_required(VERSION 3.31)
project(flicker C)

set(CMAKE_C_STANDARD 11)

include_directories(src)

add_executable(flicker
        src/chunk.c
        src/chunk.h
        src/common.h
        src/compiler.c
        src/compiler.h
        src/core.c
        src/core.fl.c
        src/core.h
        src/debug.c
        src/debug.h
        src/lexer.c
        src/lexer.h
        src/main.c
        src/memory.c
        src/memory.h
        src/native.c
        src/native.h
        src/object.c
        src/object.h
        src/shishua.c
        src/shishua.h
        src/table.c
        src/table.h
        src/utils.c
        src/utils.h
        src/value.c
        src/value.h
        src/vm.c
        src/vm.h)

find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(flicker PUBLIC ${MATH_LIBRARY})
endif()