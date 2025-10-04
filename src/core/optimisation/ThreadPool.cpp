// ============================================================================
// Fichier: src/core/optimization/ThreadPool.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "ThreadPool.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

ThreadPool::ThreadPool(size_t numThreads)
    : stop_(false)
    , activeTasks_(0)
    , completedTasks_(0) {
    
    // Si numThreads == 0, utiliser le nombre de cores
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
            numThreads = 4; // Fallback
        }
    }
    
    Logger::info("ThreadPool", "Creating ThreadPool with " + 
                std::to_string(numThreads) + " threads");
    
    // Créer les threads workers
    workers_.reserve(numThreads);
    
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this]() {
            workerThread();
        });
    }
    
    Logger::info("ThreadPool", "✓ ThreadPool created");
}

ThreadPool::~ThreadPool() {
    shutdown();
}

// ============================================================================
// SOUMISSION DE TÂCHES
// ============================================================================

void ThreadPool::post(Task task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        
        if (stop_) {
            Logger::warn("ThreadPool", "Cannot post task: pool is stopped");
            return;
        }
        
        tasks_.emplace(std::move(task));
    }
    
    condition_.notify_one();
}

// ============================================================================
// CONTRÔLE
// ============================================================================

void ThreadPool::shutdown() {
    Logger::info("ThreadPool", "Shutting down ThreadPool...");
    
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    
    // Réveiller tous les workers
    condition_.notify_all();
    
    // Attendre la fin de tous les threads
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    Logger::info("ThreadPool", "✓ ThreadPool shut down");
    Logger::info("ThreadPool", "  Total tasks completed: " + 
                std::to_string(completedTasks_.load()));
}

void ThreadPool::shutdownNow() {
    Logger::info("ThreadPool", "Immediate shutdown of ThreadPool...");
    
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
        
        // Vider la file de tâches
        std::queue<Task> empty;
        std::swap(tasks_, empty);
    }
    
    condition_.notify_all();
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    Logger::info("ThreadPool", "✓ ThreadPool shut down immediately");
}

void ThreadPool::wait() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    
    waitCondition_.wait(lock, [this]() {
        return tasks_.empty() && activeTasks_ == 0;
    });
}

bool ThreadPool::isRunning() const {
    return !stop_;
}

// ============================================================================
// INFORMATIONS
// ============================================================================

size_t ThreadPool::getThreadCount() const {
    return workers_.size();
}

size_t ThreadPool::getPendingTaskCount() const {
    std::unique_lock<std::mutex> lock(queueMutex_);
    return tasks_.size();
}

size_t ThreadPool::getActiveTaskCount() const {
    return activeTasks_;
}

uint64_t ThreadPool::getCompletedTaskCount() const {
    return completedTasks_;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void ThreadPool::workerThread() {
    while (true) {
        Task task;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            // Attendre une tâche ou l'arrêt
            condition_.wait(lock, [this]() {
                return stop_ || !tasks_.empty();
            });
            
            // Si arrêt et pas de tâches, quitter
            if (stop_ && tasks_.empty()) {
                return;
            }
            
            // Récupérer une tâche
            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
                activeTasks_++;
            }
        }
        
        // Exécuter la tâche
        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                Logger::error("ThreadPool", "Task exception: " + std::string(e.what()));
            } catch (...) {
                Logger::error("ThreadPool", "Unknown task exception");
            }
            
            activeTasks_--;
            completedTasks_++;
            
            // Notifier wait()
            waitCondition_.notify_all();
        }
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ThreadPool.cpp
// ============================================================================