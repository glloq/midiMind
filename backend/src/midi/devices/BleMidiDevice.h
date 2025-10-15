// ============================================================================
// Fichier: backend/src/midi/devices/BleMidiDevice.h
// Version: 4.0.1
// Date: 2025-10-15
// ============================================================================
// Description:
//   Périphérique MIDI Bluetooth Low Energy (BLE MIDI).
//   Implémentation complète via BlueZ/D-Bus.
//
// Responsabilités:
//   - Connexion BLE via BlueZ (D-Bus)
//   - Découverte GATT services/characteristics
//   - Encodage/décodage BLE MIDI packets
//   - Réception asynchrone via thread
//   - Envoi via GATT WriteValue
//
// Architecture:
//   MidiMessage → encodeMidiToBle() → GATT WriteValue → BLE Device
//   BLE Device → GATT Notify → bleLoop() → handleMessage() → Callback
//
// Dépendances:
//   - BlueZ 5.x (org.bluez via D-Bus)
//   - GLib/GIO pour D-Bus
//   - libbluetooth pour HCI
//
// Compilation:
//   Nécessite -DHAS_BLUEZ -lglib-2.0 -lgio-2.0 -lbluetooth
//
// Thread-safety: OUI
//
// Auteur: MidiMind Team
// ============================================================================

#pragma once

#include "MidiDevice.h"
#include "../../core/Logger.h"
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>

// Forward declarations GLib/D-Bus (évite includes massifs)
#ifdef HAS_BLUEZ
typedef struct _GDBusConnection GDBusConnection;
typedef struct _GDBusProxy GDBusProxy;
typedef struct _GMainLoop GMainLoop;
#endif

namespace midiMind {

/**
 * @class BleMidiDevice
 * @brief Périphérique MIDI Bluetooth Low Energy
 * 
 * @details
 * Implémentation complète du protocole BLE MIDI via BlueZ.
 * 
 * Protocole BLE MIDI:
 * - Service UUID: 03b80e5a-ede8-4b33-a751-6ce34ec4c700
 * - Characteristic UUID: 7772e5db-3868-4112-a1a9-f2669d106bf3
 * - Format: [header][timestamp_high][timestamp_low][midi_bytes...]
 * 
 * Architecture:
 * ```
 * Application
 *     ↓
 * BleMidiDevice
 *     ↓
 * BlueZ (D-Bus)
 *     ↓
 * GATT Services
 *     ↓
 * BLE Stack
 *     ↓
 * Bluetooth Device
 * ```
 * 
 * Cycle de vie:
 * 1. Construction avec adresse MAC
 * 2. connect() → Initialise BlueZ, découvre GATT, démarre thread
 * 3. sendMessage() → Encode et envoie via GATT
 * 4. bleLoop() → Reçoit notifications GATT
 * 5. disconnect() → Arrête thread, déconnecte BlueZ
 * 
 * Thread-safety:
 * - connect/disconnect protégés par status_
 * - sendMessage thread-safe via D-Bus
 * - bleLoop thread séparé pour réception
 * 
 * Limitations:
 * - Nécessite BlueZ 5.x installé et démarré
 * - Nécessite droits CAP_NET_ADMIN ou root pour scan
 * - Un seul adaptateur Bluetooth (hci0) supporté
 * 
 * @example Utilisation
 * ```cpp
 * // Créer device
 * auto ble = std::make_shared<BleMidiDevice>(
 *     "ble_aa_bb_cc_dd_ee_ff",
 *     "iPad MIDI",
 *     "AA:BB:CC:DD:EE:FF"
 * );
 * 
 * // Vérifier Bluetooth disponible
 * if (!BleMidiDevice::isBluetoothAvailable()) {
 *     Logger::error("No Bluetooth adapter");
 *     return;
 * }
 * 
 * // Connecter
 * if (!ble->connect()) {
 *     Logger::error("Connection failed");
 *     return;
 * }
 * 
 * // Callback réception
 * ble->setOnMessageReceived([](const MidiMessage& msg) {
 *     Logger::info("BLE Received: " + msg.toString());
 * });
 * 
 * // Envoyer
 * ble->sendMessage(MidiMessage::noteOn(0, 60, 100));
 * 
 * // Déconnecter
 * ble->disconnect();
 * ```
 * 
 * @note Nécessite compilation avec -DHAS_BLUEZ
 * @note Si HAS_BLUEZ non défini, connect() retournera toujours false
 */
class BleMidiDevice : public MidiDevice {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param id ID unique du device (ex: "ble_aa_bb_cc_dd_ee_ff")
     * @param name Nom lisible (ex: "iPad MIDI", "iPhone de Pierre")
     * @param address Adresse MAC Bluetooth (ex: "AA:BB:CC:DD:EE:FF")
     * 
     * @note L'adresse doit être au format MAC avec ':' ou '-'
     * @note Le device doit être déjà appairé via bluetoothctl
     */
    BleMidiDevice(const std::string& id, 
                  const std::string& name,
                  const std::string& address);
    
