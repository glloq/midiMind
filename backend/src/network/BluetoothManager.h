// ============================================================================
// Fichier: src/network/BluetoothManager.h
// Version: 1.0.0
// Date: 2025-10-15
// ============================================================================
// Description:
//   Gestionnaire Bluetooth générique pour découverte et gestion de devices.
//   Complément de BleMidiDevice (spécifique BLE MIDI).
//   
// Fonctionnalités:
//   - Scan des appareils Bluetooth/BLE
//   - Pairing/Unpairing
//   - Connexion/Déconnexion
//   - Gestion d'état des devices
//   - Support BlueZ via D-Bus
//
// Architecture:
//   Thread-safe avec mutex
//   Asynchrone via callbacks
//   Compatible avec BleMidiDevice
// ============================================================================

#ifndef MIDIMIND_BLUETOOTHMANAGER_H
#define MIDIMIND_BLUETOOTHMANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <optional>
#include <map>
#include <nlohmann/json.hpp>

// Support BlueZ D-Bus (optionnel)
#ifdef HAS_BLUEZ
#include <gio/gio.h>
#endif

namespace midiMind {

using json = nlohmann::json;

// ============================================================================
// ÉNUMÉRATIONS
// ============================================================================

/**
 * @brief Type d'appareil Bluetooth
 */
enum class BluetoothDeviceType {
    UNKNOWN,
    AUDIO,              ///< Casque, enceinte
    INPUT,              ///< Clavier, souris
    PHONE,              ///< Smartphone
    COMPUTER,           ///< PC, laptop
    BLE_MIDI,           ///< Device BLE MIDI
    OTHER
};

/**
 * @brief État d'un appareil
 */
enum class BluetoothDeviceState {
    DISCOVERED,         ///< Découvert mais non connecté
    PAIRED,             ///< Appairé mais non connecté
    CONNECTED,          ///< Connecté
    CONNECTING,         ///< Connexion en cours
    DISCONNECTING       ///< Déconnexion en cours
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @brief Informations sur un appareil Bluetooth
 */
struct BluetoothDevice {
    std::string address;              ///< Adresse MAC (AA:BB:CC:DD:EE:FF)
    std::string name;                 ///< Nom de l'appareil
    BluetoothDeviceType type;         ///< Type d'appareil
    BluetoothDeviceState state;       ///< État actuel
    int rssi;                         ///< Force du signal (dBm)
    bool paired;                      ///< Déjà appairé
    bool trusted;                     ///< Appareil de confiance
    bool blocked;                     ///< Bloqué
    std::vector<std::string> uuids;   ///< Services UUID disponibles
    std::map<std::string, std::string> properties; ///< Propriétés additionnelles
};

// ============================================================================
// CLASSE BLUETOOTHMANAGER
// ============================================================================

/**
 * @brief Gestionnaire Bluetooth générique
 * 
 * Gère la découverte, le pairing et la connexion des appareils Bluetooth.
 * Utilise BlueZ via D-Bus sur Linux.
 * 
 * @note Thread-safe, opérations asynchrones
 */
class BluetoothManager {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé lors de la découverte d'un appareil
     */
    using DeviceDiscoveredCallback = std::function<void(const BluetoothDevice&)>;
    
    /**
     * @brief Callback appelé lors d'un changement d'état
     */
    using DeviceStateChangedCallback = std::function<void(const std::string& address, 
                                                          BluetoothDeviceState state)>;
    
    /**
     * @brief Callback appelé quand le scan est terminé
     */
    using ScanCompleteCallback = std::function<void(int devicesFound)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    BluetoothManager();
    ~BluetoothManager();
    
    // Désactiver copie
    BluetoothManager(const BluetoothManager&) = delete;
    BluetoothManager& operator=(const BluetoothManager&) = delete;
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    /**
     * @brief Initialise le gestionnaire Bluetooth
     * 
     * Se connecte à BlueZ via D-Bus et configure l'adaptateur.
     * 
     * @return true si initialisé avec succès
     */
    bool initialize();
    
    /**
     * @brief Vérifie si le gestionnaire est initialisé
     */
    bool isInitialized() const;
    
    /**
     * @brief Vérifie si le Bluetooth est disponible sur le système
     */
    static bool isBluetoothAvailable();
    
    // ========================================================================
    // SCAN / DÉCOUVERTE
    // ========================================================================
    
    /**
     * @brief Lance un scan des appareils Bluetooth
     * 
     * @param duration Durée du scan en secondes (0 = infini)
     * @param filterUuids Liste d'UUIDs à filtrer (vide = tous)
     * 
     * @return true si le scan a démarré
     * 
     * @note Asynchrone, utiliser setOnDeviceDiscovered() pour les résultats
     */
    bool startScan(int duration = 10, 
                   const std::vector<std::string>& filterUuids = {});
    
    /**
     * @brief Arrête le scan en cours
     */
    void stopScan();
    
    /**
     * @brief Vérifie si un scan est en cours
     */
    bool isScanning() const;
    
    /**
     * @brief Récupère la liste des appareils découverts
     */
    std::vector<BluetoothDevice> getDiscoveredDevices() const;
    
