cmake_minimum_required(VERSION 3.12)
project(redis_mysql_microservice CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

# hiredis
find_library(HIREDIS_LIB hiredis)
if(NOT HIREDIS_LIB)
  message(FATAL_ERROR "hiredis not found. Install libhiredis-dev (Debian/Ubuntu).")
endif()

# mysqlclient (C API)
find_library(MYSQLCLIENT_LIB mysqlclient)
if(NOT MYSQLCLIENT_LIB)
  message(FATAL_ERROR "mysqlclient not found. Install libmysqlclient-dev (Debian/Ubuntu).")
endif()

find_path(MYSQL_INCLUDE_DIR mysql/mysql.h
  PATH_SUFFIXES include
  PATHS /usr/include /usr/local/include /usr/include/mysql /usr/local/include/mysql)
if(NOT MYSQL_INCLUDE_DIR)
  message(FATAL_ERROR "Could not find mysql/mysql.h. Install libmysqlclient-dev.")
endif()

include_directories(${MYSQL_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(redis_mysql_ms
    src/main.cpp
    include/utils.hpp
    include/redis_client.hpp
    include/mysql_pool.hpp
    include/dao.hpp
)

target_link_libraries(redis_mysql_ms
    ${HIREDIS_LIB}
    ${MYSQLCLIENT_LIB}
    Threads::Threads
)

add_custom_command(TARGET redis_mysql_ms POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Built redis_mysql_ms. Run with: ./redis_mysql_ms --help"
)