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

const std::string STATIC_ROOT = "./resources/";

bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

class HttpRequest {
public:
    enum Method { GET, POST, PUT, DELETE, UNKNOWN };

    // Overload new and delete operators to use the memory pool
    void* operator new(size_t size) {
        return Kama_memoryPool::MemoryPool::allocate(size);
    }

    void operator delete(void* ptr, size_t size) {
        Kama_memoryPool::MemoryPool::deallocate(ptr, size);
    }

    // 添加静态方法来控制是否使用内存池
    static void enableMemoryPool(bool enable) {
        useMemoryPool = enable;
    }

    static bool isMemoryPoolEnabled() {
        return useMemoryPool;
    }

    // 构造函数和析构函数
    HttpRequest() = default;
    ~HttpRequest() = default;

    bool parse(const std::string& request) {
        std::istringstream stream(request);
        std::string methodStr;
        stream >> methodStr >> path_ >> version_;

        if (methodStr == "GET") method_ = GET;
        else if (methodStr == "POST") method_ = POST;
        else if (methodStr == "PUT") method_ = PUT;
        else if (methodStr == "DELETE") method_ = DELETE;
        else method_ = UNKNOWN;

        std::string line;
        std::getline(stream, line);
        while (std::getline(stream, line) && line != "\r") {
            if (line.empty()) break;
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 2);
                if (!value.empty() && value.back() == '\r') {
                    value.pop_back();
                }
                headers_[key] = value;
            }
        }

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
    const std::string getHeader(const std::string& name) const {
        auto it = headers_.find(name);
        return it != headers_.end() ? it->second : "";
    }

private:
    static bool useMemoryPool;  // 控制是否使用内存池的标志
    Method method_;
    std::string path_;
    std::string version_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};

class HttpServer {
    public:
        HttpServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
            : server_(loop, addr, name), loop_(loop) {
            server_.setConnectionCallback(
                std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
            server_.setMessageCallback(
                std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            server_.setThreadNum(3);
            mkdir(STATIC_ROOT.c_str(), 0755);
        }
    
        void start() { server_.start(); }
    
    private:
        void onConnection(const TcpConnectionPtr &conn) {
            if (conn->connected()) {
                LOG_INFO << "HTTP Connection UP: " << conn->peerAddress().toIpPort();
            } else {
                LOG_INFO << "HTTP Connection DOWN: " << conn->peerAddress().toIpPort();
            }
        }
    
        void sendError(const TcpConnectionPtr &conn, const HttpRequest& req, int code, const std::string& message) {
            std::string response = "HTTP/1.1 " + std::to_string(code) + " " + message + "\r\n";
            response += "Content-Type: text/html\r\n";
            
            // 根据请求决定Connection头
            std::string connection = req.getHeader("Connection");
            bool close = shouldCloseConnection(req);
            response += close ? "Connection: close\r\n" : "Connection: keep-alive\r\n";
            response += "\r\n";
            response += "<html><body><h1>" + std::to_string(code) + " " + message + "</h1></body></html>";
            
            conn->send(response);
            if (close) conn->shutdown();
        }
    
        bool shouldCloseConnection(const HttpRequest& req) {
            const std::string& connection = req.getHeader("Connection");
            if (connection == "close") return true;
            if (req.getVersion() == "HTTP/1.0" && connection != "keep-alive") return true;
            return false;
        }
    
        std::string getContentType(const std::string& path) {
            if (endsWith(path, ".html")) return "text/html";
            if (endsWith(path, ".css")) return "text/css";
            if (endsWith(path, ".js")) return "application/javascript";
            if (endsWith(path, ".jpg") || endsWith(path, ".jpeg")) return "image/jpeg";
            if (endsWith(path, ".png")) return "image/png";
            return "text/plain";
        }
    
        void serveFile(const TcpConnectionPtr &conn, const HttpRequest& req, const std::string& path) {
            std::string fullPath = STATIC_ROOT + path;
            std::ifstream file(fullPath, std::ios::binary);
            
            if (!file) {
                sendError(conn, req, 404, "Not Found");
                return;
            }
    
            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);
    
            std::string content(size, '\0');
            file.read(&content[0], size);
    
            std::string response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: " + getContentType(path) + "\r\n";
            response += "Content-Length: " + std::to_string(size) + "\r\n";
            
            // 根据请求决定Connection头
            bool close = shouldCloseConnection(req);
            response += close ? "Connection: close\r\n" : "Connection: keep-alive\r\n";
            response += "\r\n";
            response += content;
    
            conn->send(response);
            if (close) conn->shutdown();
        }
    
        void handleRequest(const TcpConnectionPtr &conn, const HttpRequest& req) {
            if (req.getPath().find("..") != std::string::npos) {
                sendError(conn, req, 403, "Forbidden");
                return;
            }
    
            switch (req.getMethod()) {
                case HttpRequest::GET:
                    if (req.getPath() == "/") {
                        serveFile(conn, req, "/index.html");
                    } else {
                        serveFile(conn, req, req.getPath());
                    }
                    break;
    
                case HttpRequest::POST:
                    // POST处理逻辑...
                    sendError(conn, req, 501, "Not Implemented");
                    break;
    
                case HttpRequest::PUT:
                    // PUT处理逻辑...
                    sendError(conn, req, 501, "Not Implemented");
                    break;
    
                case HttpRequest::DELETE:
                    // DELETE处理逻辑...
                    sendError(conn, req, 501, "Not Implemented");
                    break;
    
                default:
                    sendError(conn, req, 405, "Method Not Allowed");
                    break;
            }
        }
    
        void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
            std::string request = buf->retrieveAllAsString();
            HttpRequest req;
            if (!req.parse(request)) {
                sendError(conn, req, 400, "Bad Request");
                return;
            }
            handleRequest(conn, req);
        }
    
        TcpServer server_;
        EventLoop *loop_;
    };

    
