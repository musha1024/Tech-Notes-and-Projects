cmake_minimum_required(VERSION 3.16)
project(cpp_kv_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(WITH_REDIS "Enable Redis cache" ON)

find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# vcpkg port name: unofficial-mysql-connector-cpp
find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)

if (WITH_REDIS)
  find_package(redis++ CONFIG REQUIRED)
endif()

add_executable(cpp_kv_service
  src/main.cpp
  src/app/config.cpp
  src/db/mysql_pool.cpp
  src/db/redis_client.cpp
  src/model/user_dao.cpp
)

target_include_directories(cpp_kv_service PRIVATE include)

target_link_libraries(cpp_kv_service PRIVATE
  httplib::httplib
  nlohmann_json::nlohmann_json
  unofficial::mysql-connector-cpp::mysql-connector-cpp
)

if (WITH_REDIS)
  target_compile_definitions(cpp_kv_service PRIVATE USE_REDIS)
  target_link_libraries(cpp_kv_service PRIVATE redis++::redis++)
endif()

if (WIN32)
  target_link_libraries(cpp_kv_service PRIVATE ws2_32)
endif()
