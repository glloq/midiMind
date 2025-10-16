// ============================================================================
// Fichier: src/midi/sysex/SysExHandler.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestionnaire principal des messages System Exclusive (SysEx).
//   Gère l'identification des devices, le contrôle GM, et Custom SysEx (0x7D).
//
// Responsabilités:
//   - Réception et dispatch des messages SysEx
//   - Identification automatique des devices
//   - Gestion du protocole Custom SysEx (blocs 1-8)
//   - Cache thread-safe des identités et capacités
//   - Notifications via callbacks
//
// Thread-safety: Oui (std::mutex)
//
// Auteur: MidiMind Team
// Date: 2025-10-06
// Version: 3.0.0
// ============================================================================

#pragma once

#include <memory>
#include <functional>
#include <map>
#include <mutex>
#include <atomic>
#include <optional>
#include <nlohmann/json.hpp>

#include "SysExMessage.h"
#include "SysExParser.h"
#include "SysExBuilder.h"
#include "DeviceIdentity.h"
#include "CustomDeviceIdentity.h"
#include "NoteMap.h"
#include "CCCapabilities.h"
#include "AirCapabilities.h"
#include "LightCapabilities.h"
#include "SensorsFeedback.h"
#include "SyncClock.h"
#include "CustomSysExParser.h"
#include "ManufacturerDatabase.h"
#include "../../core/Logger.h"

using json = nlohmann::json;

namespace midiMind {

/**
 * @class SysExHandler
 * @brief Gestionnaire principal des messages SysEx
 * 
 * @details
 * Classe centrale pour le traitement des messages System Exclusive.
 * Gère à la fois les messages SysEx standard (Identity, GM, Device Control)
 * et les messages Custom (protocole 0x7D pour instruments DIY).
 * 
 * Thread-safety: Oui
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
     * @brief Callback appelé quand un device standard est identifié
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
    
    // Custom SysEx Callbacks
    using CustomDeviceIdentifiedCallback = std::function<void(const std::string& deviceId, const CustomDeviceIdentity&)>;
    using NoteMapReceivedCallback = std::function<void(const std::string& deviceId, const NoteMap&)>;
    using CCCapabilitiesCallback = std::function<void(const std::string& deviceId, const CCCapabilities&)>;
    using AirCapabilitiesCallback = std::function<void(const std::string& deviceId, const AirCapabilities&)>;
    using LightCapabilitiesCallback = std::function<void(const std::string& deviceId, const LightCapabilities&)>;
    using SensorsFeedbackCallback = std::function<void(const std::string& deviceId, const SensorsFeedback&)>;
    using SyncClockCallback = std::function<void(const std::string& deviceId, const SyncClock&)>;
    using UnknownCustomBlockCallback = std::function<void(const std::string& deviceId, uint8_t blockId, uint8_t version, const SysExMessage&)>;
    
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
     * @param data Données brutes du message SysEx
     * @param deviceId ID du device source
     */
    void handleSysExMessage(const std::vector<uint8_t>& data, 
                           const std::string& deviceId);
    
    /**
     * @brief Traite un message SysEx reçu
     * 
     * @param message Message SysEx parsé
     * @param deviceId ID du device source
     */
    void handleSysExMessage(const SysExMessage& message, 
                           const std::string& deviceId);
    
    // ========================================================================
    // IDENTIFICATION DE DEVICES
    // ========================================================================
    
    /**
     * @brief Demande l'identité d'un device
     * 
     * Envoie un Identity Request (F0 7E <device> 06 01 F7)
     * 
     * @param deviceId ID du device
     * @return true si la requête a été envoyée
     */
    bool requestIdentity(const std::string& deviceId);
    
    /**
     * @brief Broadcast un Identity Request à tous les devices
     * 
     * @return true si la requête a été envoyée
     */
    bool requestIdentityAll();
    
    /**
     * @brief Récupère l'identité d'un device
     * 
     * @param deviceId ID du device
     * @return std::optional<DeviceIdentity> Identité ou nullopt
     */
    std::optional<DeviceIdentity> getDeviceIdentity(const std::string& deviceId) const;
    
    /**
     * @brief Liste toutes les identités connues
     * 
     * @return std::map<std::string, DeviceIdentity> Map deviceId -> identité
     */
    std::map<std::string, DeviceIdentity> listKnownIdentities() const;
    
