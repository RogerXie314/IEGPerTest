cmake_minimum_required(VERSION 3.20)
project(NativeRunner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# zlib (pre-built static lib from IEG_Code, 与 NativeEngine 共用同一份)
set(ZLIB_ROOT    "${CMAKE_SOURCE_DIR}/../../external/IEG_Code/code")
set(ZLIB_INCLUDE "${ZLIB_ROOT}/include/zlib")
set(ZLIB_LIB     "${ZLIB_ROOT}/lib/x64/zlibstat.lib")

add_executable(NativeRunner
    main.cpp
)

target_include_directories(NativeRunner PRIVATE ${ZLIB_INCLUDE})
target_link_libraries(NativeRunner PRIVATE ws2_32 "${ZLIB_LIB}")
target_compile_definitions(NativeRunner PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    ZLIB_WINAPI
)
