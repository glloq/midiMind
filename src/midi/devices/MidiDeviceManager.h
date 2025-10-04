// ============================================================================
// Fichier: src/midi/devices/MidiDeviceManager.h
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestionnaire centralisé de tous les périphériques MIDI (USB, WiFi, BT).
//   Découverte, connexion, déconnexion, et envoi de messages.
//
// Responsabilités:
//   - Découvrir les périphériques MIDI disponibles (USB, réseau, Bluetooth)
//   - Gérer les connexions/déconnexions
//   - Envoyer des messages MIDI aux périphériques
//   - Notifier les changements de périphériques (Observer Pattern)
//   - Support hot-plug USB
//
// Auteur: midiMind Team
// Date: 2025-10-02
// Version: 3.0.0
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <memory>              // Pour std::shared_ptr
#include <string>              // Pour std::string
#include <vector>              // Pour std::vector
#include <map>                 // Pour std::map
#include <mutex>               // Pour std::mutex
#include <functional>          // Pour std::function (callbacks)

#include "../MidiMessage.h"
#include "../../core/Logger.h"

namespace midiMind {

// ============================================================================
// ÉNUMÉRATIONS
// ============================================================================

/**
 * @enum DeviceType
 * @brief Types de périphériques MIDI
 */
enum class DeviceType {
    USB,           ///< Périphérique USB MIDI (ALSA)
    NETWORK,       ///< Périphérique réseau (RTP-MIDI, WiFi MIDI)
    BLUETOOTH,     ///< Périphérique Bluetooth MIDI
    VIRTUAL,       ///< Port MIDI virtuel
    UNKNOWN        ///< Type inconnu
};

/**
 * @enum DeviceDirection
 * @brief Direction du périphérique
 */
enum class DeviceDirection {
    INPUT,         ///< Entrée seulement (reçoit MIDI)
    OUTPUT,        ///< Sortie seulement (envoie MIDI)
    BIDIRECTIONAL  ///< Bidirectionnel (entrée et sortie)
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct DeviceInfo
 * @brief Informations sur un périphérique MIDI
 */
struct DeviceInfo {
    std::string id;                 ///< ID unique du périphérique
    std::string name;               ///< Nom du périphérique
    DeviceType type;                ///< Type de périphérique
    DeviceDirection direction;      ///< Direction (I/O)
    bool connected;                 ///< État de connexion
    
    // Informations spécifiques selon le type
    std::string manufacturer;       ///< Fabricant (si disponible)
    std::string model;              ///< Modèle (si disponible)
    std::string address;            ///< Adresse (IP pour réseau, MAC pour BT)
    int port;                       ///< Port (pour périphériques réseau)
    
    /**
     * @brief Constructeur par défaut
     */
    DeviceInfo()
        : type(DeviceType::UNKNOWN)
        , direction(DeviceDirection::OUTPUT)
        , connected(false)
        , port(0) {}
    
    /**
     * @brief Constructeur avec paramètres
     */
    DeviceInfo(const std::string& deviceId, const std::string& deviceName,
               DeviceType deviceType, DeviceDirection dir = DeviceDirection::OUTPUT)
        : id(deviceId)
        , name(deviceName)
        , type(deviceType)
        , direction(dir)
        , connected(false)
        , port(0) {}
};

// ============================================================================
// INTERFACE: MidiDevice (abstraction)
// ============================================================================

/**
 * @class MidiDevice
 * @brief Interface abstraite pour un périphérique MIDI
 * 
 * Classe de base pour tous les types de périphériques MIDI.
 * Chaque type (USB, Network, Bluetooth) implémente cette interface.
 */
class MidiDevice {
public:
    /**
     * @brief Destructeur virtuel
     */
    virtual ~MidiDevice() = default;
    
    /**
     * @brief Connecte au périphérique
     * 
     * @return true Si la connexion a réussi
     * @return false Si la connexion a échoué
     */
    virtual bool connect() = 0;
    
    /**
     * @brief Déconnecte du périphérique
     */
    virtual void disconnect() = 0;
    
    /**
     * @brief Vérifie si le périphérique est connecté
     * 
     * @return true Si connecté
     */
    virtual bool isConnected() const = 0;
    
