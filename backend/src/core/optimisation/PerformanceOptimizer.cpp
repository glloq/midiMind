// ============================================================================
// Fichier: src/core/optimization/PerformanceOptimizer.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "PerformanceOptimizer.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// SINGLETON
// ============================================================================

PerformanceOptimizer& PerformanceOptimizer::instance() {
    static PerformanceOptimizer instance;
    return instance;
}

// ============================================================================
// CONSTRUCTION PRIVÉE
// ============================================================================

PerformanceOptimizer::PerformanceOptimizer()
    : initialized_(false)
    , stop_(false) {
    
    Logger::info("PerformanceOptimizer", "PerformanceOptimizer constructed");
}

PerformanceOptimizer::~PerformanceOptimizer() {
    shutdown();
    Logger::info("PerformanceOptimizer", "PerformanceOptimizer destroyed");
}

// ============================================================================
// INITIALISATION
// ============================================================================

void PerformanceOptimizer::initialize(const OptimizationConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        Logger::warn("PerformanceOptimizer", "Already initialized");
        return;
    }
    
    Logger::info("PerformanceOptimizer", "═══════════════════════════════════════");
    Logger::info("PerformanceOptimizer", "  Initializing Performance Optimizations");
    Logger::info("PerformanceOptimizer", "═══════════════════════════════════════");
    
    config_ = config;
    
    // Créer le ThreadPool
    Logger::info("PerformanceOptimizer", "Creating ThreadPool...");
    threadPool_ = std::make_shared<ThreadPool>(config_.threadPoolSize);
    Logger::info("PerformanceOptimizer", "✓ ThreadPool created (" + 
                std::to_string(threadPool_->getThreadCount()) + " threads)");
    
    // Créer le MemoryPool
    Logger::info("PerformanceOptimizer", "Creating MemoryPool...");
    memoryPool_ = std::make_shared<MemoryPool>(
        config_.memoryBlockSize,
        config_.memoryInitialBlocks
    );
    Logger::info("PerformanceOptimizer", "✓ MemoryPool created");
    
    // Créer l'ObjectPool pour MidiMessage
    Logger::info("PerformanceOptimizer", "Creating MidiMessage ObjectPool...");
    midiMessagePool_ = std::make_shared<ObjectPool<MidiMessage>>(
        config_.midiMessagePoolSize,
        [](MidiMessage& msg) {
            // Reset du message avant réutilisation
            msg = MidiMessage();
        }
    );
    Logger::info("PerformanceOptimizer", "✓ MidiMessage ObjectPool created");
    
    // Démarrer l'optimisation automatique
    if (config_.autoOptimize) {
        Logger::info("PerformanceOptimizer", "Starting auto-optimization...");
        stop_ = false;
        optimizationThread_ = std::thread([this]() {
            optimizationLoop();
        });
        Logger::info("PerformanceOptimizer", "✓ Auto-optimization started");
    }
    
    initialized_ = true;
    
    Logger::info("PerformanceOptimizer", "═══════════════════════════════════════");
    Logger::info("PerformanceOptimizer", "✓ Performance Optimizations Initialized");
    Logger::info("PerformanceOptimizer", "═══════════════════════════════════════");
}

void PerformanceOptimizer::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return;
    }
    
    Logger::info("PerformanceOptimizer", "Shutting down optimizations...");
    
    // Arrêter l'optimisation automatique
    stop_ = true;
    if (optimizationThread_.joinable()) {
        optimizationThread_.join();
    }
    
    // Arrêter le ThreadPool
    if (threadPool_) {
        threadPool_->shutdown();
    }
    
    // Libérer les pools
    threadPool_.reset();
    memoryPool_.reset();
    midiMessagePool_.reset();
    
    initialized_ = false;
    
    Logger::info("PerformanceOptimizer", "✓ Optimizations shut down");
}

bool PerformanceOptimizer::isInitialized() const {
    return initialized_;
}

// ============================================================================
// THREADPOOL
// ============================================================================

std::shared_ptr<ThreadPool> PerformanceOptimizer::getThreadPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    return threadPool_;
}

// ============================================================================
// MEMORY POOL
// ============================================================================

std::shared_ptr<MemoryPool> PerformanceOptimizer::getMemoryPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    return memoryPool_;
}

