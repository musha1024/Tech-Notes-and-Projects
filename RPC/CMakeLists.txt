cmake_minimum_required(VERSION 3.15)
project(tiny_rpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic -O2)

# 头文件目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 核心库
add_library(tiny_rpc
    src/value.cpp
    src/protocol.cpp
    src/frame.cpp
    src/net.cpp
    src/server.cpp
    src/client.cpp
)

# 可执行程序：服务端与客户端分离
add_executable(tiny_rpc_server apps/server_main.cpp)
target_link_libraries(tiny_rpc_server PRIVATE tiny_rpc)

add_executable(tiny_rpc_client apps/client_main.cpp)
target_link_libraries(tiny_rpc_client PRIVATE tiny_rpc)