    /**
     * @brief Envoie un message MIDI
     * 
     * @param message Message MIDI à envoyer
     * @return true Si l'envoi a réussi
     */
    virtual bool sendMessage(const MidiMessage& message) = 0;
    
    /**
     * @brief Récupère les informations du périphérique
     * 
     * @return DeviceInfo Informations
     */
    virtual DeviceInfo getInfo() const = 0;
    
    /**
     * @brief Récupère l'ID du périphérique
     * 
     * @return std::string ID unique
     */
    virtual std::string getId() const = 0;
    
    /**
     * @brief Récupère le nom du périphérique
     * 
     * @return std::string Nom
     */
    virtual std::string getName() const = 0;
};

// ============================================================================
// CLASSE: MidiDeviceManager
// ============================================================================

/**
 * @class MidiDeviceManager
 * @brief Gestionnaire centralisé des périphériques MIDI
 * 
 * Cette classe gère tous les périphériques MIDI de l'application:
 * - Découverte automatique (USB, réseau, Bluetooth)
 * - Connexion/déconnexion
 * - Envoi de messages
 * - Notifications via callbacks (Observer Pattern)
 * 
 * @details
 * Le gestionnaire maintient une liste de tous les périphériques disponibles
 * et connectés. Il supporte le hot-plug USB et la découverte dynamique
 * des périphériques réseau et Bluetooth.
 * 
 * @note Thread-safe : toutes les méthodes sont thread-safe
 * 
 * @example Utilisation:
 * @code
 * auto manager = std::make_shared<MidiDeviceManager>();
 * 
 * // Découvrir les périphériques
 * auto devices = manager->discoverDevices();
 * 
 * // Connecter à un périphérique
 * if (manager->connect(devices[0].id)) {
 *     // Envoyer un message
 *     auto msg = MidiMessage::noteOn(0, 60, 100);
 *     manager->sendMessage(devices[0].id, msg);
 * }
 * @endcode
 */
class MidiDeviceManager {
public:
    // ========================================================================
    // TYPES - CALLBACKS
    // ========================================================================
    
    /**
     * @typedef DeviceCallback
     * @brief Callback appelé lors de changements de périphériques
     */
    using DeviceCallback = std::function<void(const std::string& deviceId)>;
    
    // ========================================================================
    // CONSTRUCTEUR / DESTRUCTEUR
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    MidiDeviceManager() {
        Logger::info("MidiDeviceManager", "MidiDeviceManager constructed");
    }
    
    /**
     * @brief Destructeur
     */
    ~MidiDeviceManager() {
        disconnectAll();
        Logger::info("MidiDeviceManager", "MidiDeviceManager destroyed");
    }
    
    // Désactiver copie et assignation
    MidiDeviceManager(const MidiDeviceManager&) = delete;
    MidiDeviceManager& operator=(const MidiDeviceManager&) = delete;
    
    // ========================================================================
    // DÉCOUVERTE DE PÉRIPHÉRIQUES
    // ========================================================================
    
    /**
     * @brief Découvre tous les périphériques MIDI disponibles
     * 
     * Scanne tous les types de périphériques (USB, réseau, Bluetooth).
     * 
     * @param rescan Si true, force un nouveau scan
     * @return std::vector<DeviceInfo> Liste des périphériques découverts
     */
    std::vector<DeviceInfo> discoverDevices(bool rescan = false) {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        if (rescan) {
            Logger::info("MidiDeviceManager", "Rescanning for MIDI devices...");
            availableDevices_.clear();
        }
        
        // Scanner USB (ALSA)
        discoverUSBDevices();
        
        // Scanner réseau (RTP-MIDI)
        discoverNetworkDevices();
        
        // Scanner Bluetooth
        discoverBluetoothDevices();
        
        Logger::info("MidiDeviceManager", 
            "Found " + std::to_string(availableDevices_.size()) + " devices");
        
        return availableDevices_;
    }
    
    /**
     * @brief Récupère la liste des périphériques disponibles (cache)
     * 
     * @return std::vector<DeviceInfo> Liste des périphériques
     */
    std::vector<DeviceInfo> getAvailableDevices() const {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        return availableDevices_;
    }
    