void* PerformanceOptimizer::allocateMemory() {
    if (!memoryPool_) {
        return nullptr;
    }
    return memoryPool_->allocate();
}

void PerformanceOptimizer::deallocateMemory(void* ptr) {
    if (memoryPool_ && ptr) {
        memoryPool_->deallocate(ptr);
    }
}

// ============================================================================
// OBJECT POOLS
// ============================================================================

std::shared_ptr<ObjectPool<MidiMessage>> PerformanceOptimizer::getMidiMessagePool() {
    std::lock_guard<std::mutex> lock(mutex_);
    return midiMessagePool_;
}

ObjectPool<MidiMessage>::PoolPtr PerformanceOptimizer::acquireMidiMessage() {
    if (!midiMessagePool_) {
        return ObjectPool<MidiMessage>::PoolPtr(nullptr, 
            ObjectPool<MidiMessage>::PoolDeleter(nullptr));
    }
    return midiMessagePool_->acquire();
}

// ============================================================================
// STATISTIQUES
// ============================================================================

json PerformanceOptimizer::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    stats["initialized"] = initialized_.load();
    stats["config"] = config_.toJson();
    
    if (threadPool_) {
        json threadPoolStats;
        threadPoolStats["thread_count"] = threadPool_->getThreadCount();
        threadPoolStats["pending_tasks"] = threadPool_->getPendingTaskCount();
        threadPoolStats["active_tasks"] = threadPool_->getActiveTaskCount();
        threadPoolStats["completed_tasks"] = threadPool_->getCompletedTaskCount();
        stats["thread_pool"] = threadPoolStats;
    }
    
    if (memoryPool_) {
        stats["memory_pool"] = memoryPool_->getStatistics();
    }
    
    if (midiMessagePool_) {
        stats["midi_message_pool"] = midiMessagePool_->getStatistics();
    }
    
    return stats;
}

OptimizationConfig PerformanceOptimizer::getConfiguration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void PerformanceOptimizer::optimizationLoop() {
    Logger::info("PerformanceOptimizer", "Auto-optimization loop started");
    
    while (!stop_) {
        // Effectuer l'optimisation
        performAutoOptimization();
        
        // Attendre l'intervalle
        uint32_t interval = config_.optimizationIntervalMs;
        for (uint32_t i = 0; i < interval / 100 && !stop_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    Logger::info("PerformanceOptimizer", "Auto-optimization loop stopped");
}

void PerformanceOptimizer::performAutoOptimization() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Analyser l'utilisation du MemoryPool
    if (memoryPool_) {
        size_t freeBlocks = memoryPool_->getFreeBlocks();
        size_t totalBlocks = memoryPool_->getTotalBlocks();
        
        float usage = totalBlocks > 0 ? 
            (1.0f - static_cast<float>(freeBlocks) / totalBlocks) : 0.0f;
        
        // Si utilisation > 80%, le pool pourrait manquer de blocs
        if (usage > 0.8f) {
            Logger::debug("PerformanceOptimizer", 
                         "MemoryPool usage high (" + 
                         std::to_string(static_cast<int>(usage * 100)) + "%)");
        }
        
        // Si utilisation < 20%, le pool a trop de blocs libres
        if (usage < 0.2f && totalBlocks > config_.memoryInitialBlocks) {
            Logger::debug("PerformanceOptimizer", 
                         "MemoryPool usage low, could shrink");
        }
    }
    
    // Analyser l'utilisation du ThreadPool
    if (threadPool_) {
        size_t pendingTasks = threadPool_->getPendingTaskCount();
        
        if (pendingTasks > 10) {
            Logger::debug("PerformanceOptimizer", 
                         "ThreadPool has " + std::to_string(pendingTasks) + 
                         " pending tasks");
        }
    }
    
    // Analyser l'utilisation de l'ObjectPool MidiMessage
    if (midiMessagePool_) {
        size_t available = midiMessagePool_->getAvailableCount();
        
        if (available < 10) {
            Logger::debug("PerformanceOptimizer", 
                         "MidiMessage pool running low (" + 
                         std::to_string(available) + " available)");
            
            // Pré-allouer plus d'objets
            midiMessagePool_->reserve(100);
        }
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER PerformanceOptimizer.cpp
// ============================================================================