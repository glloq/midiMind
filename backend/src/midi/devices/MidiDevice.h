// ============================================================================
// Fichier: backend/src/midi/devices/MidiDevice.h
// Version: 4.0.1 - VERSION FUSIONNÉE FINALE
// Date: 2025-10-15
// ============================================================================
// Description:
//   Classe abstraite de base pour tous les périphériques MIDI.
//   Combine le meilleur de v3.0.0 (SysEx, aliases) et v4.0.1 (cohérence).
//
// Corrections v4.0.1:
//   ✅ disconnect() retourne bool (au lieu de void)
//   ✅ handleMessage() sans 'override' (pas de classe parent)
//   ✅ Documentation complète
//   ✅ Signatures cohérentes
//
// Responsabilités:
//   - Définir le contrat pour tous les devices MIDI
//   - Gérer les callbacks de réception
//   - Support SysEx via SysExHandler
//   - Sérialisation JSON
//   - Statistiques
//
// Thread-safety: OUI (via classes dérivées)
//
// Auteur: MidiMind Team
// ============================================================================

#pragma once

#include "../MidiMessage.h"
#include "../sysex/SysExHandler.h"
#include "../../core/Logger.h"
#include <string>
#include <atomic>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace midiMind {

using json = nlohmann::json;

// ============================================================================
// ÉNUMÉRATIONS
// ============================================================================

/**
 * @enum DeviceType
 * @brief Type de périphérique MIDI
 */
enum class DeviceType {
    USB,            ///< Périphérique USB (ALSA)
    WIFI,           ///< Périphérique WiFi/Network (RTP-MIDI)
    BLUETOOTH,      ///< Périphérique Bluetooth Low Energy
    VIRTUAL,        ///< Port MIDI virtuel
    UNKNOWN         ///< Type inconnu
};

/**
 * @enum DeviceDirection
 * @brief Direction du flux MIDI
 */
enum class DeviceDirection {
    INPUT,          ///< Entrée uniquement (receive)
    OUTPUT,         ///< Sortie uniquement (send)
    BIDIRECTIONAL   ///< Entrée et sortie
};

/**
 * @enum DeviceStatus
 * @brief État de connexion du périphérique
 */
enum class DeviceStatus {
    DISCONNECTED,   ///< Déconnecté
    CONNECTING,     ///< Connexion en cours
    CONNECTED,      ///< Connecté et prêt
    ERROR           ///< Erreur de connexion
};

// ============================================================================
// CLASSE ABSTRAITE MIDIDEVICE
// ============================================================================

/**
 * @class MidiDevice
 * @brief Classe abstraite pour tous les périphériques MIDI
 * 
 * @details
 * Interface commune pour USB, Network, Bluetooth et Virtual devices.
 * 
 * Fonctionnalités:
 * - Connexion/déconnexion avec gestion d'état
 * - Envoi/réception de messages MIDI
 * - Support SysEx via SysExHandler
 * - Callbacks pour messages reçus
 * - Statistiques (messages sent/received)
 * - Sérialisation JSON
 * 
 * Thread-safety: Les implémentations doivent être thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * auto device = std::make_shared<UsbMidiDevice>("usb_128_0", "KeyStep", 128, 0);
 * 
 * // Connexion
 * if (!device->connect()) {
 *     Logger::error("Connection failed");
 *     return;
 * }
 * 
 * // Callback
 * device->setOnMessageReceived([](const MidiMessage& msg) {
 *     Logger::info("Received: " + msg.toString());
 * });
 * 
 * // Envoi
 * device->sendMessage(MidiMessage::noteOn(0, 60, 100));
 * 
 * // Déconnexion
 * device->disconnect();
 * ```
 */
class MidiDevice {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé lors de la réception d'un message MIDI
     */
    using MessageCallback = std::function<void(const MidiMessage&)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param id ID unique du périphérique
     * @param name Nom lisible
     * @param type Type de périphérique
     * @param direction Direction du flux MIDI
     */
    MidiDevice(const std::string& id, 
               const std::string& name, 
               DeviceType type,
               DeviceDirection direction = DeviceDirection::BIDIRECTIONAL)
        : id_(id)
        , name_(name)
        , type_(type)
        , direction_(direction)
        , status_(DeviceStatus::DISCONNECTED)
        , messagesReceived_(0)
        , messagesSent_(0) {
        
        Logger::debug("MidiDevice", "Created: " + name + " (" + id + ")");
    }
    