    /**
     * @brief Efface l'identité d'un device
     * 
     * @param deviceId ID du device
     */
    void clearDeviceIdentity(const std::string& deviceId);
    
    /**
     * @brief Efface toutes les identités
     */
    void clearAllIdentities();
    
    // ========================================================================
    // CUSTOM SYSEX (PROTOCOLE 0x7D)
    // ========================================================================
    
    /**
     * @brief Récupère l'identité Custom d'un device
     * 
     * @param deviceId ID du device
     * @return std::optional<CustomDeviceIdentity> Identité ou nullopt
     */
    std::optional<CustomDeviceIdentity> getCustomIdentity(const std::string& deviceId) const;
    
    /**
     * @brief Récupère la Note Map d'un device
     * 
     * @param deviceId ID du device
     * @return std::optional<NoteMap> Note map ou nullopt
     */
    std::optional<NoteMap> getNoteMap(const std::string& deviceId) const;
    
    /**
     * @brief Récupère les CC supportés d'un device
     */
    std::optional<CCCapabilities> getCCCapabilities(const std::string& deviceId) const;
    
    /**
     * @brief Récupère les capacités Air d'un device
     */
    std::optional<AirCapabilities> getAirCapabilities(const std::string& deviceId) const;
    
    /**
     * @brief Récupère les capacités Lumières d'un device
     */
    std::optional<LightCapabilities> getLightCapabilities(const std::string& deviceId) const;
    
    /**
     * @brief Récupère les capteurs d'un device
     */
    std::optional<SensorsFeedback> getSensorsFeedback(const std::string& deviceId) const;
    
    /**
     * @brief Récupère les capacités Sync d'un device
     */
    std::optional<SyncClock> getSyncClock(const std::string& deviceId) const;
    
    /**
     * @brief Liste tous les Custom Devices connus
     * 
     * @return std::map<std::string, CustomDeviceIdentity> Map deviceId -> identité
     */
    std::map<std::string, CustomDeviceIdentity> listKnownCustomDevices() const;
    
    /**
     * @brief Efface toutes les données Custom d'un device
     * 
     * @param deviceId ID du device
     */
    void clearCustomIdentity(const std::string& deviceId);
    
    /**
     * @brief Efface toutes les identités Custom
     */
    void clearAllCustomIdentities();
    
    // ========================================================================
    // AUTO-IDENTIFICATION
    // ========================================================================
    
    /**
     * @brief Active/désactive l'auto-identification
     * 
     * Quand activé, un Identity Request est automatiquement envoyé
     * à chaque nouveau device détecté.
     * 
     * @param enabled true pour activer
     */
    void setAutoIdentify(bool enabled);
    
    /**
     * @brief Vérifie si l'auto-identification est activée
     */
    bool isAutoIdentifyEnabled() const;
    
    /**
     * @brief Définit le délai avant l'auto-identification
     * 
     * @param delayMs Délai en millisecondes
     */
    void setAutoIdentifyDelay(uint32_t delayMs);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback pour device identifié
     */
    void setOnDeviceIdentified(DeviceIdentifiedCallback callback);
    
    /**
     * @brief Définit le callback pour envoyer un SysEx
     */
    void setOnSendSysEx(SendSysExCallback callback);
    
    /**
     * @brief Définit le callback pour SysEx non géré
     */
    void setOnUnhandledSysEx(UnhandledSysExCallback callback);
    
    // Custom SysEx Callbacks
    void setOnCustomDeviceIdentified(CustomDeviceIdentifiedCallback callback);
    void setOnNoteMapReceived(NoteMapReceivedCallback callback);
    void setOnCCCapabilities(CCCapabilitiesCallback callback);
    void setOnAirCapabilities(AirCapabilitiesCallback callback);
    void setOnLightCapabilities(LightCapabilitiesCallback callback);
    void setOnSensorsFeedback(SensorsFeedbackCallback callback);
    void setOnSyncClock(SyncClockCallback callback);
    void setOnUnknownCustomBlock(UnknownCustomBlockCallback callback);
    
    // ========================================================================
    // CONTRÔLE GÉNÉRAL MIDI
    // ========================================================================
    
    /**
     * @brief Envoie GM System On
     * 
     * @param deviceId ID du device
     * @return true si envoyé avec succès
     */
    bool sendGMSystemOn(const std::string& deviceId);
    
