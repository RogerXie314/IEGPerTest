cmake_minimum_required(VERSION 3.20)
project(NativeEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(NativeEngine SHARED
    native_engine.h
    native_engine.cpp
)

target_link_libraries(NativeEngine PRIVATE ws2_32)
target_compile_definitions(NativeEngine PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)