    /**
     * @brief Destructeur virtuel
     */
    virtual ~MidiDevice() {
        Logger::debug("MidiDevice", "Destroyed: " + name_);
    }
    
    // Désactiver copie et move
    MidiDevice(const MidiDevice&) = delete;
    MidiDevice& operator=(const MidiDevice&) = delete;
    
    // ========================================================================
    // MÉTHODES VIRTUELLES PURES (OBLIGATOIRES)
    // ========================================================================
    
    /**
     * @brief Connecte le périphérique
     * 
     * @return true Si connexion réussie
     * 
     * @note MUST BE IMPLEMENTED
     * @note Doit mettre à jour status_
     * @note Doit être thread-safe
     */
    virtual bool connect() = 0;
    
    /**
     * @brief Déconnecte le périphérique
     * 
     * @return true Si déconnexion réussie
     * 
     * @note MUST BE IMPLEMENTED
     * @note Doit mettre à jour status_
     * @note Doit arrêter les threads de réception
     * @note Doit être thread-safe
     */
    virtual bool disconnect() = 0;
    
    /**
     * @brief Envoie un message MIDI
     * 
     * @param msg Message à envoyer
     * @return true Si envoi réussi
     * 
     * @note MUST BE IMPLEMENTED
     * @note Doit incrémenter messagesSent_ en cas de succès
     * @note Doit être thread-safe
     */
    virtual bool sendMessage(const MidiMessage& msg) = 0;
    
    // ========================================================================
    // MÉTHODES VIRTUELLES AVEC IMPLÉMENTATION PAR DÉFAUT
    // ========================================================================
    
    /**
     * @brief Alias pour connect() (compatibilité API)
     * 
     * @return true Si connexion réussie
     */
    virtual bool open() {
        return connect();
    }
    
    /**
     * @brief Alias pour disconnect() (compatibilité API)
     */
    virtual void close() {
        disconnect();
    }
    
    /**
     * @brief Alias pour sendMessage() (compatibilité API)
     * 
     * @param msg Message à envoyer
     */
    virtual void send(const MidiMessage& msg) {
        sendMessage(msg);
    }
    
    /**
     * @brief Reçoit un message (si disponible)
     * 
     * @return MidiMessage Message reçu ou message vide
     * 
     * @note Par défaut retourne message vide
     * @note Les devices avec buffer peuvent surcharger
     */
    virtual MidiMessage receive() {
        return MidiMessage();
    }
    
    /**
     * @brief Vérifie si des messages sont en attente
     * 
     * @return true Si des messages sont disponibles
     * 
     * @note Par défaut retourne false
     * @note Les devices avec buffer peuvent surcharger
     */
    virtual bool hasMessages() const {
        return false;
    }
    
    /**
     * @brief Récupère le port/adresse du device
     * 
     * @return std::string Port ou adresse (vide par défaut)
     * 
     * @note USB: "128:0", Network: "192.168.1.42:5004", etc.
     */
    virtual std::string getPort() const {
        return "";
    }
    
