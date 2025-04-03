#获取当前目录下的所有源文件项目
file(GLOB THREAD_FILE ${CMAKE_CURRENT_SOURCE_DIR}/*cc)

# 创建共享库
add_library(thread_lib SHARED ${THREAD_FILE})