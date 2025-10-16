// ============================================================================
// Fichier: backend/src/api/CommandProcessorV2.h
// Version: 3.1.2 - CORRECTIONS COMPLÈTES
// Date: 2025-10-16
// ============================================================================
// CORRECTIFS v3.1.2:
//   ✅ Ajout membre database_
//   ✅ Ajout paramètre database au constructeur
//   ✅ Signatures cohérentes avec .cpp v3.1.2
//   ✅ Documentation complète mise à jour
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
//   ├── ProcessorManager      : Processeurs MIDI
//   └── Database              : Persistance SQLite
//
// Auteur: MidiMind Team
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "../core/commands/CommandFactory.h"
#include "../core/Logger.h"
#include "../midi/devices/MidiDeviceManager.h"
#include "../midi/MidiRouter.h"
#include "../midi/player/MidiPlayer.h"
#include "../midi/files/MidiFileManager.h"
#include "../midi/sysex/SysExHandler.h"
#include "../midi/processing/ProcessorManager.h"
#include "../storage/Database.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// CLASSE: CommandProcessorV2
// ============================================================================

/**
 * @class CommandProcessorV2
 * @brief Processeur de commandes API v2
 * 
 * Utilise CommandFactory pour enregistrer toutes les commandes
 * sous forme de lambdas et les exécuter de manière thread-safe.
 * 
 * Catégories de commandes (v3.1.2):
 * - devices.*     : Gestion des périphériques MIDI (5 commandes)
 * - routing.*     : Configuration du routage (6 commandes)
 * - playback.*    : Contrôle du lecteur (11 commandes)
 * - files.*       : Gestion des fichiers (12 commandes)
 * - editor.*      : Édition MIDI JsonMidi (7 commandes)
 * - processing.*  : Processeurs MIDI (5+ commandes)
 * - network.*     : Configuration réseau (6 commandes)
 * - system.*      : Informations système (6 commandes)
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
 *     sysExHandler, processorManager, database
 * );
 * 
 * std::string request = R"({"command": "devices.list", "params": {}})";
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
     * @param fileManager Gestionnaire fichiers MIDI
     * @param sysExHandler Gestionnaire SysEx (optionnel)
     * @param processorManager Gestionnaire processeurs (optionnel)
     * @param database Base de données SQLite (optionnel)
     * 
     * @note Les paramètres peuvent être nullptr
     * @note Enregistre automatiquement toutes les commandes disponibles
     * 
     * @example
     * ```cpp
     * auto processor = std::make_shared<CommandProcessorV2>(
     *     deviceManager,
     *     router,
     *     player,
     *     fileManager,
     *     sysExHandler,
     *     processorManager,
     *     database
     * );
     * ```
     */
    CommandProcessorV2(
        std::shared_ptr<MidiDeviceManager> deviceManager,
        std::shared_ptr<MidiRouter> router,
        std::shared_ptr<MidiPlayer> player,
        std::shared_ptr<MidiFileManager> fileManager,
        std::shared_ptr<SysExHandler> sysExHandler = nullptr,
        std::shared_ptr<ProcessorManager> processorManager = nullptr,
        std::shared_ptr<Database> database = nullptr
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
     * @throws Aucune exception (toutes catchées et retournées comme erreur)
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
     * @note Nécessite fileManager_ et database_
     */
    void registerFileCommands();
    
    /**
     * @brief Enregistre les commandes editor.*
     */
    void registerEditorCommands();
    
    /**
     * @brief Enregistre les commandes processing.*
     * @note Check nullptr processorManager_
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
    
    /// Command factory (thread-safe)
    CommandFactory factory_;
    
    /// Device manager
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    
    /// Router MIDI
    std::shared_ptr<MidiRouter> router_;
    
    /// Lecteur MIDI
    std::shared_ptr<MidiPlayer> player_;
    
    /// Gestionnaire fichiers MIDI
    std::shared_ptr<MidiFileManager> fileManager_;
    
    /// Gestionnaire SysEx (optionnel)
    std::shared_ptr<SysExHandler> sysExHandler_;
    
    /// Gestionnaire processeurs (optionnel)
    std::shared_ptr<ProcessorManager> processorManager_;
    
    /// Base de données SQLite (optionnel)
    std::shared_ptr<Database> database_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CommandProcessorV2.h v3.1.2 - COMPLET ET COHÉRENT
// ============================================================================
