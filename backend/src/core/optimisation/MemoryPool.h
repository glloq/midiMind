// ============================================================================
// Fichier: src/core/optimization/MemoryPool.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Pool de mémoire pour réduire les allocations/désallocations.
//   Particulièrement utile pour les objets de taille fixe fréquemment alloués.
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
#include <cstddef>
#include <cstring>

#include "../Logger.h"

namespace midiMind {

/**
 * @class MemoryPool
 * @brief Pool de mémoire pour allocations rapides
 * 
 * @details
 * Pré-alloue un grand bloc de mémoire et distribue des chunks
 * de taille fixe. Réduit drastiquement les appels à malloc/free.
 * 
 * Caractéristiques:
 * - Allocation O(1)
 * - Désallocation O(1)
 * - Thread-safe
 * - Pas de fragmentation
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * MemoryPool pool(1024, 100); // 100 blocs de 1024 bytes
 * 
 * // Allouer
 * void* ptr = pool.allocate();
 * 
 * // Utiliser...
 * 
 * // Libérer
 * pool.deallocate(ptr);
 * ```
 */
class MemoryPool {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param blockSize Taille d'un bloc (bytes)
     * @param numBlocks Nombre de blocs initiaux
     */
    MemoryPool(size_t blockSize, size_t numBlocks);
    
    /**
     * @brief Destructeur
     */
    ~MemoryPool();
    
    // Désactiver copie
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    
    // ========================================================================
    // ALLOCATION
    // ========================================================================
    
    /**
     * @brief Alloue un bloc de mémoire
     * 
     * @return void* Pointeur vers le bloc alloué
     * 
     * @note Thread-safe
     * @note Retourne nullptr si le pool est plein
     */
    void* allocate();
    
    /**
     * @brief Libère un bloc de mémoire
     * 
     * @param ptr Pointeur vers le bloc à libérer
     * 
     * @note Thread-safe
     */
    void deallocate(void* ptr);
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Récupère la taille d'un bloc
     * 
     * @note Thread-safe
     */
    size_t getBlockSize() const;
    
    /**
     * @brief Récupère le nombre total de blocs
     * 
     * @note Thread-safe
     */
    size_t getTotalBlocks() const;
    
    /**
     * @brief Récupère le nombre de blocs libres
     * 
     * @note Thread-safe
     */
    size_t getFreeBlocks() const;
    
    /**
     * @brief Récupère le nombre de blocs utilisés
     * 
     * @note Thread-safe
     */
    size_t getUsedBlocks() const;
    
    /**
     * @brief Vérifie si le pool est plein
     * 
     * @note Thread-safe
     */
    bool isFull() const;
    
    /**
     * @brief Récupère les statistiques
     * 
     * @return json Statistiques
     * 
     * @note Thread-safe
     */
    json getStatistics() const;

private:
    // ========================================================================
    // STRUCTURES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Nœud de la liste chaînée de blocs libres
     */
    struct FreeBlock {
        FreeBlock* next;
    };
    
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Agrandit le pool
     */
    void expand(size_t numBlocks);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Taille d'un bloc
    size_t blockSize_;
    
    /// Nombre total de blocs
    size_t totalBlocks_;
    
    /// Nombre de blocs libres
    size_t freeBlocks_;
    
    /// Chunks de mémoire alloués
    std::vector<void*> chunks_;
    
    /// Tête de la liste chaînée de blocs libres
    FreeBlock* freeList_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Statistiques
    uint64_t allocations_;
    uint64_t deallocations_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MemoryPool.h
// ============================================================================