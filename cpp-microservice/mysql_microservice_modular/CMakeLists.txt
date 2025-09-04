cmake_minimum_required(VERSION 3.16)
project(mysql_microservice_modular LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- MySQL C API ----
find_path(MYSQL_INCLUDE_DIR mysql.h
  PATH_SUFFIXES mysql mysql/mysql include/mysql include
  PATHS /usr/include /usr/include/mysql /usr/local/include /usr/local/include/mysql /opt/homebrew/include
)
find_library(MYSQL_LIBRARY
  NAMES mysqlclient libmysql
  PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib /opt/homebrew/lib
)
if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
  message(FATAL_ERROR "未找到 MySQL client 开发库，请安装 libmysqlclient-dev")
endif()

add_executable(mysql_microservice
  src/main.cpp
  src/config.cpp
  src/mysql_pool.cpp
  src/handlers.cpp
)

# 这里加入项目内置头文件和你自己的 include/
target_include_directories(mysql_microservice
  PRIVATE
    ${MYSQL_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party        # <- 本地 vendor 的头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/include           # <- 你的 public headers
)

target_link_libraries(mysql_microservice
  PRIVATE
    ${MYSQL_LIBRARY}
)

find_package(Threads)
if(Threads_FOUND)
  target_link_libraries(mysql_microservice PRIVATE Threads::Threads)
endif()
if(WIN32)
  target_link_libraries(mysql_microservice PRIVATE ws2_32)
endif()
if(APPLE)
  set_target_properties(mysql_microservice PROPERTIES
    BUILD_RPATH "@executable_path/../lib;@loader_path/../lib"
  )
endif()
