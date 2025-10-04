// ============================================================================
// Fichier: src/midi/MidiFileManager.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestionnaire de bibliothèque de fichiers MIDI et playlists (VERSION REFACTORISÉE).
//   Façade qui utilise FileSystemScanner, MidiFileDatabase et MidiFileAnalyzer.
//   Indexe les fichiers, extrait les métadonnées, gère les recherches
//   et les playlists dans une base de données SQLite.
//
// Responsabilités:
//   - Scanner le système de fichiers pour trouver les fichiers MIDI
//   - Indexer les fichiers dans SQLite via MidiFileDatabase
//   - Extraire et stocker les métadonnées via MidiFileAnalyzer
//   - Fournir des fonctions de recherche
//   - Gérer les playlists (CRUD) via PlaylistManager
//   - Calculer des statistiques de la bibliothèque
//
// Architecture:
//   MidiFileManager (Façade)
//   ├── FileSystemScanner    : Scan du filesystem
//   ├── MidiFileDatabase     : Persistance SQLite
//   ├── MidiFileAnalyzer     : Extraction métadonnées
//   └── PlaylistManager      : Gestion playlists
//
// Thread-safety: OUI - Toutes les méthodes publiques sont thread-safe
//
// Patterns: Facade Pattern, Dependency Injection
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <memory>              // Pour std::shared_ptr, std::unique_ptr
#include <string>              // Pour std::string
#include <vector>              // Pour std::vector
#include <mutex>               // Pour std::mutex
#include <atomic>              // Pour std::atomic
#include <functional>          // Pour std::function
#include <thread>              // Pour std::thread
#include <optional>            // Pour std::optional
#include <nlohmann/json.hpp>   // Pour JSON

#include "../core/Logger.h"
#include "filemanager/FileSystemScanner.h"
#include "filemanager/MidiFileDatabase.h"
#include "filemanager/PlaylistManager.h"
#include "MidiFileAnalyzer.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct MidiFileInfo
 * @brief Informations complètes sur un fichier MIDI
 * 
 * Structure unifiée qui combine les informations du filesystem,
 * de la base de données et de l'analyse MIDI.
 */
struct MidiFileInfo {
    // Identifiants
    std::string id;              ///< ID unique (UUID)
    std::string filepath;        ///< Chemin complet du fichier
    std::string filename;        ///< Nom du fichier
    std::string relativePath;    ///< Chemin relatif au répertoire racine
    
    // Métadonnées de base
    std::string title;           ///< Titre (depuis métadonnées MIDI ou filename)
    std::string composer;        ///< Compositeur (si disponible)
    std::string copyright;       ///< Copyright (si disponible)
    
    // Informations techniques
    uint64_t sizeBytes;          ///< Taille en bytes
    uint32_t durationMs;         ///< Durée en millisecondes
    uint16_t trackCount;         ///< Nombre de pistes
    double tempo;                ///< Tempo principal (BPM)
    uint16_t ticksPerQuarterNote; ///< TPQN (résolution temporelle)
    uint16_t format;             ///< Format MIDI (0, 1, ou 2)
    
    // Métadonnées utilisateur
    std::vector<std::string> tags;  ///< Tags personnalisés
    int rating;                  ///< Note (0-5 étoiles)
    
    // Timestamps
    uint64_t addedTimestamp;     ///< Date d'ajout (ms depuis epoch)
    uint64_t modifiedTimestamp;  ///< Date de modification fichier
    uint64_t lastPlayedTimestamp; ///< Dernière lecture
    int playCount;               ///< Nombre de lectures
    
    /**
     * @brief Constructeur par défaut
     */
    MidiFileInfo()
        : sizeBytes(0)
        , durationMs(0)
        , trackCount(0)
        , tempo(120.0)
        , ticksPerQuarterNote(480)
        , format(1)
        , rating(0)
        , addedTimestamp(0)
        , modifiedTimestamp(0)
        , lastPlayedTimestamp(0)
        , playCount(0) {}
    
    /**
     * @brief Convertit en JSON
     * 
     * @return json Objet JSON avec toutes les informations
     */
    json toJson() const {
        json j;
        j["id"] = id;
        j["filepath"] = filepath;
        j["filename"] = filename;
        j["relative_path"] = relativePath;
        j["title"] = title;
        j["composer"] = composer;
        j["copyright"] = copyright;
        j["size_bytes"] = sizeBytes;
        j["size_mb"] = static_cast<double>(sizeBytes) / (1024 * 1024);
        j["duration_ms"] = durationMs;
        j["duration_seconds"] = durationMs / 1000;
        j["track_count"] = trackCount;
        j["tempo"] = tempo;
        j["ticks_per_quarter_note"] = ticksPerQuarterNote;
        j["format"] = format;
        j["tags"] = tags;
        j["rating"] = rating;
        j["added_timestamp"] = addedTimestamp;
        j["modified_timestamp"] = modifiedTimestamp;
        j["last_played_timestamp"] = lastPlayedTimestamp;
        j["play_count"] = playCount;
        return j;
    }
    
