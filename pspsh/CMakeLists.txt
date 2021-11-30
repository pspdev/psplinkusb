cmake_minimum_required(VERSION 3.0)

project(pspsh)

add_compile_options(-Wall -D_PCTERM)

set(SOURCES
    pspsh.cpp
    parse_args.cpp
    pspkerror.cpp
    asm.cpp
    disasm.cpp 
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    readline
    ncurses
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/../psplink
)
