add_subdirectory(libs)

find_package(Threads)
find_package(ZLIB REQUIRED)
include_directories(/opt/local/include)
add_executable(dns-cc dns-cc.c)
target_link_libraries(dns-cc fec crypto compress display nameres ${ZLIB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(dns-cc
        PROPERTIES
        LINK_FLAGS -lpthread)