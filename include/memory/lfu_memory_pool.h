#ifndef MYWEBSERVER_LFU_MEMORY_POOL_H
#define MYWEBSERVER_LFU_MEMORY_POOL_H

#include <unordered_map>
#include <list>
#include <memory>

class LFUMemoryPool {
private:
    struct Block {
        void* memory;
        size_t size;
        int frequency;
        Block(void* m, size_t s) : memory(m), size(s), frequency(1) {}
    };

    struct FrequencyGroup {
        int frequency;
        std::list<Block*> blocks;
        FrequencyGroup(int f) : frequency(f) {}
    };

    size_t total_size_;
    size_t used_size_;
    std::unordered_map<void*, Block*> block_map_;
    std::list<FrequencyGroup> freq_groups_;
    
    void incrementFrequency(Block* block);
    Block* evict();

public:
    explicit LFUMemoryPool(size_t total_size);
    ~LFUMemoryPool();

    void* allocate(size_t size);
    void deallocate(void* ptr);
    
    // 获取统计信息
    size_t getTotalSize() const { return total_size_; }
    size_t getUsedSize() const { return used_size_; }
    size_t getFreeSize() const { return total_size_ - used_size_; }
};

#endif // MYWEBSERVER_LFU_MEMORY_POOL_H 