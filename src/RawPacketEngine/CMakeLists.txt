cmake_minimum_required(VERSION 3.15)
project(RawPacketEngine LANGUAGES C CXX)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

# Npcap SDK 路径（优先使用环境变量，默认 C:/npcap-sdk）
if(DEFINED ENV{NPCAP_SDK_DIR})
    set(NPCAP_SDK_DIR "$ENV{NPCAP_SDK_DIR}")
else()
    set(NPCAP_SDK_DIR "C:/npcap-sdk")
endif()

add_library(RawPacketEngine SHARED
    raw_packet_engine.cpp
    raw_packet_engine.h
)

target_include_directories(RawPacketEngine PRIVATE
    "${NPCAP_SDK_DIR}/Include"
)

target_link_directories(RawPacketEngine PRIVATE
    "${NPCAP_SDK_DIR}/Lib/x64"
)

target_link_libraries(RawPacketEngine PRIVATE
    Packet
    wpcap
    ws2_32
    Iphlpapi
)

# 输出到 build/Release，由 dotnet publish 打包进单文件 EXE
set_target_properties(RawPacketEngine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build/Release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/build/Debug"
)
