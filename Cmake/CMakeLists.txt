# 要求最低 CMake 版本
cmake_minimum_required(VERSION 3.12)

# 项目信息
project(MyProject LANGUAGES CXX VERSION 0.1.0)

# C++ 标准 (Google 推荐 >= C++14，这里设为 C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # 禁用 GNU 扩展，保持标准 C++

# 开启编译警告并视为错误（符合 Google 标准要求严格检查）
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# 开启 clang-tidy（静态检查）
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--warnings-as-errors=*")
endif()

# 包含头文件目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 源文件
file(GLOB SOURCES src/*.cpp)

# 生成可执行文件
add_executable(my_app ${SOURCES})
