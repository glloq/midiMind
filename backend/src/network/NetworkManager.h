// ============================================================================
// Fichier: src/network/networkmanager.h
// Version: 3.0.0 - COMPLET
// Date: 2025-10-15
// ============================================================================
// Description:
//   Gestionnaire réseau principal pour MidiMind v3.0
//   Orchestre tous les modules réseau : WiFi, Bluetooth, mDNS, RTP-MIDI, BLE
//
// Architecture:
//   - Thread-safe avec mutex
//   - Non-bloquant via callbacks
//   - Gestion centralisée de tous les composants réseau
//
// Composants gérés:
//   - WifiManager          : Connexion WiFi client
//   - BluetoothManager     : Découverte/pairing Bluetooth
//   - WiFiHotspot          : Mode Access Point
//   - BleMidiDevice        : BLE MIDI
//   - MdnsDiscovery        : Découverte mDNS/Bonjour
//   - RtpMidiServer        : Serveur RTP-MIDI
// ============================================================================

#ifndef MIDIMIND_NETWORKMANAGER_H
#define MIDIMIND_NETWORKMANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <optional>
#include "../external/json.hpp"

// Composants réseau
#include "WifiManager.h"
#include "BluetoothManager.h"
#include "wifi/WiFiHotspot.h"
#include "bluetooth/BleMidiDevice.h"
#include "discovery/MdnsDiscovery.h"
#include "rtpmidi/RtpMidiServer.h"

namespace midiMind {

using json = nlohmann::json;

// ============================================================================
// ÉNUMÉRATIONS
// ============================================================================

/**
 * @brief Type de device réseau
 */
enum class NetworkDeviceType {
    UNKNOWN,
    RTP_MIDI,           ///< Device RTP-MIDI (réseau)
    BLE_MIDI,           ///< Device BLE MIDI (Bluetooth)
    WIFI_CLIENT,        ///< Client WiFi connecté au hotspot
    BLUETOOTH_DEVICE    ///< Device Bluetooth générique
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @brief Informations sur un device réseau
 */
struct NetworkDeviceInfo {
    std::string id;                 ///< Identifiant unique
    std::string name;               ///< Nom du device
    NetworkDeviceType type;         ///< Type de device
    std::string address;            ///< Adresse (IP, MAC, etc.)
    uint16_t port;                  ///< Port (si applicable)
    bool connected;                 ///< État de connexion
    uint64_t lastSeen;              ///< Timestamp dernière activité (ms)
};

/**
 * @brief Statistiques réseau globales
 */
struct NetworkStatistics {
    // RTP-MIDI
    size_t rtpDevicesDiscovered;
    size_t rtpDevicesConnected;
    uint64_t rtpBytesReceived;
    uint64_t rtpBytesSent;
    
    // BLE MIDI
    size_t bleDevicesConnected;
    uint64_t bleBytesReceived;
    uint64_t bleBytesSent;
    
    // WiFi Hotspot
    bool hotspotActive;
    size_t hotspotClients;
    
    // WiFi Client
    bool wifiConnected;
    std::string wifiSsid;
    int wifiSignalStrength;
    
    // Bluetooth
    size_t bluetoothDevicesDiscovered;
    size_t bluetoothDevicesPaired;
};

// ============================================================================
// CLASSE NETWORKMANAGER
// ============================================================================

/**
 * @brief Gestionnaire réseau principal
 * 
 * Orchestre tous les modules réseau de MidiMind :
 * - WiFi (client et hotspot)
 * - Bluetooth (découverte, pairing, BLE MIDI)
 * - mDNS (découverte de services)
 * - RTP-MIDI (serveur réseau)
 * 
 * @note Thread-safe, toutes les opérations sont protégées par mutex
 */
class NetworkManager {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé lors de la découverte d'un device
     */
    using DeviceDiscoveredCallback = std::function<void(const NetworkDeviceInfo&)>;
    
    /**
     * @brief Callback appelé lors de la connexion d'un device
     */
    using DeviceConnectedCallback = std::function<void(const std::string& deviceId)>;
    
    /**
     * @brief Callback appelé lors de la déconnexion d'un device
     */
    using DeviceDisconnectedCallback = std::function<void(const std::string& deviceId)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * Initialise tous les sous-gestionnaires réseau.
     * 
     * @throws std::runtime_error Si l'initialisation échoue
     */
    NetworkManager();
    
    /**
     * @brief Destructeur
     * 
     * Arrête tous les services et libère les ressources proprement.
     */
    ~NetworkManager();
    
    // Désactiver copie
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;
    
    // ========================================================================
    // WIFI CLIENT
    // ========================================================================
    
    /**
     * @brief Lance un scan des réseaux WiFi disponibles
     * 
     * @return true Si le scan a été lancé
     * 
     * @note Asynchrone, utiliser WifiManager::setOnScanComplete()
     */
    bool startWifiScan();
    