    /**
     * @brief Récupère les informations d'un périphérique
     * 
     * @param deviceId ID du périphérique
     * @return DeviceInfo Informations (vide si non trouvé)
     */
    DeviceInfo getDeviceInfo(const std::string& deviceId) const {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        for (const auto& info : availableDevices_) {
            if (info.id == deviceId) {
                return info;
            }
        }
        
        return DeviceInfo();  // Vide si non trouvé
    }
    
    // ========================================================================
    // CONNEXION / DÉCONNEXION
    // ========================================================================
    
    /**
     * @brief Connecte à un périphérique
     * 
     * @param deviceId ID du périphérique
     * @return true Si la connexion a réussi
     * @return false Si la connexion a échoué
     */
    bool connect(const std::string& deviceId) {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        // Vérifier si déjà connecté
        if (connectedDevices_.find(deviceId) != connectedDevices_.end()) {
            Logger::warn("MidiDeviceManager", 
                "Device already connected: " + deviceId);
            return true;
        }
        
        // Trouver le périphérique dans la liste disponible
        DeviceInfo* info = nullptr;
        for (auto& dev : availableDevices_) {
            if (dev.id == deviceId) {
                info = &dev;
                break;
            }
        }
        
        if (!info) {
            Logger::error("MidiDeviceManager", 
                "Device not found: " + deviceId);
            return false;
        }
        
        // Créer l'instance du périphérique selon le type
        std::shared_ptr<MidiDevice> device = createDevice(*info);
        
        if (!device) {
            Logger::error("MidiDeviceManager", 
                "Failed to create device: " + deviceId);
            return false;
        }
        
        // Connecter
        if (!device->connect()) {
            Logger::error("MidiDeviceManager", 
                "Failed to connect to device: " + deviceId);
            return false;
        }
        
        // Ajouter à la liste des connectés
        connectedDevices_[deviceId] = device;
        info->connected = true;
        
        Logger::info("MidiDeviceManager", 
            "Connected to device: " + deviceId + " (" + info->name + ")");
        
        // Notifier les callbacks
        if (onDeviceConnected_) {
            onDeviceConnected_(deviceId);
        }
        
        return true;
    }
    
    /**
     * @brief Déconnecte d'un périphérique
     * 
     * @param deviceId ID du périphérique
     * @return true Si la déconnexion a réussi
     */
    bool disconnect(const std::string& deviceId) {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = connectedDevices_.find(deviceId);
        if (it == connectedDevices_.end()) {
            Logger::warn("MidiDeviceManager", 
                "Device not connected: " + deviceId);
            return false;
        }
        
        // Déconnecter
        it->second->disconnect();
        connectedDevices_.erase(it);
        
        // Mettre à jour l'info
        for (auto& dev : availableDevices_) {
            if (dev.id == deviceId) {
                dev.connected = false;
                break;
            }
        }
        
        Logger::info("MidiDeviceManager", 
            "Disconnected from device: " + deviceId);
        
        // Notifier les callbacks
        if (onDeviceDisconnected_) {
            onDeviceDisconnected_(deviceId);
        }
        
        return true;
    }
    
    /**
     * @brief Déconnecte tous les périphériques
     */
    void disconnectAll() {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        Logger::info("MidiDeviceManager", 
            "Disconnecting " + std::to_string(connectedDevices_.size()) + " devices...");
        
        for (auto& [id, device] : connectedDevices_) {
            device->disconnect();
            
            // Mettre à jour l'info
            for (auto& dev : availableDevices_) {
                if (dev.id == id) {
                    dev.connected = false;
                    break;
                }
            }
        }
        
        connectedDevices_.clear();
        Logger::info("MidiDeviceManager", "All devices disconnected");
    }
    
    /**
     * @brief Vérifie si un périphérique est connecté
     * 
     * @param deviceId ID du périphérique
     * @return true Si connecté
     */
    bool isConnected(const std::string& deviceId) const {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        return connectedDevices_.find(deviceId) != connectedDevices_.end();
    }
    
