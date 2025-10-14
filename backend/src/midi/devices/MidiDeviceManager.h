// ============================================================================
// Fichier: backend/src/midi/devices/MidiDeviceManager.h
// Version: 3.2.0 - CORRECTIONS CRITIQUES APPLIQUÉES
// Date: 2025-10-13
// ============================================================================
// CORRECTIONS v3.2.0:
//   ✅ FIX #2: Déclaration reconnectDevice() ajoutée
//   ✅ Documentation mise à jour
//
// Description:
//   Header du gestionnaire centralisé de périphériques MIDI.
//   Thread-safe avec Observer Pattern et Factory Pattern.
//
// Auteur: MidiMind Team (Corrections par Claude)
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <map>
#include <functional>
#include "MidiDevice.h"
#include "DeviceInfo.h"
#include "../MidiMessage.h"

namespace midiMind {

/**
 * @class MidiDeviceManager
 * @brief Gestionnaire centralisé des périphériques MIDI
 * 
 * Responsabilités:
 * - Découverte automatique (USB, Network, Bluetooth)
 * - Connexion/Déconnexion thread-safe
 * - Routage des messages MIDI
 * - Notifications via callbacks (Observer Pattern)
 * - Factory Pattern pour création devices
 * 
 * @note Thread-safe : toutes les méthodes sont protégées par mutex
 * @note Singleton Pattern recommandé pour usage
 * 
 * @version 3.2.0 - Corrections critiques appliquées
 */
class MidiDeviceManager {
public:
    // ========================================================================
    // TYPES - CALLBACKS
    // ========================================================================
    
    /**
     * @typedef DeviceConnectedCallback
     * @brief Callback appelé lors de la connexion d'un device
     */
    using DeviceConnectedCallback = std::function<void(const std::string& deviceId)>;
    
    /**
     * @typedef DeviceDisconnectedCallback
     * @brief Callback appelé lors de la déconnexion d'un device
     */
    using DeviceDisconnectedCallback = std::function<void(const std::string& deviceId)>;
    
    /**
     * @typedef MidiReceivedCallback
     * @brief Callback appelé lors de la réception d'un message MIDI
     * 
     * @param deviceId ID du device source
     * @param message Message MIDI reçu
     * 
     * @note ✅ NOUVEAU v3.2.0: Ce callback est maintenant fonctionnel
     */
    using MidiReceivedCallback = std::function<void(
        const std::string& deviceId, 
        const MidiMessage& message
    )>;
    
    // ========================================================================
    // CONSTRUCTEUR / DESTRUCTEUR
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    MidiDeviceManager();
    
    /**
     * @brief Destructeur
     * 
     * Déconnecte automatiquement tous les devices
     */
    ~MidiDeviceManager();
    
    // Désactiver copie et assignation
    MidiDeviceManager(const MidiDeviceManager&) = delete;
    MidiDeviceManager& operator=(const MidiDeviceManager&) = delete;
    
    // ========================================================================
    // DÉCOUVERTE DE PÉRIPHÉRIQUES
    // ========================================================================
    
    /**
     * @brief Découvre tous les périphériques MIDI disponibles
     * 
     * Scanner tous les types: USB (ALSA), Network (mDNS), Bluetooth (BLE)
     * 
     * @param fullScan Si true, efface cache et rescanne complètement
     *                 Si false, scan incrémental (ajoute nouveaux)
     * @return std::vector<DeviceInfo> Liste des devices découverts
     * 
     * @note ✅ AMÉLIORÉ v3.2.0: Bluetooth scan BLE réel avec BleMidiPlugin
     * @note Thread-safe (lock sur devicesMutex_)
     * @note Durée typique: USB ~50ms, Network ~500ms, BT ~2s
     */
    std::vector<DeviceInfo> discoverDevices(bool fullScan = true);
    
    /**
     * @brief Récupère la liste des devices disponibles (cache)
     * 
     * @return std::vector<DeviceInfo> Liste des devices découverts
     * 
     * @note Ne rescanne pas, retourne le cache
     * @note Thread-safe
     */
    std::vector<DeviceInfo> getAvailableDevices() const;
    
    // ========================================================================
    // CONNEXION / DÉCONNEXION
    // ========================================================================
    
