// ============================================================================
// Fichier: src/network/wifi/WiFiHotspot.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestion du WiFi Hotspot (mode point d'accès).
//   Configure le Raspberry Pi en point d'accès WiFi pour permettre
//   aux tablettes/smartphones de se connecter directement.
//
// Responsabilités:
//   - Configurer hostapd (daemon point d'accès)
//   - Configurer dnsmasq (serveur DHCP/DNS)
//   - Gérer l'interface réseau (wlan0)
//   - Monitorer les clients connectés
//
// Thread-safety: OUI
//
// Dépendances: hostapd, dnsmasq, iproute2
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <thread>
#include <nlohmann/json.hpp>

#include "../../core/Logger.h"

using json = nlohmann::json;

namespace midiMind {

/**
 * @struct WiFiClient
 * @brief Information sur un client WiFi connecté
 */
struct WiFiClient {
    std::string macAddress;     ///< Adresse MAC
    std::string ipAddress;      ///< Adresse IP attribuée
    std::string hostname;       ///< Nom d'hôte (si disponible)
    uint64_t connectedSince;    ///< Timestamp de connexion (ms)
    uint64_t bytesReceived;     ///< Bytes reçus
    uint64_t bytesSent;         ///< Bytes envoyés
    int signalStrength;         ///< Force du signal (dBm)
    
    json toJson() const {
        json j;
        j["mac_address"] = macAddress;
        j["ip_address"] = ipAddress;
        j["hostname"] = hostname;
        j["connected_since"] = connectedSince;
        j["bytes_received"] = bytesReceived;
        j["bytes_sent"] = bytesSent;
        j["signal_strength"] = signalStrength;
        return j;
    }
};

/**
 * @class WiFiHotspot
 * @brief Gestionnaire de hotspot WiFi
 * 
 * @details
 * Configure le Raspberry Pi en mode point d'accès WiFi:
 * - hostapd: Gère le point d'accès WiFi
 * - dnsmasq: Fournit DHCP et DNS
 * - IP fixe: 192.168.4.1 par défaut
 * - Plage DHCP: 192.168.4.2-192.168.4.20
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @note Nécessite les privilèges root pour configurer le réseau
 * 
 * @example Utilisation
 * ```cpp
 * WiFiHotspot hotspot;
 * 
 * hotspot.setOnClientConnected([](const WiFiClient& client) {
 *     Logger::info("WiFi", "Client connected: " + client.ipAddress);
 * });
 * 
 * hotspot.start("MidiMind-Studio", "midimind2025", 6);
 * ```
 */
class WiFiHotspot {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé lors de la connexion d'un client
     */
    using ClientConnectedCallback = std::function<void(const WiFiClient&)>;
    
    /**
     * @brief Callback appelé lors de la déconnexion d'un client
     */
    using ClientDisconnectedCallback = std::function<void(const std::string& macAddress)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    WiFiHotspot();
    
    /**
     * @brief Destructeur
     */
    ~WiFiHotspot();
    
    // Désactiver copie
    WiFiHotspot(const WiFiHotspot&) = delete;
    WiFiHotspot& operator=(const WiFiHotspot&) = delete;
    
    // ========================================================================
    // CONTRÔLE
    // ========================================================================
    
    /**
     * @brief Démarre le hotspot WiFi
     * 
     * @param ssid SSID du réseau (nom visible)
     * @param password Mot de passe WPA2 (min. 8 caractères)
     * @param channel Canal WiFi (1-11 pour 2.4GHz)
     * @param ipAddress IP du Raspberry Pi (défaut: 192.168.4.1)
     * @return true Si le démarrage a réussi
     * 
     * @note Nécessite les privilèges root (sudo)
     * 
     * @example
     * ```cpp
     * hotspot.start("MidiMind-Studio", "supersecret123", 6);
     * ```
     */
    bool start(const std::string& ssid,
              const std::string& password,
              uint8_t channel = 6,
              const std::string& ipAddress = "192.168.4.1");
    
    /**
     * @brief Arrête le hotspot
     * 
     * @note Restaure la configuration réseau précédente
     */
    void stop();
    
    /**
     * @brief Vérifie si le hotspot est actif
     * 
     * @return true Si actif
     */
    bool isRunning() const;
    
    // ========================================================================
    // GESTION DES CLIENTS
    // ========================================================================
    
    /**
     * @brief Liste les clients connectés
     * 
     * @return std::vector<WiFiClient> Liste des clients
     */
    std::vector<WiFiClient> listClients() const;
    
