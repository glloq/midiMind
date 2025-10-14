// ============================================================================
// Fichier: backend/src/core/EventBus.cpp
// Version: 3.0.1 - COMPLET
// Date: 2025-10-13
// ============================================================================
// Description:
//   Bus d'événements pub/sub pour communication inter-modules
//   Implémentation thread-safe avec filtrage et priorités
//
// CORRECTIONS v3.0.1:
//   ✅ Implémentation complète de tous les templates
//   ✅ Méthode unsubscribe() implémentée
//   ✅ Méthode publishImpl() complète
//   ✅ Processeur asynchrone complet
//   ✅ Toutes les méthodes privées ajoutées
//
// Fonctionnalités:
//   - Pub/Sub type-safe
//   - Filtrage événements
//   - Priorités handlers
//   - Émission asynchrone
//   - Statistiques et monitoring
// ============================================================================

#include "EventBus.h"
#include "Logger.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// IMPL PIMPL
// ============================================================================

class EventBus::Impl {
public:
    // ========================================================================
    // TYPES INTERNES
    // ========================================================================
    
    struct HandlerInfo {
        uint64_t id;
        int priority;
        std::function<void(const std::any&)> handler;
        std::function<bool(const std::any&)> filter;
        std::weak_ptr<Subscription> subscription;
    };
    
    struct AsyncEvent {
        std::type_index typeIdx;
        std::any event;
        
        AsyncEvent(std::type_index idx, std::any evt)
            : typeIdx(idx), event(std::move(evt)) {}
    };
    
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    Impl() 
        : nextId_(1)
        , running_(true)
        , totalEventsPublished_(0)
        , totalEventsDelivered_(0) {
        
        // Démarrer le thread de traitement asynchrone
        asyncThread_ = std::thread(&Impl::asyncProcessor, this);
        
        Logger::debug("EventBus", "EventBus implementation created");
    }
    
    ~Impl() {
        // Arrêter le thread asynchrone
        {
            std::lock_guard<std::mutex> lock(asyncMutex_);
            running_ = false;
        }
        asyncCondition_.notify_all();
        
        if (asyncThread_.joinable()) {
            asyncThread_.join();
        }
        
        Logger::debug("EventBus", "EventBus implementation destroyed");
    }
    
    // ========================================================================
    // SOUSCRIPTION
    // ========================================================================
    
    template<typename EventType>
    SubscriptionHandle subscribeImpl(std::function<void(const EventType&)> handler,
                                    std::function<bool(const EventType&)> filter,
                                    int priority) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto typeIdx = std::type_index(typeid(EventType));
        uint64_t id = nextId_++;
        
        // Créer la souscription
        auto subscription = std::make_shared<Subscription>(
            id,
            [this, typeIdx, id]() { this->unsubscribe(typeIdx, id); }
        );
        
        // Créer l'info du handler
        HandlerInfo info;
        info.id = id;
        info.priority = priority;
        info.subscription = subscription;
        
        // Wrapper pour le handler typé
        info.handler = [handler](const std::any& event) {
            try {
                handler(std::any_cast<const EventType&>(event));
            } catch (const std::bad_any_cast& e) {
                Logger::error("EventBus", "Type mismatch in handler: " + std::string(e.what()));
            } catch (const std::exception& e) {
                Logger::error("EventBus", "Handler error: " + std::string(e.what()));
            }
        };
        
        // Wrapper pour le filter typé
        if (filter) {
            info.filter = [filter](const std::any& event) -> bool {
                try {
                    return filter(std::any_cast<const EventType&>(event));
                } catch (...) {
                    return false;
                }
            };
        } else {
            info.filter = [](const std::any&) { return true; };
        }
        
        // Ajouter le handler
        handlers_[typeIdx].push_back(info);
        
        // Trier par priorité (plus haute priorité en premier)
        std::sort(handlers_[typeIdx].begin(), handlers_[typeIdx].end(),
            [](const HandlerInfo& a, const HandlerInfo& b) {
                return a.priority > b.priority;
            });
        
        Logger::debug("EventBus", 
            "Subscribed handler " + std::to_string(id) + 
            " with priority " + std::to_string(priority));
        
