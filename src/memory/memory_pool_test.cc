#include "lfu_memory_pool.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <random>

void testMemoryPool(bool useLFU) {
    const size_t POOL_SIZE = 1024 * 1024 * 50;  // 50MB
    const int NUM_ALLOCATIONS = 1000;
    const int NUM_OPERATIONS = 100000;

    std::vector<void*> allocated_ptrs;
    allocated_ptrs.reserve(NUM_ALLOCATIONS);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> size_dist(512, 4096);  // 512B to 4KB
    std::uniform_int_distribution<> op_dist(0, 99);  // 控制分配/释放比例
    std::uniform_int_distribution<> hot_block_dist(0, NUM_ALLOCATIONS / 10);  // 高频访问前 10%

    LFUMemoryPool* pool = new LFUMemoryPool(POOL_SIZE);
    size_t allocation_fails = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        if (op_dist(gen) < 80 || allocated_ptrs.empty()) {  // 80% 分配概率
            size_t size = size_dist(gen);
            void* ptr = pool->allocate(size);
            if (ptr) {
                allocated_ptrs.push_back(ptr);
            } else {
                ++allocation_fails;
            }
        } else {  // 释放，倾向于高频块
            size_t index = hot_block_dist(gen) % allocated_ptrs.size();
            pool->deallocate(allocated_ptrs[index]);
            allocated_ptrs.erase(allocated_ptrs.begin() + index);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    for (void* ptr : allocated_ptrs) {
        pool->deallocate(ptr);
    }

    std::cout << (useLFU ? "LFU" : "Standard") << " Memory Pool Test Results:" << std::endl;
    std::cout << "Total operations: " << NUM_OPERATIONS << std::endl;
    std::cout << "Time taken: " << duration.count() << "ms" << std::endl;
    std::cout << "Operations per second: " << (NUM_OPERATIONS * 1000.0 / duration.count()) << std::endl;
    std::cout << "Allocation failures: " << allocation_fails << std::endl;
    std::cout << "Final pool stats:" << std::endl;
    std::cout << "  Total size: " << pool->getTotalSize() << " bytes" << std::endl;
    std::cout << "  Used size: " << pool->getUsedSize() << " bytes" << std::endl;
    std::cout << "  Free size: " << pool->getFreeSize() << " bytes" << std::endl;
    std::cout << std::endl;

    delete pool;
}

int main() {
    std::cout << "Testing memory pools..." << std::endl;
    std::cout << "=======================" << std::endl;
    testMemoryPool(true);
    testMemoryPool(false);
    return 0;
}