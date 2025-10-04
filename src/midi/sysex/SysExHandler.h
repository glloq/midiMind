// ============================================================================
// Fichier: src/midi/sysex/SysExHandler.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestionnaire principal des messages SysEx.
//   Coordonne le parsing, la génération, et les callbacks.
//   Gère l'identification automatique des devices.
//
// Responsabilités:
//   - Recevoir et parser les messages SysEx entrants
//   - Envoyer des Identity Requests automatiquement
//   - Gérer les callbacks de découverte de devices
//   - Maintenir un cache des identités connues
//   - Auto-reconnexion intelligente
//
// Thread-safety: OUI
//
// Patterns: Observer Pattern, Command Pattern
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <functional>
#include <chrono>

#include "SysExMessage.h"
#include "SysExParser.h"
#include "SysExBuilder.h"
#include "DeviceIdentity.h"
#include "manufacturers/ManufacturerDatabase.h"
#include "../core/Logger.h"

namespace midiMind {

// Forward declaration
class MidiDevice;

/**
 * @class SysExHandler
 * @brief Gestionnaire principal des messages SysEx
 * 
 * @details
 * Coordonne toutes les opérations SysEx:
 * - Parsing des messages entrants
 * - Génération de requêtes d'identification
 * - Gestion des callbacks
 * - Cache des identités
 * - Auto-reconnexion
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * auto handler = std::make_shared<SysExHandler>();
 * 
 * // Callback pour nouvelles identités
 * handler->setOnDeviceIdentified([](const DeviceIdentity& id) {
 *     Logger::info("Identified: " + id.toString());
 * });
 * 
 * // Demander l'identification d'un device
 * handler->requestIdentity(deviceId);
 * 
 * // Traiter un message SysEx reçu
 * handler->handleSysExMessage(sysexData, deviceId);
 * ```
 */
class SysExHandler {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé quand un device est identifié
     */
    using DeviceIdentifiedCallback = std::function<void(const std::string& deviceId, const DeviceIdentity&)>;
    
    /**
     * @brief Callback appelé pour envoyer un message SysEx
     * @param deviceId ID du device destinataire
     * @param message Message à envoyer
     */
    using SendSysExCallback = std::function<void(const std::string& deviceId, const SysExMessage&)>;
    
    /**
     * @brief Callback appelé pour les messages SysEx non gérés
     */
    using UnhandledSysExCallback = std::function<void(const std::string& deviceId, const SysExMessage&)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    SysExHandler();
    
    /**
     * @brief Destructeur
     */
    ~SysExHandler();
    
    // Désactiver copie
    SysExHandler(const SysExHandler&) = delete;
    SysExHandler& operator=(const SysExHandler&) = delete;
    
    // ========================================================================
    // RÉCEPTION DE MESSAGES
    // ========================================================================
    
    /**
     * @brief Traite un message SysEx reçu
     * 
     * Parse le message et déclenche les callbacks appropriés.
     * 
     * @param data Données SysEx brutes
     * @param deviceId ID du device source
     * 
     * @note Thread-safe
     */
    void handleSysExMessage(const std::vector<uint8_t>& data, const std::string& deviceId);
    
    /**
     * @brief Traite un message SysEx reçu
     * 
     * @param message Message SysEx déjà parsé
     * @param deviceId ID du device source
     * 
     * @note Thread-safe
     */
    void handleSysExMessage(const SysExMessage& message, const std::string& deviceId);
    
    // ========================================================================
    // IDENTIFICATION DE DEVICES
    // ========================================================================
    
    /**
     * @brief Demande l'identité d'un device
     * 
     * Envoie un Identity Request au device.
     * La réponse sera traitée via handleSysExMessage().
     * 
     * @param deviceId ID du device
     * @return true Si la requête a été envoyée
     * 
     * @note Thread-safe
     * 
     * @example
     * ```cpp
     * handler->requestIdentity("device_usb_0");
     * ```
     */
    bool requestIdentity(const std::string& deviceId);
    
    /**
     * @brief Demande l'identité de tous les devices
     * 
     * Envoie un broadcast Identity Request.
     * 
     * @return true Si la requête a été envoyée
     * 
     * @note Thread-safe
     */
    bool requestIdentityAll();
    
    /**
     * @brief Récupère l'identité connue d'un device
     * 
     * @param deviceId ID du device
     * @return std::optional<DeviceIdentity> Identité ou nullopt
     * 
     * @note Thread-safe
     */
    std::optional<DeviceIdentity> getDeviceIdentity(const std::string& deviceId) const;
    
