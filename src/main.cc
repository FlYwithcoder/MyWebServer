#include <string>
#include <TcpServer.h>
#include <Logger.h>
#include <sys/stat.h>
#include <sstream>
#include "AsyncLogging.h"
#include "LFU.h"
#include "MemoryPool.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <chrono>
#include "HttpServer.h"

static const off_t kRollSize = 1*1024*1024;

AsyncLogging* g_asyncLog = NULL;
AsyncLogging * getAsyncLog(){
    return g_asyncLog;
}
 void asyncLog(const char* msg, int len)
{
    AsyncLogging* logging = getAsyncLog();
    if (logging)
    {
        logging->append(msg, len);
    }
}

// 定义静态成员变量
bool HttpRequest::useMemoryPool = false;
int main(int argc,char *argv[]) {
    //第一步启动日志，双缓冲异步写入磁盘.
    //创建一个文件夹
    const std::string LogDir="logs";
    mkdir(LogDir.c_str(),0755);
    //使用std::stringstream 构建日志文件夹
    std::ostringstream LogfilePath;
    LogfilePath << LogDir << "/" << ::basename(argv[0]); // 完整的日志文件路径
    AsyncLogging log(LogfilePath.str(), kRollSize);
    g_asyncLog = &log;
    Logger::setOutput(asyncLog); // 为Logger设置输出回调, 重新配接输出位置
    log.start(); // 开启日志后端线程
    
    //第二步启动内存池和LFU缓存

    // Pre-warm the memory pool
    const size_t PREWARM_BLOCKS = 1000;
    std::vector<void*> prewarmBlocks;
    for (size_t i = 0; i < PREWARM_BLOCKS; ++i) {
        prewarmBlocks.push_back(Kama_memoryPool::MemoryPool::allocate(128));
    }
    for (void* block : prewarmBlocks) {
        Kama_memoryPool::MemoryPool::deallocate(block, 128);
    }

#ifdef MEM_POOL_TEST
    // 测试内存池性能
    const int TEST_REQUESTS = 1000000;  // 增加测试次数以获得更明显的效果
    
    // 不使用内存池的测试
    HttpRequest::enableMemoryPool(false);
    auto start1 = std::chrono::high_resolution_clock::now();
    std::vector<HttpRequest*> requests1;
    requests1.reserve(TEST_REQUESTS);
    
    for (int i = 0; i < TEST_REQUESTS; ++i) {
        requests1.push_back(new HttpRequest());
    }
    for (auto req : requests1) {
        delete req;
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);
    
    // 使用内存池的测试
    HttpRequest::enableMemoryPool(true);
    auto start2 = std::chrono::high_resolution_clock::now();
    std::vector<HttpRequest*> requests2;
    requests2.reserve(TEST_REQUESTS);
    
    for (int i = 0; i < TEST_REQUESTS; ++i) {
        requests2.push_back(new HttpRequest());
    }
    for (auto req : requests2) {
        delete req;
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);
    
    std::cout << "Performance Test Results (with " << TEST_REQUESTS << " requests):" << std::endl;
    std::cout << "Without Memory Pool: " << duration1.count() << "ms" << std::endl;
    std::cout << "With Memory Pool: " << duration2.count() << "ms" << std::endl;
    std::cout << "Performance improvement: " 
              << (duration1.count() - duration2.count()) * 100.0 / duration1.count() 
              << "%" << std::endl;
#endif
        
    // 启用内存池用于实际服务
    HttpRequest::enableMemoryPool(true);

    // 初始化缓存
    const int CAPACITY = 5;  
    KamaCache::KLfuCache<int, std::string> lfu(CAPACITY);
    //第三步启动底层网络模块
    EventLoop loop;
    InetAddress addr(8080);
    HttpServer server(&loop, addr, "HttpServer"); // 改用 HttpServer
    server.start();
 // 主loop开始事件循环  epoll_wait阻塞 等待就绪事件(主loop只注册了监听套接字的fd，所以只会处理新连接事件)
    std::cout << "================================================Start Web Server================================================" << std::endl;
    loop.loop();
    std::cout << "================================================Stop Web Server=================================================" << std::endl;
    //结束日志打印
    log.stop();
}

