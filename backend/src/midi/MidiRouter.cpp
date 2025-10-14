// ============================================================================
// Fichier: backend/src/midi/MidiRouter.cpp
// VERSION AVEC VALIDATION SYSEX
// Date: 06/10/2025
// ============================================================================

#include "MidiRouter.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

MidiRouter::MidiRouter()
    : messagesRouted_(0)
    , messagesDropped_(0)
    , notesBlocked_(0)
    , ccBlocked_(0)
    , messagesValidated_(0)
    , sysexHandler_(nullptr)
{
    Logger::info("MidiRouter", "MidiRouter constructed");
}

MidiRouter::~MidiRouter() {
    Logger::info("MidiRouter", "MidiRouter destroyed");
    Logger::info("MidiRouter", "  Messages routed: " + std::to_string(messagesRouted_));
    Logger::info("MidiRouter", "  Messages dropped: " + std::to_string(messagesDropped_));
    Logger::info("MidiRouter", "  Notes blocked: " + std::to_string(notesBlocked_));
    Logger::info("MidiRouter", "  CC blocked: " + std::to_string(ccBlocked_));
}

// ============================================================================
// CONFIGURATION SYSEXHANDLER
// ============================================================================

void MidiRouter::setSysExHandler(std::shared_ptr<SysExHandler> handler) {
    sysexHandler_ = handler;
    Logger::info("MidiRouter", "SysExHandler configured for validation");
}

// ============================================================================
// VALIDATION DES MESSAGES
// ============================================================================

bool MidiRouter::validateMessage(const MidiMessage& message, 
                                 const std::string& deviceId) {
    if (!sysexHandler_) {
        // Pas de SysExHandler = pas de validation
        return true;
    }
    
    messagesValidated_++;
    
    // ========================================================================
    // VALIDATION NOTE ON/OFF
    // ========================================================================
    if (message.getType() == MidiMessageType::NOTE_ON || 
        message.getType() == MidiMessageType::NOTE_OFF) {
        
        auto noteMap = sysexHandler_->getNoteMap(deviceId);
        
        if (noteMap.has_value()) {
            uint8_t note = message.getData()[0];  // Note number
            
            if (!noteMap->isNotePlayable(note)) {
                notesBlocked_++;
                
                Logger::debug("MidiRouter", 
                    "❌ Blocked note " + std::to_string(note) + 
                    " (not playable on " + deviceId + ")");
                
                return false;
            }
        }
    }
    
    // ========================================================================
    // VALIDATION CONTROL CHANGE
    // ========================================================================
    if (message.getType() == MidiMessageType::CONTROL_CHANGE) {
        auto ccCaps = sysexHandler_->getCCCapabilities(deviceId);
        
        if (ccCaps.has_value()) {
            uint8_t ccNumber = message.getData()[0];  // CC number
            
            if (!ccCaps->isSupported(ccNumber)) {
                ccBlocked_++;
                
                Logger::debug("MidiRouter", 
                    "❌ Blocked CC " + std::to_string(ccNumber) + 
                    " (not supported on " + deviceId + ")");
                
                return false;
            }
        }
    }
    
    // ========================================================================
    // VALIDATION PITCH BEND (Optionnel - si capabilities définies)
    // ========================================================================
    // TODO: Ajouter validation Pitch Bend si nécessaire
    
    return true;
}

// ============================================================================
// ROUTING
// ============================================================================