    /**
     * @brief Crée depuis MidiFileEntry (BDD) + analyse
     */
    static MidiFileInfo fromDatabaseEntry(const MidiFileEntry& entry) {
        MidiFileInfo info;
        info.id = entry.id;
        info.filepath = entry.filepath;
        info.filename = entry.filename;
        info.relativePath = entry.relativePath;
        info.title = entry.title;
        info.composer = entry.composer;
        info.sizeBytes = entry.fileSizeBytes;
        info.durationMs = entry.durationMs;
        info.trackCount = entry.trackCount;
        info.tags = entry.tags;
        info.addedTimestamp = entry.addedDate * 1000ULL;
        info.modifiedTimestamp = entry.lastModified * 1000ULL;
        info.lastPlayedTimestamp = entry.lastPlayed * 1000ULL;
        info.playCount = entry.playCount;
        info.rating = 0; // TODO: Ajouter rating à MidiFileEntry
        
        // Valeurs par défaut pour les champs non stockés
        info.tempo = 120.0;
        info.ticksPerQuarterNote = 480;
        info.format = 1;
        
        return info;
    }
};

/**
 * @struct Playlist
 * @brief Structure de playlist
 * 
 * @note Compatible avec PlaylistManager
 */
struct Playlist {
    std::string id;              ///< ID unique (UUID)
    std::string name;            ///< Nom de la playlist
    std::string description;     ///< Description
    std::vector<std::string> fileIds; ///< Liste des IDs de fichiers
    uint64_t createdTimestamp;   ///< Date de création
    uint64_t modifiedTimestamp;  ///< Date de modification
    
    json toJson() const {
        json j;
        j["id"] = id;
        j["name"] = name;
        j["description"] = description;
        j["file_ids"] = fileIds;
        j["file_count"] = fileIds.size();
        j["created_timestamp"] = createdTimestamp;
        j["modified_timestamp"] = modifiedTimestamp;
        return j;
    }
};

// ============================================================================
// CLASSE PRINCIPALE
// ============================================================================

/**
 * @class MidiFileManager
 * @brief Gestionnaire de bibliothèque MIDI (Façade refactorisée)
 * 
 * @details
 * Cette version refactorisée utilise une architecture en composants:
 * - FileSystemScanner pour le scan du filesystem
 * - MidiFileDatabase pour la persistance SQLite
 * - MidiFileAnalyzer pour l'extraction de métadonnées
 * - PlaylistManager pour la gestion des playlists
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * Le scan s'exécute dans un thread séparé.
 * 
 * @example Utilisation typique
 * ```cpp
 * auto manager = std::make_shared<MidiFileManager>("/home/pi/midi-files", "midimind.db");
 * 
 * // Scanner la bibliothèque
 * manager->scanLibrary(true, false);
 * 
 * // Rechercher des fichiers
 * auto results = manager->searchFiles("beethoven");
 * 
 * // Créer une playlist
 * std::vector<std::string> ids = {"file1", "file2"};
 * auto playlist = manager->createPlaylist("Ma Playlist", "Description", ids);
 * ```
 */
class MidiFileManager {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé à la fin du scan
     * @param filesFound Nombre de fichiers trouvés
     * @param filesAdded Nombre de fichiers ajoutés
     * @param filesUpdated Nombre de fichiers mis à jour
     */
    using ScanCompleteCallback = std::function<void(size_t filesFound, size_t filesAdded, size_t filesUpdated)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param midiFilesDirectory Répertoire racine des fichiers MIDI
     * @param databasePath Chemin de la base de données SQLite
     * 
     * @throws std::runtime_error Si l'initialisation échoue
     */
    explicit MidiFileManager(const std::string& midiFilesDirectory,
                            const std::string& databasePath);
    
    /**
     * @brief Destructeur
     * 
     * @note Attend la fin du scan en cours avant de détruire
     */
    ~MidiFileManager();
    
    // Désactiver copie et assignation
    MidiFileManager(const MidiFileManager&) = delete;
    MidiFileManager& operator=(const MidiFileManager&) = delete;
    
    // ========================================================================
    // SCAN DE LA BIBLIOTHÈQUE
    // ========================================================================
    
