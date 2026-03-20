cmake_minimum_required(VERSION 3.20)
project(NativeEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# zlib (pre-built static lib from IEG_Code)
set(ZLIB_ROOT "${CMAKE_SOURCE_DIR}/../../external/IEG_Code/code")
set(ZLIB_INCLUDE "${ZLIB_ROOT}/include/zlib")
set(ZLIB_LIB     "${ZLIB_ROOT}/lib/x64/zlibstat.lib")

add_library(NativeEngine SHARED
    native_engine.h
    native_engine.cpp
)

target_include_directories(NativeEngine PRIVATE ${ZLIB_INCLUDE})
target_link_libraries(NativeEngine PRIVATE ws2_32 "${ZLIB_LIB}")
target_compile_definitions(NativeEngine PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN ZLIB_WINAPI)
