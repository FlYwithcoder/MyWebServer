#顶层Cmake，设置全局链接库以及添加子目录，和项目的头文件.h位置
#设置最低版本和项目名称
cmake_minimum_required(VERSION 3.0)
project(kama-webserver)

#设置全局的c++标准
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED True)

#设置头文件目录，供所有子项目使用
include_directories(${CMAKE_SOURCE_DIR}/include/eventloop)
include_directories(${CMAKE_SOURCE_DIR}/include/log)
include_directories(${CMAKE_SOURCE_DIR}/include/memory)
include_directories(${CMAKE_SOURCE_DIR}/include/net)
include_directories(${CMAKE_SOURCE_DIR}/include/thread)
include_directories(${CMAKE_SOURCE_DIR}/include/utils)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

#设置全局链接库
set(LIBS
    pthread 
)

#添加子目录使用
add_subdirectory(src)

