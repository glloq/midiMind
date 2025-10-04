// ============================================================================
// Fichier: src/midi/MidiRouter.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "MidiRouter.h"
#include "../core/StringUtils.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

MidiRouter::MidiRouter()
    : messagesRouted_(0)
    , messagesDropped_(0) {
    
    Logger::info("MidiRouter", "MidiRouter constructed");
}

MidiRouter::~MidiRouter() {
    Logger::info("MidiRouter", "MidiRouter destroyed");
    Logger::info("MidiRouter", "  Total messages routed: " + std::to_string(messagesRouted_));
    Logger::info("MidiRouter", "  Total messages dropped: " + std::to_string(messagesDropped_));
}

// ============================================================================
// ROUTING
// ============================================================================

void MidiRouter::route(const MidiMessage& message) {
    // ✅ LECTURE PARTAGÉE : plusieurs threads peuvent router en même temps
    std::shared_lock<std::shared_mutex> lock(routesMutex_);
    
    bool routed = false;
    
    // Trier par priorité (plus élevé en premier)
    std::vector<std::shared_ptr<MidiRoute>> sortedRoutes = routes_;
    std::sort(sortedRoutes.begin(), sortedRoutes.end(),
        [](const auto& a, const auto& b) { return a->priority > b->priority; });
    
    for (const auto& route : sortedRoutes) {
        if (!route->enabled) continue;
        
        if (matchesRoute(message, *route)) {
            sendToDevice(message, route->destinationDeviceId);
            routed = true;
            messagesRouted_++;
        }
    }
    
    if (!routed) {
        messagesDropped_++;
        Logger::debug("MidiRouter", "Message dropped (no matching route)");
    }
    
    // Callback
    if (messageCallback_) {
        std::lock_guard<std::mutex> cbLock(callbackMutex_);
        try {
            messageCallback_(message);
        } catch (const std::exception& e) {
            Logger::error("MidiRouter", "Callback exception: " + std::string(e.what()));
        }
    }
}

void MidiRouter::routeTo(const MidiMessage& message, const std::string& deviceId) {
    sendToDevice(message, deviceId);
    messagesRouted_++;
}

// ============================================================================
// GESTION DES ROUTES
// ============================================================================

void MidiRouter::addRoute(std::shared_ptr<MidiRoute> route) {
    // ✅ ÉCRITURE EXCLUSIVE : aucun autre thread ne peut lire/écrire
    std::unique_lock<std::shared_mutex> lock(routesMutex_);
    
    // Générer un ID si vide
    if (route->id.empty()) {
        route->id = StringUtils::generateUuid();
    }
    
    routes_.push_back(route);
    
    Logger::info("MidiRouter", "Route added: " + route->name + " (ID: " + route->id + ")");
}

bool MidiRouter::removeRoute(const std::string& id) {
    // ✅ ÉCRITURE EXCLUSIVE
    std::unique_lock<std::shared_mutex> lock(routesMutex_);
    
    auto it = std::find_if(routes_.begin(), routes_.end(),
        [&id](const auto& route) { return route->id == id; });
    
    if (it != routes_.end()) {
        Logger::info("MidiRouter", "Route removed: " + (*it)->name);
        routes_.erase(it);
        return true;
    }
    
    return false;
}

std::shared_ptr<MidiRoute> MidiRouter::getRoute(const std::string& id) const {
    // ✅ LECTURE PARTAGÉE
    std::shared_lock<std::shared_mutex> lock(routesMutex_);
    
    auto it = std::find_if(routes_.begin(), routes_.end(),
        [&id](const auto& route) { return route->id == id; });
    
    return it != routes_.end() ? *it : nullptr;
}

std::vector<std::shared_ptr<MidiRoute>> MidiRouter::getRoutes() const {
    // ✅ LECTURE PARTAGÉE
    std::shared_lock<std::shared_mutex> lock(routesMutex_);
    return routes_;
}

void MidiRouter::setRouteEnabled(const std::string& id, bool enabled) {
    // ✅ ÉCRITURE EXCLUSIVE
    std::unique_lock<std::shared_mutex> lock(routesMutex_);
    
    auto it = std::find_if(routes_.begin(), routes_.end(),
        [&id](const auto& route) { return route->id == id; });
    
    if (it != routes_.end()) {
        (*it)->enabled = enabled;
        Logger::info("MidiRouter", "Route " + (*it)->name + " " + 
                    (enabled ? "enabled" : "disabled"));
    }
}

