#pragma once

#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <sstream>
#include "LFU.h"
#include <filesystem>       


// 资源缓存类，用于缓存静态资源文件内容
class ResourceCache {
public:
    static ResourceCache& getInstance() {
        static ResourceCache instance;
        return instance;
    }

    // 获取缓存的资源
    bool getResource(const std::string& path, std::string& content, std::string& contentType) {
        if (!enabled_) return false;
        
        CacheItem item;
        bool found = cache_.get(path, item);
        if (found) {
            content = item.content;
            contentType = item.contentType;
            hit_count_++;
            resource_hit_times_[path]++;
        }
        total_requests_++;
        resource_request_times_[path]++;
        return found;
    }

    // 添加资源到缓存
    void addResource(const std::string& path, const std::string& content, const std::string& contentType) {
        if (!enabled_) return;
        
        CacheItem item;
        item.content = content;
        item.contentType = contentType;
        item.size = content.size();
        item.timestamp = std::chrono::system_clock::now();
        
        // 如果缓存接近容量上限，可以清理一些不常用的资源
        if (current_size_ + item.size > max_cache_size_ && cache_.getSize() > 0) {
            pruneCache();
        }
        
        // 添加到缓存
        cache_.put(path, item);
        current_size_ += item.size;
    }

    // 启用/禁用缓存
    void enable(bool enabled) {
        enabled_ = enabled;
        if (!enabled) {
            clearCache();
        }
    }
    
    // 设置缓存大小（以字节为单位）
    void setCacheSize(size_t bytes) {
        // 确保新的缓存大小不小于当前的使用量
        if (bytes < current_size_) {
            pruneCache();
        }
        max_cache_size_ = bytes;
    }
    
    // 设置缓存项目数上限
    void setCacheCapacity(size_t capacity) {
        // 由于KLfuCache不支持赋值操作，我们需要重新构建缓存
        // 先保存当前缓存中的所有内容
        cache_capacity_ = capacity;
        clearCache();  // 直接清空缓存，而不是创建新实例
    }

    // 清空缓存
    void clearCache() {
        cache_.purge();
        current_size_ = 0;
        hit_count_ = 0;
        total_requests_ = 0;
        resource_request_times_.clear();
        resource_hit_times_.clear();
    }

    // 获取缓存命中率
    double getHitRate() const {
        if (total_requests_ == 0) return 0.0;
        return static_cast<double>(hit_count_) / total_requests_;
    }

    // 获取缓存大小
    size_t getCacheSize() const {
        return current_size_;
    }

    // 获取缓存条目数
    size_t getCacheEntryCount() const {
        return cache_.getSize();
    }

    // 获取总请求数
    size_t getTotalRequests() const {
        return total_requests_;
    }

    // 获取命中次数
    size_t getHitCount() const {
        return hit_count_;
    }
    
    // 获取最大缓存大小
    size_t getMaxCacheSize() const {
        return max_cache_size_;
    }
    
    // 获取资源访问频率统计
    std::string getResourceStats() const {
        std::stringstream ss;
        ss << "资源访问统计:\n";
        
        // 按请求次数排序
        std::vector<std::pair<std::string, size_t>> resources;
        for (const auto& pair : resource_request_times_) {
            resources.push_back({pair.first, pair.second});
        }
        
        std::sort(resources.begin(), resources.end(), 
            [](const std::pair<std::string, size_t>& a, const std::pair<std::string, size_t>& b) { 
                return a.second > b.second; 
            });
        
        // 只显示前10个最频繁访问的资源
        const int maxDisplay = std::min(10, static_cast<int>(resources.size()));
        
        ss << "前" << maxDisplay << "个最常访问的资源:\n";
        for (int i = 0; i < maxDisplay; i++) {
            const auto& res = resources[i];
            // 使用find代替operator[]，这在const方法中是安全的
            size_t hits = 0;
            auto it = resource_hit_times_.find(res.first);
            if (it != resource_hit_times_.end()) {
                hits = it->second;
            }
            double hitRate = static_cast<double>(hits) / res.second * 100.0;
            
            ss << (i+1) << ". " << res.first << "\n";
            ss << "   请求次数: " << res.second << "\n";
            ss << "   命中次数: " << hits << "\n";
            ss << "   命中率: " << hitRate << "%\n";
        }
        
        return ss.str();
    }

private:
    // 缓存项结构
    struct CacheItem {
        std::string content;
        std::string contentType;
        size_t size;
        std::chrono::system_clock::time_point timestamp;
    };