    /**
     * @brief Récupère les informations détaillées
     * 
     * @return json Informations du device
     */
    virtual json getInfo() const {
        json info;
        info["id"] = id_;
        info["name"] = name_;
        info["type"] = static_cast<int>(type_);
        info["direction"] = static_cast<int>(direction_);
        info["status"] = static_cast<int>(status_.load());
        info["connected"] = isConnected();
        info["messages_received"] = messagesReceived_.load();
        info["messages_sent"] = messagesSent_.load();
        info["port"] = getPort();
        return info;
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    /**
     * @brief Récupère l'ID unique
     */
    std::string getId() const {
        return id_;
    }
    
    /**
     * @brief Récupère le nom
     */
    std::string getName() const {
        return name_;
    }
    
    /**
     * @brief Récupère le type
     */
    DeviceType getType() const {
        return type_;
    }
    
    /**
     * @brief Récupère la direction
     */
    DeviceDirection getDirection() const {
        return direction_;
    }
    
    /**
     * @brief Récupère le statut
     */
    DeviceStatus getStatus() const {
        return status_.load();
    }
    
    /**
     * @brief Vérifie si le device est ouvert/connecté
     * 
     * @return true Si connecté
     */
    bool isOpen() const {
        return status_.load() == DeviceStatus::CONNECTED;
    }
    
    /**
     * @brief Vérifie si le device est connecté
     * 
     * @return true Si connecté
     */
    bool isConnected() const {
        return status_.load() == DeviceStatus::CONNECTED;
    }
    
    /**
     * @brief Récupère le nombre de messages reçus
     */
    uint64_t getMessagesReceived() const {
        return messagesReceived_.load();
    }
    
    /**
     * @brief Récupère le nombre de messages envoyés
     */
    uint64_t getMessagesSent() const {
        return messagesSent_.load();
    }
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de réception MIDI
     * 
     * @param callback Fonction appelée pour chaque message reçu
     * 
     * @note Thread-safe
     * @note Appelé depuis le thread de réception
     */
    void setOnMessageReceived(MessageCallback callback) {
        onMessageReceived_ = callback;
        Logger::debug("MidiDevice", name_ + ": Message callback set");
    }
    
    /**
     * @brief Alias pour setOnMessageReceived() (compatibilité API)
     */
    void setMessageCallback(MessageCallback callback) {
        setOnMessageReceived(callback);
    }
    
    /**
     * @brief Supprime le callback
     */
    void clearCallback() {
        onMessageReceived_ = nullptr;
        Logger::debug("MidiDevice", name_ + ": Callback cleared");
    }
    
    // ========================================================================
    // SUPPORT SYSEX
    // ========================================================================
    
    /**
     * @brief Définit le SysExHandler pour ce device
     * 
     * @param handler Handler SysEx partagé
     * 
     * @note Permet de déléguer le traitement des messages SysEx
     */
    void setSysExHandler(std::shared_ptr<SysExHandler> handler) {
        sysexHandler_ = handler;
        Logger::debug("MidiDevice", name_ + ": SysExHandler set");
    }
    
    /**
     * @brief Récupère le SysExHandler
     */
    std::shared_ptr<SysExHandler> getSysExHandler() const {
        return sysexHandler_;
    }

protected:
    // ========================================================================
    // MÉTHODES PROTÉGÉES POUR CLASSES DÉRIVÉES
    // ========================================================================
    
    /**
     * @brief Traite un message MIDI reçu
     * 
     * @param message Message reçu
     * 
     * @note À appeler par les classes dérivées lors de la réception
     * @note Gère automatiquement les messages SysEx
     * @note Appelle le callback utilisateur
     */
    void handleMessage(const MidiMessage& message) {
        // Incrémenter compteur
        messagesReceived_++;
        
        // Si c'est un message SysEx ET qu'on a un handler
        if (message.getType() == MidiMessageType::SYSTEM_EXCLUSIVE && sysexHandler_) {
            // Déléguer au SysExHandler
            sysexHandler_->handleSysExMessage(message.getData(), getId());
        }
        
        // Notifier le callback utilisateur
        if (onMessageReceived_) {
            try {
                onMessageReceived_(message);
            } catch (const std::exception& e) {
                Logger::error("MidiDevice", 
                    name_ + ": Callback exception: " + e.what());
            }
        }
    }
    
    /**
     * @brief Définit le statut du device
     * 
     * @param status Nouveau statut
     * 
     * @note Thread-safe (atomic)
     */
    void setStatus(DeviceStatus status) {
        status_.store(status);
    }
    
    /**
     * @brief Incrémente le compteur de messages reçus
     * 
     * @note Thread-safe (atomic)
     */
    void incrementMessagesReceived() {
        messagesReceived_++;
    }
    
    /**
     * @brief Incrémente le compteur de messages envoyés
     * 
     * @note Thread-safe (atomic)
     */
    void incrementMessagesSent() {
        messagesSent_++;
    }
    
    // ========================================================================
    // MEMBRES PROTÉGÉS
    // ========================================================================
    
    /// ID unique du périphérique
    std::string id_;
    
    /// Nom lisible
    std::string name_;
    
    /// Type de périphérique
    DeviceType type_;
    
    /// Direction du flux MIDI
    DeviceDirection direction_;
    
    /// Statut de connexion (atomic pour thread-safety)
    std::atomic<DeviceStatus> status_;
    
    /// Compteur messages reçus (atomic)
    std::atomic<uint64_t> messagesReceived_;
    
    /// Compteur messages envoyés (atomic)
    std::atomic<uint64_t> messagesSent_;
    
    /// Callback de réception
    MessageCallback onMessageReceived_;
    
    /// Handler pour messages SysEx
    std::shared_ptr<SysExHandler> sysexHandler_;
};

// ============================================================================
// ALIAS DE TYPE
// ============================================================================

/**
 * @brief Pointeur partagé vers un périphérique MIDI
 */
using MidiDevicePtr = std::shared_ptr<MidiDevice>;

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiDevice.h v4.0.1 - VERSION FUSIONNÉE FINALE
// ============================================================================