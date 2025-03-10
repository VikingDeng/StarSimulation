cmake_minimum_required(VERSION 3.10)
project(StarSimulation)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 手动查找 hiredis 的头文件和库
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIB NAMES hiredis)

if(NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIB)
    message(FATAL_ERROR "hiredis 未找到！请确保已安装 libhiredis-dev 开发包。")
endif()

# 2. 编译子目录（data/)
add_subdirectory(data)

# 3. 创建主程序并指定依赖关系
add_executable(main main.cpp)
target_include_directories(main PRIVATE ${HIREDIS_INCLUDE_DIR})
target_link_libraries(main PRIVATE dataset ${HIREDIS_LIB})
