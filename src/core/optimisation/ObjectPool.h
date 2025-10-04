// ============================================================================
// Fichier: src/core/optimization/ObjectPool.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Pool d'objets typés réutilisables.
//   Template générique pour créer des pools d'objets quelconques.
//
// Thread-safety: OUI
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <queue>
#include <functional>

#include "../Logger.h"

namespace midiMind {

/**
 * @class ObjectPool
 * @brief Pool d'objets typés
 * 
 * @details
 * Pool générique pour réutiliser des objets au lieu de les créer/détruire.
 * Particulièrement utile pour MidiMessage, buffers, etc.
 * 
 * Caractéristiques:
 * - Construction/destruction automatiques
 * - Reset automatique avant réutilisation
 * - Thread-safe
 * - Expansion automatique
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @tparam T Type d'objet à pooler
 * 
 * @example Utilisation
 * ```cpp
 * // Pool de MidiMessage
 * ObjectPool<MidiMessage> pool(100);
 * 
 * // Acquérir un objet
 * auto msg = pool.acquire();
 * msg->setNoteOn(1, 60, 100);
 * 
 * // L'objet sera automatiquement retourné au pool à la destruction
 * ```
 */
template<typename T>
class ObjectPool {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Fonction de reset appelée avant réutilisation
     */
    using ResetFunction = std::function<void(T&)>;
    
    /**
     * @brief Deleter personnalisé pour retourner l'objet au pool
     */
    class PoolDeleter {
    public:
        explicit PoolDeleter(ObjectPool<T>* pool) : pool_(pool) {}
        
        void operator()(T* obj) {
            if (pool_ && obj) {
                pool_->release(obj);
            }
        }
        
    private:
        ObjectPool<T>* pool_;
    };
    
    /**
     * @brief Type de pointeur retourné
     */
    using PoolPtr = std::unique_ptr<T, PoolDeleter>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param initialSize Taille initiale du pool
     * @param resetFunc Fonction de reset (optionnelle)
     */
    explicit ObjectPool(size_t initialSize = 10, ResetFunction resetFunc = nullptr)
        : resetFunc_(resetFunc)
        , totalCreated_(0)
        , totalAcquired_(0)
        , totalReleased_(0) {
        
        Logger::info("ObjectPool", "Creating ObjectPool<" + 
                    std::string(typeid(T).name()) + ">");
        Logger::info("ObjectPool", "  Initial size: " + std::to_string(initialSize));
        
        // Créer les objets initiaux
        for (size_t i = 0; i < initialSize; ++i) {
            available_.push(createObject());
        }
        
        Logger::info("ObjectPool", "✓ ObjectPool created");
    }
    
    /**
     * @brief Destructeur
     */
    ~ObjectPool() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Logger::info("ObjectPool", "Destroying ObjectPool<" + 
                    std::string(typeid(T).name()) + ">");
        Logger::info("ObjectPool", "  Total created: " + std::to_string(totalCreated_));
        Logger::info("ObjectPool", "  Total acquired: " + std::to_string(totalAcquired_));
        Logger::info("ObjectPool", "  Total released: " + std::to_string(totalReleased_));
        
        // Libérer tous les objets disponibles
        while (!available_.empty()) {
            delete available_.front();
            available_.pop();
        }
        
        Logger::info("ObjectPool", "✓ ObjectPool destroyed");
    }
    
    // Désactiver copie
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    
    // ========================================================================
    // ACQUISITION / LIBÉRATION
    // ========================================================================
    
    /**
     * @brief Acquiert un objet du pool
     * 
     * @return PoolPtr Pointeur unique vers l'objet
     * 
     * @note Thread-safe
     * @note L'objet sera automatiquement retourné au pool
     */
    PoolPtr acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        T* obj = nullptr;
        
        if (available_.empty()) {
            // Créer un nouvel objet
            obj = createObject();
            Logger::debug("ObjectPool", "Pool empty, created new object");
        } else {
            // Réutiliser un objet existant
            obj = available_.front();
            available_.pop();
            
            // Réinitialiser l'objet
            if (resetFunc_) {
                resetFunc_(*obj);
            }
        }
        
        totalAcquired_++;
        
        return PoolPtr(obj, PoolDeleter(this));
    }
    
    /**
     * @brief Tente d'acquérir un objet sans créer si pool vide
     * 
     * @return PoolPtr Pointeur ou nullptr si pool vide
     * 
     * @note Thread-safe
     */
    PoolPtr tryAcquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (available_.empty()) {
            return PoolPtr(nullptr, PoolDeleter(this));
        }
        
        T* obj = available_.front();
        available_.pop();
        
        if (resetFunc_) {
            resetFunc_(*obj);
        }
        
        totalAcquired_++;
        
        return PoolPtr(obj, PoolDeleter(this));
    }
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit la fonction de reset
     * 
     * @param resetFunc Fonction de reset
     * 
     * @note Thread-safe
     */
    void setResetFunction(ResetFunction resetFunc) {
        std::lock_guard<std::mutex> lock(mutex_);
        resetFunc_ = resetFunc;
    }
    
    /**
     * @brief Pré-alloue des objets
     * 
     * @param count Nombre d'objets à pré-allouer
     * 
     * @note Thread-safe
     */
    void reserve(size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (size_t i = 0; i < count; ++i) {
            available_.push(createObject());
        }
        
        Logger::debug("ObjectPool", "Reserved " + std::to_string(count) + " objects");
    }
    
    /**
     * @brief Libère les objets non utilisés
     * 
     * @note Thread-safe
     */
    void shrink() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t count = 0;
        
        while (!available_.empty()) {
            delete available_.front();
            available_.pop();
            count++;
        }
        
        Logger::debug("ObjectPool", "Shrunk pool (freed " + std::to_string(count) + " objects)");
    }
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Récupère le nombre d'objets disponibles
     * 
     * @note Thread-safe
     */
    size_t getAvailableCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_.size();
    }
    
    /**
     * @brief Récupère le nombre total d'objets créés
     * 
     * @note Thread-safe
     */
    size_t getTotalCreated() const {
        return totalCreated_;
    }
    
    /**
     * @brief Récupère les statistiques
     * 
     * @return json Statistiques
     * 
     * @note Thread-safe
     */
    json getStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        json stats;
        stats["type"] = typeid(T).name();
        stats["available"] = available_.size();
        stats["total_created"] = totalCreated_.load();
        stats["total_acquired"] = totalAcquired_.load();
        stats["total_released"] = totalReleased_.load();
        
        return stats;
    }

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Crée un nouvel objet
     */
    T* createObject() {
        totalCreated_++;
        return new T();
    }
    
    /**
     * @brief Libère un objet (retour au pool)
     */
    void release(T* obj) {
        if (!obj) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        available_.push(obj);
        totalReleased_++;
    }
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// File d'objets disponibles
    std::queue<T*> available_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Fonction de reset
    ResetFunction resetFunc_;
    
    /// Statistiques
    std::atomic<size_t> totalCreated_;
    std::atomic<size_t> totalAcquired_;
    std::atomic<size_t> totalReleased_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ObjectPool.h
// ============================================================================