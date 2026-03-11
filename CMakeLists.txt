cmake_minimum_required(VERSION 3.10)
project(raspi-cefore-gateway)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 必要なパッケージを検索
find_package(Threads REQUIRED)

# CEFOREのパス設定
# オプション: cmake -DCEFORE_ROOT=/path/to/cefore で指定可能
if(NOT DEFINED CEFORE_ROOT)
    set(CEFORE_ROOT "/usr/local")
endif()

message(STATUS "CEFORE_ROOT: ${CEFORE_ROOT}")

# インクルードパスとライブラリパスを設定
set(CEFORE_INCLUDE "${CEFORE_ROOT}/include")
set(CEFORE_LIB_DIR "${CEFORE_ROOT}/lib")

# CEFOREライブラリを検索
find_library(CEFORE_LIB
    NAMES cefore
    PATHS ${CEFORE_LIB_DIR}
    PATH_SUFFIXES lib
)

if(NOT CEFORE_LIB)
    message(FATAL_ERROR "CEFORE library not found. Please install CEFORE or set CEFORE_ROOT.")
endif()

message(STATUS "CEFORE_INCLUDE: ${CEFORE_INCLUDE}")
message(STATUS "CEFORE_LIB: ${CEFORE_LIB}")

# インクルードディレクトリ
include_directories(
    ${CEFORE_INCLUDE}
    ${CMAKE_SOURCE_DIR}/include
)

# ソースファイル
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

# 実行ファイル
add_executable(gateway ${SOURCES})

# リンクライブラリ
# 静的ライブラリは依存ライブラリの前に配置
target_link_libraries(gateway
    -Wl,--start-group
    ${CEFORE_LIB}
    ssl
    crypto
    pthread
    dl
    -Wl,--end-group
)

# インストールターゲット
install(TARGETS gateway DESTINATION bin)