    /**
     * @brief Scanne la bibliothèque pour indexer les fichiers
     * 
     * Lance un scan asynchrone du système de fichiers. Pour chaque fichier .mid trouvé:
     * 1. Extrait les métadonnées avec MidiFileAnalyzer
     * 2. Insère ou met à jour dans la base de données
     * 3. Met à jour le cache en mémoire
     * 
     * @param recursive Si true, scanne les sous-répertoires
     * @param updateExisting Si true, ré-analyse les fichiers existants
     * @return true Si le scan a démarré, false si un scan est déjà en cours
     * 
     * @note Thread-safe. Le scan s'exécute dans un thread séparé.
     * 
     * @example
     * ```cpp
     * manager->setOnScanComplete([](size_t found, size_t added, size_t updated) {
     *     Logger::info("Scan", "Found: " + std::to_string(found));
     * });
     * manager->scanLibrary(true, false);
     * ```
     */
    bool scanLibrary(bool recursive = true, bool updateExisting = false);
    
    /**
     * @brief Vérifie si un scan est en cours
     * 
     * @return true Si un scan est en cours
     * 
     * @note Thread-safe
     */
    bool isScanning() const;
    
    /**
     * @brief Définit le callback de fin de scan
     * 
     * @param callback Fonction appelée à la fin du scan
     * 
     * @note Thread-safe
     */
    void setOnScanComplete(ScanCompleteCallback callback);
    
    // ========================================================================
    // RECHERCHE ET LISTAGE
    // ========================================================================
    
    /**
     * @brief Liste tous les fichiers
     * 
     * @param limit Nombre maximum de résultats (0 = tous)
     * @param offset Offset pour pagination
     * @return std::vector<MidiFileInfo> Liste des fichiers
     * 
     * @note Thread-safe. Accède à la base de données.
     * 
     * @example
     * ```cpp
     * // Première page (20 fichiers)
     * auto page1 = manager->listFiles(20, 0);
     * 
     * // Deuxième page
     * auto page2 = manager->listFiles(20, 20);
     * ```
     */
    std::vector<MidiFileInfo> listFiles(int limit = 0, int offset = 0) const;
    
    /**
     * @brief Recherche des fichiers par mots-clés
     * 
     * Recherche dans:
     * - Nom du fichier
     * - Titre
     * - Compositeur
     * - Tags
     * 
     * @param query Texte de recherche (minimum 2 caractères)
     * @return std::vector<MidiFileInfo> Fichiers correspondants
     * 
     * @note Thread-safe. Utilise SQLite FTS si disponible.
     * 
     * @example
     * ```cpp
     * auto results = manager->searchFiles("mozart sonata");
     * ```
     */
    std::vector<MidiFileInfo> searchFiles(const std::string& query) const;
    
    /**
     * @brief Récupère un fichier par son ID
     * 
     * @param fileId ID du fichier
     * @return std::optional<MidiFileInfo> Informations du fichier ou std::nullopt
     * 
     * @note Thread-safe
     */
    std::optional<MidiFileInfo> getFile(const std::string& fileId) const;
    
    /**
     * @brief Récupère l'analyse complète d'un fichier
     * 
     * @param fileId ID du fichier
     * @return std::optional<MidiFileAnalysis> Analyse détaillée ou std::nullopt
     * 
     * @note Cette méthode charge et analyse le fichier à la volée.
     *       Pour des performances optimales, mettre en cache si nécessaire.
     * 
     * @throws std::runtime_error Si le fichier n'existe pas ou est corrompu
     */
    std::optional<MidiFileAnalysis> getFileAnalysis(const std::string& fileId) const;
    
    // ========================================================================
    // MÉTADONNÉES UTILISATEUR
    // ========================================================================
    
    /**
     * @brief Met à jour les tags d'un fichier
     * 
     * @param fileId ID du fichier
     * @param tags Nouveaux tags
     * @return true Si la mise à jour a réussi
     * 
     * @note Thread-safe
     */
    bool updateTags(const std::string& fileId, const std::vector<std::string>& tags);
    
    /**
     * @brief Met à jour la note d'un fichier
     * 
     * @param fileId ID du fichier
     * @param rating Note (0-5 étoiles)
     * @return true Si la mise à jour a réussi
     * 
     * @note Thread-safe
     */
    bool updateRating(const std::string& fileId, int rating);
    
    /**
     * @brief Incrémente le compteur de lectures
     * 
     * @param fileId ID du fichier
     * 
     * @note Appelé automatiquement par MidiPlayer. Thread-safe.
     */
    void incrementPlayCount(const std::string& fileId);
    