    /**
     * @brief Récupère un appareil par son adresse
     */
    std::optional<BluetoothDevice> getDevice(const std::string& address) const;
    
    // ========================================================================
    // PAIRING
    // ========================================================================
    
    /**
     * @brief Appaire avec un appareil
     * 
     * @param address Adresse MAC de l'appareil
     * @param pin Code PIN si nécessaire (vide sinon)
     * 
     * @return true si le pairing a réussi
     */
    bool pair(const std::string& address, const std::string& pin = "");
    
    /**
     * @brief Supprime un pairing
     */
    bool unpair(const std::string& address);
    
    /**
     * @brief Liste tous les appareils appairés
     */
    std::vector<BluetoothDevice> getPairedDevices() const;
    
    // ========================================================================
    // CONNEXION
    // ========================================================================
    
    /**
     * @brief Connecte à un appareil
     * 
     * @param address Adresse MAC de l'appareil
     * 
     * @return true si la connexion a réussi
     * 
     * @note L'appareil doit être appairé au préalable
     */
    bool connect(const std::string& address);
    
    /**
     * @brief Déconnecte un appareil
     */
    bool disconnect(const std::string& address);
    
    /**
     * @brief Vérifie si un appareil est connecté
     */
    bool isConnected(const std::string& address) const;
    
    /**
     * @brief Liste les appareils connectés
     */
    std::vector<BluetoothDevice> getConnectedDevices() const;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit un appareil comme "trusted"
     */
    bool setTrusted(const std::string& address, bool trusted);
    
    /**
     * @brief Bloque/débloque un appareil
     */
    bool setBlocked(const std::string& address, bool blocked);
    
    /**
     * @brief Active/désactive le Bluetooth
     */
    bool setPowered(bool enabled);
    
    /**
     * @brief Vérifie si le Bluetooth est activé
     */
    bool isPowered() const;
    
    /**
     * @brief Active/désactive la découvrabilité
     */
    bool setDiscoverable(bool enabled, int timeout = 180);
    
    /**
     * @brief Vérifie si l'adaptateur est découvrable
     */
    bool isDiscoverable() const;
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Récupère des informations sur l'adaptateur
     */
    json getAdapterInfo() const;
    
    /**
     * @brief Récupère l'état complet du gestionnaire
     */
    json getStatus() const;
    
    /**
     * @brief Récupère l'adresse MAC de l'adaptateur
     */
    std::string getAdapterAddress() const;
    
    /**
     * @brief Récupère le nom de l'adaptateur
     */
    std::string getAdapterName() const;
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    void setOnDeviceDiscovered(DeviceDiscoveredCallback callback);
    void setOnDeviceStateChanged(DeviceStateChangedCallback callback);
    void setOnScanComplete(ScanCompleteCallback callback);

private:
    // ========================================================================
    // MÉTHODES PRIVÉES - BLUEZ/D-BUS
    // ========================================================================
    
#ifdef HAS_BLUEZ
    bool connectToDBus();
    bool getDefaultAdapter();
    GDBusProxy* getDeviceProxy(const std::string& address);
    BluetoothDevice parseDeviceFromProxy(GDBusProxy* proxy);
    BluetoothDeviceType detectDeviceType(const std::vector<std::string>& uuids);
    
    static void onDeviceAdded(GDBusConnection* connection,
                             const gchar* sender,
                             const gchar* objectPath,
                             const gchar* interfaceName,
                             const gchar* signalName,
                             GVariant* parameters,
                             gpointer userData);
    
    static void onPropertiesChanged(GDBusConnection* connection,
                                   const gchar* sender,
                                   const gchar* objectPath,
                                   const gchar* interfaceName,
                                   const gchar* signalName,
                                   GVariant* parameters,
                                   gpointer userData);
#endif
    
    // Threads
    void scanLoop();
    
    // Helpers
    std::string executeCommand(const std::string& command) const;
    void updateDeviceState(const std::string& address, BluetoothDeviceState state);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    mutable std::mutex mutex_;
    
    // État
    std::atomic<bool> initialized_;
    std::atomic<bool> scanning_;
    std::atomic<bool> powered_;
    std::atomic<bool> discoverable_;
    
    // Données
    std::map<std::string, BluetoothDevice> devices_;   ///< address -> device
    std::vector<std::string> filterUuids_;
    
    // BlueZ D-Bus
#ifdef HAS_BLUEZ
    GDBusConnection* dbusConnection_;
    GDBusProxy* adapterProxy_;
    guint deviceAddedSignal_;
    guint propertiesChangedSignal_;
#else
    void* dbusConnection_;
    void* adapterProxy_;
#endif
    
    std::string adapterPath_;
    std::string adapterAddress_;
    std::string adapterName_;
    
    // Threads
    std::thread scanThread_;
    int scanDuration_;
    
    // Callbacks
    DeviceDiscoveredCallback onDeviceDiscovered_;
    DeviceStateChangedCallback onDeviceStateChanged_;
    ScanCompleteCallback onScanComplete_;
};

} // namespace midiMind

#endif // MIDIMIND_BLUETOOTHMANAGER_H

// ============================================================================
// FIN DU FICHIER BluetoothManager.h v1.0.0
// ============================================================================