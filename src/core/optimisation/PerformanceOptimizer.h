// ============================================================================
// Fichier: src/core/optimization/PerformanceOptimizer.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestionnaire centralisé des optimisations de performance.
//   Fournit un accès unifié à tous les pools et optimisations.
//
// Responsabilités:
//   - Gérer ThreadPool, MemoryPool, ObjectPools
//   - Configurer les optimisations
//   - Fournir des statistiques globales
//   - Optimiser automatiquement selon la charge
//
// Thread-safety: OUI
//
// Patterns: Facade Pattern, Singleton Pattern
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <memory>
#include <mutex>
#include <map>
#include <string>

#include "ThreadPool.h"
#include "MemoryPool.h"
#include "ObjectPool.h"
#include "../Logger.h"
#include "../../midi/MidiMessage.h"

namespace midiMind {

/**
 * @struct OptimizationConfig
 * @brief Configuration des optimisations
 */
struct OptimizationConfig {
    /// ThreadPool
    size_t threadPoolSize;              ///< Nombre de threads (0 = auto)
    
    /// MemoryPool
    size_t memoryBlockSize;             ///< Taille d'un bloc mémoire
    size_t memoryInitialBlocks;         ///< Nombre de blocs initiaux
    
    /// ObjectPools
    size_t midiMessagePoolSize;         ///< Taille du pool MidiMessage
    
    /// Optimisations auto
    bool autoOptimize;                  ///< Optimisation automatique
    uint32_t optimizationIntervalMs;    ///< Intervalle d'optimisation (ms)
    
    /**
     * @brief Constructeur avec valeurs par défaut
     */
    OptimizationConfig()
        : threadPoolSize(0)
        , memoryBlockSize(1024)
        , memoryInitialBlocks(100)
        , midiMessagePoolSize(1000)
        , autoOptimize(true)
        , optimizationIntervalMs(60000) {}
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        j["thread_pool_size"] = threadPoolSize;
        j["memory_block_size"] = memoryBlockSize;
        j["memory_initial_blocks"] = memoryInitialBlocks;
        j["midi_message_pool_size"] = midiMessagePoolSize;
        j["auto_optimize"] = autoOptimize;
        j["optimization_interval_ms"] = optimizationIntervalMs;
        return j;
    }
};

/**
 * @class PerformanceOptimizer
 * @brief Gestionnaire centralisé des optimisations
 * 
 * @details
 * Point d'entrée unique pour toutes les optimisations de performance.
 * Fournit un accès aux pools et gère l'optimisation automatique.
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * auto optimizer = PerformanceOptimizer::instance();
 * 
 * // Initialiser
 * optimizer->initialize(config);
 * 
 * // Utiliser le ThreadPool
 * optimizer->submitTask([]() {
 *     // Traitement...
 * });
 * 
 * // Acquérir un MidiMessage
 * auto msg = optimizer->acquireMidiMessage();
 * ```
 */
class PerformanceOptimizer {
public:
    // ========================================================================
    // SINGLETON
    // ========================================================================
    
    /**
     * @brief Récupère l'instance singleton
     */
    static PerformanceOptimizer& instance();
    
    // Désactiver copie et move
    PerformanceOptimizer(const PerformanceOptimizer&) = delete;
    PerformanceOptimizer& operator=(const PerformanceOptimizer&) = delete;
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    /**
     * @brief Initialise les optimisations
     * 
     * @param config Configuration
     * 
     * @note Thread-safe
     */
    void initialize(const OptimizationConfig& config);
    
    /**
     * @brief Arrête toutes les optimisations
     * 
     * @note Thread-safe
     */
    void shutdown();
    
    /**
     * @brief Vérifie si initialisé
     * 
     * @note Thread-safe
     */
    bool isInitialized() const;
    
    // ========================================================================
    // THREADPOOL
    // ========================================================================
    
    /**
     * @brief Récupère le ThreadPool
     * 
     * @note Thread-safe
     */
    std::shared_ptr<ThreadPool> getThreadPool();
    
    /**
     * @brief Soumet une tâche au ThreadPool
     * 
     * @tparam F Type de la fonction
     * @tparam Args Types des arguments
     * @param f Fonction
     * @param args Arguments
     * @return std::future<ReturnType> Future
     * 
     * @note Thread-safe
     */
    template<typename F, typename... Args>
    auto submitTask(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    // ========================================================================
    // MEMORY POOL
    // ========================================================================
    
    /**
     * @brief Récupère le MemoryPool
     * 
     * @note Thread-safe
     */
    std::shared_ptr<MemoryPool> getMemoryPool();
    
    /**
     * @brief Alloue de la mémoire depuis le pool
     * 
     * @return void* Pointeur ou nullptr
     * 
     * @note Thread-safe
     */
    void* allocateMemory();
    
    /**
     * @brief Libère de la mémoire vers le pool
     * 
     * @param ptr Pointeur à libérer
     * 
     * @note Thread-safe
     */
    void deallocateMemory(void* ptr);
    
    // ========================================================================
    // OBJECT POOLS
    // ========================================================================
    
    /**
     * @brief Récupère le pool MidiMessage
     * 
     * @note Thread-safe
     */
    std::shared_ptr<ObjectPool<MidiMessage>> getMidiMessagePool();
    
    /**
     * @brief Acquiert un MidiMessage du pool
     * 
     * @return ObjectPool<MidiMessage>::PoolPtr Pointeur unique
     * 
     * @note Thread-safe
     */
    ObjectPool<MidiMessage>::PoolPtr acquireMidiMessage();
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère toutes les statistiques
     * 
     * @return json Statistiques globales
     * 
     * @note Thread-safe
     */
    json getStatistics() const;
    
    /**
     * @brief Récupère la configuration actuelle
     * 
     * @return OptimizationConfig Configuration
     * 
     * @note Thread-safe
     */
    OptimizationConfig getConfiguration() const;

private:
    // ========================================================================
    // CONSTRUCTION PRIVÉE (Singleton)
    // ========================================================================
    
    PerformanceOptimizer();
    ~PerformanceOptimizer();
    
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Thread d'optimisation automatique
     */
    void optimizationLoop();
    
    /**
     * @brief Effectue l'optimisation automatique
     */
    void performAutoOptimization();
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Configuration
    OptimizationConfig config_;
    
    /// Flag d'initialisation
    std::atomic<bool> initialized_;
    
    /// ThreadPool
    std::shared_ptr<ThreadPool> threadPool_;
    
    /// MemoryPool
    std::shared_ptr<MemoryPool> memoryPool_;
    
    /// ObjectPool pour MidiMessage
    std::shared_ptr<ObjectPool<MidiMessage>> midiMessagePool_;
    
    /// Thread d'optimisation automatique
    std::thread optimizationThread_;
    
    /// Flag d'arrêt
    std::atomic<bool> stop_;
};

// ============================================================================
// IMPLÉMENTATION DES TEMPLATES
// ============================================================================

template<typename F, typename... Args>
auto PerformanceOptimizer::submitTask(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    if (!threadPool_) {
        throw std::runtime_error("PerformanceOptimizer not initialized");
    }
    
    return threadPool_->submit(std::forward<F>(f), std::forward<Args>(args)...);
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER PerformanceOptimizer.h
// ============================================================================