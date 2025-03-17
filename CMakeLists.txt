cmake_minimum_required(VERSION 3.10)
project(StarSimulation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIB NAMES hiredis)

message(STATUS "HIREDIS_INCLUDE_DIR: ${HIREDIS_INCLUDE_DIR}")
message(STATUS "HIREDIS_LIB: ${HIREDIS_LIB}")

if(NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIB)
    message(FATAL_ERROR "hiredis 未找到！请确保已安装 libhiredis-dev 开发包。")
endif()

# 查找 OpenCV
find_package(OpenCV REQUIRED)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV 未找到！请确保已安装 OpenCV 并正确配置。")
else()
    message(STATUS "OpenCV 找到 (版本: ${OpenCV_VERSION})")
    include_directories(${OpenCV_INCLUDE_DIRS})
endif()

# 2. 编译子目录（data/)
add_subdirectory(data)
add_subdirectory(src)

# 3. 创建主程序并指定依赖关系
add_executable(main main.cpp ) # 假设你将绘图代码放在 star_map_drawer.cpp 中
target_include_directories(main PRIVATE ${HIREDIS_INCLUDE_DIR})
target_link_libraries(main PRIVATE dataset src ${HIREDIS_LIB} ${OpenCV_LIBS}) # 将 OpenCV_LIBS 添加到链接库