// ============================================================================
// Fichier: backend/src/midi/devices/MidiDevice.h
// Version: 4.0.2 - CORRECTION Include DeviceInfo.h
// Date: 2025-10-16
// ============================================================================
// CORRECTIONS v4.0.2:
//   ✅ FIX: Suppression des redéfinitions d'enums
//   ✅ FIX: Include DeviceInfo.h pour les types
//   ✅ Conserve toutes les fonctionnalités de v4.0.1
//   ✅ Évite les conflits de définition
//
// Description:
//   Classe abstraite de base pour tous les périphériques MIDI.
//   Combine le meilleur de v3.0.0 (SysEx, aliases) et v4.0.1 (cohérence).
//
// ResponsabilitÃ©s:
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
#include "DeviceInfo.h"  // ✅ AJOUT v4.0.2: Importe les enums au lieu de les redéfinir
#include <string>
#include <atomic>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace midiMind {

using json = nlohmann::json;

// ============================================================================
// NOTE: Les énumérations DeviceType, DeviceDirection, DeviceStatus
//       sont maintenant définies dans DeviceInfo.h et importées via include
// ============================================================================

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
        , messagesSent_(0)
        , sysexHandler_(nullptr)
    {
        Logger::debug("MidiDevice", "Created: " + name + " (" + id + ")");
    }
    
    /**
     * @brief Destructeur virtuel
     */
    virtual ~MidiDevice() {
        Logger::debug("MidiDevice", "Destroyed: " + name_ + " (" + id_ + ")");
    }
    
    // ========================================================================
    // MÉTHODES VIRTUELLES PURES (à implémenter dans les classes dérivées)
    // ========================================================================
    
    /**
     * @brief Ouvre/connecte le périphérique
     * 
     * @return true Si succès
     * 
     * @note Doit être thread-safe
     * @note Doit mettre à jour status_
     */
    virtual bool open() = 0;
    
    /**
     * @brief Ferme/déconnecte le périphérique
     * 
     * @return bool True si succès (v4.0.1: corrigé, était void)
     * 
     * @note Doit être thread-safe
     * @note Doit mettre à jour status_
     */
    virtual bool close() = 0;
    
    /**
     * @brief Envoie un message MIDI
     * 
     * @param message Message à envoyer
     * @return true Si succès
     * 
     * @note Doit être thread-safe
     * @note Doit incrémenter messagesSent_ en cas de succès
     */
    virtual bool send(const MidiMessage& message) = 0;
    
    // ========================================================================
    // MÉTHODES PUBLIQUES
    // ========================================================================
    
    /**
     * @brief Connecte le périphérique (alias de open())
     * 
     * @return true Si succès
     */
    bool connect() {
        return open();
    }
    
    /**
     * @brief Déconnecte le périphérique (alias de close())
     * 
     * @return bool True si succès
     */
    bool disconnect() {
        return close();
    }
    
    /**
     * @brief Envoie un message MIDI (alias de send())
     * 
     * @param message Message à envoyer
     * @return true Si succès
     */
    bool sendMessage(const MidiMessage& message) {
        return send(message);
    }
    
    /**
     * @brief Configure le callback de réception
     * 
     * @param callback Fonction à appeler pour chaque message reçu
     * 
     * @note Thread-safe
     */
    void setOnMessageReceived(MessageCallback callback) {
        onMessageReceived_ = callback;
    }
    
    /**
     * @brief Configure le handler SysEx
     * 
     * @param handler Handler pour messages SysEx
     */
    void setSysExHandler(std::shared_ptr<SysExHandler> handler) {
        sysexHandler_ = handler;
    }
    
    /**
     * @brief Récupère le handler SysEx
     * 
     * @return std::shared_ptr<SysExHandler> Handler actuel (peut être nullptr)
     */
    std::shared_ptr<SysExHandler> getSysExHandler() const {
        return sysexHandler_;
    }
    
    /**
     * @brief Traite un message MIDI reçu
     * 
     * @param message Message reçu
     * 
     * @note Appelée par les classes dérivées lors de la réception
     * @note Gère automatiquement les messages SysEx si handler configuré
     * @note Incrémente le compteur messagesReceived_
     * @note Appelle le callback onMessageReceived_ si configuré
     * 
     * v4.0.1: Pas de 'override' car ce n'est pas une méthode virtuelle parente
     */
    void handleMessage(const MidiMessage& message) {
        // Incrémenter le compteur
        incrementMessagesReceived();
        
        // Si c'est un message SysEx et qu'on a un handler
        if (message.isSysEx() && sysexHandler_) {
            sysexHandler_->handleMessage(message);
        }
        
        // Callback utilisateur
        if (onMessageReceived_) {
            try {
                onMessageReceived_(message);
            } catch (const std::exception& e) {
                Logger::error("MidiDevice", 
                    "Exception in message callback: " + std::string(e.what()));
            }
        }
    }
    
    /**
     * @brief Récupère le port système (optionnel)
     * 
     * @return std::string Port (vide si non applicable)
     * 
     * @note Peut être redéfini par les classes dérivées
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
        return isOpen();
    }
    
    /**
     * @brief Récupère le nombre de messages reçus
     * 
     * @return uint64_t Compteur (thread-safe)
     */
    uint64_t getMessagesReceived() const {
        return messagesReceived_.load();
    }
    
    /**
     * @brief Récupère le nombre de messages envoyés
     * 
     * @return uint64_t Compteur (thread-safe)
     */
    uint64_t getMessagesSent() const {
        return messagesSent_.load();
    }
    
    /**
     * @brief Réinitialise les statistiques
     * 
     * @note Thread-safe
     */
    void resetStats() {
        messagesReceived_.store(0);
        messagesSent_.store(0);
    }
    
protected:
    // ========================================================================
    // MÉTHODES PROTÉGÉES (pour classes dérivées)
    // ========================================================================
    
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
// FIN DU FICHIER MidiDevice.h v4.0.2 - CORRECTION Include DeviceInfo.h
// ============================================================================
