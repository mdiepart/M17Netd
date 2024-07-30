cmake_minimum_required(VERSION 3.10)

project(M17Netd
	DESCRIPTION "M17Netd - Network bridges over M17"
	LANGUAGES C CXX)

set(m17netd_src
    src/m17netd.cpp
)

add_executable(M17Netd ${m17netd_src})


set(BUILD_STATIC_LIB ON)
add_subdirectory(lib/libm17)
target_link_libraries(M17Netd PUBLIC m17-static)
