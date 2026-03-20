cmake_minimum_required(VERSION 3.15)
project(NativeSender LANGUAGES C CXX)

# 输出 x64 DLL
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

add_library(NativeSender SHARED
    NativeSender.cpp
    NativeSender.h
)

target_link_libraries(NativeSender PRIVATE ws2_32)

# 输出到 build/Release，由 dotnet publish 打包进单文件 EXE
set_target_properties(NativeSender PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build/Release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/build/Debug"
)
