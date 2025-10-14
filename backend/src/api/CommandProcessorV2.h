// ============================================================================
// Fichier: backend/src/api/CommandProcessorV2.h
// Version: 3.0.7 - CORRECTION DÉCLARATIONS COMPLÈTES
// Date: 2025-10-14
// ============================================================================
// Description:
//   Processeur de commandes API v2 - Header avec TOUTES les déclarations
//
// CORRECTIONS v3.0.7:
//   ✅ Ajout déclaration registerProcessingCommands()
//   ✅ Ajout commentaires pour catégories optionnelles
//   ✅ Mise à jour documentation
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
 * Catégories de commandes (v3.0.7):
 * - devices.*     : Gestion des périphériques MIDI (5 commandes)
 * - routing.*     : Configuration du routage (6 commandes)
 * - playback.*    : Contrôle du lecteur (9 commandes)
 * - files.*       : Gestion des fichiers (7 commandes)
 * - editor.*      : Édition MIDI JsonMidi (7 commandes)
 * - processing.*  : Processeurs MIDI (5+ commandes)
 * - network.*     : Configuration réseau (6 commandes)
 * - system.*      : Informations système (5 commandes)
 * - logger.*      : Configuration logs (3 commandes)
 * 
 * Catégories optionnelles:
 * - loops.*       : Enregistrement loops (si LoopManager disponible)
 * - instruments.* : Gestion instruments (si InstrumentManager disponible)
 * 
 * Version 3.0.7:
 * - Total: ~50+ commandes selon modules actifs
 * - Thread-safe avec mutex
 * - Validation JSON automatique
 * - Gestion d'erreurs complète
 * - Support toutes catégories documentées
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
     * @param player Lecteur de fichiers MIDI
     * @param fileManager Gestionnaire de bibliothèque MIDI
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
    
    // Désactiver copie et assignation
    CommandProcessorV2(const CommandProcessorV2&) = delete;
    CommandProcessorV2& operator=(const CommandProcessorV2&) = delete;
    
    // ========================================================================
    // TRAITEMENT DES COMMANDES
    // ========================================================================
    
    /**
     * @brief Traite une commande JSON
     * 
     * @param command Objet JSON contenant "command" et optionnellement "params"
     * @return json Réponse avec "success", "error" ou "data"
     * 
     * @example
     * json cmd = {
     *     {"command", "devices.scan"},
     *     {"params", {}}
     * };
     * json response = processor.processCommand(cmd);
     */
    json processCommand(const json& command);
    
    // ========================================================================
    // INTROSPECTION
    // ========================================================================
    
    /**
     * @brief Retourne le nombre total de commandes enregistrées
     */
    size_t getCommandCount() const;
    
    /**
     * @brief Liste toutes les commandes disponibles
     * 
     * @return std::vector<std::string> Liste des noms de commandes
     */
    std::vector<std::string> listCommands() const;
    
    /**
     * @brief Liste les commandes groupées par catégorie
     * 
     * @return Map catégorie → liste de commandes
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
     * Parcourt toutes les catégories et appelle leurs fonctions
     * d'enregistrement respectives.
     */
    void registerAllCommands();
    
    /**
     * @brief Enregistre les commandes devices.*
     * 
     * Commandes: scan, list, connect, disconnect, info
     */
    void registerDeviceCommands();
    
    /**
     * @brief Enregistre les commandes routing.*
     * 
     * Commandes: add, remove, list, clear, enable, disable
     */
    void registerRoutingCommands();
    
    /**
     * @brief Enregistre les commandes playback.*
     * 
     * Commandes: load, play, pause, stop, seek, status, 
     *            getMetadata, setLoop, setSpeed
     */
    void registerPlaybackCommands();
    
    /**
     * @brief Enregistre les commandes files.*
     * 
     * Commandes: list, scan, upload, delete, move, 
     *            getMetadata, updateMetadata
     */
    void registerFileCommands();
    
    /**
     * @brief Enregistre les commandes editor.*
     * 
     * Commandes: edit, save, validate, import, export,
     *            getStructure, preview
     */
    void registerEditorCommands();
    
    /**
     * @brief Enregistre les commandes processing.*
     * 
     * Commandes: add, remove, list, enable, disable,
     *            configure, getProcessors
     */
    void registerProcessingCommands();
    
    /**
     * @brief Enregistre les commandes network.*
     * 
     * Commandes: wifi.scan, wifi.connect, wifi.status,
     *            bluetooth.scan, bluetooth.pair, mdns.discover
     */
    void registerNetworkCommands();
    
    /**
     * @brief Enregistre les commandes system.*
     * 
     * Commandes: info, shutdown, restart, getStats, commands
     */
    void registerSystemCommands();
    
    /**
     * @brief Enregistre les commandes logger.*
     * 
     * Commandes: setLevel, getLevel, getStats
     */
    void registerLoggerCommands();
    
    // Méthodes optionnelles (décommenter si modules disponibles)
    /*
    void registerLoopCommands();
    void registerInstrumentCommands();
    */
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    /**
     * @brief Valide le format d'une commande JSON
     * 
     * Vérifie:
     * - Format JSON objet
     * - Présence du champ "command" (string)
     * - Validité du champ "params" si présent (objet)
     * 
     * @param command Commande à valider
     * @return true Si la commande est valide
     */
    bool validateCommand(const json& command) const;
    
    /**
     * @brief Crée une réponse d'erreur standardisée
     * 
     * @param message Message d'erreur
     * @param code Code d'erreur
     * @return json Réponse {"success": false, "error": ..., "error_code": ...}
     */
    json createErrorResponse(const std::string& message,
                            const std::string& code) const;
    
    /**
     * @brief Crée une réponse de succès standardisée
     * 
     * @param data Données optionnelles à inclure
     * @return json Réponse {"success": true, ...}
     */
    json createSuccessResponse(const json& data = json::object()) const;
    
    // ========================================================================
    // ATTRIBUTS
    // ========================================================================
    
    /// Factory de commandes (pattern Command)
    CommandFactory factory_;
    
    /// Gestionnaire de périphériques MIDI
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    
    /// Routeur MIDI
    std::shared_ptr<MidiRouter> router_;
    
    /// Lecteur de fichiers MIDI
    std::shared_ptr<MidiPlayer> player_;
    
    /// Gestionnaire de bibliothèque MIDI
    std::shared_ptr<MidiFileManager> fileManager_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CommandProcessorV2.h v3.0.7 - CORRECTION COMPLÈTE
// ============================================================================