    /**
     * @brief Se connecte à un réseau WiFi
     * 
     * @param ssid SSID du réseau
     * @param password Mot de passe
     * @param autoReconnect Activer la reconnexion automatique
     * 
     * @return true Si la connexion a été lancée
     */
    bool connectWifi(const std::string& ssid, 
                     const std::string& password,
                     bool autoReconnect = true);
    
    /**
     * @brief Se déconnecte du réseau WiFi actuel
     */
    bool disconnectWifi();
    
    /**
     * @brief Vérifie si connecté à un réseau WiFi
     */
    bool isWifiConnected() const;
    
    /**
     * @brief Récupère les réseaux WiFi découverts
     */
    std::vector<WiFiNetwork> getWifiNetworks() const;
    
    // ========================================================================
    // WIFI HOTSPOT
    // ========================================================================
    
    /**
     * @brief Démarre le hotspot WiFi
     * 
     * @param ssid SSID du hotspot
     * @param password Mot de passe (min. 8 caractères)
     * @param channel Canal WiFi (1-11)
     * 
     * @return true Si le hotspot a démarré
     * 
     * @note Nécessite les privilèges root ou capabilities
     */
    bool startWiFiHotspot(const std::string& ssid, 
                         const std::string& password,
                         uint8_t channel = 6);
    
    /**
     * @brief Arrête le hotspot WiFi
     */
    void stopWiFiHotspot();
    
    /**
     * @brief Vérifie si le hotspot est actif
     */
    bool isWiFiHotspotRunning() const;
    
    /**
     * @brief Liste les clients connectés au hotspot
     */
    std::vector<WiFiClient> getHotspotClients() const;
    
    // ========================================================================
    // BLUETOOTH
    // ========================================================================
    
    /**
     * @brief Lance un scan des appareils Bluetooth
     * 
     * @param duration Durée du scan en secondes (0 = infini)
     * 
     * @return true Si le scan a été lancé
     */
    bool startBluetoothScan(int duration = 10);
    
    /**
     * @brief Arrête le scan Bluetooth
     */
    void stopBluetoothScan();
    
    /**
     * @brief Appaire avec un appareil Bluetooth
     * 
     * @param address Adresse MAC de l'appareil
     * @param pin Code PIN si nécessaire
     * 
     * @return true Si le pairing a réussi
     */
    bool pairBluetoothDevice(const std::string& address, const std::string& pin = "");
    
    /**
     * @brief Connecte à un appareil Bluetooth
     * 
     * @param address Adresse MAC de l'appareil
     * 
     * @return true Si la connexion a réussi
     */
    bool connectBluetoothDevice(const std::string& address);
    
    /**
     * @brief Déconnecte un appareil Bluetooth
     */
    bool disconnectBluetoothDevice(const std::string& address);
    
    /**
     * @brief Récupère les appareils Bluetooth découverts
     */
    std::vector<BluetoothDevice> getBluetoothDevices() const;
    
    // ========================================================================
    // BLE MIDI
    // ========================================================================
    
    /**
     * @brief Démarre le service BLE MIDI
     * 
     * @param deviceName Nom du device BLE visible
     * 
     * @return true Si le service a démarré
     */
    bool startBleMidi(const std::string& deviceName = "MidiMind");
    
    /**
     * @brief Arrête le service BLE MIDI
     */
    void stopBleMidi();
    
    /**
     * @brief Vérifie si le service BLE MIDI est actif
     */
    bool isBleMidiRunning() const;
    
    // ========================================================================
    // RTP-MIDI
    // ========================================================================
    
    /**
     * @brief Démarre le serveur RTP-MIDI
     * 
     * @param port Port UDP pour les connexions (défaut: 5004)
     * 
     * @return true Si le serveur a démarré
     */
    bool startRtpMidi(uint16_t port = 5004);
    
    /**
     * @brief Arrête le serveur RTP-MIDI
     */
    void stopRtpMidi();
    
    /**
     * @brief Vérifie si le serveur RTP-MIDI est actif
     */
    bool isRtpMidiRunning() const;
    
    // ========================================================================
    // mDNS DISCOVERY
    // ========================================================================
    
    /**
     * @brief Démarre la découverte mDNS
     * 
     * @return true Si la découverte a démarré
     */
    bool startDiscovery();
    
    /**
     * @brief Arrête la découverte mDNS
     */
    void stopDiscovery();
    
    /**
     * @brief Publie un service mDNS
     * 
     * @param name Nom du service
     * @param type Type de service (ex: "_apple-midi._udp")
     * @param port Port du service
     * 
     * @return true Si le service a été publié
     */
    bool publishService(const std::string& name, 
                       const std::string& type,
                       uint16_t port);
    
    // ========================================================================
    // GESTION DES DEVICES
    // ========================================================================
    
    /**
     * @brief Liste tous les devices réseau découverts
     * 
     * @return Liste des devices (RTP-MIDI, BLE, WiFi, Bluetooth)
     */
    std::vector<NetworkDeviceInfo> listDevices() const;
    
