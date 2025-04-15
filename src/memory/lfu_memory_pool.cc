#include "lfu_memory_pool.h"
#include <cstdlib>
#include <algorithm>

LFUMemoryPool::LFUMemoryPool(size_t total_size)
    : total_size_(total_size), used_size_(0) {
    freq_groups_.emplace_back(1);  // 初始化频率为1的组
}

LFUMemoryPool::~LFUMemoryPool() {
    for (auto& pair : block_map_) {
        free(pair.second->memory);
        delete pair.second;
    }
}

void LFUMemoryPool::incrementFrequency(Block* block) {
    // 找到当前频率组
    auto it = std::find_if(freq_groups_.begin(), freq_groups_.end(),
        [block](const FrequencyGroup& group) {
            return group.frequency == block->frequency;
        });
    
    // 从当前频率组移除
    it->blocks.remove(block);
    if (it->blocks.empty() && it != freq_groups_.begin()) {
        freq_groups_.erase(it);
    }

    // 增加频率
    block->frequency++;

    // 找到或创建新的频率组
    auto next_it = std::find_if(freq_groups_.begin(), freq_groups_.end(),
        [block](const FrequencyGroup& group) {
            return group.frequency == block->frequency;
        });

    if (next_it == freq_groups_.end()) {
        freq_groups_.emplace_back(block->frequency);
        next_it = --freq_groups_.end();
    }

    next_it->blocks.push_back(block);
}

LFUMemoryPool::Block* LFUMemoryPool::evict() {
    if (freq_groups_.empty() || freq_groups_.front().blocks.empty()) {
        return nullptr;
    }

    // 获取最低频率组的第一个块
    Block* victim = freq_groups_.front().blocks.front();
    freq_groups_.front().blocks.pop_front();

    // 如果该频率组为空，则删除
    if (freq_groups_.front().blocks.empty()) {
        freq_groups_.pop_front();
    }

    used_size_ -= victim->size;
    block_map_.erase(victim->memory);
    
    return victim;
}

void* LFUMemoryPool::allocate(size_t size) {
    // 检查是否有足够的空间
    while (used_size_ + size > total_size_) {
        Block* victim = evict();
        if (!victim) {
            return nullptr;
        }
        free(victim->memory);
        delete victim;
    }

    void* memory = malloc(size);
    if (!memory) {
        return nullptr;
    }

    Block* block = new Block(memory, size);
    block_map_[memory] = block;
    used_size_ += size;

    // 将新块添加到频率为1的组
    if (freq_groups_.empty() || freq_groups_.front().frequency != 1) {
        freq_groups_.emplace_front(1);
    }
    freq_groups_.front().blocks.push_back(block);

    return memory;
}

void LFUMemoryPool::deallocate(void* ptr) {
    auto it = block_map_.find(ptr);
    if (it == block_map_.end()) {
        return;
    }

    Block* block = it->second;
    used_size_ -= block->size;

    // 从频率组中移除
    auto freq_it = std::find_if(freq_groups_.begin(), freq_groups_.end(),
        [block](const FrequencyGroup& group) {
            return group.frequency == block->frequency;
        });
    
    if (freq_it != freq_groups_.end()) {
        freq_it->blocks.remove(block);
        if (freq_it->blocks.empty()) {
            freq_groups_.erase(freq_it);
        }
    }

    free(block->memory);
    delete block;
    block_map_.erase(it);
} 