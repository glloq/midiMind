// ============================================================================
// Fichier: backend/src/api/CommandProcessorV2.h
// Version: 3.1.1 - CORRECTIONS CRITIQUES PHASE 1
// Date: 2025-10-15
// ============================================================================
// CORRECTIFS v3.1.1 (PHASE 1 - CRITIQUES):
//   ✅ 1.4 Ajout membre processorManager_
//   ✅ Ajout paramètre constructeur pour ProcessorManager
//   ✅ Documentation mise à jour
//
// Description:
//   Header du processeur de commandes API v2
//   Gère 50+ commandes dans 11 catégories
//
// Responsabilités:
//   - Initialiser CommandFactory avec toutes les commandes
//   - Router les requêtes vers les bonnes commandes
//   - Valider les entrées JSON
//   - Gérer les erreurs et exceptions
//   - Retourner des réponses au format protocole v3.0
//
// Architecture:
//   CommandProcessorV2
//   ├── CommandFactory        : Enregistrement et exécution
//   ├── MidiDeviceManager     : Gestion devices
//   ├── MidiRouter            : Routage MIDI
//   ├── MidiPlayer            : Lecture fichiers
//   ├── MidiFileManager       : Bibliothèque fichiers
//   ├── SysExHandler          : Messages SysEx
//   └── ProcessorManager      : Processeurs MIDI (NOUVEAU)
//
// Auteur: MidiMind Team
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <memory>
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>

#include "../core/commands/CommandFactory.h"
#include "../core/Logger.h"
#include "../midi/devices/MidiDeviceManager.h"
#include "../midi/routing/MidiRouter.h"
#include "../midi/playback/MidiPlayer.h"
#include "../midi/files/MidiFileManager.h"
#include "../midi/sysex/SysExHandler.h"
#include "../midi/processing/ProcessorManager.h"  // ✅ NOUVEAU

using json = nlohmann::json;

namespace midiMind {

/**
 * @class CommandProcessorV2
 * @brief Processeur de commandes API v2
 * 
 * Utilise CommandFactory pour enregistrer toutes les commandes
 * sous forme de lambdas et les exécuter de manière thread-safe.
 * 
 * Catégories de commandes (v3.1.1):
 * - devices.*     : Gestion des périphériques MIDI (5 commandes)
 * - routing.*     : Configuration du routage (6 commandes)
 * - playback.*    : Contrôle du lecteur (9 commandes)
 * - files.*       : Gestion des fichiers (7 commandes)
 * - editor.*      : Édition MIDI JsonMidi (7 commandes)
 * - processing.*  : Processeurs MIDI (5+ commandes) ✅ Gestion nullptr
 * - network.*     : Configuration réseau (6 commandes)
 * - system.*      : Informations système (5 commandes)
 * - logger.*      : Configuration logs (3 commandes)
 * - loops.*       : Enregistrement loops (6 commandes)
 * - instruments.* : Profils instruments (8 commandes)
 * 
 * @note Thread-safe: Utilise CommandFactory thread-safe
 * 
 * @example Utilisation
 * ```cpp
 * auto processor = std::make_shared<CommandProcessorV2>(
 *     deviceManager, router, player, fileManager, 
 *     sysExHandler, processorManager
 * );
 * 
 * std::string request = R"({"command": "devices.list"})";
 * auto response = processor->processCommand(request);
 * ```
 */
class CommandProcessorV2 {
public:
    // ========================================================================
    // CONSTRUCTEUR / DESTRUCTEUR
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param deviceManager Gestionnaire devices MIDI
     * @param router Routeur MIDI
     * @param player Lecteur MIDI
     * @param fileManager Gestionnaire fichiers
     * @param sysExHandler Gestionnaire SysEx (optionnel)
     * @param processorManager Gestionnaire processeurs (optionnel) ✅ NOUVEAU
     * 
     * @note Les paramètres peuvent être nullptr
     * @note Enregistre automatiquement toutes les commandes disponibles
     */
    CommandProcessorV2(
        std::shared_ptr<MidiDeviceManager> deviceManager,
        std::shared_ptr<MidiRouter> router,
        std::shared_ptr<MidiPlayer> player,
        std::shared_ptr<MidiFileManager> fileManager,
        std::shared_ptr<SysExHandler> sysExHandler = nullptr,
        std::shared_ptr<ProcessorManager> processorManager = nullptr  // ✅ NOUVEAU
    );
    
    /**
     * @brief Destructeur
     */
    ~CommandProcessorV2();
    