        return subscription;
    }
    
    void unsubscribe(std::type_index typeIdx, uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = handlers_.find(typeIdx);
        if (it == handlers_.end()) {
            return;
        }
        
        auto& handlerList = it->second;
        handlerList.erase(
            std::remove_if(handlerList.begin(), handlerList.end(),
                [id](const HandlerInfo& info) {
                    return info.id == id;
                }),
            handlerList.end()
        );
        
        // Supprimer la clé si plus de handlers
        if (handlerList.empty()) {
            handlers_.erase(it);
        }
        
        Logger::debug("EventBus", "Unsubscribed handler " + std::to_string(id));
    }
    
    // ========================================================================
    // PUBLICATION
    // ========================================================================
    
    template<typename EventType>
    void publishImpl(const EventType& event, bool async) {
        auto typeIdx = std::type_index(typeid(EventType));
        
        totalEventsPublished_++;
        
        if (async) {
            // Mode asynchrone
            std::lock_guard<std::mutex> lock(asyncMutex_);
            asyncQueue_.emplace(typeIdx, event);
            asyncCondition_.notify_one();
            
        } else {
            // Mode synchrone
            deliverEvent(typeIdx, event);
        }
    }
    
    void deliverEvent(std::type_index typeIdx, const std::any& event) {
        std::vector<HandlerInfo> handlersCopy;
        
        // Copier les handlers sous lock
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = handlers_.find(typeIdx);
            if (it == handlers_.end()) {
                return; // Aucun handler pour ce type
            }
            
            // Copier uniquement les handlers valides (subscription encore active)
            for (const auto& info : it->second) {
                if (!info.subscription.expired()) {
                    handlersCopy.push_back(info);
                }
            }
        }
        
        // Appeler les handlers sans lock (évite deadlock)
        for (const auto& info : handlersCopy) {
            try {
                // Appliquer le filtre
                if (!info.filter(event)) {
                    continue;
                }
                
                // Appeler le handler
                info.handler(event);
                totalEventsDelivered_++;
                
            } catch (const std::exception& e) {
                Logger::error("EventBus", 
                    "Handler exception: " + std::string(e.what()));
            }
        }
    }
    
    // ========================================================================
    // PROCESSEUR ASYNCHRONE
    // ========================================================================
    
    void asyncProcessor() {
        Logger::debug("EventBus", "Async processor started");
        
        while (running_) {
            std::unique_lock<std::mutex> lock(asyncMutex_);
            
            // Attendre un événement ou le signal d'arrêt
            asyncCondition_.wait(lock, [this] {
                return !asyncQueue_.empty() || !running_;
            });
            
            // Traiter tous les événements en attente
            while (!asyncQueue_.empty() && running_) {
                auto asyncEvent = std::move(asyncQueue_.front());
                asyncQueue_.pop();
                
                // Libérer le lock pendant la livraison
                lock.unlock();
                
                deliverEvent(asyncEvent.typeIdx, asyncEvent.event);
                
                lock.lock();
            }
        }
        
        Logger::debug("EventBus", "Async processor stopped");
    }
    
    // ========================================================================
    // NETTOYAGE
    // ========================================================================
    
    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Supprimer les subscriptions expirées
        for (auto& pair : handlers_) {
            auto& handlerList = pair.second;
            
            handlerList.erase(
                std::remove_if(handlerList.begin(), handlerList.end(),
                    [](const HandlerInfo& info) {
                        return info.subscription.expired();
                    }),
                handlerList.end()
            );
        }
        
        // Supprimer les types sans handlers
        for (auto it = handlers_.begin(); it != handlers_.end(); ) {
            if (it->second.empty()) {
                it = handlers_.erase(it);
            } else {
                ++it;
            }
        }
        
        Logger::debug("EventBus", "Cleanup completed");
    }
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    size_t getSubscriberCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t count = 0;
        for (const auto& pair : handlers_) {
            count += pair.second.size();
        }
        return count;
    }
    
    size_t getEventTypeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return handlers_.size();
    }
    
    uint64_t getTotalEventsPublished() const {
        return totalEventsPublished_.load();
    }
    
    uint64_t getTotalEventsDelivered() const {
        return totalEventsDelivered_.load();
    }
    
    size_t getAsyncQueueSize() const {
        std::lock_guard<std::mutex> lock(asyncMutex_);
        return asyncQueue_.size();
    }
    
private:
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    // Handlers par type d'événement
    std::unordered_map<std::type_index, std::vector<HandlerInfo>> handlers_;
    
    // Mutex pour protéger handlers_
    mutable std::mutex mutex_;
    
    // ID auto-incrémentée pour les handlers
    uint64_t nextId_;
    
    // Thread et file asynchrone
    std::thread asyncThread_;
    std::queue<AsyncEvent> asyncQueue_;
    mutable std::mutex asyncMutex_;
    std::condition_variable asyncCondition_;
    std::atomic<bool> running_;
    
    // Statistiques
    std::atomic<uint64_t> totalEventsPublished_;
    std::atomic<uint64_t> totalEventsDelivered_;
};

// ============================================================================
// SUBSCRIPTION - Implémentation
// ============================================================================

EventBus::Subscription::Subscription(uint64_t id, std::function<void()> unsubscriber)
    : id_(id)
    , unsubscriber_(std::move(unsubscriber))
    , active_(true) {
}

EventBus::Subscription::~Subscription() {
    unsubscribe();
}

void EventBus::Subscription::unsubscribe() {
    if (active_.exchange(false)) {
        if (unsubscriber_) {
            unsubscriber_();
        }
    }
}

bool EventBus::Subscription::isActive() const {
    return active_.load();
}

uint64_t EventBus::Subscription::getId() const {
    return id_;
}

// ============================================================================
// EVENTBUS - Implémentation publique
// ============================================================================

EventBus::EventBus()
    : impl_(std::make_unique<Impl>()) {
    Logger::info("EventBus", "EventBus created");
}

EventBus::~EventBus() {
    Logger::info("EventBus", "EventBus destroyed");
}

void EventBus::cleanup() {
    impl_->cleanup();
}

size_t EventBus::getSubscriberCount() const {
    return impl_->getSubscriberCount();
}

size_t EventBus::getEventTypeCount() const {
    return impl_->getEventTypeCount();
}

uint64_t EventBus::getTotalEventsPublished() const {
    return impl_->getTotalEventsPublished();
}

uint64_t EventBus::getTotalEventsDelivered() const {
    return impl_->getTotalEventsDelivered();
}

size_t EventBus::getAsyncQueueSize() const {
    return impl_->getAsyncQueueSize();
}

// ============================================================================
// INSTANCIATION DES TEMPLATES (exemples courants)
// ============================================================================

// Note: Les templates sont instanciés à la demande par le compilateur
// Les méthodes subscribe/publish sont définies dans le header

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER EventBus.cpp v3.0.1 - COMPLET
// ============================================================================
