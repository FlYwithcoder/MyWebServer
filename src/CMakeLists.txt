#获取当前目录下的所有源文件项目
file(GLOB SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/*cc)
list(REMOVE_ITEM SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/main.cc)

#创建可执行文件
add_executable(main  main.cc)
add_executable(memory_pool_test
	memory/memory_pool_test.cc
	memory/lfu_memory_pool.cc
)

#链接必要的库
target_link_libraries(main utils_lib thread_lib net_lib memory_lib log_lib eventloop_lib ${LIBS})

option(MEM_POOL_TEST "Run  MemPool Test or Not" OFF)
if(MEM_POOL_TEST)
	add_definitions("-DMEM_POOL_TEST")
	message(STATUS "Run  MemPool Test")
else()
	message(STATUS "Not Run  MemPool Test")
endif()

add_subdirectory(net)
add_subdirectory(eventloop)
add_subdirectory(log)
add_subdirectory(thread)
add_subdirectory(utils)
add_subdirectory(memory)
