// ============================================================================
// Fichier: backend/src/midi/devices/plugins/BleMidiPlugin.h
// Version: 2.0.0 - ADAPTÉ À DEVICEPLUGIN.H
// Date: 2025-10-13
// ============================================================================
// Description:
//   Plugin pour découverte et connexion de périphériques BLE MIDI.
//   Utilise BlueZ (Linux Bluetooth stack) pour scanner les devices BLE.
//   ✅ Compatible avec DevicePlugin.h (IDevicePlugin interface)
//   ✅ Auto-registration via macro REGISTER_DEVICE_PLUGIN
//
// Fonctionnalités:
//   ✅ Scan BLE MIDI (UUID Service: 03B80E5A-EDE8-4B33-A751-6CE34EC4C700)
//   ✅ Découverte automatique avec filtrage
//   ✅ Timeout configurable
//   ✅ Interface IDevicePlugin
//   ✅ Thread-safe
//   ✅ DeviceInfo avec metadata JSON
//
// Dépendances:
//   - BlueZ (libbluetooth-dev sur Debian/Ubuntu)
//   - D-Bus (libdbus-1-dev)
//   - nlohmann/json
//
// Auteur: MidiMind Team (Généré par Claude)
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include "../DevicePlugin.h"  // ✅ TON FICHIER !

// Forward declarations pour ne pas inclure les headers BlueZ
struct _GDBusConnection;
typedef struct _GDBusConnection GDBusConnection;
struct _GVariant;
typedef struct _GVariant GVariant;

namespace midiMind {

/**
 * @class BleMidiPlugin
 * @brief Plugin pour périphériques BLE MIDI
 * 
 * Ce plugin utilise BlueZ via D-Bus pour découvrir les périphériques
 * Bluetooth Low Energy qui exposent le service MIDI (BLE MIDI spec).
 * 
 * ✅ S'enregistre automatiquement via REGISTER_DEVICE_PLUGIN
 * ✅ Utilise DeviceInfo avec metadata JSON
 * ✅ Compatible avec DevicePluginRegistry
 * 
 * @note Nécessite BlueZ >= 5.44
 * @note Nécessite permissions Bluetooth (groupe 'bluetooth' ou root)
 * 
 * @version 2.0.0
 */
class BleMidiPlugin : public IDevicePlugin {
public:
    // ========================================================================
    // CONSTRUCTEUR / DESTRUCTEUR
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    BleMidiPlugin();
    
    /**
     * @brief Destructeur
     */
    virtual ~BleMidiPlugin();
    
    // ========================================================================
    // INTERFACE IDevicePlugin
    // ========================================================================
    
    /**
     * @brief Nom du plugin
     * @return "BLE MIDI"
     */
    std::string getName() const override;
    
    /**
     * @brief Version du plugin
     * @return "2.0.0"
     */
    std::string getVersion() const override;
    
    /**
     * @brief Type de périphériques gérés
     * @return DeviceType::BLUETOOTH
     */
    DeviceType getType() const override;
    
    /**
     * @brief Supporte la découverte automatique
     * @return true
     */
    bool supportsDiscovery() const override;
    
    /**
     * @brief Supporte le hot-plug
     * @return true (via D-Bus signals)
     */
    bool supportsHotplug() const override;
    
    /**
     * @brief Initialise le plugin
     * 
     * Séquence:
     *  1. Connexion à D-Bus system bus
     *  2. Vérification de BlueZ
     *  3. Récupération de l'adaptateur Bluetooth par défaut
     *  4. Configuration des callbacks D-Bus
     * 
     * @return true Si initialisation réussie
     * @return false Si échec (pas de Bluetooth, permissions, etc.)
     * 
     * @note Peut prendre jusqu'à 1 seconde
     * @note Nécessite permissions Bluetooth
     */
    bool initialize() override;
    
    /**
     * @brief Arrête le plugin proprement
     * 
     * Séquence:
     *  1. Arrêter le scan en cours si actif
     *  2. Libérer les ressources D-Bus
     *  3. Déconnecter du bus
     */
    void shutdown() override;
    
    /**
     * @brief Découvre les périphériques BLE MIDI disponibles
     * 
     * Lance un scan BLE et filtre les devices qui exposent le service
     * BLE MIDI (UUID: 03B80E5A-EDE8-4B33-A751-6CE34EC4C700).
     * 
     * Séquence:
     *  1. Démarrer scan BLE via BlueZ
     *  2. Attendre scan_timeout_ secondes
     *  3. Récupérer les devices découverts
     *  4. Filtrer ceux avec service BLE MIDI
     *  5. Arrêter le scan
     *  6. Retourner liste des DeviceInfo
     * 
     * @return std::vector<DeviceInfo> Liste des devices BLE MIDI trouvés
     * 
     * @note Bloque pendant scan_timeout_ secondes (défaut: 5s)
     * @note Retourne liste vide si aucun device trouvé
     * @note Nécessite initialize() avant
     */
    std::vector<DeviceInfo> discover() override;
    
