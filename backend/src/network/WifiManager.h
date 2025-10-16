// ============================================================================
// Fichier: src/network/WifiManager.h
// Version: 1.0.0
// Date: 2025-10-15
// ============================================================================
// Description:
//   Gestionnaire WiFi pour connexion client à des réseaux existants.
//   Complète WiFiHotspot.cpp (mode AP) avec le mode client.
//   Utilise wpa_supplicant via D-Bus ou commandes système.
//
// Fonctionnalités:
//   - Scan des réseaux WiFi disponibles
//   - Connexion à un réseau avec SSID/password
//   - Déconnexion et gestion d'état
//   - Surveillance de la connexion (signal, débit)
//   - Auto-reconnexion
//
// Architecture:
//   Thread-safe avec mutex
//   Non-bloquant via threads
//   Callbacks pour événements
// ============================================================================

#ifndef MIDIMIND_WIFIMANAGER_H
#define MIDIMIND_WIFIMANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <optional>
#include <nlohmann/json.hpp>
namespace midiMind {

using json = nlohmann::json;

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @brief Informations sur un réseau WiFi
 */
struct WiFiNetwork {
    std::string ssid;              ///< SSID du réseau
    std::string bssid;             ///< BSSID (adresse MAC de l'AP)
    int signalStrength;            ///< Force du signal (dBm)
    int frequency;                 ///< Fréquence (MHz)
    int channel;                   ///< Canal WiFi (1-14)
    std::string security;          ///< Type de sécurité (WPA2, WEP, Open)
    bool connected;                ///< Actuellement connecté
};

/**
 * @brief Statistiques de connexion WiFi
 */
struct WiFiConnectionStats {
    bool connected;
    std::string ssid;
    std::string bssid;
    int signalStrength;            ///< dBm
    int linkSpeed;                 ///< Mbps
    int frequency;                 ///< MHz
    std::string ipAddress;
    uint64_t bytesReceived;
    uint64_t bytesSent;
    uint64_t uptime;               ///< Secondes
};

// ============================================================================
// CLASSE WIFIMANAGER
// ============================================================================

/**
 * @brief Gestionnaire de connexion WiFi client
 * 
 * Gère les connexions WiFi en mode client (connexion à des réseaux existants).
 * Complémentaire de WiFiHotspot (mode AP).
 * 
 * @note Thread-safe, non-bloquant
 */
class WifiManager {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé quand le scan est terminé
     * @param networks Liste des réseaux découverts
     */
    using ScanCompleteCallback = std::function<void(const std::vector<WiFiNetwork>&)>;
    
    /**
     * @brief Callback appelé lors de la connexion
     * @param success true si connexion réussie
     * @param ssid SSID du réseau
     */
    using ConnectionCallback = std::function<void(bool success, const std::string& ssid)>;
    
    /**
     * @brief Callback appelé lors de la déconnexion
     * @param ssid SSID du réseau déconnecté
     */
    using DisconnectionCallback = std::function<void(const std::string& ssid)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    WifiManager();
    ~WifiManager();
    
    // Désactiver copie
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;
    
    // ========================================================================
    // SCAN
    // ========================================================================
    
    /**
     * @brief Lance un scan des réseaux WiFi disponibles
     * 
     * Opération asynchrone. Utiliser setOnScanComplete() pour récupérer les résultats.
     * 
     * @return true si le scan a été lancé
     * 
     * @note Non-bloquant
     */
    bool startScan();
    
    /**
     * @brief Vérifie si un scan est en cours
     */
    bool isScanning() const;
    
    /**
     * @brief Récupère les derniers résultats de scan
     * 
     * @return Liste des réseaux découverts lors du dernier scan
     */
    std::vector<WiFiNetwork> getLastScanResults() const;
    
    // ========================================================================
    // CONNEXION
    // ========================================================================
    
    /**
     * @brief Se connecte à un réseau WiFi
     * 
     * @param ssid SSID du réseau
     * @param password Mot de passe (vide pour réseau ouvert)
     * @param autoReconnect Activer la reconnexion automatique
     * 
     * @return true si la tentative de connexion a été lancée
     * 
     * @note Asynchrone. Utiliser setOnConnectionChange() pour notification
     */
    bool connect(const std::string& ssid, 
                 const std::string& password,
                 bool autoReconnect = true);
    
    /**
     * @brief Se déconnecte du réseau actuel
     * 
     * @return true si déconnexion lancée
     */
    bool disconnect();
    
    /**
     * @brief Vérifie si connecté à un réseau
     */
    bool isConnected() const;
    
    /**
     * @brief Récupère le SSID du réseau connecté
     * 
     * @return SSID ou chaîne vide si non connecté
     */
    std::string getConnectedSsid() const;
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques de connexion
     * 
     * @return Statistiques ou nullopt si non connecté
     */
    std::optional<WiFiConnectionStats> getConnectionStats() const;
    
    /**
     * @brief Récupère des informations JSON sur l'état WiFi
     */
    json getStatus() const;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit l'interface WiFi à utiliser
     * 
     * @param interface Nom de l'interface (ex: "wlan0")
     */
    void setInterface(const std::string& interface);
    
    /**
     * @brief Récupère le nom de l'interface
     */
    std::string getInterface() const;
    
    /**
     * @brief Active/désactive la reconnexion automatique
     */
    void setAutoReconnect(bool enable);
    
    /**
     * @brief Vérifie si l'auto-reconnexion est activée
     */
    bool isAutoReconnectEnabled() const;
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    void setOnScanComplete(ScanCompleteCallback callback);
    void setOnConnectionChange(ConnectionCallback callback);
    void setOnDisconnection(DisconnectionCallback callback);
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Vérifie si les dépendances système sont installées
     * 
     * @return true si wpasupplicant/iw sont disponibles
     */
    static bool areDependenciesInstalled();

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    // Threads
    void scanLoop();
    void monitorLoop();
    void connectionLoop();
    
    // Exécution commandes
    bool executeCommand(const std::string& command) const;
    std::string executeCommandWithOutput(const std::string& command) const;
    
    // Parsing
    std::vector<WiFiNetwork> parseIwlistOutput(const std::string& output) const;
    WiFiConnectionStats parseIwconfigOutput(const std::string& output) const;
    
    // Helpers
    std::string readFile(const std::string& path) const;
    bool writeFile(const std::string& path, const std::string& content) const;
    bool configureWpaSupplicant(const std::string& ssid, const std::string& password);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    mutable std::mutex mutex_;
    
    // Configuration
    std::string interface_;                  ///< Interface WiFi (wlan0)
    bool autoReconnect_;                     ///< Auto-reconnexion
    
    // État
    std::atomic<bool> scanning_;             ///< Scan en cours
    std::atomic<bool> connected_;            ///< Connecté
    std::atomic<bool> connecting_;           ///< Connexion en cours
    std::atomic<bool> running_;              ///< Threads actifs
    
    // Données
    std::vector<WiFiNetwork> lastScanResults_;
    std::string connectedSsid_;
    WiFiConnectionStats currentStats_;
    
    // Threads
    std::thread scanThread_;
    std::thread monitorThread_;
    std::thread connectionThread_;
    
    // Callbacks
    ScanCompleteCallback onScanComplete_;
    ConnectionCallback onConnectionChange_;
    DisconnectionCallback onDisconnection_;
    
    // Reconnexion
    std::string pendingConnectSsid_;
    std::string pendingConnectPassword_;
    int reconnectAttempts_;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 5;
};

} // namespace midiMind

#endif // MIDIMIND_WIFIMANAGER_H

// ============================================================================
// FIN DU FICHIER WifiManager.h v1.0.0
// ============================================================================