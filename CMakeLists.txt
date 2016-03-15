cmake_minimum_required(VERSION 3.1)
project(dns-cc)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/extras/cmakeFind)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
add_definitions("-Wall")

add_subdirectory(src)

#
#set(SOURCE_FILES
#        src/compress.h
#        src/dns-cc.c
#        src/nameres.h
#        src/display.h)
#
#find_package(Threads)
#find_package(ZLIB)
#find_package(Curses)
#
#
#add_library(display src/display.h)
#add_library(nameres src/nameres.h)
#add_library(compress src/compress.h)

#add_executable(dns-cc src/dns-cc.c)
#target_link_libraries(src/dns-cc.c src/display.h)
