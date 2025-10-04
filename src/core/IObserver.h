// ============================================================================
// Fichier: src/core/IObserver.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Interface pour le pattern Observer.
//   Permet aux objets de s'abonner à des événements.
//
// Pattern: Observer Pattern
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <memory>
#include <vector>
#include <algorithm>
#include <mutex>

namespace midiMind {

/**
 * @class IObserver
 * @brief Interface Observer
 * 
 * @details
 * Interface de base pour implémenter le pattern Observer.
 * Les observateurs reçoivent des notifications des sujets observés.
 * 
 * @tparam EventType Type d'événement
 * 
 * @example Utilisation
 * ```cpp
 * class MyObserver : public IObserver<MidiEvent> {
 * public:
 *     void onNotify(const MidiEvent& event) override {
 *         // Traiter l'événement
 *     }
 * };
 * ```
 */
template<typename EventType>
class IObserver {
public:
    virtual ~IObserver() = default;
    
    /**
     * @brief Méthode appelée lors d'une notification
     * 
     * @param event Événement reçu
     */
    virtual void onNotify(const EventType& event) = 0;
};

/**
 * @class ISubject
 * @brief Interface Subject (Observable)
 * 
 * @details
 * Interface de base pour les objets observables.
 * Gère une liste d'observateurs et les notifie des changements.
 * 
 * @tparam EventType Type d'événement
 * 
 * @example Utilisation
 * ```cpp
 * class MySubject : public ISubject<MidiEvent> {
 * public:
 *     void someAction() {
 *         MidiEvent event;
 *         notify(event); // Notifier tous les observateurs
 *     }
 * };
 * ```
 */
template<typename EventType>
class ISubject {
public:
    virtual ~ISubject() = default;
    
    /**
     * @brief Ajoute un observateur
     * 
     * @param observer Observateur à ajouter
     */
    virtual void attach(std::shared_ptr<IObserver<EventType>> observer) {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_.push_back(observer);
    }
    
    /**
     * @brief Retire un observateur
     * 
     * @param observer Observateur à retirer
     */
    virtual void detach(std::shared_ptr<IObserver<EventType>> observer) {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_.erase(
            std::remove(observers_.begin(), observers_.end(), observer),
            observers_.end()
        );
    }
    
    /**
     * @brief Notifie tous les observateurs
     * 
     * @param event Événement à envoyer
     */
    virtual void notify(const EventType& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Nettoyer les weak_ptr expirés
        observers_.erase(
            std::remove_if(observers_.begin(), observers_.end(),
                [](const std::weak_ptr<IObserver<EventType>>& wp) {
                    return wp.expired();
                }),
            observers_.end()
        );
        
        // Notifier tous les observateurs
        for (auto& weakObserver : observers_) {
            if (auto observer = weakObserver.lock()) {
                try {
                    observer->onNotify(event);
                } catch (const std::exception& e) {
                    // Logger l'erreur mais continuer
                    // (éviter qu'une exception dans un observer ne casse tout)
                }
            }
        }
    }
    
    /**
     * @brief Récupère le nombre d'observateurs
     */
    size_t getObserverCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return observers_.size();
    }

protected:
    /// Liste des observateurs (weak_ptr pour éviter les cycles)
    std::vector<std::weak_ptr<IObserver<EventType>>> observers_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
};

/**
 * @class Observable
 * @brief Classe de base observable avec typed events
 * 
 * @details
 * Implémentation concrète de ISubject avec support multi-événements.
 * 
 * @example Utilisation
 * ```cpp
 * enum class MyEvent { STARTED, STOPPED, ERROR };
 * 
 * class MyClass : public Observable<MyEvent> {
 * public:
 *     void start() {
 *         emit(MyEvent::STARTED);
 *     }
 * };
 * ```
 */
template<typename EventType>
class Observable : public ISubject<EventType> {
public:
    /**
     * @brief Émet un événement
     * 
     * @param event Événement à émettre
     */
    void emit(const EventType& event) {
        this->notify(event);
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER IObserver.h
// ============================================================================