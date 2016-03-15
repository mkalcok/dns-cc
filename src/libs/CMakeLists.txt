find_package(Curses REQUIRED)
find_package(CARES)

include_directories(${CURSES_INCLUDE_DIR})

add_library(display display.c)
target_link_libraries(display ${CURSES_LIBRARIES})

add_library(compress compress.c)

add_library(nameres nameres.c)
target_link_libraries(nameres ${CARES_LIBRARIES})