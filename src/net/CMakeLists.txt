#获取当前目录下的所有源文件项目
file(GLOB NET_FILE ${CMAKE_CURRENT_SOURCE_DIR}/*cc)

#创建静态库或共享库
add_library(net_lib SHARED ${NET_FILE})