// ============================================================================
// Fichier: src/network/bluetooth/BleMidiDevice.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Support Bluetooth Low Energy MIDI (BLE MIDI).
//   Permet au Raspberry Pi d'être visible comme périphérique MIDI Bluetooth.
//
// Responsabilités:
//   - Annoncer le service BLE MIDI
//   - Gérer les connexions BLE entrantes
//   - Encoder/décoder les messages MIDI selon la spec BLE MIDI
//   - Gérer les notifications GATT
//
// Thread-safety: OUI
//
// Dépendances: BlueZ 5.x, D-Bus
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <thread>

#include "../../core/Logger.h"
#include "../../midi/MidiMessage.h"

namespace midiMind {

/**
 * @class BleMidiDevice
 * @brief Périphérique BLE MIDI
 * 
 * @details
 * Implémente le protocole BLE MIDI (Bluetooth Low Energy MIDI).
 * Le Raspberry Pi devient un périphérique BLE visible par:
 * - iOS (GarageBand, etc.)
 * - Android (via BLE MIDI apps)
 * - macOS/Windows (avec driver BLE MIDI)
 * 
 * UUID du service: 03B80E5A-EDE8-4B33-A751-6CE34EC4C700
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @note Nécessite BlueZ 5.48+ et les permissions Bluetooth
 * 
 * @example Utilisation
 * ```cpp
 * BleMidiDevice ble;
 * 
 * ble.setOnMidiReceived([](const MidiMessage& msg) {
 *     Logger::info("BLE", "Received MIDI message");
 * });
 * 
 * ble.start("MidiMind BLE");
 * ```
 */
class BleMidiDevice {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé lors de la réception d'un message MIDI
     */
    using MidiReceivedCallback = std::function<void(const MidiMessage&)>;
    
    /**
     * @brief Callback appelé lors de la connexion d'un client
     */
    using ClientConnectedCallback = std::function<void(const std::string& address)>;
    
    /**
     * @brief Callback appelé lors de la déconnexion d'un client
     */
    using ClientDisconnectedCallback = std::function<void(const std::string& address)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    BleMidiDevice();
    
    /**
     * @brief Destructeur
     */
    ~BleMidiDevice();
    
    // Désactiver copie
    BleMidiDevice(const BleMidiDevice&) = delete;
    BleMidiDevice& operator=(const BleMidiDevice&) = delete;
    
    // ========================================================================
    // CONTRÔLE
    // ========================================================================
    
    /**
     * @brief Démarre le service BLE MIDI
     * 
     * @param deviceName Nom du périphérique BLE (visible lors du scan)
     * @return true Si le démarrage a réussi
     * 
     * @note Thread-safe
     * 
     * @example
     * ```cpp
     * if (ble.start("MidiMind Studio")) {
     *     Logger::info("BLE", "BLE MIDI started");
     * }
     * ```
     */
    bool start(const std::string& deviceName = "MidiMind");
    
    /**
     * @brief Arrête le service BLE MIDI
     * 
     * @note Thread-safe
     */
    void stop();
    
    /**
     * @brief Vérifie si le service est actif
     * 
     * @return true Si actif
     * 
     * @note Thread-safe
     */
    bool isRunning() const;
    
    /**
     * @brief Vérifie si un client est connecté
     * 
     * @return true Si au moins un client est connecté
     * 
     * @note Thread-safe
     */
    bool isConnected() const;
    
    // ========================================================================
    // ENVOI DE MESSAGES
    // ========================================================================
    
    /**
     * @brief Envoie un message MIDI à tous les clients connectés
     * 
     * @param message Message MIDI à envoyer
     * @return true Si l'envoi a réussi
     * 
     * @note Thread-safe
     */
    bool sendMidi(const MidiMessage& message);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de réception MIDI
     */
    void setOnMidiReceived(MidiReceivedCallback callback);
    
    /**
     * @brief Définit le callback de connexion client
     */
    void setOnClientConnected(ClientConnectedCallback callback);
    
    /**
     * @brief Définit le callback de déconnexion client
     */
    void setOnClientDisconnected(ClientDisconnectedCallback callback);
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Récupère la liste des clients connectés
     * 
     * @return std::vector<std::string> Adresses MAC des clients
     * 
     * @note Thread-safe
     */
    std::vector<std::string> getConnectedClients() const;
    
    /**
     * @brief Récupère les statistiques BLE
     * 
     * @return json Statistiques
     * 
     * Format:
     * ```json
     * {
     *   "device_name": "MidiMind",
     *   "running": true,
     *   "connected_clients": 1,
     *   "messages_received": 1234,
     *   "messages_sent": 5678,
     *   "bytes_received": 12340,
     *   "bytes_sent": 56780
     * }
     * ```
     */
    json getStatistics() const;
    
    /**
     * @brief Vérifie si Bluetooth est disponible
     * 
     * @return true Si l'adaptateur Bluetooth est présent et actif
     */
    static bool isBluetoothAvailable();

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Thread principal BLE
     */
    void bleLoop();
    
    /**
     * @brief Initialise BlueZ via D-Bus
     */
    bool initBluez();
    
    /**
     * @brief Libère les ressources BlueZ
     */
    void cleanupBluez();
    
    /**
     * @brief Enregistre le service GATT BLE MIDI
     */
    bool registerGattService();
    
    /**
     * @brief Démarre l'advertisement BLE
     */
    bool startAdvertising();
    
    /**
     * @brief Arrête l'advertisement BLE
     */
    void stopAdvertising();
    
    /**
     * @brief Gère une notification GATT (message MIDI reçu)
     */
    void handleGattNotification(const std::vector<uint8_t>& data);
    
    /**
     * @brief Encode un message MIDI au format BLE MIDI
     */
    std::vector<uint8_t> encodeBleMessage(const MidiMessage& message) const;
    
    /**
     * @brief Décode un message BLE MIDI
     */
    std::vector<MidiMessage> decodeBleMessage(const std::vector<uint8_t>& data) const;
    
    /**
     * @brief Récupère le timestamp BLE (13 bits)
     */
    uint16_t getBleTimestamp() const;
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// État
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    
    /// Thread BLE
    std::thread bleThread_;
    
    /// Configuration
    std::string deviceName_;
    
    /// Clients connectés
    std::vector<std::string> connectedClients_;
    
    /// Callbacks
    MidiReceivedCallback onMidiReceived_;
    ClientConnectedCallback onClientConnected_;
    ClientDisconnectedCallback onClientDisconnected_;
    
    /// Statistiques
    std::atomic<uint64_t> messagesReceived_;
    std::atomic<uint64_t> messagesSent_;
    std::atomic<uint64_t> bytesReceived_;
    std::atomic<uint64_t> bytesSent_;
    
    /// Handles BlueZ/D-Bus (opaques)
    void* dbusConnection_;
    void* gattService_;
    void* advertisement_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER BleMidiDevice.h
// ============================================================================