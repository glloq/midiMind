// ============================================================================
// Fichier: src/core/optimization/LockFreeQueue.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   File FIFO lock-free pour communication inter-threads haute performance.
//   Utilise des opérations atomiques pour éviter les mutex.
//
// Thread-safety: OUI (lock-free)
//
// Patterns: Lock-Free Data Structure
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <atomic>
#include <memory>
#include <optional>

namespace midiMind {

/**
 * @class LockFreeQueue
 * @brief File FIFO lock-free
 * 
 * @details
 * Implémente une file lock-free SPSC (Single Producer Single Consumer)
 * optimisée pour la communication inter-threads avec latence minimale.
 * 
 * Caractéristiques:
 * - Pas de mutex (lock-free)
 * - Latence très faible
 * - Wait-free pour le producteur
 * - Optimisé pour SPSC (peut être étendu à MPMC)
 * 
 * Thread-safety: Lock-free (safe avec 1 producteur et 1 consommateur)
 * 
 * @tparam T Type d'élément
 * 
 * @example Utilisation
 * ```cpp
 * LockFreeQueue<MidiMessage> queue(1024);
 * 
 * // Thread producteur
 * queue.push(message);
 * 
 * // Thread consommateur
 * auto msg = queue.pop();
 * if (msg) {
 *     // Traiter le message
 * }
 * ```
 */
template<typename T>
class LockFreeQueue {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param capacity Capacité de la file (doit être une puissance de 2)
     */
    explicit LockFreeQueue(size_t capacity)
        : capacity_(roundUpToPowerOf2(capacity))
        , mask_(capacity_ - 1)
        , buffer_(new T[capacity_])
        , writeIndex_(0)
        , readIndex_(0) {
    }
    
    /**
     * @brief Destructeur
     */
    ~LockFreeQueue() {
        delete[] buffer_;
    }
    
    // Désactiver copie et move
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
    LockFreeQueue(LockFreeQueue&&) = delete;
    LockFreeQueue& operator=(LockFreeQueue&&) = delete;
    
    // ========================================================================
    // OPÉRATIONS
    // ========================================================================
    
    /**
     * @brief Ajoute un élément à la file
     * 
     * @param item Élément à ajouter
     * @return true Si l'ajout a réussi, false si la file est pleine
     * 
     * @note Wait-free
     */
    bool push(const T& item) {
        const size_t currentWrite = writeIndex_.load(std::memory_order_relaxed);
        const size_t nextWrite = (currentWrite + 1) & mask_;
        
        // Vérifier si la file est pleine
        if (nextWrite == readIndex_.load(std::memory_order_acquire)) {
            return false; // File pleine
        }
        
        // Écrire l'élément
        buffer_[currentWrite] = item;
        
        // Publier la nouvelle position d'écriture
        writeIndex_.store(nextWrite, std::memory_order_release);
        
        return true;
    }
    
    /**
     * @brief Ajoute un élément à la file (move)
     * 
     * @param item Élément à ajouter
     * @return true Si l'ajout a réussi
     * 
     * @note Wait-free
     */
    bool push(T&& item) {
        const size_t currentWrite = writeIndex_.load(std::memory_order_relaxed);
        const size_t nextWrite = (currentWrite + 1) & mask_;
        
        if (nextWrite == readIndex_.load(std::memory_order_acquire)) {
            return false;
        }
        
        buffer_[currentWrite] = std::move(item);
        writeIndex_.store(nextWrite, std::memory_order_release);
        
        return true;
    }
    
    /**
     * @brief Retire un élément de la file
     * 
     * @return std::optional<T> Élément ou nullopt si vide
     * 
     * @note Lock-free
     */
    std::optional<T> pop() {
        const size_t currentRead = readIndex_.load(std::memory_order_relaxed);
        
        // Vérifier si la file est vide
        if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
            return std::nullopt; // File vide
        }
        
        // Lire l'élément
        T item = std::move(buffer_[currentRead]);
        
        // Publier la nouvelle position de lecture
        const size_t nextRead = (currentRead + 1) & mask_;
        readIndex_.store(nextRead, std::memory_order_release);
        
        return item;
    }
    
    /**
     * @brief Lit un élément sans le retirer (peek)
     * 
     * @return std::optional<T> Élément ou nullopt si vide
     * 
     * @note Lock-free
     */
    std::optional<T> peek() const {
        const size_t currentRead = readIndex_.load(std::memory_order_relaxed);
        
        if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        
        return buffer_[currentRead];
    }
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Vérifie si la file est vide
     * 
     * @note Lock-free
     */
    bool isEmpty() const {
        return readIndex_.load(std::memory_order_acquire) == 
               writeIndex_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Vérifie si la file est pleine
     * 
     * @note Lock-free
     */
    bool isFull() const {
        const size_t nextWrite = (writeIndex_.load(std::memory_order_acquire) + 1) & mask_;
        return nextWrite == readIndex_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Récupère le nombre approximatif d'éléments
     * 
     * @note Lock-free mais approximatif
     */
    size_t size() const {
        const size_t write = writeIndex_.load(std::memory_order_acquire);
        const size_t read = readIndex_.load(std::memory_order_acquire);
        
        if (write >= read) {
            return write - read;
        } else {
            return capacity_ - (read - write);
        }
    }
    
    /**
     * @brief Récupère la capacité
     */
    size_t capacity() const {
        return capacity_;
    }

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Arrondit à la puissance de 2 supérieure
     */
    static size_t roundUpToPowerOf2(size_t n) {
        if (n == 0) return 1;
        
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        n++;
        
        return n;
    }
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Capacité (puissance de 2)
    const size_t capacity_;
    
    /// Masque pour modulo rapide
    const size_t mask_;
    
    /// Buffer circulaire
    T* buffer_;
    
    /// Index d'écriture (producteur)
    /// Aligné pour éviter le false sharing
    alignas(64) std::atomic<size_t> writeIndex_;
    
    /// Index de lecture (consommateur)
    /// Aligné pour éviter le false sharing
    alignas(64) std::atomic<size_t> readIndex_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER LockFreeQueue.h
// ============================================================================