    // 清理缓存中不常用的项目
    void pruneCache() {
        // LFU策略会自动移除最不常使用的项目
        while (current_size_ > max_cache_size_ * 0.8 && cache_.getSize() > 1) {
            // 这里不需要手动删除，LFU缓存会自动移除最不常使用的项目
            // 但我们需要更新当前使用的内存大小
            current_size_ = 0;
            // 重新计算当前缓存大小
            for (const auto& item : cache_.getItems()) {
                // 修复：正确访问Node中的value字段，再访问CacheItem的size
                current_size_ += item.second->value.size;
            }
        }
    }

    ResourceCache()
        : enabled_(false), max_cache_size_(100 * 1024 * 1024), // 默认100MB
          current_size_(0), hit_count_(0), total_requests_(0),
          cache_capacity_(1000), // 默认最大1000个缓存项
          cache_(1000)
    {}

    ResourceCache(const ResourceCache&) = delete;
    ResourceCache& operator=(const ResourceCache&) = delete;

    bool enabled_;
    size_t max_cache_size_;  // 最大缓存大小(字节)
    size_t current_size_;    // 当前缓存大小(字节)
    size_t hit_count_;       // 缓存命中次数
    size_t total_requests_;  // 总请求次数
    size_t cache_capacity_;  // 缓存容量（项目数）
    std::unordered_map<std::string, size_t> resource_request_times_; // 各资源请求次数统计
    std::unordered_map<std::string, size_t> resource_hit_times_;     // 各资源命中次数统计
    KamaCache::KLfuCache<std::string, CacheItem> cache_; // LFU缓存
}; 