    /**
     * @brief Envoie GM System Off
     * 
     * @param deviceId ID du device
     * @return true si envoyé avec succès
     */
    bool sendGMSystemOff(const std::string& deviceId);
    
    /**
     * @brief Envoie Master Volume
     * 
     * @param deviceId ID du device
     * @param volume Volume (0-16383)
     * @return true si envoyé avec succès
     */
    bool sendMasterVolume(const std::string& deviceId, uint16_t volume);
    
    /**
     * @brief Envoie Master Fine Tuning
     * 
     * @param deviceId ID du device
     * @param cents Tuning en cents (-8192 à +8191)
     * @return true si envoyé avec succès
     */
    bool sendMasterFineTuning(const std::string& deviceId, int16_t cents);
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques du handler
     * 
     * @return json Statistiques au format JSON
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
     * @brief Traite un message Custom SysEx (0x7D)
     */
    void handleCustomSysEx(const SysExMessage& message, const std::string& deviceId);
    
    /**
     * @brief Traite un Bloc 1 - Identification Custom
     */
    void handleCustomIdentification(const SysExMessage& message, const std::string& deviceId);
    
    /**
     * @brief Traite un Bloc 2 - Note Map
     */
    void handleNoteMap(const SysExMessage& message, const std::string& deviceId);
    
    /**
     * @brief Traite un Bloc 3 - CC Supportés
     */
    void handleCCSupported(const SysExMessage& message, const std::string& deviceId);
    
    /**
     * @brief Traite un Bloc 4 - Capacités Air
     */
    void handleAirCapabilities(const SysExMessage& message, const std::string& deviceId);
    
    /**
     * @brief Traite un Bloc 5 - Capacités Lumières
     */
    void handleLightCapabilities(const SysExMessage& message, const std::string& deviceId);
    
    /**
     * @brief Traite un Bloc 7 - Capteurs/Feedback
     */
    void handleSensorsFeedback(const SysExMessage& message, const std::string& deviceId);
    
    /**
     * @brief Traite un Bloc 8 - Sync & Clock
     */
    void handleSyncClock(const SysExMessage& message, const std::string& deviceId);
    
    /**
     * @brief Traite un bloc custom non implémenté
     */
    void handleUnknownCustomBlock(const SysExMessage& message, const std::string& deviceId);
    
    /**
     * @brief Envoie un message SysEx
     */
    bool sendSysEx(const std::string& deviceId, const SysExMessage& message);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    // Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    // Cache des identités standard
    std::map<std::string, DeviceIdentity> identityCache_;
    
    // Cache Custom SysEx (Blocs 1-8)
    std::map<std::string, CustomDeviceIdentity> customIdentities_;
    std::map<std::string, NoteMap> noteMaps_;
    std::map<std::string, CCCapabilities> ccCapabilities_;
    std::map<std::string, AirCapabilities> airCapabilities_;
    std::map<std::string, LightCapabilities> lightCapabilities_;
    std::map<std::string, SensorsFeedback> sensorsFeedback_;
    std::map<std::string, SyncClock> syncClock_;
    
    // Callbacks
    DeviceIdentifiedCallback onDeviceIdentified_;
    SendSysExCallback onSendSysEx_;
    UnhandledSysExCallback onUnhandledSysEx_;
    
    // Custom SysEx Callbacks
    CustomDeviceIdentifiedCallback onCustomDeviceIdentified_;
    NoteMapReceivedCallback onNoteMapReceived_;
    CCCapabilitiesCallback onCCCapabilities_;
    AirCapabilitiesCallback onAirCapabilities_;
    LightCapabilitiesCallback onLightCapabilities_;
    SensorsFeedbackCallback onSensorsFeedback_;
    SyncClockCallback onSyncClock_;
    UnknownCustomBlockCallback onUnknownCustomBlock_;
    
    // Configuration
    bool autoIdentify_;
    uint32_t autoIdentifyDelayMs_;
    
    // Statistiques
    std::atomic<uint64_t> messagesReceived_;
    std::atomic<uint64_t> messagesSent_;
    std::atomic<uint64_t> identityRepliesReceived_;
    std::atomic<uint64_t> identityRequestsSent_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SysExHandler.h
// ============================================================================
