cmake_minimum_required(VERSION 3.10)
project(raspi-cefore-gateway)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)
find_library(CEFORE_LIB cefore REQUIRED)
find_path(CEFORE_INCLUDE cefore/cef_frame.h REQUIRED)

# Include directories
include_directories(
    ${CEFORE_INCLUDE}
    ${CMAKE_SOURCE_DIR}/include
)

# Source files
set(SOURCES
    src/main.cpp
    src/uart_receiver.cpp
    src/packet_parser.cpp
    src/cefore_interface.cpp
    src/name_mapper.cpp
    src/gateway_fib.cpp
    src/main_controller.cpp
    include/third_party/base64.cpp
)

# Executable
add_executable(gateway ${SOURCES})

# Link libraries
target_link_libraries(gateway
    ${CEFORE_LIB}
    Threads::Threads
)

# Install target
install(TARGETS gateway DESTINATION bin)
