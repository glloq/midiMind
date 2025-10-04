// ============================================================================
// Fichier: src/core/optimization/MemoryPool.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "MemoryPool.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

MemoryPool::MemoryPool(size_t blockSize, size_t numBlocks)
    : blockSize_(std::max(blockSize, sizeof(FreeBlock)))
    , totalBlocks_(0)
    , freeBlocks_(0)
    , freeList_(nullptr)
    , allocations_(0)
    , deallocations_(0) {
    
    Logger::info("MemoryPool", "Creating MemoryPool");
    Logger::info("MemoryPool", "  Block size: " + std::to_string(blockSize_));
    Logger::info("MemoryPool", "  Initial blocks: " + std::to_string(numBlocks));
    
    // Allouer le premier chunk
    expand(numBlocks);
    
    Logger::info("MemoryPool", "✓ MemoryPool created");
}

MemoryPool::~MemoryPool() {
    Logger::info("MemoryPool", "Destroying MemoryPool...");
    Logger::info("MemoryPool", "  Total allocations: " + std::to_string(allocations_));
    Logger::info("MemoryPool", "  Total deallocations: " + std::to_string(deallocations_));
    
    // Libérer tous les chunks
    for (void* chunk : chunks_) {
        ::operator delete(chunk);
    }
    
    Logger::info("MemoryPool", "✓ MemoryPool destroyed");
}

// ============================================================================
// ALLOCATION
// ============================================================================

void* MemoryPool::allocate() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Si plus de blocs libres, agrandir le pool
    if (freeList_ == nullptr) {
        Logger::debug("MemoryPool", "Pool full, expanding...");
        expand(totalBlocks_); // Doubler la taille
    }
    
    // Récupérer le premier bloc libre
    void* ptr = freeList_;
    freeList_ = freeList_->next;
    
    freeBlocks_--;
    allocations_++;
    
    return ptr;
}

void MemoryPool::deallocate(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Remettre le bloc dans la liste libre
    FreeBlock* block = static_cast<FreeBlock*>(ptr);
    block->next = freeList_;
    freeList_ = block;
    
    freeBlocks_++;
    deallocations_++;
}

// ============================================================================
// INFORMATIONS
// ============================================================================

size_t MemoryPool::getBlockSize() const {
    return blockSize_;
}

size_t MemoryPool::getTotalBlocks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalBlocks_;
}

size_t MemoryPool::getFreeBlocks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return freeBlocks_;
}

size_t MemoryPool::getUsedBlocks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalBlocks_ - freeBlocks_;
}

bool MemoryPool::isFull() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return freeBlocks_ == 0;
}

json MemoryPool::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    stats["block_size"] = blockSize_;
    stats["total_blocks"] = totalBlocks_;
    stats["free_blocks"] = freeBlocks_;
    stats["used_blocks"] = totalBlocks_ - freeBlocks_;
    stats["usage_percent"] = totalBlocks_ > 0 ? 
        ((totalBlocks_ - freeBlocks_) * 100.0 / totalBlocks_) : 0.0;
    stats["total_allocations"] = allocations_;
    stats["total_deallocations"] = deallocations_;
    stats["chunks"] = chunks_.size();
    
    return stats;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void MemoryPool::expand(size_t numBlocks) {
    // Allouer un nouveau chunk
    size_t chunkSize = blockSize_ * numBlocks;
    void* chunk = ::operator new(chunkSize);
    
    chunks_.push_back(chunk);
    
    // Initialiser la liste chaînée de blocs libres
    char* ptr = static_cast<char*>(chunk);
    
    for (size_t i = 0; i < numBlocks; ++i) {
        FreeBlock* block = reinterpret_cast<FreeBlock*>(ptr);
        block->next = freeList_;
        freeList_ = block;
        
        ptr += blockSize_;
    }
    
    totalBlocks_ += numBlocks;
    freeBlocks_ += numBlocks;
    
    Logger::debug("MemoryPool", "Expanded pool by " + std::to_string(numBlocks) + 
                 " blocks (total: " + std::to_string(totalBlocks_) + ")");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MemoryPool.cpp
// ============================================================================