    // Désactiver copie
    CommandProcessorV2(const CommandProcessorV2&) = delete;
    CommandProcessorV2& operator=(const CommandProcessorV2&) = delete;
    
    // ========================================================================
    // TRAITEMENT DES COMMANDES
    // ========================================================================
    
    /**
     * @brief Traite une commande JSON
     * 
     * @param jsonString Requête JSON (format: {"command": "...", "params": {...}})
     * @return json Réponse JSON (format: {"success": bool, "data": {...}})
     * 
     * @note Thread-safe
     * 
     * @example Format requête
     * ```json
     * {
     *   "command": "devices.list",
     *   "params": {}
     * }
     * ```
     * 
     * @example Format réponse succès
     * ```json
     * {
     *   "success": true,
     *   "data": {
     *     "devices": [...]
     *   }
     * }
     * ```
     * 
     * @example Format réponse erreur
     * ```json
     * {
     *   "success": false,
     *   "error": "Error message",
     *   "error_code": "ERROR_CODE"
     * }
     * ```
     */
    json processCommand(const std::string& jsonString);
    
    // ========================================================================
    // INTROSPECTION
    // ========================================================================
    
    /**
     * @brief Compte le nombre de commandes enregistrées
     * @return size_t Nombre de commandes
     * @note Thread-safe
     */
    size_t getCommandCount() const;
    
    /**
     * @brief Liste toutes les commandes disponibles
     * @return vector<string> Liste des noms de commandes
     * @note Thread-safe
     * 
     * @example
     * ```cpp
     * auto commands = processor->listCommands();
     * // ["devices.list", "devices.scan", "routing.add", ...]
     * ```
     */
    std::vector<std::string> listCommands() const;
    
    /**
     * @brief Liste les commandes par catégorie
     * @return map<category, vector<commands>> Commandes par catégorie
     * @note Thread-safe
     * 
     * @example
     * ```cpp
     * auto categories = processor->listCommandsByCategory();
     * // {
     * //   "devices": ["devices.list", "devices.scan", ...],
     * //   "routing": ["routing.add", "routing.remove", ...],
     * //   ...
     * // }
     * ```
     */
    std::unordered_map<std::string, std::vector<std::string>> 
    listCommandsByCategory() const;
    
private:
    // ========================================================================
    // MÉTHODES PRIVÉES - ENREGISTREMENT
    // ========================================================================
    
    /**
     * @brief Enregistre toutes les catégories de commandes
     * @note Appelé automatiquement par le constructeur
     */
    void registerAllCommands();
    
    /**
     * @brief Enregistre les commandes devices.*
     */
    void registerDeviceCommands();
    
    /**
     * @brief Enregistre les commandes routing.*
     */
    void registerRoutingCommands();
    
    /**
     * @brief Enregistre les commandes playback.*
     */
    void registerPlaybackCommands();
    
    /**
     * @brief Enregistre les commandes files.*
     */
    void registerFileCommands();
    
    /**
     * @brief Enregistre les commandes editor.*
     */
    void registerEditorCommands();
    
    /**
     * @brief Enregistre les commandes processing.*
     * @note ✅ CORRECTIF 1.4: Check nullptr processorManager_
     */
    void registerProcessingCommands();
    
    /**
     * @brief Enregistre les commandes network.*
     */
    void registerNetworkCommands();
    
    /**
     * @brief Enregistre les commandes system.*
     */
    void registerSystemCommands();
    
    /**
     * @brief Enregistre les commandes logger.*
     */
    void registerLoggerCommands();
    
    /**
     * @brief Enregistre les commandes loops.*
     */
    void registerLoopCommands();
    
    /**
     * @brief Enregistre les commandes instruments.*
     */
    void registerInstrumentCommands();
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Command factory
    CommandFactory factory_;
    
    /// Device manager
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    
    /// Router MIDI
    std::shared_ptr<MidiRouter> router_;
    
    /// Lecteur MIDI
    std::shared_ptr<MidiPlayer> player_;
    
    /// Gestionnaire fichiers
    std::shared_ptr<MidiFileManager> fileManager_;
    
    /// Gestionnaire SysEx (optionnel)
    std::shared_ptr<SysExHandler> sysExHandler_;
    
    /// ✅ Gestionnaire processeurs (optionnel) - NOUVEAU v3.1.1
    std::shared_ptr<ProcessorManager> processorManager_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CommandProcessorV2.h v3.1.1 - CORRECTIONS PHASE 1 COMPLÈTES
// ============================================================================