    // ========================================================================
    // GESTION DES PLAYLISTS
    // ========================================================================
    
    /**
     * @brief Crée une nouvelle playlist
     * 
     * @param name Nom de la playlist
     * @param description Description
     * @param fileIds IDs des fichiers (optionnel)
     * @return Playlist créée
     * 
     * @throws std::invalid_argument Si le nom est vide
     * 
     * @note Thread-safe
     */
    Playlist createPlaylist(const std::string& name,
                           const std::string& description = "",
                           const std::vector<std::string>& fileIds = {});
    
    /**
     * @brief Liste toutes les playlists
     * 
     * @return std::vector<Playlist> Liste des playlists
     * 
     * @note Thread-safe
     */
    std::vector<Playlist> listPlaylists() const;
    
    /**
     * @brief Récupère une playlist par son ID
     * 
     * @param playlistId ID de la playlist
     * @return std::optional<Playlist> Playlist ou std::nullopt
     * 
     * @note Thread-safe
     */
    std::optional<Playlist> getPlaylist(const std::string& playlistId) const;
    
    /**
     * @brief Ajoute des fichiers à une playlist
     * 
     * @param playlistId ID de la playlist
     * @param fileIds IDs des fichiers à ajouter
     * @return true Si l'ajout a réussi
     * 
     * @note Thread-safe
     */
    bool addToPlaylist(const std::string& playlistId, const std::vector<std::string>& fileIds);
    
    /**
     * @brief Retire des fichiers d'une playlist
     * 
     * @param playlistId ID de la playlist
     * @param fileIds IDs des fichiers à retirer
     * @return true Si le retrait a réussi
     * 
     * @note Thread-safe
     */
    bool removeFromPlaylist(const std::string& playlistId, const std::vector<std::string>& fileIds);
    
    /**
     * @brief Supprime une playlist
     * 
     * @param playlistId ID de la playlist
     * @return true Si la suppression a réussi
     * 
     * @note Thread-safe. Ne supprime pas les fichiers.
     */
    bool deletePlaylist(const std::string& playlistId);
    
    /**
     * @brief Renomme une playlist
     * 
     * @param playlistId ID de la playlist
     * @param newName Nouveau nom
     * @return true Si le renommage a réussi
     * 
     * @note Thread-safe
     */
    bool renamePlaylist(const std::string& playlistId, const std::string& newName);
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques de la bibliothèque
     * 
     * @return json Objet JSON avec statistiques
     * 
     * Statistiques fournies:
     * - total_files: Nombre total de fichiers
     * - total_duration_ms: Durée totale en ms
     * - total_duration_hours: Durée totale en heures
     * - total_size_bytes: Taille totale en bytes
     * - total_size_mb: Taille totale en MB
     * - total_plays: Nombre total de lectures
     * - avg_file_size_mb: Taille moyenne d'un fichier
     * - avg_duration_ms: Durée moyenne d'un fichier
     * - total_playlists: Nombre de playlists
     * 
     * @note Thread-safe
     */
    json getStatistics() const;
    
    // ========================================================================
    // ACCESSEURS
    // ========================================================================
    
    /**
     * @brief Récupère le répertoire racine
     */
    std::string getRootDirectory() const { return rootDir_; }
    
    /**
     * @brief Récupère le chemin de la base de données
     */
    std::string getDatabasePath() const { return dbPath_; }

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Crée une entrée MidiFileEntry depuis un ScannedFile
     * 
     * Analyse le fichier MIDI et extrait toutes les métadonnées.
     * 
     * @param file Fichier scanné
     * @return MidiFileEntry Entrée pour la base de données
     */
    MidiFileEntry createEntryFromScannedFile(const ScannedFile& file);
    
    /**
     * @brief Génère un UUID unique
     * 
     * @return std::string UUID au format "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
     */
    std::string generateUUID();
    
    /**
     * @brief Convertit Playlist (interne) en Playlist (publique)
     */
    Playlist convertPlaylist(const PlaylistInfo& internal) const;
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Répertoire racine des fichiers MIDI
    std::string rootDir_;
    
    /// Chemin de la base de données
    std::string dbPath_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Flag de scan en cours
    std::atomic<bool> scanning_;
    
    /// Composants (Dependency Injection)
    std::unique_ptr<FileSystemScanner> scanner_;
    std::unique_ptr<MidiFileDatabase> database_;
    std::unique_ptr<PlaylistManager> playlistManager_;
    
    /// Callback de fin de scan
    ScanCompleteCallback onScanComplete_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiFileManager.h
// ============================================================================