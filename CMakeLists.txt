cmake_minimum_required(VERSION 3.11)

project(M17Netd
	DESCRIPTION "M17Netd - Network bridges over M17"
	LANGUAGES C CXX)

include(FetchContent)
include(UsePkgConfig)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find and declare libraries / dependencies
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

find_package(PkgConfig)
pkg_search_module(FFTW REQUIRED fftw3f IMPORTED_TARGET)

set(BUILD_STATIC_LIB ON)
set(BUILD_SHARED_LIB OFF)
add_subdirectory(lib/libm17)
find_library(liquid_LIB liquid)
find_package(ALSA REQUIRED)
find_package(Threads REQUIRED)

if(liquid_LIB STREQUAL "liquid-NOTFOUND")
	message(SEND_ERROR "libliquid could not be found on your system.")
endif()

# Define source files
set(m17netd_src
    src/m17netd.cpp
	src/config.cpp
	src/tuntap.cpp
	src/tun_threads.cpp
	src/radio_thread.cpp
	src/m17tx_thread.cpp
	src/sdrnode.cpp
	src/m17tx.cpp
	src/spi.cpp
	src/sx1255.cpp
	src/M17Demodulator.cpp
	src/m17rx.cpp
)

# Add tests target
add_custom_target(tests)

# Declare executables
add_executable(M17Netd ${m17netd_src} ${common_src})
add_executable(test_tone EXCLUDE_FROM_ALL src/test_tone.cpp src/sx1255.cpp src/spi.cpp)
add_executable(test_demod EXCLUDE_FROM_ALL src/test_demod.cpp src/M17Demodulator.cpp src/m17rx.cpp)
add_executable(test_acq EXCLUDE_FROM_ALL src/test_acq.cpp src/sdrnode.cpp src/sx1255.cpp src/spi.cpp)
add_executable(test_bert_rx EXCLUDE_FROM_ALL src/test_bert_rx.cpp src/sdrnode.cpp src/sx1255.cpp src/spi.cpp src/M17Demodulator.cpp src/m17rx.cpp)
add_executable(test_bert_tx EXCLUDE_FROM_ALL src/test_bert_tx.cpp src/sdrnode.cpp src/sx1255.cpp src/spi.cpp src/m17tx.cpp)
add_executable(test_bert_encode_decode EXCLUDE_FROM_ALL src/test_bert_encode_decode.cpp)
add_dependencies(tests test_tone test_demod test_acq test_bert_rx test_bert_tx test_bert_encode_decode)

# Include directories
target_include_directories(M17Netd PUBLIC
				${PROJECT_SOURCE_DIR}/inc
				${tomlplusplus_SOURCE_DIR}
				PkgConfig::FFTW
				)

target_include_directories(test_demod PUBLIC
				${PROJECT_SOURCE_DIR}/inc)

target_include_directories(test_tone PUBLIC
				${PROJECT_SOURCE_DIR}/inc)

target_include_directories(test_acq PUBLIC
				${PROJECT_SOURCE_DIR}/inc)

target_include_directories(test_bert_rx PUBLIC
				${PROJECT_SOURCE_DIR}/inc)

target_include_directories(test_bert_tx PUBLIC
				${PROJECT_SOURCE_DIR}/inc)

target_include_directories(test_bert_encode_decode PUBLIC
				${PROJECT_SOURCE_DIR}/inc)			


# Link libraries
target_link_libraries(M17Netd PUBLIC
				m17-static
				Threads::Threads
				${liquid_LIB}
				${ALSA_LIBRARIES}
				PkgConfig::FFTW
				)

target_link_libraries(test_demod PUBLIC
				m17-static
				${liquid_LIB}
				)

target_link_libraries(test_tone PUBLIC
				m17-static
				${ALSA_LIBRARIES}
				)

target_link_libraries(test_acq PUBLIC
				${ALSA_LIBRARIES}
				)

target_link_libraries(test_bert_rx PUBLIC
				m17-static
				${liquid_LIB}
				${ALSA_LIBRARIES}
)

target_link_libraries(test_bert_tx PUBLIC
				m17-static
				${liquid_LIB}
				${ALSA_LIBRARIES}
)

target_link_libraries(test_bert_encode_decode PUBLIC
				m17-static
)

# Comilation options
target_compile_options(M17Netd PRIVATE
-Wall
$<$<CONFIG:Debug>:
	-g
>
$<$<CONFIG:Release>:
	-O2
>
)

target_compile_options(test_tone PRIVATE
-Wall
$<$<CONFIG:Debug>:
	-g
>
$<$<CONFIG:Release>:
	-O2
>
)

target_compile_options(test_demod PRIVATE
-Wall
$<$<CONFIG:Debug>:
	-g
>
$<$<CONFIG:Release>:
	-O2
>
)

target_compile_options(test_bert_rx PRIVATE
-Wall
$<$<CONFIG:Debug>:
	-g
>
$<$<CONFIG:Release>:
	-O2
>
)

target_compile_options(test_bert_tx PRIVATE
-Wall
$<$<CONFIG:Debug>:
	-g
>
$<$<CONFIG:Release>:
	-O2
>
)

target_compile_options(test_bert_encode_decode PRIVATE
-Wall
$<$<CONFIG:Debug>:
	-g
>
$<$<CONFIG:Release>:
	-O2
>
)