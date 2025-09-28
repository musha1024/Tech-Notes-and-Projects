cmake_minimum_required(VERSION 3.14)
project(CppChatRoom LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


add_compile_options(-Wall -Wextra -Wpedantic -O2)


# 通用头文件
include_directories(${CMAKE_SOURCE_DIR}/src)


# 服务端
add_executable(chat_server
src/server/main.cpp
src/server/ChatServer.cpp
)


# 客户端
add_executable(chat_client
src/client/main.cpp
src/client/ChatClient.cpp
)


# 链接 pthread（客户端用到 std::thread）、rt 在部分系统可能不需要
find_package(Threads REQUIRED)
target_link_libraries(chat_client PRIVATE Threads::Threads)