// 测试资源缓存性能
void testResourceCachePerformance() {
    const int TEST_ITERATIONS = 5000;
    const int FILE_SIZE_KB = 100;
    const int NUM_FILES = 50;
    
    std::vector<std::string> highFreqResources;
    std::vector<std::string> lowFreqResources;
    
    std::cout << "===== 资源缓存性能测试 (改进版) =====" << std::endl;
    std::cout << "测试配置:" << std::endl;
    std::cout << "- 迭代次数: " << TEST_ITERATIONS << std::endl;
    std::cout << "- 文件数量: " << NUM_FILES << std::endl;
    std::cout << "- 文件大小: " << FILE_SIZE_KB << "KB" << std::endl;
    std::cout << "- 访问模式: 80/20法则 (20%文件贡献80%访问量)" << std::endl;
    
    std::string testDir = std::filesystem::current_path().string() + "/test_files";
    mkdir(testDir.c_str(), 0755);
    
    std::string dummyContent(FILE_SIZE_KB * 1024, 'X');
    
    for (int i = 0; i < NUM_FILES; i++) {
        std::string fileName = "/file_" + std::to_string(i) + ".dat";
        std::string fullPath = testDir + fileName;
        
        std::ofstream file(fullPath);
        if (file) {
            file << dummyContent;
            file.close();
            
            if (i < NUM_FILES * 0.2) {
                highFreqResources.push_back(fullPath);
            } else {
                lowFreqResources.push_back(fullPath);
            }
        }
    }
    
    ResourceCache::getInstance().enable(false);
    ResourceCache::getInstance().clearCache();
    
    auto startNoCache = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        if (i % 5 < 4) {
            int index = i % highFreqResources.size();
            std::string fullPath = highFreqResources[index];
            std::ifstream file(fullPath, std::ios::binary);
            if (file) {
                file.seekg(0, std::ios::end);
                size_t size = file.tellg();
                file.seekg(0, std::ios::beg);
                std::string content(size, '\0');
                file.read(&content[0], size);
            }
        } else {
            int index = i % lowFreqResources.size();
            std::string fullPath = lowFreqResources[index];
            std::ifstream file(fullPath, std::ios::binary);
            if (file) {
                file.seekg(0, std::ios::end);
                size_t size = file.tellg();
                file.seekg(0, std::ios::beg);
                std::string content(size, '\0');
                file.read(&content[0], size);
            }
        }
    }
    
    auto endNoCache = std::chrono::high_resolution_clock::now();
    auto durationNoCache = std::chrono::duration_cast<std::chrono::milliseconds>(endNoCache - startNoCache);
    
    ResourceCache::getInstance().enable(true);
    ResourceCache::getInstance().clearCache();
    
    auto startWithCache = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        if (i % 5 < 4) {
            int index = i % highFreqResources.size();
            std::string fullPath = highFreqResources[index];
            std::string content;
            std::string contentType;
            
            if (!ResourceCache::getInstance().getResource(fullPath, content, contentType)) {
                std::ifstream file(fullPath, std::ios::binary);
                if (file) {
                    file.seekg(0, std::ios::end);
                    size_t size = file.tellg();
                    file.seekg(0, std::ios::beg);
                    content.resize(size);
                    file.read(&content[0], size);
                    contentType = "application/octet-stream";
                    ResourceCache::getInstance().addResource(fullPath, content, contentType);
                }
            }
        } else {
            int index = i % lowFreqResources.size();
            std::string fullPath = lowFreqResources[index];
            std::string content;
            std::string contentType;
            
            if (!ResourceCache::getInstance().getResource(fullPath, content, contentType)) {
                std::ifstream file(fullPath, std::ios::binary);
                if (file) {
                    file.seekg(0, std::ios::end);
                    size_t size = file.tellg();
                    file.seekg(0, std::ios::beg);
                    content.resize(size);
                    file.read(&content[0], size);
                    contentType = "application/octet-stream";
                    ResourceCache::getInstance().addResource(fullPath, content, contentType);
                }
            }
        }
    }
    
    auto endWithCache = std::chrono::high_resolution_clock::now();
    auto durationWithCache = std::chrono::duration_cast<std::chrono::milliseconds>(endWithCache - startWithCache);
    
    std::cout << "\n测试结果:" << std::endl;
    std::cout << "未使用LFU缓存的耗时: " << durationNoCache.count() << "ms" << std::endl;
    std::cout << "使用LFU缓存的耗时: " << durationWithCache.count() << "ms" << std::endl;
    double improvement = (durationNoCache.count() - durationWithCache.count()) * 100.0 / durationNoCache.count();
    std::cout << "性能提升: " << improvement << "%" << std::endl;
    std::cout << "缓存命中率: " << (ResourceCache::getInstance().getHitRate() * 100) << "%" << std::endl;
    std::cout << "缓存大小: " << (ResourceCache::getInstance().getCacheSize() / 1024) << "KB" << std::endl;
    std::cout << "缓存项目数: " << ResourceCache::getInstance().getCacheEntryCount() << std::endl;
    std::cout << "===========================" << std::endl;
    
    for (const auto& path : highFreqResources) {
        remove(path.c_str());
    }
    for (const auto& path : lowFreqResources) {
        remove(path.c_str());
    }
    rmdir(testDir.c_str());
}

