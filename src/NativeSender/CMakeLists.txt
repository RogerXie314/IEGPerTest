cmake_minimum_required(VERSION 3.15)
project(NativeSender LANGUAGES C CXX)

# 输出 x64 DLL
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

add_library(NativeSender SHARED
    NativeSender.cpp
    NativeSender.h
)

target_link_libraries(NativeSender PRIVATE ws2_32)

# 和 SimulatorApp 发布到同一个 artifacts 目录
set_target_properties(NativeSender PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../../artifacts/SimulatorAppPublish"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/../../artifacts/SimulatorAppPublish"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/../../artifacts/SimulatorAppPublish"
)