void MidiRouter::clearRoutes() {
    // ✅ ÉCRITURE EXCLUSIVE
    std::unique_lock<std::shared_mutex> lock(routesMutex_);
    
    size_t count = routes_.size();
    routes_.clear();
    
    Logger::info("MidiRouter", "Cleared " + std::to_string(count) + " routes");
}

size_t MidiRouter::getRouteCount() const {
    // ✅ LECTURE PARTAGÉE
    std::shared_lock<std::shared_mutex> lock(routesMutex_);
    return routes_.size();
}

// ============================================================================
// DEVICES
// ============================================================================

void MidiRouter::registerDevice(std::shared_ptr<MidiDevice> device) {
    // ✅ ÉCRITURE EXCLUSIVE
    std::unique_lock<std::shared_mutex> lock(devicesMutex_);
    
    devices_[device->getName()] = device;
    
    Logger::info("MidiRouter", "Device registered: " + device->getName());
}

void MidiRouter::unregisterDevice(const std::string& deviceId) {
    // ✅ ÉCRITURE EXCLUSIVE
    std::unique_lock<std::shared_mutex> lock(devicesMutex_);
    
    devices_.erase(deviceId);
    
    Logger::info("MidiRouter", "Device unregistered: " + deviceId);
}

std::shared_ptr<MidiDevice> MidiRouter::getDevice(const std::string& deviceId) const {
    // ✅ LECTURE PARTAGÉE
    std::shared_lock<std::shared_mutex> lock(devicesMutex_);
    
    auto it = devices_.find(deviceId);
    return it != devices_.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<MidiDevice>> MidiRouter::getDevices() const {
    // ✅ LECTURE PARTAGÉE
    std::shared_lock<std::shared_mutex> lock(devicesMutex_);
    
    std::vector<std::shared_ptr<MidiDevice>> result;
    for (const auto& [id, device] : devices_) {
        result.push_back(device);
    }
    return result;
}

// ============================================================================
// CALLBACKS
// ============================================================================

void MidiRouter::setMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    messageCallback_ = callback;
}

// ============================================================================
// STATISTIQUES
// ============================================================================

json MidiRouter::getStatistics() const {
    // ✅ LECTURE PARTAGÉE pour routes
    std::shared_lock<std::shared_mutex> routesLock(routesMutex_);
    
    json stats;
    stats["routes_count"] = routes_.size();
    stats["messages_routed"] = messagesRouted_.load();
    stats["messages_dropped"] = messagesDropped_.load();
    
    return stats;
}

void MidiRouter::resetStatistics() {
    messagesRouted_ = 0;
    messagesDropped_ = 0;
    
    Logger::info("MidiRouter", "Statistics reset");
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

bool MidiRouter::matchesRoute(const MidiMessage& message, const MidiRoute& route) const {
    // Vérifier le canal
    if (!route.channelFilter.empty()) {
        uint8_t channel = message.getChannel();
        if (std::find(route.channelFilter.begin(), route.channelFilter.end(), channel) 
            == route.channelFilter.end()) {
            return false;
        }
    }
    
    // Vérifier le type de message
    if (!route.messageTypeFilter.empty()) {
        std::string msgType = MidiMessage::messageTypeToString(message.getType());
        if (std::find(route.messageTypeFilter.begin(), route.messageTypeFilter.end(), msgType)
            == route.messageTypeFilter.end()) {
            return false;
        }
    }
    
    return true;
}

void MidiRouter::sendToDevice(const MidiMessage& message, const std::string& deviceId) {
    // ✅ LECTURE PARTAGÉE pour devices
    std::shared_lock<std::shared_mutex> lock(devicesMutex_);
    
    auto it = devices_.find(deviceId);
    if (it != devices_.end() && it->second->isOpen()) {
        try {
            it->second->send(message);
        } catch (const std::exception& e) {
            Logger::error("MidiRouter", "Failed to send to device " + deviceId + ": " + 
                         std::string(e.what()));
        }
    } else {
        Logger::warn("MidiRouter", "Device not found or not open: " + deviceId);
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiRouter.cpp
// ============================================================================