#include <string>
#include <TcpServer.h>
#include <Logger.h>
#include <sys/stat.h>
#include <sstream>
#include "AsyncLogging.h"
#include "LFU.h"
#include "memoryPool.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
// 日志文件滚动大小为1MB (1*1024*1024字节)
static const off_t kRollSize = 1*1024*1024;

// 静态文件根目录
const std::string STATIC_ROOT = "./resources/";

// 检查字符串是否以指定后缀结尾
bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

class HttpRequest {
public:
    enum Method { GET, POST, PUT, DELETE, UNKNOWN };
    
    bool parse(const std::string& request) {
        std::istringstream stream(request);
        std::string methodStr;
        stream >> methodStr >> path_ >> version_;

        // 解析请求方法
        if (methodStr == "GET") method_ = GET;
        else if (methodStr == "POST") method_ = POST;
        else if (methodStr == "PUT") method_ = PUT;
        else if (methodStr == "DELETE") method_ = DELETE;
        else method_ = UNKNOWN;

        // 解析请求头
        std::string line;
        std::getline(stream, line); // 读取第一行剩余部分
        while (std::getline(stream, line) && line != "\r") {
            if (line.empty()) break;
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 2); // 跳过": "
                if (!value.empty() && value.back() == '\r') {
                    value.pop_back();
                }
                headers_[key] = value;
            }
        }

        // 解析请求体
        if (method_ == POST || method_ == PUT) {
            std::string body;
            while (std::getline(stream, line)) {
                body += line + "\n";
            }
            body_ = body;
        }

        return true;
    }

    Method getMethod() const { return method_; }
    const std::string& getPath() const { return path_; }
    const std::string& getVersion() const { return version_; }
    const std::string& getBody() const { return body_; }
    const std::unordered_map<std::string, std::string>& getHeaders() const { return headers_; }

private:
    Method method_;
    std::string path_;
    std::string version_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};

class HttpServer {
public:
    HttpServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : server_(loop, addr, name)
        , loop_(loop)
    {
        server_.setConnectionCallback(
            std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
        
        server_.setMessageCallback(
            std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        server_.setThreadNum(3);

        // 确保静态文件目录存在
        mkdir(STATIC_ROOT.c_str(), 0755);
    }

    void start() {
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr &conn) {
        if (conn->connected()) {
            LOG_INFO<<"HTTP Connection UP :"<<conn->peerAddress().toIpPort().c_str();
        } else {
            LOG_INFO<<"HTTP Connection DOWN :"<<conn->peerAddress().toIpPort().c_str();
        }
    }

    void sendError(const TcpConnectionPtr &conn, int code, const std::string& message) {
        std::string response = "HTTP/1.1 " + std::to_string(code) + " " + message + "\r\n";
        response += "Content-Type: text/html\r\n";
        response += "Connection: close\r\n";
        response += "\r\n";
        response += "<html><body><h1>" + std::to_string(code) + " " + message + "</h1></body></html>";
        conn->send(response);
    }

    std::string getContentType(const std::string& path) {
        if (endsWith(path, ".html")) return "text/html";
        if (endsWith(path, ".css")) return "text/css";
        if (endsWith(path, ".js")) return "application/javascript";
        if (endsWith(path, ".jpg") || endsWith(path, ".jpeg")) return "image/jpeg";
        if (endsWith(path, ".png")) return "image/png";
        return "text/plain";
    }

    void serveFile(const TcpConnectionPtr &conn, const std::string& path) {
        std::string fullPath = STATIC_ROOT + path;
        std::ifstream file(fullPath, std::ios::binary);
        
        if (!file) {
            sendError(conn, 404, "Not Found");
            return;
        }

        // 获取文件大小
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        // 读取文件内容
        std::string content(size, '\0');
        file.read(&content[0], size);

        // 发送响应
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: " + getContentType(path) + "\r\n";
        response += "Content-Length: " + std::to_string(size) + "\r\n";
        response += "Connection: keep-alive\r\n";
        response += "\r\n";
        response += content;

        conn->send(response);
    }

    void handleRequest(const TcpConnectionPtr &conn, const HttpRequest& req) {
        // 安全检查：防止目录遍历攻击
        if (req.getPath().find("..") != std::string::npos) {
            sendError(conn, 403, "Forbidden");
            return;
        }

        switch (req.getMethod()) {
            case HttpRequest::GET:
                if (req.getPath() == "/") {
                    serveFile(conn, "/index.html");
                } else {
                    serveFile(conn, req.getPath());
                }
                break;

            case HttpRequest::POST:
                LOG_INFO << "Received POST request with body: " << req.getBody();
                // 这里添加具体的 POST 处理逻辑
                break;

            case HttpRequest::PUT:
                LOG_INFO << "Received PUT request with body: " << req.getBody();
                // 这里添加具体的 PUT 处理逻辑
                break;

            case HttpRequest::DELETE:
                LOG_INFO << "Received DELETE request for path: " << req.getPath();
                // 这里添加具体的 DELETE 处理逻辑
                break;

            default:
                sendError(conn, 405, "Method Not Allowed");
                break;
        }
    }

    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
        std::string request = buf->retrieveAllAsString();
        
        HttpRequest req;
        if (!req.parse(request)) {
            sendError(conn, 400, "Bad Request");
            return;
        }

        handleRequest(conn, req);
    }

    TcpServer server_;
    EventLoop *loop_;
};

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
     // 初始化内存池
    memoryPool::HashBucket::initMemoryPool();

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