    /**
     * @brief Connecte à un device réseau
     * 
     * @param deviceId Identifiant du device
     * 
     * @return true Si la connexion a été lancée
     */
    bool connectDevice(const std::string& deviceId);
    
    /**
     * @brief Déconnecte un device réseau
     * 
     * @param deviceId Identifiant du device
     * 
     * @return true Si la déconnexion a réussi
     */
    bool disconnectDevice(const std::string& deviceId);
    
    /**
     * @brief Récupère les informations d'un device
     * 
     * @param deviceId Identifiant du device
     * 
     * @return Informations du device ou nullopt si non trouvé
     */
    std::optional<NetworkDeviceInfo> getDevice(const std::string& deviceId) const;
    
    // ========================================================================
    // STATISTIQUES & INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques réseau globales
     * 
     * Agrège les statistiques de tous les modules réseau.
     * 
     * @return Statistiques complètes
     */
    NetworkStatistics getStatistics() const;
    
    /**
     * @brief Récupère l'adresse IP locale
     * 
     * @return Adresse IP (ex: "192.168.1.100")
     */
    std::string getLocalIpAddress() const;
    
    /**
     * @brief Récupère les informations réseau complètes
     * 
     * Inclut : IP, MAC, hostname, mode réseau, interfaces, trafic
     * 
     * @return Informations JSON
     */
    json getNetworkInfo() const;
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de découverte de device
     * 
     * Appelé lorsqu'un nouveau device est découvert sur le réseau.
     * 
     * @param callback Fonction à appeler
     */
    void setOnDeviceDiscovered(DeviceDiscoveredCallback callback);
    
    /**
     * @brief Définit le callback de connexion de device
     * 
     * @param callback Fonction à appeler
     */
    void setOnDeviceConnected(DeviceConnectedCallback callback);
    
    /**
     * @brief Définit le callback de déconnexion de device
     * 
     * @param callback Fonction à appeler
     */
    void setOnDeviceDisconnected(DeviceDisconnectedCallback callback);
    
    // ========================================================================
    // ACCÈS AUX GESTIONNAIRES (pour usage avancé)
    // ========================================================================
    
    /**
     * @brief Accès direct au WifiManager
     * 
     * @return Référence au gestionnaire WiFi
     * 
     * @note Pour usage avancé uniquement
     */
    WifiManager& getWifiManager() { return *wifiManager_; }
    
    /**
     * @brief Accès direct au BluetoothManager
     */
    BluetoothManager& getBluetoothManager() { return *bluetoothManager_; }
    
    /**
     * @brief Accès direct au WiFiHotspot
     */
    WiFiHotspot& getWiFiHotspot() { return *wifiHotspot_; }
    
    /**
     * @brief Accès direct au BleMidiDevice
     */
    BleMidiDevice& getBleMidiDevice() { return *bleMidiDevice_; }
    
    /**
     * @brief Accès direct au MdnsDiscovery
     */
    MdnsDiscovery& getMdnsDiscovery() { return *mdnsDiscovery_; }
    
    /**
     * @brief Accès direct au RtpMidiServer
     */
    RtpMidiServer& getRtpMidiServer() { return *rtpMidiServer_; }

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Gère la découverte d'un device
     */
    void handleDeviceDiscovered(const NetworkDeviceInfo& info);
    
    /**
     * @brief Gère la connexion d'un device
     */
    void handleDeviceConnected(const std::string& deviceId);
    
    /**
     * @brief Gère la déconnexion d'un device
     */
    void handleDeviceDisconnected(const std::string& deviceId);
    
    /**
     * @brief Détecte l'adresse IP locale
     */
    std::string detectLocalIpAddress() const;
    
    /**
     * @brief Détecte l'adresse MAC
     */
    std::string detectMacAddress() const;
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    // Thread-safety
    mutable std::mutex mutex_;
    
    // Gestionnaires réseau
    std::unique_ptr<WifiManager> wifiManager_;
    std::unique_ptr<BluetoothManager> bluetoothManager_;
    std::unique_ptr<WiFiHotspot> wifiHotspot_;
    std::unique_ptr<BleMidiDevice> bleMidiDevice_;
    std::unique_ptr<MdnsDiscovery> mdnsDiscovery_;
    std::unique_ptr<RtpMidiServer> rtpMidiServer_;
    
    // État
    std::vector<NetworkDeviceInfo> discoveredDevices_;
    NetworkStatistics stats_;
    
    // Callbacks
    DeviceDiscoveredCallback onDeviceDiscovered_;
    DeviceConnectedCallback onDeviceConnected_;
    DeviceDisconnectedCallback onDeviceDisconnected_;
};

} // namespace midiMind

#endif // MIDIMIND_NETWORKMANAGER_H

// ============================================================================
// FIN DU FICHIER networkmanager.h v3.0.0
// ============================================================================