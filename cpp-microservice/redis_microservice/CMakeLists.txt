cmake_minimum_required(VERSION 3.12)
project(redis_microservice CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)

if (ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-fsanitize=address,undefined)
  add_link_options(-fsanitize=address,undefined)
endif()

add_executable(redis_microservice
  src/main.cpp
)

# For Linux/macOS: add pthread automatically
if(UNIX)
  target_link_libraries(redis_microservice PRIVATE pthread)
endif()

target_include_directories(redis_microservice PRIVATE ${CMAKE_SOURCE_DIR}/include)
