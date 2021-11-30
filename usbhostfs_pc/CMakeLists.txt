cmake_minimum_required(VERSION 3.0)

project(usbhostfs_pc)

add_compile_options(-Wall -ggdb -DPC_SIDE -D_FILE_OFFSET_BITS=64)

set(SOURCES
    main.c 
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    usb
    pthread
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/../usbhostfs
)