    /**
     * @brief Liste toutes les identités connues
     * 
     * @return std::map<std::string, DeviceIdentity> Map deviceId -> Identity
     * 
     * @note Thread-safe
     */
    std::map<std::string, DeviceIdentity> listKnownIdentities() const;
    
    /**
     * @brief Efface l'identité d'un device du cache
     * 
     * @param deviceId ID du device
     * 
     * @note Thread-safe
     */
    void clearDeviceIdentity(const std::string& deviceId);
    
    /**
     * @brief Efface toutes les identités du cache
     * 
     * @note Thread-safe
     */
    void clearAllIdentities();
    
    // ========================================================================
    // AUTO-IDENTIFICATION
    // ========================================================================
    
    /**
     * @brief Active/désactive l'auto-identification
     * 
     * Si activé, envoie automatiquement un Identity Request
     * lors de la connexion d'un nouveau device.
     * 
     * @param enabled true pour activer
     * 
     * @note Thread-safe
     */
    void setAutoIdentify(bool enabled);
    
    /**
     * @brief Vérifie si l'auto-identification est activée
     * 
     * @return true Si activé
     * 
     * @note Thread-safe
     */
    bool isAutoIdentifyEnabled() const;
    
    /**
     * @brief Définit le délai avant auto-identification (ms)
     * 
     * Délai d'attente après la connexion d'un device avant
     * d'envoyer l'Identity Request.
     * 
     * @param delayMs Délai en millisecondes (défaut: 500ms)
     * 
     * @note Thread-safe
     */
    void setAutoIdentifyDelay(uint32_t delayMs);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback d'identification de device
     */
    void setOnDeviceIdentified(DeviceIdentifiedCallback callback);
    
    /**
     * @brief Définit le callback d'envoi de SysEx
     */
    void setOnSendSysEx(SendSysExCallback callback);
    
    /**
     * @brief Définit le callback pour messages non gérés
     */
    void setOnUnhandledSysEx(UnhandledSysExCallback callback);
    
    // ========================================================================
    // CONTRÔLE GÉNÉRAL MIDI
    // ========================================================================
    
    /**
     * @brief Envoie GM System On à un device
     * 
     * @param deviceId ID du device
     * @return true Si envoyé avec succès
     */
    bool sendGMSystemOn(const std::string& deviceId);
    
    /**
     * @brief Envoie GM System Off à un device
     * 
     * @param deviceId ID du device
     * @return true Si envoyé avec succès
     */
    bool sendGMSystemOff(const std::string& deviceId);
    
    /**
     * @brief Envoie Master Volume à un device
     * 
     * @param deviceId ID du device
     * @param volume Volume (0-16383, 16383 = 100%)
     * @return true Si envoyé avec succès
     */
    bool sendMasterVolume(const std::string& deviceId, uint16_t volume);
    
    /**
     * @brief Envoie Master Fine Tuning à un device
     * 
     * @param deviceId ID du device
     * @param cents Tuning en cents (-8192 à +8191)
     * @return true Si envoyé avec succès
     */
    bool sendMasterFineTuning(const std::string& deviceId, int16_t cents);
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques SysEx
     * 
     * @return json Statistiques
     */
    json getStatistics() const;

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Traite un Identity Reply
     */
    void handleIdentityReply(const SysExMessage& message, const std::string& deviceId);
    
    /**
     * @brief Traite un message General MIDI
     */
    void handleGeneralMidi(const SysExMessage& message, const std::string& deviceId);
    
    /**
     * @brief Traite un message Device Control
     */
    void handleDeviceControl(const SysExMessage& message, const std::string& deviceId);
    
    /**
     * @brief Envoie un message SysEx
     */
    bool sendSysEx(const std::string& deviceId, const SysExMessage& message);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Cache des identités connues
    std::map<std::string, DeviceIdentity> identityCache_;
    
    /// Callbacks
    DeviceIdentifiedCallback onDeviceIdentified_;
    SendSysExCallback onSendSysEx_;
    UnhandledSysExCallback onUnhandledSysEx_;
    
    /// Configuration
    bool autoIdentify_;
    uint32_t autoIdentifyDelayMs_;
    
    /// Statistiques
    std::atomic<uint64_t> messagesReceived_;
    std::atomic<uint64_t> messagesSent_;
    std::atomic<uint64_t> identityRepliesReceived_;
    std::atomic<uint64_t> identityRequestsSent_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SysExHandler.h
// ============================================================================