    /**
     * @brief Destructeur
     * 
     * @note Appelle disconnect() automatiquement
     * @note Libère toutes les ressources D-Bus/BlueZ
     */
    ~BleMidiDevice() override;
    
    // Désactiver copie et move
    BleMidiDevice(const BleMidiDevice&) = delete;
    BleMidiDevice& operator=(const BleMidiDevice&) = delete;
    
    // ========================================================================
    // IMPLÉMENTATION MidiDevice (MÉTHODES VIRTUELLES)
    // ========================================================================
    
    /**
     * @brief Connecte le périphérique BLE
     * 
     * Séquence:
     * 1. Initialise BlueZ (connexion D-Bus)
     * 2. Vérifie adaptateur Bluetooth disponible
     * 3. Obtient proxy D-Bus du device
     * 4. Connecte via org.bluez.Device1.Connect
     * 5. Découvre services GATT MIDI
     * 6. Active notifications GATT
     * 7. Démarre thread de réception (bleLoop)
     * 
     * @return true Si connexion réussie
     * 
     * @note Bloquant (peut prendre jusqu'à 30s)
     * @note Échoue si device non appairé
     * @note Échoue si service MIDI non disponible
     * @note Sans HAS_BLUEZ, retourne toujours false
     */
    bool connect() override;
    
    /**
     * @brief Déconnecte le périphérique BLE
     * 
     * Séquence:
     * 1. Arrête le thread de réception
     * 2. Désactive notifications GATT
     * 3. Déconnecte via org.bluez.Device1.Disconnect
     * 4. Libère proxies D-Bus
     * 
     * @return true Si déconnexion réussie
     * 
     * @note Thread-safe
     * @note Ne lève pas d'exception en cas d'erreur
     */
    bool disconnect() override;
    
    /**
     * @brief Envoie un message MIDI via BLE
     * 
     * Processus:
     * 1. Encode message au format BLE MIDI (avec timestamp)
     * 2. Écrit via GATT characteristic (WriteValue)
     * 3. Incrémente compteur si succès
     * 
     * @param msg Message MIDI à envoyer
     * @return true Si envoi réussi
     * 
     * @note Thread-safe
     * @note Bloquant (timeout 1s)
     * @note Échoue si non connecté
     * @note Échoue si characteristic non disponible
     * 
     * @example
     * ```cpp
     * ble->sendMessage(MidiMessage::noteOn(0, 60, 100));
     * ble->sendMessage(MidiMessage::controlChange(0, 7, 64));
     * ```
     */
    bool sendMessage(const MidiMessage& msg) override;
    
    // ========================================================================
    // MÉTHODES PUBLIQUES SPÉCIFIQUES BLE
    // ========================================================================
    
    /**
     * @brief Récupère l'adresse MAC Bluetooth
     * 
     * @return std::string Adresse MAC (ex: "AA:BB:CC:DD:EE:FF")
     */
    std::string getAddress() const {
        return address_;
    }
    
    /**
     * @brief Récupère le chemin D-Bus du device
     * 
     * @return std::string Chemin (ex: "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF")
     * 
     * @note Utile pour debugging
     */
    std::string getDevicePath() const {
        return getDevicePath(address_);
    }
    
    // ========================================================================
    // MÉTHODES STATIQUES
    // ========================================================================
    
    /**
     * @brief Vérifie si un adaptateur Bluetooth est disponible
     * 
     * @return true Si adaptateur trouvé et fonctionnel
     * 
     * @note Vérifie via HCI (hci_get_route)
     * @note Nécessite HAS_BLUEZ défini
     * @note Ne nécessite pas de device existant
     * 
     * @example
     * ```cpp
     * if (!BleMidiDevice::isBluetoothAvailable()) {
     *     Logger::error("Install BlueZ or enable Bluetooth");
     *     return;
     * }
     * ```
     */
    static bool isBluetoothAvailable();

private:
    // ========================================================================
    // MÉTHODES PRIVÉES - BLUEZ/D-BUS
    // ========================================================================
    
