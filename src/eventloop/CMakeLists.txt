#获取当前目录下的所有源文件项目
file(GLOB LEVENTLOOP_FILE ${CMAKE_CURRENT_SOURCE_DIR}/*cc)

#创建静态库或共享库
add_library(eventloop_lib SHARED ${LEVENTLOOP_FILE})