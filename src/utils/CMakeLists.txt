#获取当前目录下的所有源文件项目
file(GLOB UTILS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/*cc)

# 创建共享库
add_library(utils_lib SHARED ${UTILS_FILE})