    /**
     * @brief Crée une instance de BleMidiDevice
     * 
     * @param info Informations du périphérique
     * @return std::shared_ptr<MidiDevice> Instance créée
     * 
     * @note Pour l'instant, retourne nullptr (création gérée par MidiDeviceManager)
     */
    std::shared_ptr<MidiDevice> createDevice(const DeviceInfo& info) override;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Configure le timeout du scan
     * 
     * @param seconds Durée du scan en secondes (1-30)
     * 
     * @note Défaut: 5 secondes
     */
    void setScanTimeout(int seconds);
    
    /**
     * @brief Active/désactive les logs verbeux
     * 
     * @param verbose true pour activer
     * 
     * @note Défaut: false
     */
    void setVerbose(bool verbose);
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Vérifie si le plugin est initialisé
     * 
     * @return true Si initialize() a réussi
     */
    bool isInitialized() const;
    
    /**
     * @brief Récupère le nom de l'adaptateur Bluetooth utilisé
     * 
     * @return std::string Nom de l'adaptateur (ex: "hci0")
     * 
     * @note Retourne "" si pas initialisé
     */
    std::string getAdapterName() const;
    
    /**
     * @brief Récupère l'adresse MAC de l'adaptateur Bluetooth
     * 
     * @return std::string Adresse MAC (ex: "AA:BB:CC:DD:EE:FF")
     * 
     * @note Retourne "" si pas initialisé
     */
    std::string getAdapterAddress() const;

private:
    // ========================================================================
    // MÉTHODES PRIVÉES - BLUEZ/D-BUS
    // ========================================================================
    
    /**
     * @brief Récupère l'adaptateur Bluetooth par défaut
     * 
     * @return bool true si adapté trouvé
     */
    bool getDefaultAdapter();
    
    /**
     * @brief Démarre le scan BLE
     * 
     * @return bool true si scan démarré
     */
    bool startScan();
    
    /**
     * @brief Arrête le scan BLE
     */
    void stopScan();
    
    /**
     * @brief Récupère les devices découverts depuis BlueZ
     * 
     * @return std::vector<DeviceInfo> Liste des devices
     */
    std::vector<DeviceInfo> getDiscoveredDevices();
    
    /**
     * @brief Vérifie si un device expose le service BLE MIDI
     * 
     * @param devicePath Chemin D-Bus du device
     * @return bool true si service BLE MIDI présent
     */
    bool hasBleMidiService(const std::string& devicePath);
    
    /**
     * @brief Récupère les propriétés d'un device BlueZ
     * 
     * @param devicePath Chemin D-Bus du device
     * @return GVariant* Dictionnaire des propriétés (à libérer avec g_variant_unref)
     */
    GVariant* getDeviceProperties(const std::string& devicePath);
    
    /**
     * @brief Extrait une propriété string d'un dictionnaire GVariant
     * 
     * @param properties Dictionnaire GVariant
     * @param key Clé de la propriété
     * @return std::string Valeur ou "" si absente
     */
    std::string getStringProperty(GVariant* properties, const char* key);
    
    // ========================================================================
    // CONSTANTES
    // ========================================================================
    
    /**
     * @brief UUID du service BLE MIDI (BLE MIDI Specification)
     */
    static constexpr const char* BLE_MIDI_SERVICE_UUID = 
        "03b80e5a-ede8-4b33-a751-6ce34ec4c700";
    
    /**
     * @brief UUID de la caractéristique MIDI I/O
     */
    static constexpr const char* BLE_MIDI_CHARACTERISTIC_UUID = 
        "7772e5db-3868-4112-a1a9-f2669d106bf3";
    
    /**
     * @brief Chemin D-Bus de BlueZ
     */
    static constexpr const char* BLUEZ_SERVICE = "org.bluez";
    
    /**
     * @brief Interface BlueZ Adapter
     */
    static constexpr const char* ADAPTER_INTERFACE = "org.bluez.Adapter1";
    
    /**
     * @brief Interface BlueZ Device
     */
    static constexpr const char* DEVICE_INTERFACE = "org.bluez.Device1";
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /**
     * @brief Connexion D-Bus
     */
    GDBusConnection* dbus_connection_;
    
    /**
     * @brief Chemin D-Bus de l'adaptateur Bluetooth (ex: "/org/bluez/hci0")
     */
    std::string adapter_path_;
    
    /**
     * @brief Nom de l'adaptateur (ex: "hci0")
     */
    std::string adapter_name_;
    
    /**
     * @brief Adresse MAC de l'adaptateur
     */
    std::string adapter_address_;
    
    /**
     * @brief État d'initialisation
     */
    bool initialized_;
    
    /**
     * @brief Scan en cours
     */
    bool scanning_;
    
    /**
     * @brief Timeout du scan en secondes
     */
    int scan_timeout_;
    
    /**
     * @brief Logs verbeux activés
     */
    bool verbose_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER BleMidiPlugin.h v2.0.0
// ============================================================================