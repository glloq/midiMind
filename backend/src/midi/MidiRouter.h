// ============================================================================
// Fichier: backend/src/midi/MidiRouter.h
// VERSION AVEC VALIDATION SYSEX
// Date: 06/10/2025
// ============================================================================

#pragma once

#include "MidiMessage.h"
#include "devices/MidiDevice.h"
#include "sysex/SysExHandler.h"
#include <memory>
#include <vector>
#include <map>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @brief Routeur MIDI intelligent avec validation SysEx
 * 
 * Le MidiRouter gère le routage des messages MIDI entre devices avec:
 * - Validation basée sur NoteMap (notes jouables)
 * - Validation basée sur CCCapabilities (CC supportés)
 * - Filtrage par canal et type de message
 * - Priorités de routes
 * - Thread-safe avec shared_mutex
 * 
 * @note Thread-safety:
 * - route() : Lecture partagée (plusieurs threads simultanés OK)
 * - addRoute() : Écriture exclusive (bloque toute lecture/écriture)
 * 
 * @example Utilisation
 * ```cpp
 * MidiRouter router;
 * router.setSysExHandler(sysexHandler);
 * 
 * // Thread 1 : Routing (lecture)
 * router.route(message);  // Lecture partagée
 * 
 * // Thread 2 : Modification (écriture)
 * router.addRoute(route);  // Écriture exclusive
 * ```
 */

struct RouteStats {
    std::string routeId;
    std::string routeName;
    uint64_t messagesRouted;
    uint64_t messagesFiltered;
    uint64_t lastActivity;  // timestamp
    bool isActive;
};

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
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Configure le SysExHandler pour la validation
     * 
     * @param handler SysExHandler partagé
     * 
     * @note Doit être appelé AVANT de commencer le routage
     */
    void setSysExHandler(std::shared_ptr<SysExHandler> handler);
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    /**
     * @brief Valide un message MIDI avant routage
     * 
     * Vérifie:
     * - Note jouable (via NoteMap du SysExHandler)
     * - CC supporté (via CCCapabilities du SysExHandler)
     * 
     * @param message Message à valider
     * @param deviceId ID du device destination
     * @return true si message valide, false sinon
     * 
     * @note Thread-safe
     * @note Si pas de SysExHandler, retourne toujours true
     */
    bool validateMessage(const MidiMessage& message, const std::string& deviceId);
    
    // ========================================================================
    // ROUTING
    // ========================================================================
    
    /**
     * @brief Route un message MIDI
     * 
     * @param message Message à router
     * 
     * @note Thread-safe (lecture partagée avec shared_lock)
     * @note Valide automatiquement avant envoi
     */
    void route(const MidiMessage& message);
    
    /**
     * @brief Route un message vers un device spécifique
     * 
     * @param message Message
     * @param deviceId ID du device destination
     * 
     * @note Thread-safe
     * @note Valide automatiquement avant envoi
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
     * @brief Supprime une route
     * 
     * @param id ID de la route
     * @return true si supprimée, false si non trouvée
     * 
     * @note Thread-safe (écriture exclusive)
     */
    bool removeRoute(const std::string& id);
    
    /**
     * @brief Récupère une route par ID
     * 
     * @param id ID de la route
     * @return Route ou nullptr
     * 
     * @note Thread-safe (lecture partagée)
     */
    std::shared_ptr<MidiRoute> getRoute(const std::string& id) const;
    
    /**
     * @brief Récupère toutes les routes
     * 
     * @return Vecteur de routes
     * 
     * @note Thread-safe (lecture partagée)
     */
    std::vector<std::shared_ptr<MidiRoute>> getRoutes() const;
    
    /**
     * @brief Active/Désactive une route
     * 
     * @param id ID de la route
     * @param enabled État
     * 
     * @note Thread-safe (écriture exclusive)
     */
    void setRouteEnabled(const std::string& id, bool enabled);
    
    /**
     * @brief Supprime toutes les routes
     * 
     * @note Thread-safe (écriture exclusive)
     */
    void clearRoutes();
    
    // ========================================================================
    // GESTION DES DEVICES
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
     * @brief Récupère un device par ID
     * 
     * @param id ID du device
     * @return Device ou nullptr
     * 
     * @note Thread-safe (lecture partagée)
     */
    std::shared_ptr<MidiDevice> getDevice(const std::string& id) const;
    
    /**
     * @brief Récupère tous les devices
     * 
     * @return Vecteur de devices
     * 
     * @note Thread-safe (lecture partagée)
     */
    std::vector<std::shared_ptr<MidiDevice>> getDevices() const;
    
    // ========================================================================
    // CALLBACK
    // ========================================================================
    
    /**
     * @brief Configure le callback de message
     * 
     * @param callback Callback à appeler pour chaque message routé
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
     * @return JSON avec statistiques complètes
     * 
     * @note Thread-safe
     * 
     * @example Format JSON
     * ```json
     * {
     *   "messages_routed": 15234,
     *   "messages_dropped": 42,
     *   "messages_validated": 15276,
     *   "validation": {
     *     "notes_blocked": 23,
     *     "cc_blocked": 19,
     *     "total_blocked": 42,
     *     "block_rate": 0.27
     *   },
     *   "routes": {
     *     "count": 8,
     *     "enabled": 6,
     *     "disabled": 2
     *   },
     *   "devices": {
     *     "count": 3
     *   }
     * }
     * ```
     */
    json getStats() const;
    
    /**
     * @brief Réinitialise les statistiques
     * 
     * @note Thread-safe
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
    
    // Routes et devices
    std::vector<std::shared_ptr<MidiRoute>> routes_;
    std::map<std::string, std::shared_ptr<MidiDevice>> devices_;
    
    // Thread-safety
    mutable std::shared_mutex routesMutex_;  // Pour routes_
    mutable std::shared_mutex devicesMutex_; // Pour devices_
    mutable std::mutex callbackMutex_;       // Pour callback_
    
    // Callback
    MessageCallback messageCallback_;
    
    // Statistiques (atomic pour thread-safety sans lock)
    std::atomic<uint64_t> messagesRouted_;
    std::atomic<uint64_t> messagesDropped_;
    std::atomic<uint64_t> notesBlocked_;
    std::atomic<uint64_t> ccBlocked_;
    std::atomic<uint64_t> messagesValidated_;
    
    // SysExHandler pour validation
    std::shared_ptr<SysExHandler> sysexHandler_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiRouter.h
// ============================================================================
