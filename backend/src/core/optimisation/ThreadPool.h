// ============================================================================
// Fichier: src/core/optimization/ThreadPool.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Pool de threads réutilisables pour éviter la création/destruction.
//   Optimise les performances en réutilisant les threads existants.
//
// Thread-safety: OUI
//
// Patterns: Object Pool Pattern
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>

#include "../Logger.h"

namespace midiMind {

/**
 * @class ThreadPool
 * @brief Pool de threads haute performance
 * 
 * @details
 * Implémente un pool de threads workers qui peuvent exécuter des tâches
 * de manière asynchrone. Évite le coût de création/destruction des threads.
 * 
 * Caractéristiques:
 * - Taille dynamique (min/max threads)
 * - File de tâches prioritaires
 * - Support std::future pour résultats asynchrones
 * - Arrêt gracieux
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * ThreadPool pool(4); // 4 threads workers
 * 
 * // Soumettre une tâche
 * auto future = pool.submit([]() {
 *     // Traitement...
 *     return 42;
 * });
 * 
 * // Récupérer le résultat
 * int result = future.get();
 * ```
 */
class ThreadPool {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    using Task = std::function<void()>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param numThreads Nombre de threads (0 = nombre de cores)
     */
    explicit ThreadPool(size_t numThreads = 0);
    
    /**
     * @brief Destructeur
     * 
     * Attend la fin de toutes les tâches en cours.
     */
    ~ThreadPool();
    
    // Désactiver copie
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // ========================================================================
    // SOUMISSION DE TÂCHES
    // ========================================================================
    
    /**
     * @brief Soumet une tâche au pool
     * 
     * @tparam F Type de la fonction
     * @tparam Args Types des arguments
     * @param f Fonction à exécuter
     * @param args Arguments de la fonction
     * @return std::future<ReturnType> Future pour récupérer le résultat
     * 
     * @note Thread-safe
     * 
     * @example
     * ```cpp
     * auto future = pool.submit([](int x) { return x * 2; }, 21);
     * int result = future.get(); // 42
     * ```
     */
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    /**
     * @brief Soumet une tâche sans retour (fire and forget)
     * 
     * @param task Tâche à exécuter
     * 
     * @note Thread-safe
     */
    void post(Task task);
    
    // ========================================================================
    // CONTRÔLE
    // ========================================================================
    
    /**
     * @brief Arrête le pool (attend la fin des tâches en cours)
     * 
     * @note Thread-safe
     */
    void shutdown();
    
    /**
     * @brief Arrête le pool immédiatement (annule les tâches en attente)
     * 
     * @note Thread-safe
     */
    void shutdownNow();
    
    /**
     * @brief Attend la fin de toutes les tâches
     * 
     * @note Thread-safe
     */
    void wait();
    
    /**
     * @brief Vérifie si le pool est actif
     * 
     * @note Thread-safe
     */
    bool isRunning() const;
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Récupère le nombre de threads
     * 
     * @note Thread-safe
     */
    size_t getThreadCount() const;
    
    /**
     * @brief Récupère le nombre de tâches en attente
     * 
     * @note Thread-safe
     */
    size_t getPendingTaskCount() const;
    
    /**
     * @brief Récupère le nombre de tâches actives
     * 
     * @note Thread-safe
     */
    size_t getActiveTaskCount() const;
    
    /**
     * @brief Récupère le nombre total de tâches traitées
     * 
     * @note Thread-safe
     */
    uint64_t getCompletedTaskCount() const;

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Fonction worker
     */
    void workerThread();
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Threads workers
    std::vector<std::thread> workers_;
    
    /// File de tâches
    std::queue<Task> tasks_;
    
    /// Mutex pour protéger la file
    std::mutex queueMutex_;
    
    /// Condition variable pour les workers
    std::condition_variable condition_;
    
    /// Condition variable pour wait()
    std::condition_variable waitCondition_;
    
    /// Flag d'arrêt
    std::atomic<bool> stop_;
    
    /// Nombre de tâches actives
    std::atomic<size_t> activeTasks_;
    
    /// Compteur de tâches complétées
    std::atomic<uint64_t> completedTasks_;
};

// ============================================================================
// IMPLÉMENTATION DES TEMPLATES
// ============================================================================

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using ReturnType = typename std::result_of<F(Args...)>::type;
    
    // Créer une tâche packagée
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<ReturnType> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        
        if (stop_) {
            throw std::runtime_error("ThreadPool is stopped");
        }
        
        // Ajouter à la file
        tasks_.emplace([task]() {
            (*task)();
        });
    }
    
    // Notifier un worker
    condition_.notify_one();
    
    return result;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ThreadPool.h
// ============================================================================