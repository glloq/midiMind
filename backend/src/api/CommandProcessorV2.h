// ============================================================================
// Fichier: backend/src/api/CommandProcessorV2.h
// Version: 3.0.6 - CORRIGÉ (suppression logger.cpp)
// Date: 2025-10-12
// ============================================================================
// Description:
//   Processeur de commandes API v2 - Header corrigé sans logger.cpp
//
// Modifications v3.0.6:
//   ✅ Suppression déclaration registerLoggerCommands()
//   ✅ Ajout registerEditorCommands()
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
//   └── MidiFileManager       : Bibliothèque fichiers
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
#include "../midi/MidiRouter.h"
#include "../midi/MidiPlayer.h"
#include "../midi/MidiFileManager.h"

using json = nlohmann::json;

namespace midiMind {

/**
 * @class CommandProcessorV2
 * @brief Processeur de commandes API v2
 * 
 * Utilise CommandFactory pour enregistrer toutes les commandes
 * sous forme de lambdas et les exécuter de manière thread-safe.
 * 
 * Catégories de commandes:
 * - devices.*  : Gestion des périphériques MIDI
 * - routing.*  : Configuration du routage
 * - playback.* : Contrôle du lecteur
 * - files.*    : Gestion des fichiers
 * - editor.*   : Édition MIDI (JsonMidi)
 * - network.*  : Configuration réseau
 * - system.*   : Informations système
 * 
 * Version 3.0.6:
 * - Total: ~42-45 commandes selon modules actifs
 * - Thread-safe avec mutex
 * - Validation JSON automatique
 * - Gestion d'erreurs complète
 */
class CommandProcessorV2 {
public:
    // ========================================================================
    // CONSTRUCTEUR / DESTRUCTEUR
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * Initialise le processeur avec tous les modules et enregistre
     * automatiquement toutes les commandes disponibles.
     * 
     * @param deviceManager Gestionnaire de périphériques MIDI
     * @param router Routeur MIDI
     * @param player Lecteur MIDI
     * @param fileManager Gestionnaire de fichiers MIDI
     */
    CommandProcessorV2(
        std::shared_ptr<MidiDeviceManager> deviceManager,
        std::shared_ptr<MidiRouter> router,
        std::shared_ptr<MidiPlayer> player,
        std::shared_ptr<MidiFileManager> fileManager
    );
    
    /**
     * @brief Destructeur
     */
    ~CommandProcessorV2();
    
    // ========================================================================
    // TRAITEMENT DES COMMANDES
    // ========================================================================
    
    /**
     * @brief Traite une commande JSON
     * 
     * Format d'entrée attendu:
     * {
     *   "command": "category.action",
     *   "params": { ... }
     * }
     * 
     * Format de sortie:
     * {
     *   "success": true/false,
     *   "data": { ... },
     *   "error": "message si échec",
     *   "error_code": "CODE_ERREUR"
     * }
     * 
     * @param request Requête JSON
     * @return json Réponse JSON
     * 
     * @note Thread-safe
     */
    json processCommand(const json& request);
    
    // ========================================================================
    // INTROSPECTION
    // ========================================================================
    
    /**
     * @brief Compte le nombre total de commandes enregistrées
     * @return size_t Nombre de commandes
     */
    size_t getCommandCount() const;
    
    /**
     * @brief Liste toutes les commandes disponibles
     * @return std::vector<std::string> Noms des commandes
     */
    std::vector<std::string> listCommands() const;
    
    /**
     * @brief Liste les commandes par catégorie
     * @return std::unordered_map<std::string, std::vector<std::string>>
     *         Map catégorie -> liste de commandes
     */
    std::unordered_map<std::string, std::vector<std::string>> 
    listCommandsByCategory() const;

private:
    // ========================================================================
    // ENREGISTREMENT DES COMMANDES
    // ========================================================================
    
    /**
     * @brief Enregistre toutes les catégories de commandes
     * 
     * Appelé automatiquement par le constructeur.
     * Enregistre dans l'ordre:
     * 1. Device commands
     * 2. Routing commands
     * 3. Playback commands
     * 4. File commands
     * 5. Editor commands
     * 6. Network commands
     * 7. System commands
     */
    void registerAllCommands();
    
    /**
     * @brief Enregistre les commandes devices.* (5 commandes)
     * 
     * Commandes:
     * - devices.scan, devices.list, devices.connect,
     *   devices.disconnect, devices.info
     */
    void registerDeviceCommands();
    
    /**
     * @brief Enregistre les commandes routing.* (6 commandes)
     * 
     * Commandes:
     * - routing.addRoute, routing.removeRoute, routing.listRoutes,
     *   routing.updateRoute, routing.clearRoutes, routing.getStats
     */
    void registerRoutingCommands();
    
    /**
     * @brief Enregistre les commandes playback.* (9 commandes)
     * 
     * Commandes:
     * - playback.load, playback.play, playback.pause, playback.stop,
     *   playback.seek, playback.status, playback.getMetadata,
     *   playback.setLoop, playback.setTempo
     */
    void registerPlaybackCommands();
    
    /**
     * @brief Enregistre les commandes files.* (7 commandes)
     * 
     * Commandes:
     * - files.list, files.scan, files.delete, files.upload,
     *   files.getMetadata, files.convert, files.move
     */
    void registerFileCommands();
    
    /**
     * @brief Enregistre les commandes editor.* (7 commandes)
     * 
     * Commandes:
     * - editor.load, editor.save, editor.addNote, editor.deleteNote,
     *   editor.addCC, editor.undo, editor.redo
     */
    void registerEditorCommands();
    
    /**
     * @brief Enregistre les commandes network.* (6 commandes)
     * 
     * Commandes:
     * - network.status, network.getInterfaces, network.scanWifi,
     *   network.connectWifi, network.startHotspot, network.stopHotspot
     */
    void registerNetworkCommands();
    
    /**
     * @brief Enregistre les commandes system.* (5 commandes)
     * 
     * Commandes:
     * - system.status, system.info, system.getCommands,
     *   system.shutdown, system.restart
     */
    void registerSystemCommands();
    
    // ✅ SUPPRIMÉ: void registerLoggerCommands();
    
    // ========================================================================
    // VALIDATION ET UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Valide le format d'une commande JSON
     * 
     * Vérifie:
     * - Structure JSON valide
     * - Champ "command" présent et string
     * - Champ "params" optionnel mais doit être objet si présent
     * 
     * @param command Commande JSON à valider
     * @return bool true si valide
     */
    bool validateCommand(const json& command) const;
    
    /**
     * @brief Crée une réponse d'erreur standardisée
     * 
     * @param message Message d'erreur
     * @param code Code d'erreur
     * @return json Réponse d'erreur
     */
    json createErrorResponse(const std::string& message,
                            const std::string& code) const;
    
    /**
     * @brief Crée une réponse de succès standardisée
     * 
     * @param data Données de réponse
     * @return json Réponse de succès
     */
    json createSuccessResponse(const json& data) const;
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    CommandFactory factory_;                              ///< Factory de commandes
    
    std::shared_ptr<MidiDeviceManager> deviceManager_;   ///< Gestionnaire devices
    std::shared_ptr<MidiRouter> router_;                 ///< Routeur MIDI
    std::shared_ptr<MidiPlayer> player_;                 ///< Lecteur MIDI
    std::shared_ptr<MidiFileManager> fileManager_;       ///< Gestionnaire fichiers
    
    mutable std::mutex mutex_;                            ///< Mutex pour thread-safety
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CommandProcessorV2.h
// ============================================================================