// 更真实的缓存性能测试（原有函数，保持不变）
void testRealWorldCachePerformance(const std::vector<std::string>& filePaths, int iterations) {
    std::cout << "===== 真实环境缓存性能测试 =====" << std::endl;
    std::cout << "测试配置:" << std::endl;
    std::cout << "- 迭代次数: " << iterations << std::endl;
    std::cout << "- 文件数量: " << filePaths.size() << std::endl;
    
    std::cout << "测试文件:" << std::endl;
    size_t totalSize = 0;
    for (const auto& path : filePaths) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (file) {
            size_t size = file.tellg();
            totalSize += size;
            std::cout << "- " << path << " (" << (size / 1024) << " KB)" << std::endl;
        } else {
            std::cout << "- " << path << " (无法访问)" << std::endl;
        }
    }
    std::cout << "总文件大小: " << (totalSize / 1024) << " KB" << std::endl;
    
    std::cout << "正在清除内存缓存..." << std::endl;
    const size_t FLUSH_SIZE = 500 * 1024 * 1024;
    char* bigBuffer = new char[FLUSH_SIZE];
    for (size_t i = 0; i < FLUSH_SIZE; i += 4096) {
        bigBuffer[i] = 1;
    }
    delete[] bigBuffer;
    
    ResourceCache::getInstance().enable(false);
    ResourceCache::getInstance().clearCache();
    
    auto startNoCache = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        std::string fullPath;
        if (i % 5 < 4) {
            int index = i % std::max(1, static_cast<int>(filePaths.size() * 0.2));
            fullPath = filePaths[index];
        } else {
            int index = std::max(1, static_cast<int>(filePaths.size() * 0.2)) + 
                       (i % std::max(1, static_cast<int>(filePaths.size() * 0.8)));
            if (index >= filePaths.size()) index = filePaths.size() - 1;
            fullPath = filePaths[index];
        }
        
        std::ifstream file(fullPath, std::ios::binary);
        if (file) {
            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::string content(size, '\0');
            file.read(&content[0], size);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }
    
    auto endNoCache = std::chrono::high_resolution_clock::now();
    auto durationNoCache = std::chrono::duration_cast<std::chrono::milliseconds>(endNoCache - startNoCache);
    
    ResourceCache::getInstance().enable(true);
    ResourceCache::getInstance().clearCache();
    
    auto startWithCache = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        std::string fullPath;
        if (i % 5 < 4) {
            int index = i % std::max(1, static_cast<int>(filePaths.size() * 0.2));
            fullPath = filePaths[index];
        } else {
            int index = std::max(1, static_cast<int>(filePaths.size() * 0.2)) + 
                       (i % std::max(1, static_cast<int>(filePaths.size() * 0.8)));
            if (index >= filePaths.size()) index = filePaths.size() - 1;
            fullPath = filePaths[index];
        }
        
        std::string content;
        std::string contentType;
        
        if (!ResourceCache::getInstance().getResource(fullPath, content, contentType)) {
            std::ifstream file(fullPath, std::ios::binary);
            if (file) {
                file.seekg(0, std::ios::end);
                size_t size = file.tellg();
                file.seekg(0, std::ios::beg);
                content.resize(size);
                file.read(&content[0], size);
                contentType = "application/octet-stream";
                ResourceCache::getInstance().addResource(fullPath, content, contentType);
            }
        }
        
        if (ResourceCache::getInstance().getHitCount() < 10) {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }
    
    auto endWithCache = std::chrono::high_resolution_clock::now();
    auto durationWithCache = std::chrono::duration_cast<std::chrono::milliseconds>(endWithCache - startWithCache);
    
    std::cout << "\n测试结果:" << std::endl;
    std::cout << "未使用LFU缓存的耗时: " << durationNoCache.count() << "ms" << std::endl;
    std::cout << "使用LFU缓存的耗时: " << durationWithCache.count() << "ms" << std::endl;
    double improvement = (durationNoCache.count() - durationWithCache.count()) * 100.0 / durationNoCache.count();
    std::cout << "性能提升: " << improvement << "%" << std::endl;
    std::cout << "缓存命中率: " << (ResourceCache::getInstance().getHitRate() * 100) << "%" << std::endl;
    std::cout << "缓存大小: " << (ResourceCache::getInstance().getCacheSize() / 1024) << "KB" << std::endl;
    std::cout << "缓存项目数: " << ResourceCache::getInstance().getCacheEntryCount() << std::endl;
    std::cout << "===========================" << std::endl;
}