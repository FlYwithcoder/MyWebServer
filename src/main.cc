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
#include <thread>
#include <vector>
#include <random>
#include "ResourceCache.h"
#include <filesystem>


// namespace fs = std::filesystem;

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

int main(int argc, char* argv[]) {
    bool runResourceTest = false;
    bool runRealWorldTest = false;
    bool runHotDataTest = false; // 新增热点数据测试选项
    bool enableCache = true;
    std::vector<std::string> testFiles;
    int testIterations = 1000;

    // 处理命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--resource-test" || arg == "-r") {
            runResourceTest = true;
        } else if (arg == "--real-world-test" || arg == "-rw") {
            runRealWorldTest = true;
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                testFiles.push_back(argv[++i]);
            }
        } else if (arg == "--iterations" || arg == "-i") {
            if (i + 1 < argc) {
                try {
                    testIterations = std::stoi(argv[++i]);
                } catch (...) {
                    std::cerr << "无效的迭代次数: " << argv[i] << std::endl;
                }
            }
        } else if (arg == "--no-cache" || arg == "-n") {
            enableCache = false;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "用法: " << argv[0] << " [选项]" << std::endl;
            std::cout << "选项:" << std::endl;
            std::cout << "  -r, --resource-test        运行标准资源缓存测试" << std::endl;
            std::cout << "  -rw, --real-world-test     运行真实环境缓存测试，后面跟文件路径" << std::endl;
            std::cout << "  -i, --iterations <数值>    设置测试迭代次数" << std::endl;
            std::cout << "  -n, --no-cache            禁用LFU资源缓存" << std::endl;
            std::cout << "  -h, --help                显示帮助信息" << std::endl;
            std::cout << "示例:" << std::endl;
            std::cout << "  " << argv[0] << " -rw ./resources/test.html ./resources/large.jpg -i 2000" << std::endl;
            std::cout << "  " << argv[0] << " -hd" << std::endl;
            return 0;
        }
    }

    // 初始化日志
    const std::string LogDir = "logs";
    mkdir(LogDir.c_str(), 0755);
    std::ostringstream LogfilePath;
    LogfilePath << LogDir << "/" << ::basename(argv[0]);
    AsyncLogging log(LogfilePath.str(), kRollSize);
    g_asyncLog = &log;
    Logger::setOutput(asyncLog);
    log.start();

    // 初始化内存池
    const size_t PREWARM_BLOCKS = 1000;
    std::vector<void*> prewarmBlocks;
    for (size_t i = 0; i < PREWARM_BLOCKS; ++i) {
        prewarmBlocks.push_back(Kama_memoryPool::MemoryPool::allocate(128));
    }
    for (void* block : prewarmBlocks) {
        Kama_memoryPool::MemoryPool::deallocate(block, 128);
    }

#ifdef MEM_POOL_TEST
    int threadCount = 4;
    int testRequests = 1000000;
    testMemoryPoolPerformance(threadCount, testRequests);
#endif
        
    HttpRequest::enableMemoryPool(true);

    // 执行测试
    if (runResourceTest) {
        testResourceCachePerformance();
        return 0;
    }
    
    if (runRealWorldTest) {
        if (testFiles.empty()) {
            std::string testDir = std::filesystem::current_path().string() + "/static";
            mkdir(testDir.c_str(), 0755);
            
            std::vector<std::pair<std::string, size_t>> defaultFiles = {
                {"/test_small.dat", 100 * 1024},
                {"/test_medium.dat", 1 * 1024 * 1024},
                {"/test_large.dat", 10 * 1024 * 1024},
            };
            
            for (const auto& file : defaultFiles) {
                std::string path = testDir + file.first;
                std::ifstream check(path);
                if (!check) {
                    std::cout << "创建测试文件: " << path << " (" << (file.second / 1024) << " KB)" << std::endl;
                    std::ofstream create(path, std::ios::binary);
                    std::string content(file.second, 'X');
                    create << content;
                }
                testFiles.push_back(path);
            }
        }
        
        testRealWorldCachePerformance(testFiles, testIterations);
        return 0;
    }

    // 启动HTTP服务器
    EventLoop loop;
    InetAddress addr(8080);
    HttpServer server(&loop, addr, "HttpServer");
    server.enableResourceCache(enableCache);
    
    if (enableCache) {
        std::cout << "LFU资源缓存已启用. 访问 http://localhost:8080/cache-stats 查看缓存统计" << std::endl;
        std::cout << "访问 http://localhost:8080/toggle-cache 可以动态切换缓存状态" << std::endl;
    } else {
        std::cout << "LFU资源缓存已禁用. 访问 http://localhost:8080/toggle-cache 可以动态启用缓存" << std::endl;
    }
    
    server.start();
    std::cout << "================================================Start Web Server================================================" << std::endl;
    loop.loop();
    std::cout << "================================================Stop Web Server=================================================" << std::endl;
    log.stop();
    return 0;
}