    /**
     * @brief Connecte à un périphérique
     * 
     * Séquence:
     *  1. Vérifier si déjà connecté
     *  2. Trouver DeviceInfo dans availableDevices_
     *  3. Créer instance via Factory (createDevice)
     *  4. Ouvrir le device (device->open())
     *  5. ✅ NOUVEAU v3.2.0: Configurer callback réception MIDI
     *  6. Ajouter à connectedDevices_
     *  7. Appeler callback onDeviceConnected_
     * 
     * @param deviceId ID du device (DeviceInfo.id)
     * @return true Si connexion réussie
     * @return false Si échec (device non trouvé ou erreur ouverture)
     * 
     * @note ✅ FIX #1 v3.2.0: Configure maintenant le callback de réception MIDI
     * @note Thread-safe (exclusive lock)
     */
    bool connect(const std::string& deviceId);
    
    /**
     * @brief Déconnecte un périphérique
     * 
     * Séquence:
     *  1. Trouver device dans connectedDevices_
     *  2. Fermer le device (device->close())
     *  3. Retirer de connectedDevices_
     *  4. Mettre à jour DeviceInfo.connected = false
     *  5. Appeler callback onDeviceDisconnected_
     * 
     * @param deviceId ID du device
     * 
     * @note Thread-safe
     */
    void disconnect(const std::string& deviceId);
    
    /**
     * @brief Déconnecte TOUS les périphériques
     * 
     * Parcourt tous les devices connectés et appelle close() sur chacun.
     * Appelé automatiquement par le destructeur.
     * 
     * @note Thread-safe
     */
    void disconnectAll();
    
    /**
     * @brief Vérifie si un device est connecté
     * 
     * @param deviceId ID du device
     * @return true Si connecté
     * 
     * @note Thread-safe
     */
    bool isConnected(const std::string& deviceId) const;
    
    /**
     * @brief Reconnecte un périphérique
     * 
     * Déconnecte puis reconnecte le device après un délai de 100ms.
     * Utile après une erreur de communication.
     * 
     * @param deviceId ID du device
     * @return true Si reconnexion réussie
     * @return false Si échec
     * 
     * @note ✅ NOUVEAU v3.2.0: Méthode ajoutée (FIX #2)
     * @note Bloque pendant ~100ms
     */
    bool reconnectDevice(const std::string& deviceId);
    
    // ========================================================================
    // ACCÈS AUX DEVICES
    // ========================================================================
    
    /**
     * @brief Récupère un device par son ID
     * 
     * @param deviceId ID du device
     * @return std::shared_ptr<MidiDevice> Pointeur vers device ou nullptr
     * 
     * @note Thread-safe
     * @note Retourne nullptr si device non connecté
     */
    std::shared_ptr<MidiDevice> getDevice(const std::string& deviceId) const;
    
    /**
     * @brief Récupère tous les devices connectés
     * 
     * @return std::vector<std::shared_ptr<MidiDevice>> Liste des devices
     * 
     * @note Thread-safe
     */
    std::vector<std::shared_ptr<MidiDevice>> getConnectedDevices() const;
    
    /**
     * @brief Récupère les devices d'un type spécifique
     * 
     * @param type Type de device (USB, NETWORK, BLUETOOTH, VIRTUAL)
     * @return std::vector<std::shared_ptr<MidiDevice>> Liste filtrée
     * 
     * @note Thread-safe
     */
    std::vector<std::shared_ptr<MidiDevice>> getDevicesByType(DeviceType type) const;
    
    /**
     * @brief Récupère les devices Network
     * 
     * @return std::vector<std::shared_ptr<MidiDevice>> Liste des devices Network
     * 
     * @note Raccourci pour getDevicesByType(DeviceType::NETWORK)
     */
    std::vector<std::shared_ptr<MidiDevice>> getNetworkDevices() const;
    
    /**
     * @brief Récupère les devices Bluetooth
     * 
     * @return std::vector<std::shared_ptr<MidiDevice>> Liste des devices Bluetooth
     * 
     * @note Raccourci pour getDevicesByType(DeviceType::BLUETOOTH)
     */
    std::vector<std::shared_ptr<MidiDevice>> getBluetoothDevices() const;
    
