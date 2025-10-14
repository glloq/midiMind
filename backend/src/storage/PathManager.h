// ============================================================================
// Fichier: src/storage/PathManager.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestionnaire centralisé des chemins de l'application.
//   Définit et gère tous les chemins utilisés par MidiMind.
//
// Thread-safety: OUI
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <mutex>

#include "FileSystem.h"
#include "../core/Logger.h"

namespace midiMind {

/**
 * @class PathManager
 * @brief Gestionnaire centralisé des chemins
 * 
 * @details
 * Définit tous les chemins utilisés par l'application.
 * 
 * Structure:
 * ```
 * /home/pi/MidiMind/
 * ├── config/
 * │   ├── config.json
 * │   └── presets/
 * ├── data/
 * │   ├── midimind.db
 * │   └── sessions/
 * ├── midi/
 * │   ├── files/
 * │   └── recordings/
 * ├── logs/
 * └── backups/
 * ```
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * PathManager::instance().setBasePath("/home/pi/MidiMind");
 * PathManager::instance().initialize();
 * 
 * auto configPath = PathManager::instance().getConfigPath();
 * auto dbPath = PathManager::instance().getDatabasePath();
 * ```
 */
class PathManager {
public:
    // ========================================================================
    // SINGLETON
    // ========================================================================
    
    /**
     * @brief Récupère l'instance singleton
     */
    static PathManager& instance();
    
    // Désactiver copie et move
    PathManager(const PathManager&) = delete;
    PathManager& operator=(const PathManager&) = delete;
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    /**
     * @brief Initialise les chemins
     * 
     * Crée tous les dossiers nécessaires.
     * 
     * @note Thread-safe
     */
    void initialize();
    
    /**
     * @brief Définit le chemin de base
     * 
     * @param basePath Chemin de base (défaut: /home/pi/MidiMind)
     * 
     * @note Thread-safe
     */
    void setBasePath(const std::string& basePath);
    
    /**
     * @brief Récupère le chemin de base
     * 
     * @note Thread-safe
     */
    std::string getBasePath() const;
    
    // ========================================================================
    // CHEMINS PRINCIPAUX
    // ========================================================================
    
    /**
     * @brief Récupère le chemin du dossier config
     */
    std::string getConfigPath() const;
    
    /**
     * @brief Récupère le chemin du fichier config.json
     */
    std::string getConfigFilePath() const;
    
    /**
     * @brief Récupère le chemin du dossier presets
     */
    std::string getPresetsPath() const;
    
    /**
     * @brief Récupère le chemin du dossier data
     */
    std::string getDataPath() const;
    
    /**
     * @brief Récupère le chemin de la base de données
     */
    std::string getDatabasePath() const;
    
    /**
     * @brief Récupère le chemin du dossier sessions
     */
    std::string getSessionsPath() const;
    
    /**
     * @brief Récupère le chemin du dossier MIDI
     */
    std::string getMidiPath() const;
    
    /**
     * @brief Récupère le chemin des fichiers MIDI
     */
    std::string getMidiFilesPath() const;
    
    /**
     * @brief Récupère le chemin des enregistrements MIDI
     */
    std::string getMidiRecordingsPath() const;
    
    /**
     * @brief Récupère le chemin du dossier logs
     */
    std::string getLogsPath() const;
    
    /**
     * @brief Récupère le chemin du fichier log actuel
     */
    std::string getLogFilePath() const;
    
    /**
     * @brief Récupère le chemin du dossier backups
     */
    std::string getBackupsPath() const;
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Nettoie les vieux fichiers
     * 
     * @param directory Dossier à nettoyer
     * @param maxAgeDays Âge maximum en jours
     * @return int Nombre de fichiers supprimés
     * 
     * @note Thread-safe
     */
    int cleanOldFiles(const std::string& directory, int maxAgeDays);
    
    /**
     * @brief Crée un backup de la base de données
     * 
     * @return std::string Chemin du backup créé
     * 
     * @note Thread-safe
     */
    std::string createDatabaseBackup();

private:
    // ========================================================================
    // CONSTRUCTION PRIVÉE (Singleton)
    // ========================================================================
    
    PathManager();
    ~PathManager();
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Chemin de base
    std::string basePath_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER PathManager.h
// ============================================================================