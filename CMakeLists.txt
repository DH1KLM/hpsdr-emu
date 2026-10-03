cmake_minimum_required(VERSION 3.20)
project(hpsdr_emu_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hpsdr-emu
    cpp/src/main.cpp
    cpp/src/RadioState.cpp
    cpp/src/PacketCodec.cpp
    cpp/src/SignalGenerator.cpp
    cpp/src/Protocol1Server.cpp
    cpp/src/Protocol2Server.cpp
)

target_include_directories(hpsdr-emu PRIVATE cpp/include)

if(WIN32)
    target_link_libraries(hpsdr-emu PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(hpsdr-emu PRIVATE /W4 /permissive-)
else()
    target_compile_options(hpsdr-emu PRIVATE -Wall -Wextra -Wpedantic)
endif()