    // ========================================================================
    // ENVOI DE MESSAGES
    // ========================================================================
    
    /**
     * @brief Envoie un message MIDI à un device spécifique
     * 
     * @param deviceId ID du device destination
     * @param message Message MIDI à envoyer
     * @return true Si envoi réussi
     * @return false Si échec (device non connecté ou erreur)
     * 
     * @note Thread-safe
     */
    bool sendMessage(const std::string& deviceId, const MidiMessage& message);
    
    /**
     * @brief Broadcast un message MIDI à TOUS les devices connectés
     * 
     * @param message Message MIDI à broadcaster
     * 
     * @note Thread-safe
     * @note Continue même si un device échoue
     */
    void broadcastMessage(const MidiMessage& message);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Configure le callback de connexion
     * 
     * @param callback Fonction appelée lors de la connexion d'un device
     * 
     * @note Thread-safe
     */
    void setOnDeviceConnected(DeviceConnectedCallback callback);
    
    /**
     * @brief Configure le callback de déconnexion
     * 
     * @param callback Fonction appelée lors de la déconnexion d'un device
     * 
     * @note Thread-safe
     */
    void setOnDeviceDisconnected(DeviceDisconnectedCallback callback);
    
    /**
     * @brief Configure le callback de réception MIDI
     * 
     * @param callback Fonction appelée lors de la réception d'un message MIDI
     * 
     * @note ✅ FIX #1 v3.2.0: Ce callback est maintenant fonctionnel !
     * @note Thread-safe
     * @note Appelé depuis le thread du device (contexte temps réel)
     */
    void setOnMidiReceived(MidiReceivedCallback callback);
    
private:
    // ========================================================================
    // MÉTHODES PRIVÉES - SCANNERS
    // ========================================================================
    
    /**
     * @brief Scanne les devices USB via ALSA
     * 
     * @note Linux only (snd_seq API)
     */
    void scanUSBDevices();
    
    /**
     * @brief Scanne les devices virtuels
     * 
     * @note Charge depuis config + crée au moins un par défaut
     */
    void scanVirtualDevices();
    
    /**
     * @brief Scanne les devices Network via mDNS
     * 
     * @note TODO: Implémenter scan mDNS réel pour RTP-MIDI
     */
    void scanNetworkDevices();
    
    /**
     * @brief Scanne les devices Bluetooth via BLE
     * 
     * @note ✅ AMÉLIORÉ v3.2.0: Utilise BleMidiPlugin pour scan BLE réel
     */
    void scanBluetoothDevices();
    
    // ========================================================================
    // MÉTHODES PRIVÉES - FACTORY
    // ========================================================================
    
    /**
     * @brief Crée une instance de device selon son type (Factory Pattern)
     * 
     * @param info Informations du device
     * @return std::shared_ptr<MidiDevice> Instance créée ou nullptr si échec
     * 
     * @note Créé le bon type: UsbMidiDevice, NetworkMidiDevice, etc.
     */
    std::shared_ptr<MidiDevice> createDevice(const DeviceInfo& info);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /**
     * @brief Mutex pour thread-safety
     */
    mutable std::mutex devicesMutex_;
    
    /**
     * @brief Liste des devices disponibles (découverts)
     */
    std::vector<DeviceInfo> availableDevices_;
    
    /**
     * @brief Map des devices connectés
     * 
     * Clé: deviceId
     * Valeur: shared_ptr vers MidiDevice
     */
    std::map<std::string, std::shared_ptr<MidiDevice>> connectedDevices_;
    
    /**
     * @brief Callback appelé lors de la connexion d'un device
     */
    DeviceConnectedCallback onDeviceConnected_;
    
    /**
     * @brief Callback appelé lors de la déconnexion d'un device
     */
    DeviceDisconnectedCallback onDeviceDisconnected_;
    
    /**
     * @brief Callback appelé lors de la réception d'un message MIDI
     * 
     * @note ✅ FIX #1 v3.2.0: Maintenant fonctionnel !
     */
    MidiReceivedCallback onMidiReceived_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiDeviceManager.h v3.2.0 - CORRIGÉ
// ============================================================================