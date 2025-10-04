// ============================================================================
// src/core/patterns/Subject.h
// Pattern Observer - Classe Subject générique
// ============================================================================
#pragma once

#include <vector>
#include <algorithm>
#include <mutex>
#include <memory>

namespace midiMind {

/**
 * @brief Interface Observer générique
 */
template<typename EventType>
class IObserver {
public:
    virtual ~IObserver() = default;
    virtual void onNotify(const EventType& event) = 0;
};

/**
 * @brief Classe Subject pour le pattern Observer
 * 
 * Permet à des observateurs de s'abonner aux événements.
 * Thread-safe.
 */
template<typename EventType>
class Subject {
public:
    using Observer = IObserver<EventType>;
    
    /**
     * @brief Ajoute un observateur
     */
    void addObserver(std::shared_ptr<Observer> observer) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Éviter les doublons
        if (std::find(observers_.begin(), observers_.end(), observer) == observers_.end()) {
            observers_.push_back(observer);
        }
    }
    
    /**
     * @brief Retire un observateur
     */
    void removeObserver(std::shared_ptr<Observer> observer) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        observers_.erase(
            std::remove(observers_.begin(), observers_.end(), observer),
            observers_.end()
        );
    }
    
    /**
     * @brief Retire tous les observateurs
     */
    void clearObservers() {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_.clear();
    }
    
    /**
     * @brief Nombre d'observateurs
     */
    size_t getObserverCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return observers_.size();
    }

protected:
    /**
     * @brief Notifie tous les observateurs
     */
    void notify(const EventType& event) {
        // Copier les observateurs sous lock
        std::vector<std::shared_ptr<Observer>> observersCopy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            observersCopy = observers_;
        }
        
        // Notifier sans lock pour éviter deadlocks
        for (auto& observer : observersCopy) {
            if (observer) {
                observer->onNotify(event);
            }
        }
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<Observer>> observers_;
};