void MidiRouter::route(const MidiMessage& message) {
    // Lecture partagée : plusieurs threads peuvent router en même temps
    std::shared_lock<std::shared_mutex> lock(routesMutex_);
    
    bool routed = false;
    
    // Trier par priorité (plus élevé en premier)
    std::vector<std::shared_ptr<MidiRoute>> sortedRoutes = routes_;
    std::sort(sortedRoutes.begin(), sortedRoutes.end(),
        [](const auto& a, const auto& b) { return a->priority > b->priority; });
    
    for (const auto& route : sortedRoutes) {
        if (!route->enabled) continue;
        
        if (matchesRoute(message, *route)) {
            // ✅ VALIDATION AVANT ENVOI
            if (validateMessage(message, route->destinationDeviceId)) {
                sendToDevice(message, route->destinationDeviceId);
                routed = true;
                messagesRouted_++;
            } else {
                // Message bloqué par validation
                messagesDropped_++;
            }
        }
    }
    
    if (!routed) {
        messagesDropped_++;
        Logger::debug("MidiRouter", "Message dropped (no matching route or blocked)");
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
    // ✅ VALIDATION DIRECTE
    if (!validateMessage(message, deviceId)) {
        messagesDropped_++;
        return;
    }
    
    sendToDevice(message, deviceId);
    messagesRouted_++;
}

// ============================================================================
// GESTION DES ROUTES
// ============================================================================

void MidiRouter::addRoute(std::shared_ptr<MidiRoute> route) {
    // Écriture exclusive
    std::unique_lock<std::shared_mutex> lock(routesMutex_);
    
    // Générer un ID si vide
    if (route->id.empty()) {
        route->id = StringUtils::generateUuid();
    }
    
    routes_.push_back(route);
    
    Logger::info("MidiRouter", "Route added: " + route->name + " (ID: " + route->id + ")");
}

bool MidiRouter::removeRoute(const std::string& id) {
    // Écriture exclusive
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
    // Lecture partagée
    std::shared_lock<std::shared_mutex> lock(routesMutex_);
    
    auto it = std::find_if(routes_.begin(), routes_.end(),
        [&id](const auto& route) { return route->id == id; });
    
    return it != routes_.end() ? *it : nullptr;
}

std::vector<std::shared_ptr<MidiRoute>> MidiRouter::getRoutes() const {
    // Lecture partagée
    std::shared_lock<std::shared_mutex> lock(routesMutex_);
    return routes_;
}

void MidiRouter::clearAllRoutes() {
    // Écriture exclusive
    std::unique_lock<std::shared_mutex> lock(routesMutex_);
    
    size_t count = routes_.size();
    routes_.clear();
    
    Logger::info("MidiRouter", 
        "Cleared " + std::to_string(count) + " routes");
}

RouteStats MidiRouter::getRouteStats(const std::string& routeId) const {
    std::shared_lock<std::shared_mutex> lock(routesMutex_);
    
    auto route = getRoute(routeId);
    if (!route) {
        THROW_ERROR(ErrorCode::ROUTE_NOT_FOUND, "Route not found: " + routeId);
    }
    
    RouteStats stats;
    stats.routeId = route->id;
    stats.routeName = route->name;
    stats.messagesRouted = 0;  // TODO: Tracker ces stats
    stats.messagesFiltered = 0;
    stats.lastActivity = 0;
    stats.isActive = route->enabled;
    
    return stats;
}


void MidiRouter::setRouteEnabled(const std::string& id, bool enabled) {
    // Écriture exclusive
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
    // Écriture exclusive
    std::unique_lock<std::shared_mutex> lock(routesMutex_);
    
    routes_.clear();
    Logger::info("MidiRouter", "All routes cleared");
}

// ============================================================================
// GESTION DES DEVICES
// ============================================================================

void MidiRouter::registerDevice(std::shared_ptr<MidiDevice> device) {
    // Écriture exclusive
    std::unique_lock<std::shared_mutex> lock(devicesMutex_);
    
    devices_[device->getId()] = device;
    Logger::info("MidiRouter", "Device registered: " + device->getName());
}

void MidiRouter::unregisterDevice(const std::string& deviceId) {
    // Écriture exclusive
    std::unique_lock<std::shared_mutex> lock(devicesMutex_);
    
    auto it = devices_.find(deviceId);
    if (it != devices_.end()) {
        Logger::info("MidiRouter", "Device unregistered: " + it->second->getName());
        devices_.erase(it);
    }
}

std::shared_ptr<MidiDevice> MidiRouter::getDevice(const std::string& id) const {
    // Lecture partagée
    std::shared_lock<std::shared_mutex> lock(devicesMutex_);
    
    auto it = devices_.find(id);
    return it != devices_.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<MidiDevice>> MidiRouter::getDevices() const {
    // Lecture partagée
    std::shared_lock<std::shared_mutex> lock(devicesMutex_);
    
    std::vector<std::shared_ptr<MidiDevice>> deviceList;
    for (const auto& [id, device] : devices_) {
        deviceList.push_back(device);
    }
    return deviceList;
}

// ============================================================================
// CALLBACK
// ============================================================================

void MidiRouter::setMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    messageCallback_ = callback;
    Logger::info("MidiRouter", "Message callback configured");
}

// ============================================================================
// STATISTIQUES
// ============================================================================

json MidiRouter::getStats() const {
    json stats;
    
    // Statistiques de base
    stats["messages_routed"] = messagesRouted_.load();
    stats["messages_dropped"] = messagesDropped_.load();
    stats["messages_validated"] = messagesValidated_.load();
    
    // Statistiques de validation
    stats["validation"]["notes_blocked"] = notesBlocked_.load();
    stats["validation"]["cc_blocked"] = ccBlocked_.load();
    stats["validation"]["total_blocked"] = 
        notesBlocked_.load() + ccBlocked_.load();
    
    // Pourcentages
    uint64_t totalMessages = messagesRouted_.load() + messagesDropped_.load();
    if (totalMessages > 0) {
        stats["validation"]["block_rate"] = 
            (double)(notesBlocked_.load() + ccBlocked_.load()) / totalMessages * 100.0;
    } else {
        stats["validation"]["block_rate"] = 0.0;
    }
    
    // Statistiques des routes
    {
        std::shared_lock<std::shared_mutex> lock(routesMutex_);
        stats["routes"]["count"] = routes_.size();
        
        int enabledCount = 0;
        for (const auto& route : routes_) {
            if (route->enabled) enabledCount++;
        }
        stats["routes"]["enabled"] = enabledCount;
        stats["routes"]["disabled"] = routes_.size() - enabledCount;
    }
    
    // Statistiques des devices
    {
        std::shared_lock<std::shared_mutex> lock(devicesMutex_);
        stats["devices"]["count"] = devices_.size();
    }
    
    return stats;
}

void MidiRouter::resetStatistics() {
    messagesRouted_ = 0;
    messagesDropped_ = 0;
    notesBlocked_ = 0;
    ccBlocked_ = 0;
    messagesValidated_ = 0;
    
    Logger::info("MidiRouter", "Statistics reset");
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

bool MidiRouter::matchesRoute(const MidiMessage& message, 
                              const MidiRoute& route) const {
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

void MidiRouter::sendToDevice(const MidiMessage& message, 
                              const std::string& deviceId) {
    // Lecture partagée pour devices
    std::shared_lock<std::shared_mutex> lock(devicesMutex_);
    
    auto it = devices_.find(deviceId);
    if (it != devices_.end() && it->second->isOpen()) {
        try {
            it->second->send(message);
        } catch (const std::exception& e) {
            Logger::error("MidiRouter", 
                "Failed to send to device " + deviceId + ": " + 
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
