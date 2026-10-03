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
    cpp/src/EchoBuffer.cpp
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

include(CTest)
if(BUILD_TESTING)
    add_executable(hpsdr-codec-tests tests/PacketCodecTests.cpp cpp/src/PacketCodec.cpp)
    target_include_directories(hpsdr-codec-tests PRIVATE cpp/include)
    if(MSVC)
        target_compile_options(hpsdr-codec-tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(hpsdr-codec-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME hpsdr-codec-tests COMMAND hpsdr-codec-tests)

    add_executable(hpsdr-p1-integration-tests
        tests/Protocol1IntegrationTests.cpp
        cpp/src/Protocol1Server.cpp
        cpp/src/RadioState.cpp
        cpp/src/PacketCodec.cpp
        cpp/src/SignalGenerator.cpp
        cpp/src/EchoBuffer.cpp
    )
    target_include_directories(hpsdr-p1-integration-tests PRIVATE cpp/include)
    if(WIN32)
        target_link_libraries(hpsdr-p1-integration-tests PRIVATE ws2_32)
    endif()
    if(MSVC)
        target_compile_options(hpsdr-p1-integration-tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(hpsdr-p1-integration-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME hpsdr-p1-integration-tests COMMAND hpsdr-p1-integration-tests)
endif()
