cmake_minimum_required(VERSION 3.10)

project(M17Netd
	DESCRIPTION "M17Netd - Network bridges over M17"
	LANGUAGES C CXX)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(m17netd_src
    src/m17netd.cpp
	src/tuntap.cpp
	src/tunthread.cpp
	src/radio_thread.cpp
	src/m17tx_thread.cpp
)

find_package(tomlplusplus CONFIG REQUIRED)

add_executable(M17Netd ${m17netd_src})

target_include_directories(M17Netd PUBLIC
			   ${PROJECT_SOURCE_DIR}/inc
		   	  )
# ${tomlplusplus_INCLUDE_DIRS}

set(BUILD_STATIC_LIB ON)
add_subdirectory(lib/libm17)
target_link_libraries(M17Netd PUBLIC
					  m17-static
					  tomlplusplus
					 )