    /**
     * @brief Initialise la connexion à BlueZ via D-Bus
     * 
     * @return true Si succès
     * 
     * @note Crée dbusConnection_
     * @note Vérifie que org.bluez est disponible
     */
    bool initializeBluez();
    
    /**
     * @brief Découvre les services et caractéristiques GATT MIDI
     * 
     * Processus:
     * 1. Appelle ObjectManager.GetManagedObjects
     * 2. Parcourt objets pour trouver UUID MIDI
     * 3. Crée proxy pour ioCharacteristic_
     * 4. Active notifications (StartNotify)
     * 
     * @return true Si characteristic MIDI trouvée
     * 
     * @note Crée ioCharacteristic_
     */
    bool discoverGattCharacteristics();
    
    /**
     * @brief Convertit adresse MAC en chemin D-Bus
     * 
     * @param address Adresse MAC (ex: "AA:BB:CC:DD:EE:FF")
     * @return std::string Chemin D-Bus (ex: "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF")
     * 
     * @note Remplace ':' par '_'
     * @note Assume hci0 (premier adaptateur)
     */
    std::string getDevicePath(const std::string& address) const;
    
    // ========================================================================
    // MÉTHODES PRIVÉES - PROTOCOLE BLE MIDI
    // ========================================================================
    
    /**
     * @brief Encode un message MIDI au format BLE MIDI
     * 
     * Format BLE MIDI:
     * ```
     * [header][timestamp_high][timestamp_low][midi_bytes...]
     * 
     * header = 0x80 | ((timestamp >> 7) & 0x3F)
     * timestamp_low = 0x80 | (timestamp & 0x7F)
     * timestamp = 13 bits (millisecondes mod 8192)
     * ```
     * 
     * @param msg Message MIDI à encoder
     * @return std::vector<uint8_t> Packet BLE MIDI
     * 
     * @note Ajoute timestamp automatiquement
     * @note Conforme à spec Apple BLE MIDI
     */
    std::vector<uint8_t> encodeMidiToBle(const MidiMessage& msg) const;
    
    /**
     * @brief Décode un packet BLE MIDI en message(s) MIDI
     * 
     * @param blePacket Packet BLE reçu
     * @return std::vector<MidiMessage> Messages MIDI décodés
     * 
     * @note Un packet peut contenir plusieurs messages
     * @note Gère running status
     * @note Ignore timestamp (on utilise réception time)
     */
    std::vector<MidiMessage> decodeBleToMidi(const std::vector<uint8_t>& blePacket) const;
    
    // ========================================================================
    // MÉTHODES PRIVÉES - THREAD RÉCEPTION
    // ========================================================================
    
    /**
     * @brief Boucle principale du thread BLE
     * 
     * Fonction du thread bleThread_:
     * 1. Crée GMainLoop pour D-Bus
     * 2. Boucle: traite événements D-Bus (notifications GATT)
     * 3. Appelle handleMessage() pour chaque message reçu
     * 4. S'arrête quand running_ = false
     * 
     * @note Tourne dans bleThread_
     * @note Bloquant jusqu'à disconnect()
     * @note Gère les notifications GATT automatiquement
     */
    void bleLoop();
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Adresse MAC Bluetooth du device
    std::string address_;
    
    // --- BlueZ/D-Bus handles ---
    
#ifdef HAS_BLUEZ
    /// Connexion au bus D-Bus système
    GDBusConnection* dbusConnection_;
    
    /// Proxy D-Bus du device Bluetooth (org.bluez.Device1)
    GDBusProxy* deviceProxy_;
    
    /// Proxy D-Bus de la GATT characteristic MIDI (org.bluez.GattCharacteristic1)
    GDBusProxy* ioCharacteristic_;
#else
    /// Stubs si BlueZ non disponible
    void* dbusConnection_;
    void* deviceProxy_;
    void* ioCharacteristic_;
#endif
    
    // --- Thread réception ---
    
    /// Flag d'arrêt du thread (atomic pour thread-safety)
    std::atomic<bool> running_;
    
    /// Thread de réception BLE (bleLoop)
    std::thread bleThread_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER BleMidiDevice.h v4.0.1
// ============================================================================