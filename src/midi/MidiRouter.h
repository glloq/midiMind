// ============================================================================
// Fichier: src/midi/MidiRouter.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Routeur MIDI central avec matrice de routage flexible.
//   CORRIGÉ : Utilise shared_mutex pour éviter les race conditions.
//
// Thread-safety: OUI (shared_mutex pour lectures/écritures)
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.1 (FIXED)
// ============================================================================

#pragma once

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <mutex>
#include <shared_mutex>  
#include <functional>

#include "MidiMessage.h"
#include "devices/MidiDevice.h"
#include "../core/Logger.h"
#include "../core/EventBus.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @struct MidiRoute
 * @brief Définition d'une route MIDI
 */
struct MidiRoute {
    std::string id;                             ///< Identifiant unique
    std::string name;                           ///< Nom de la route
    bool enabled;                               ///< Route active ?
    
    std::string sourceDeviceId;                 ///< Device source
    std::string destinationDeviceId;            ///< Device destination
    
    std::vector<uint8_t> channelFilter;        ///< Canaux à router (vide = tous)
    std::vector<std::string> messageTypeFilter; ///< Types de messages
    
    int priority;                               ///< Priorité (plus élevé = prioritaire)
    
    MidiRoute() : enabled(true), priority(100) {}
    
    json toJson() const {
        json j;
        j["id"] = id;
        j["name"] = name;
        j["enabled"] = enabled;
        j["source"] = sourceDeviceId;
        j["destination"] = destinationDeviceId;
        j["channel_filter"] = channelFilter;
        j["message_type_filter"] = messageTypeFilter;
        j["priority"] = priority;
        return j;
    }
};

/**
 * @class MidiRouter
 * @brief Routeur MIDI central avec protection thread-safe
 * 
 * @details
 * CORRECTION v3.0.1 :
 * - Utilise std::shared_mutex au lieu de std::mutex
 * - Permet lectures concurrentes (route())
 * - Bloque les écritures (addRoute, removeRoute)
 * - Élimine les race conditions
 * 
 * Thread-safety: Toutes les méthodes sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * MidiRouter router;
 * 
 * // Thread 1 : Routing (lecture)
 * router.route(message);  // Lecture partagée
 * 
 * // Thread 2 : Modification (écriture)
 * router.addRoute(route);  // Écriture exclusive
 * ```
 */
class MidiRouter {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback pour les messages routés
     */
    using MessageCallback = std::function<void(const MidiMessage&)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    MidiRouter();
    
    /**
     * @brief Destructeur
     */
    ~MidiRouter();
    
    // Désactiver copie
    MidiRouter(const MidiRouter&) = delete;
    MidiRouter& operator=(const MidiRouter&) = delete;
    
    // ========================================================================
    // ROUTING
    // ========================================================================
    
    /**
     * @brief Route un message MIDI
     * 
     * @param message Message à router
     * 
     * @note Thread-safe (lecture partagée avec shared_lock)
     */
    void route(const MidiMessage& message);
    
    /**
     * @brief Route un message vers un device spécifique
     * 
     * @param message Message
     * @param deviceId ID du device destination
     * 
     * @note Thread-safe
     */
    void routeTo(const MidiMessage& message, const std::string& deviceId);
    
    // ========================================================================
    // GESTION DES ROUTES
    // ========================================================================
    
    /**
     * @brief Ajoute une route
     * 
     * @param route Route à ajouter
     * 
     * @note Thread-safe (écriture exclusive avec unique_lock)
     */
    void addRoute(std::shared_ptr<MidiRoute> route);
    
    /**
     * @brief Retire une route
     * 
     * @param id ID de la route
     * @return true Si la route a été retirée
     * 
     * @note Thread-safe (écriture exclusive)
     */
    bool removeRoute(const std::string& id);
    
    /**
     * @brief Récupère une route
     * 
     * @param id ID de la route
     * @return std::shared_ptr<MidiRoute> Route ou nullptr
     * 
     * @note Thread-safe (lecture partagée)
     */
    std::shared_ptr<MidiRoute> getRoute(const std::string& id) const;
    
    /**
     * @brief Récupère toutes les routes
     * 
     * @return std::vector<std::shared_ptr<MidiRoute>> Liste des routes
     * 
     * @note Thread-safe (lecture partagée)
     */
    std::vector<std::shared_ptr<MidiRoute>> getRoutes() const;
    
    /**
     * @brief Active/désactive une route
     * 
     * @param id ID de la route
     * @param enabled true pour activer
     * 
     * @note Thread-safe (écriture exclusive)
     */
    void setRouteEnabled(const std::string& id, bool enabled);
    
    /**
     * @brief Efface toutes les routes
     * 
     * @note Thread-safe (écriture exclusive)
     */
    void clearRoutes();
    
    /**
     * @brief Récupère le nombre de routes
     * 
     * @note Thread-safe (lecture partagée)
     */
    size_t getRouteCount() const;
    
    // ========================================================================
    // DEVICES
    // ========================================================================
    
    /**
     * @brief Enregistre un device
     * 
     * @param device Device à enregistrer
     * 
     * @note Thread-safe (écriture exclusive)
     */
    void registerDevice(std::shared_ptr<MidiDevice> device);
    
    /**
     * @brief Désenregistre un device
     * 
     * @param deviceId ID du device
     * 
     * @note Thread-safe (écriture exclusive)
     */
    void unregisterDevice(const std::string& deviceId);
    
    /**
     * @brief Récupère un device
     * 
     * @param deviceId ID du device
     * @return std::shared_ptr<MidiDevice> Device ou nullptr
     * 
     * @note Thread-safe (lecture partagée)
     */
    std::shared_ptr<MidiDevice> getDevice(const std::string& deviceId) const;
    
    /**
     * @brief Récupère tous les devices
     * 
     * @note Thread-safe (lecture partagée)
     */
    std::vector<std::shared_ptr<MidiDevice>> getDevices() const;
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback pour les messages routés
     * 
     * @note Thread-safe
     */
    void setMessageCallback(MessageCallback callback);
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques
     * 
     * @note Thread-safe (lecture partagée)
     */
    json getStatistics() const;
    
    /**
     * @brief Réinitialise les statistiques
     * 
     * @note Thread-safe (écriture exclusive)
     */
    void resetStatistics();

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Vérifie si un message correspond à une route
     */
    bool matchesRoute(const MidiMessage& message, const MidiRoute& route) const;
    
    /**
     * @brief Envoie un message à un device
     */
    void sendToDevice(const MidiMessage& message, const std::string& deviceId);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex shared pour routes (lecture/écriture séparée)
    /// ✅ CORRIGÉ : shared_mutex au lieu de mutex
    mutable std::shared_mutex routesMutex_;
    
    /// Mutex shared pour devices
    /// ✅ CORRIGÉ : shared_mutex au lieu de mutex
    mutable std::shared_mutex devicesMutex_;
    
    /// Routes
    std::vector<std::shared_ptr<MidiRoute>> routes_;
    
    /// Devices enregistrés
    std::map<std::string, std::shared_ptr<MidiDevice>> devices_;
    
    /// Callback pour messages routés
    MessageCallback messageCallback_;
    
    /// Mutex pour le callback
    std::mutex callbackMutex_;
    
    /// Statistiques
    std::atomic<uint64_t> messagesRouted_;
    std::atomic<uint64_t> messagesDropped_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiRouter.h
// ============================================================================