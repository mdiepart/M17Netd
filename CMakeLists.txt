cmake_minimum_required(VERSION 3.16)

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

include_directories(inc)
add_library(sx1255 OBJECT src/sx1255.cpp)
add_library(sdrnode OBJECT src/sdrnode.cpp)
add_library(spi OBJECT src/spi.cpp)
add_library(m17rx OBJECT src/m17rx.cpp)
target_link_libraries(m17rx PUBLIC m17-static)
add_library(m17tx OBJECT src/m17tx.cpp)
target_link_libraries(m17tx PUBLIC m17-static)
add_library(M17Demodulator OBJECT src/M17Demodulator.cpp)
target_link_libraries(M17Demodulator PUBLIC m17-static)

# Define source files
set(m17netd_src
    src/m17netd.cpp
	src/config.cpp
	src/tuntap.cpp
	src/tun_threads.cpp
	src/radio_thread.cpp
	src/m17tx_thread.cpp
	$<TARGET_OBJECTS:sx1255>
	$<TARGET_OBJECTS:sdrnode>
	$<TARGET_OBJECTS:spi>
	$<TARGET_OBJECTS:m17rx>
	$<TARGET_OBJECTS:m17tx>
	$<TARGET_OBJECTS:M17Demodulator>
)

# Add tests target
add_custom_target(tests)

# Declare executables
add_executable(M17Netd ${m17netd_src} )
target_precompile_headers(M17Netd PRIVATE ${tomlplusplus_SOURCE_DIR}/toml.hpp)
target_include_directories(M17Netd PUBLIC
				${PROJECT_SOURCE_DIR}/inc
				${tomlplusplus_SOURCE_DIR}
				PkgConfig::FFTW
				)
target_link_libraries(M17Netd PUBLIC
				m17-static
				Threads::Threads
				${liquid_LIB}
				${ALSA_LIBRARIES}
				PkgConfig::FFTW
				)

## Tests binaries
# Generate tone
add_executable(test_tone EXCLUDE_FROM_ALL src/test_tone.cpp $<TARGET_OBJECTS:sx1255> $<TARGET_OBJECTS:spi>)
target_link_libraries(test_tone
	PRIVATE m17-static ${ALSA_LIBRARIES})

# Switch sdrnode to TX, without sending samples
add_executable(test_tx EXCLUDE_FROM_ALL src/test_tx.cpp $<TARGET_OBJECTS:sx1255> $<TARGET_OBJECTS:spi>)

# Test the demodulator
add_executable(test_demod EXCLUDE_FROM_ALL src/test_demod.cpp $<TARGET_OBJECTS:M17Demodulator> $<TARGET_OBJECTS:m17rx>)
target_link_libraries(test_demod
	PRIVATE m17-static ${liquid_LIB})

# Perform acquisition from sdrnode
add_executable(test_acq EXCLUDE_FROM_ALL src/test_acq.cpp $<TARGET_OBJECTS:sdrnode> $<TARGET_OBJECTS:sx1255> $<TARGET_OBJECTS:spi>)
target_link_libraries(test_acq
	PRIVATE ${ALSA_LIBRARIES})

# Filters an acquisition
add_executable(test_filter EXCLUDE_FROM_ALL src/test_filter.cpp)
target_link_libraries(test_filter
	PRIVATE ${liquid_LIB})

# Receives and decodes an M17 BERT signal from an SDRNode
add_executable(test_bert_rx EXCLUDE_FROM_ALL src/test_bert_rx.cpp $<TARGET_OBJECTS:sdrnode> $<TARGET_OBJECTS:sx1255> $<TARGET_OBJECTS:spi> $<TARGET_OBJECTS:M17Demodulator> $<TARGET_OBJECTS:m17rx>)
target_link_libraries(test_bert_rx
	PRIVATE m17-static ${liquid_LIB} ${ALSA_LIBRARIES})

# Receives and decodes an M17 BERT signal from an acquisition file
add_executable(test_bert_rx_file EXCLUDE_FROM_ALL src/test_bert_rx_file.cpp $<TARGET_OBJECTS:M17Demodulator> $<TARGET_OBJECTS:m17rx>)
target_link_libraries(test_bert_rx_file
	PRIVATE m17-static ${liquid_LIB})

# Transmits an M17 BERT signal
add_executable(test_bert_tx EXCLUDE_FROM_ALL src/test_bert_tx.cpp $<TARGET_OBJECTS:sdrnode> $<TARGET_OBJECTS:sx1255> $<TARGET_OBJECTS:spi> $<TARGET_OBJECTS:m17tx>)
target_link_libraries(test_bert_tx
	PRIVATE m17-static ${liquid_LIB} ${ALSA_LIBRARIES})

# Tests the M17 BERT implementation
add_executable(test_bert_encode_decode EXCLUDE_FROM_ALL src/test_bert_encode_decode.cpp)
target_link_libraries(test_bert_encode_decode
	PRIVATE m17-static)

add_dependencies(tests test_tone test_tx test_demod test_acq test_filter test_bert_rx test_bert_rx_file test_bert_tx test_bert_encode_decode)

# Comilation options
add_compile_options(
-Wall
$<$<CONFIG:Debug>:
	-g
>
$<$<CONFIG:Release>:
	-O2
>
)
