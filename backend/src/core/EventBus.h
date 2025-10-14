// ============================================================================
// Fichier: src/core/EventBus.h
// Projet: MidiMind v3.0 - Version simplifiée mais améliorée
// ============================================================================
// Description:
//   Bus d'événements header-only avec désinscription et priorités.
//   Version intermédiaire entre la version basique et la version complète.
//
// Thread-safety: OUI
// ============================================================================

#pragma once

#include <functional>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <typeindex>
#include <any>
#include <algorithm>
#include <atomic>

namespace midiMind {

/**
 * @class EventBus
 * @brief Bus d'événements simplifié avec désinscription
 */
class EventBus {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    using SubscriptionId = uint64_t;
    
    template<typename EventType>
    using Handler = std::function<void(const EventType&)>;
    
    enum Priority : int {
        LOW = 0,
        NORMAL = 50,
        HIGH = 100
    };
    
    // ========================================================================
    // CLASSE RAII pour désinscription automatique
    // ========================================================================
    
    class Subscription {
    public:
        Subscription() = default;
        
        Subscription(EventBus* bus, std::type_index type, SubscriptionId id)
            : bus_(bus), type_(type), id_(id), active_(true) {}
        
        ~Subscription() { unsubscribe(); }
        
        // Move only
        Subscription(Subscription&& other) noexcept
            : bus_(other.bus_), type_(other.type_), id_(other.id_), active_(other.active_) {
            other.active_ = false;
        }
        
        Subscription& operator=(Subscription&& other) noexcept {
            if (this != &other) {
                unsubscribe();
                bus_ = other.bus_;
                type_ = other.type_;
                id_ = other.id_;
                active_ = other.active_;
                other.active_ = false;
            }
            return *this;
        }
        
        // Delete copy
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;
        
        void unsubscribe() {
            if (active_ && bus_) {
                bus_->unsubscribe(type_, id_);
                active_ = false;
            }
        }
        
        bool isActive() const { return active_; }
        
    private:
        EventBus* bus_ = nullptr;
        std::type_index type_ = std::type_index(typeid(void));
        SubscriptionId id_ = 0;
        bool active_ = false;
    };
    
    // ========================================================================
    // MÉTHODES PUBLIQUES
    // ========================================================================
    
    EventBus() : nextId_(1) {}
    
    /**
     * @brief S'abonner à un événement avec désinscription automatique
     * 
     * @return Subscription RAII handle (CONSERVER pour maintenir la souscription)
     * 
     * @example
     * ```cpp
     * auto sub = bus.subscribe<MidiNoteEvent>([](const MidiNoteEvent& e) {
     *     std::cout << "Note: " << e.note << std::endl;
     * }, EventBus::HIGH);
     * // Désinscription automatique quand sub sort de scope
     * ```
     */
    template<typename EventType>
    [[nodiscard]] Subscription subscribe(Handler<EventType> handler, int priority = NORMAL) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto typeIdx = std::type_index(typeid(EventType));
        SubscriptionId id = nextId_++;
        
        HandlerInfo info;
        info.id = id;
        info.priority = priority;
        info.handler = [handler](const std::any& event) {
            try {
                handler(std::any_cast<const EventType&>(event));
            } catch (...) {
                // Ignorer les erreurs de cast
            }
        };
        
        handlers_[typeIdx].push_back(info);
        
        // Trier par priorité (décroissant)
        std::sort(handlers_[typeIdx].begin(), handlers_[typeIdx].end(),
            [](const HandlerInfo& a, const HandlerInfo& b) {
                return a.priority > b.priority;
            });
        
        return Subscription(this, typeIdx, id);
    }
    
    /**
     * @brief Publier un événement
     * 
     * @return Nombre de handlers exécutés
     */
    template<typename EventType>
    size_t publish(const EventType& event) {
        std::vector<std::function<void(const std::any&)>> handlersToCall;
        
        // Copier les handlers sous lock
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto typeIdx = std::type_index(typeid(EventType));
            auto it = handlers_.find(typeIdx);
            
            if (it != handlers_.end()) {
                for (const auto& info : it->second) {
                    handlersToCall.push_back(info.handler);
                }
            }
        }
        
        // Exécuter hors lock
        size_t count = 0;
        for (const auto& handler : handlersToCall) {
            try {
                handler(std::any(event));
                count++;
            } catch (...) {
                // Ignorer les exceptions dans les handlers
            }
        }
        
        return count;
    }
    
    /**
     * @brief Vérifier si des handlers existent pour un type
     */
    template<typename EventType>
    bool hasHandlers() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto typeIdx = std::type_index(typeid(EventType));
        auto it = handlers_.find(typeIdx);
        
        return it != handlers_.end() && !it->second.empty();
    }
    
    /**
     * @brief Obtenir le nombre de handlers pour un type
     */
    template<typename EventType>
    size_t getHandlerCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto typeIdx = std::type_index(typeid(EventType));
        auto it = handlers_.find(typeIdx);
        
        return it != handlers_.end() ? it->second.size() : 0;
    }
    
    /**
     * @brief Effacer tous les handlers
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
    }
    
    /**
     * @brief Effacer tous les handlers d'un type
     */
    template<typename EventType>
    void clearType() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto typeIdx = std::type_index(typeid(EventType));
        handlers_.erase(typeIdx);
    }

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    void unsubscribe(std::type_index type, SubscriptionId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = handlers_.find(type);
        if (it != handlers_.end()) {
            auto& vec = it->second;
            vec.erase(
                std::remove_if(vec.begin(), vec.end(),
                    [id](const HandlerInfo& info) { return info.id == id; }
                ),
                vec.end()
            );
            
            if (vec.empty()) {
                handlers_.erase(it);
            }
        }
    }
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    struct HandlerInfo {
        SubscriptionId id;
        int priority;
        std::function<void(const std::any&)> handler;
    };
    
    mutable std::mutex mutex_;
    std::unordered_map<std::type_index, std::vector<HandlerInfo>> handlers_;
    std::atomic<SubscriptionId> nextId_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER EventBus.h
// ============================================================================