    /**
     * @brief Récupère la liste des périphériques connectés
     * 
     * @return std::vector<std::shared_ptr<MidiDevice>> Liste des périphériques
     */
    std::vector<std::shared_ptr<MidiDevice>> getConnectedDevices() const {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        std::vector<std::shared_ptr<MidiDevice>> result;
        result.reserve(connectedDevices_.size());
        
        for (const auto& [id, device] : connectedDevices_) {
            result.push_back(device);
        }
        
        return result;
    }
    
    // ========================================================================
    // ENVOI DE MESSAGES
    // ========================================================================
    
    /**
     * @brief Envoie un message MIDI à un périphérique
     * 
     * @param deviceId ID du périphérique
     * @param message Message MIDI
     * @return true Si l'envoi a réussi
     */
    bool sendMessage(const std::string& deviceId, const MidiMessage& message) {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        auto it = connectedDevices_.find(deviceId);
        if (it == connectedDevices_.end()) {
            Logger::warn("MidiDeviceManager", 
                "Cannot send to disconnected device: " + deviceId);
            return false;
        }
        
        return it->second->sendMessage(message);
    }
    
    /**
     * @brief Envoie un message à tous les périphériques connectés
     * 
     * @param message Message MIDI
     */
    void broadcastMessage(const MidiMessage& message) {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        
        for (auto& [id, device] : connectedDevices_) {
            device->sendMessage(message);
        }
    }
    
    // ========================================================================
    // CALLBACKS (OBSERVER PATTERN)
    // ========================================================================
    
    /**
     * @brief Définit le callback de connexion
     * 
     * @param callback Fonction appelée lors de la connexion d'un périphérique
     */
    void onDeviceConnected(DeviceCallback callback) {
        onDeviceConnected_ = callback;
    }
    
    /**
     * @brief Définit le callback de déconnexion
     * 
     * @param callback Fonction appelée lors de la déconnexion d'un périphérique
     */
    void onDeviceDisconnected(DeviceCallback callback) {
        onDeviceDisconnected_ = callback;
    }

private:
    // ========================================================================
    // MÉTHODES PRIVÉES - DÉCOUVERTE PAR TYPE
    // ========================================================================
    
    /**
     * @brief Découvre les périphériques USB (ALSA)
     */
    void discoverUSBDevices() {
        // TODO: Implémenter la découverte ALSA
        // Pour l'instant, ajouter un périphérique factice pour tests
        
        DeviceInfo dev("usb_test_1", "Test USB MIDI Device", 
                      DeviceType::USB, DeviceDirection::OUTPUT);
        dev.manufacturer = "Test Manufacturer";
        dev.model = "Test Model";
        
        availableDevices_.push_back(dev);
    }
    
    /**
     * @brief Découvre les périphériques réseau (RTP-MIDI)
     */
    void discoverNetworkDevices() {
        // TODO: Implémenter la découverte RTP-MIDI
    }
    
    /**
     * @brief Découvre les périphériques Bluetooth
     */
    void discoverBluetoothDevices() {
        // TODO: Implémenter la découverte Bluetooth MIDI
    }
    
    /**
     * @brief Crée une instance de périphérique selon le type
     * 
     * @param info Informations du périphérique
     * @return std::shared_ptr<MidiDevice> Instance créée, ou nullptr si échec
     */
    std::shared_ptr<MidiDevice> createDevice(const DeviceInfo& info) {
        // TODO: Implémenter la création selon le type
        // Pour l'instant, retourner nullptr (périphérique factice)
        Logger::warn("MidiDeviceManager", 
            "Device creation not yet implemented for type: " + 
            std::to_string(static_cast<int>(info.type)));
        return nullptr;
    }
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Liste des périphériques disponibles (découverts)
    std::vector<DeviceInfo> availableDevices_;
    
    /// Périphériques connectés (ID -> Device)
    std::map<std::string, std::shared_ptr<MidiDevice>> connectedDevices_;
    
    /// Mutex pour thread-safety
    mutable std::mutex devicesMutex_;
    
    /// Callbacks
    DeviceCallback onDeviceConnected_;
    DeviceCallback onDeviceDisconnected_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiDeviceManager.h
// ============================================================================