    /**
     * @brief Récupère les informations d'un client
     * 
     * @param macAddress Adresse MAC du client
     * @return std::optional<WiFiClient> Info ou nullopt
     */
    std::optional<WiFiClient> getClient(const std::string& macAddress) const;
    
    /**
     * @brief Déconnecte un client
     * 
     * @param macAddress Adresse MAC du client à déconnecter
     * @return true Si la déconnexion a réussi
     */
    bool disconnectClient(const std::string& macAddress);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de connexion client
     */
    void setOnClientConnected(ClientConnectedCallback callback);
    
    /**
     * @brief Définit le callback de déconnexion client
     */
    void setOnClientDisconnected(ClientDisconnectedCallback callback);
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Change le SSID (nécessite un redémarrage)
     * 
     * @param newSsid Nouveau SSID
     * @return true Si la configuration a été mise à jour
     */
    bool changeSsid(const std::string& newSsid);
    
    /**
     * @brief Change le mot de passe (nécessite un redémarrage)
     * 
     * @param newPassword Nouveau mot de passe
     * @return true Si la configuration a été mise à jour
     */
    bool changePassword(const std::string& newPassword);
    
    /**
     * @brief Change le canal WiFi (nécessite un redémarrage)
     * 
     * @param channel Nouveau canal (1-11)
     * @return true Si la configuration a été mise à jour
     */
    bool changeChannel(uint8_t channel);
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques du hotspot
     * 
     * @return json Statistiques
     * 
     * Format:
     * ```json
     * {
     *   "running": true,
     *   "ssid": "MidiMind-Studio",
     *   "channel": 6,
     *   "ip_address": "192.168.4.1",
     *   "connected_clients": 3,
     *   "bytes_received": 123456,
     *   "bytes_sent": 789012
     * }
     * ```
     */
    json getStatistics() const;
    
    /**
     * @brief Récupère la configuration actuelle
     * 
     * @return json Configuration
     */
    json getConfiguration() const;
    
    /**
     * @brief Vérifie si les dépendances sont installées
     * 
     * @return true Si hostapd et dnsmasq sont disponibles
     */
    static bool areDependenciesInstalled();

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Thread de monitoring des clients
     */
    void monitoringLoop();
    
    /**
     * @brief Configure hostapd
     */
    bool configureHostapd();
    
    /**
     * @brief Configure dnsmasq
     */
    bool configureDnsmasq();
    
    /**
     * @brief Configure l'interface réseau
     */
    bool configureInterface();
    
    /**
     * @brief Démarre hostapd
     */
    bool startHostapd();
    
    /**
     * @brief Arrête hostapd
     */
    void stopHostapd();
    
    /**
     * @brief Démarre dnsmasq
     */
    bool startDnsmasq();
    
    /**
     * @brief Arrête dnsmasq
     */
    void stopDnsmasq();
    
    /**
     * @brief Sauvegarde la configuration réseau actuelle
     */
    void backupNetworkConfig();
    
    /**
     * @brief Restaure la configuration réseau
     */
    void restoreNetworkConfig();
    
    /**
     * @brief Parse la liste des clients depuis hostapd/dnsmasq
     */
    std::vector<WiFiClient> parseConnectedClients() const;
    
    /**
     * @brief Exécute une commande système
     */
    bool executeCommand(const std::string& command) const;
    
    /**
     * @brief Lit le contenu d'un fichier
     */
    std::string readFile(const std::string& path) const;
    
    /**
     * @brief Écrit dans un fichier
     */
    bool writeFile(const std::string& path, const std::string& content) const;
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// État
    std::atomic<bool> running_;
    
    /// Thread de monitoring
    std::thread monitoringThread_;
    
    /// Configuration
    std::string ssid_;
    std::string password_;
    uint8_t channel_;
    std::string ipAddress_;
    std::string interface_;  // Généralement "wlan0"
    
    /// Clients connectés
    std::vector<WiFiClient> connectedClients_;
    
    /// Callbacks
    ClientConnectedCallback onClientConnected_;
    ClientDisconnectedCallback onClientDisconnected_;
    
    /// Chemins des fichiers de configuration
    static constexpr const char* HOSTAPD_CONF = "/tmp/midimind_hostapd.conf";
    static constexpr const char* DNSMASQ_CONF = "/tmp/midimind_dnsmasq.conf";
    static constexpr const char* BACKUP_CONF = "/tmp/midimind_network_backup.conf";
    
    /// PIDs des processus
    int hostapdPid_;
    int dnsmasqPid_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER WiFiHotspot.h
// ============================================================================