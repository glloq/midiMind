// ============================================================================
// Fichier: backend/src/midi/MidiFileManager.h
// Version: 3.0.4 - COMPLET
// Date: 2025-10-13
// ============================================================================
// Description:
//   Gestionnaire central de la bibliothèque de fichiers MIDI
//   Gère scan, indexation, métadonnées, cache, recherche, conversion
//
// Fonctionnalités:
//   - Scan et indexation de répertoires
//   - Extraction de métadonnées MIDI
//   - Upload/Download de fichiers
//   - Conversion MIDI ↔ JsonMidi
//   - Tags et système de notes
//   - Recherche et filtrage avancé
//   - Gestion de playlists
//   - Cache mémoire pour performance
//   - Thread-safety complet
//
// Dépendances:
//   - Database (SQLite)
//   - MidiFileAnalyzer
//   - JsonMidiConverter
//
// Thread-safety: Toutes les méthodes sont thread-safe (mutex interne)
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>

#include "../storage/Database.h"
#include "MidiFileAnalyzer.h"
#include "JsonMidiConverter.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

class MidiFileAnalyzer;
class JsonMidiConverter;

// ============================================================================
// STRUCTURES DE DONNÉES
// ============================================================================

/**
 * @struct MidiFileEntry
 * @brief Entrée complète d'un fichier MIDI dans la bibliothèque
 */
struct MidiFileEntry {
    // Identification
    std::string id;
    std::string filename;
    std::string filepath;
    std::string relativePath;
    std::string directory;
    
    // Métadonnées fichier
    uint64_t fileSizeBytes;
    uint64_t lastModified;
    uint64_t addedDate;
    
    // Métadonnées MIDI
    int format;              // 0, 1, ou 2
    int division;            // Ticks per quarter note
    int trackCount;
    uint32_t durationMs;
    double initialTempo;
    std::string timeSignature;
    std::string keySignature;
    
    // Métadonnées textuelles
    std::string title;
    std::string composer;
    std::string copyright;
    std::string comments;
    
    // Annotations utilisateur
    std::string tags;        // CSV: "tag1,tag2,tag3"
    int rating;              // 0-5 étoiles
    int playCount;
    uint64_t lastPlayed;
    
    // Cache JsonMidi
    std::string jsonmidi;
    std::string jsonmidiVersion;
    
    // Constructeur par défaut
    MidiFileEntry();
    
    // Conversion JSON
    json toJson() const;
    static MidiFileEntry fromJson(const json& j);
};

/**
 * @typedef DatabaseRow
 * @brief Type pour les résultats de requêtes SQL
 */
using DatabaseRow = std::unordered_map<std::string, std::string>;

// ============================================================================
// CLASSE PRINCIPALE
// ============================================================================

/**
 * @class MidiFileManager
 * @brief Gestionnaire de bibliothèque de fichiers MIDI
 * 
 * @details
 * Cette classe gère l'intégralité du cycle de vie des fichiers MIDI:
 * - Découverte et indexation
 * - Extraction de métadonnées
 * - Stockage en base de données
 * - Cache mémoire pour performance
 * - Conversion de formats
 * - Recherche et filtrage
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe
 * 
 * Architecture:
 *   MidiFileManager
 *   ├── Database (SQLite persistance)
 *   ├── MidiFileAnalyzer (extraction métadonnées)
 *   ├── JsonMidiConverter (conversion formats)
 *   └── Cache (std::unordered_map)
 * 
 * @example Utilisation
 * @code
 * auto db = std::make_shared<Database>();
 * MidiFileManager manager(db, "/path/to/midi/files");
 * 
 * // Scanner répertoire
 * manager.scanDirectory();
 * 
 * // Lister fichiers
 * auto files = manager.listFiles();
 * 
 * // Convertir en JsonMidi
 * auto jsonmidi = manager.convertToJsonMidi("file_123");
 * @endcode
 */
class MidiFileManager {
public:
    // ========================================================================
    // CONSTRUCTEUR / DESTRUCTEUR
    // ========================================================================
    
    /**
     * @brief Constructeur
     * @param database Pointeur partagé vers Database SQLite
     * @param rootDirectory Répertoire racine des fichiers MIDI
     */
    explicit MidiFileManager(std::shared_ptr<Database> database,
                            const std::string& rootDirectory = "/home/pi/MidiMind/midi/files");
    
    /**
     * @brief Destructeur
     */
    ~MidiFileManager();
    
    // Empêcher copie et assignation
    MidiFileManager(const MidiFileManager&) = delete;
    MidiFileManager& operator=(const MidiFileManager&) = delete;
    
    // ========================================================================
    // SCAN ET INDEXATION
    // ========================================================================
    
    /**
     * @brief Scanne un répertoire pour les fichiers MIDI
     * @param directory Répertoire à scanner (vide = rootDirectory)
     * @param recursive Scanner récursivement les sous-dossiers
     * @return size_t Nombre de fichiers trouvés et indexés
     * @note Thread-safe
     */
    size_t scanDirectory(const std::string& directory = "", bool recursive = true);
    
    /**
     * @brief Indexe un fichier MIDI spécifique
     * @param filepath Chemin complet du fichier
     * @return std::optional<std::string> ID du fichier (vide si échec)
     * @note Thread-safe, extrait métadonnées et stocke en BDD
     */
    std::optional<std::string> indexFile(const std::string& filepath);
    
    /**
     * @brief Réindexe tous les fichiers de la bibliothèque
     * @note Thread-safe, processus long
     */
    void reindexAll();
    
    // ========================================================================
    // RÉCUPÉRATION DE DONNÉES
    // ========================================================================
    
    /**
     * @brief Récupère métadonnées d'un fichier par ID
     * @param fileId ID du fichier
     * @return std::optional<MidiFileEntry> Métadonnées (vide si pas trouvé)
     * @note Thread-safe, utilise cache si disponible
     */
    std::optional<MidiFileEntry> getFileMetadata(const std::string& fileId);
    
    /**
     * @brief Récupère métadonnées par chemin de fichier
     * @param filepath Chemin complet du fichier
     * @return std::optional<MidiFileEntry> Métadonnées (vide si pas trouvé)
     * @note Thread-safe
     */
    std::optional<MidiFileEntry> getFileByPath(const std::string& filepath);
    
    /**
     * @brief Liste tous les fichiers de la bibliothèque
     * @param directory Filtrer par répertoire (vide = tous)
     * @return std::vector<MidiFileEntry> Liste des fichiers
     * @note Thread-safe
     */
    std::vector<MidiFileEntry> listFiles(const std::string& directory = "");
    
    /**
     * @brief Récupère statistiques de la bibliothèque
     * @return json Statistiques {count, total_size, total_duration, etc.}
     * @note Thread-safe
     */
    json getLibraryStats();
    
    // ========================================================================
    // MODIFICATION DE FICHIERS
    // ========================================================================
    
    /**
     * @brief Renomme un fichier
     * @param filepath Chemin complet actuel du fichier
     * @param newName Nouveau nom (sans chemin, avec extension)
     * @return std::optional<std::string> Nouveau chemin complet (vide si échec)
     * @note Thread-safe, met à jour BDD et cache
     */
    std::optional<std::string> renameFile(const std::string& filepath, 
                                         const std::string& newName);
    
    /**
     * @brief Déplace un fichier vers un nouveau répertoire
     * @param fileId ID du fichier
     * @param newDirectory Nouveau répertoire de destination
     * @return std::optional<std::string> Nouveau chemin complet (vide si échec)
     * @note Thread-safe, met à jour BDD et cache
     */
    std::optional<std::string> moveFile(const std::string& fileId, 
                                       const std::string& newDirectory);
    
    /**
     * @brief Met à jour le chemin d'un fichier dans le cache
     * @param oldPath Ancien chemin complet
     * @param newPath Nouveau chemin complet
     * @note Thread-safe, NOUVEAU v3.0.4
     * @note Cette méthode met à jour uniquement le cache interne
     * @note La mise à jour de la BDD doit être faite séparément
     */
    void updateFilePath(const std::string& oldPath, const std::string& newPath);
    
    /**
     * @brief Upload un nouveau fichier MIDI
     * @param filename Nom du fichier
     * @param base64Data Données MIDI encodées en base64
     * @return std::string ID du fichier créé (vide si échec)
     * @note Thread-safe, génère ID unique
     */
    std::string uploadFile(const std::string& filename, 
                          const std::string& base64Data);
    
    /**
     * @brief Supprime un fichier
     * @param fileId ID du fichier
     * @return bool true si succès
     * @note Thread-safe, supprime fichier physique + BDD + cache
     */
    bool deleteFile(const std::string& fileId);
    
    // ========================================================================
    // TAGS ET RATING
    // ========================================================================
    
    /**
     * @brief Met à jour les tags d'un fichier
     * @param fileId ID du fichier
     * @param tags Liste des tags
     * @return bool true si succès
     * @note Thread-safe, tags stockés en CSV
     */
    bool updateTags(const std::string& fileId, 
                    const std::vector<std::string>& tags);
    
    /**
     * @brief Met à jour la note d'un fichier
     * @param fileId ID du fichier
     * @param rating Note (0-5 étoiles)
     * @return bool true si succès
     * @note Thread-safe
     */
    bool updateRating(const std::string& fileId, int rating);
    
    /**
     * @brief Incrémente le compteur de lectures
     * @param fileId ID du fichier
     * @note Thread-safe, met à jour playCount et lastPlayed
     */
    void incrementPlayCount(const std::string& fileId);
    
    // ========================================================================
    // RECHERCHE ET FILTRAGE
    // ========================================================================
    
    /**
     * @brief Recherche dans les métadonnées
     * @param query Texte à rechercher
     * @return std::vector<MidiFileEntry> Fichiers correspondants
     * @note Recherche dans: filename, title, composer, comments
     * @note Thread-safe, insensible à la casse
     */
    std::vector<MidiFileEntry> search(const std::string& query);
    
    /**
     * @brief Filtre les fichiers par tags
     * @param tags Liste des tags (opérateur AND)
     * @return std::vector<MidiFileEntry> Fichiers avec TOUS les tags
     * @note Thread-safe
     */
    std::vector<MidiFileEntry> filterByTags(const std::vector<std::string>& tags);
    
    /**
     * @brief Filtre les fichiers par note minimale
     * @param minRating Note minimale (0-5)
     * @return std::vector<MidiFileEntry> Fichiers avec rating >= minRating
     * @note Thread-safe
     */
    std::vector<MidiFileEntry> filterByRating(int minRating);
    
    /**
     * @brief Filtre les fichiers par compositeur
     * @param composer Nom du compositeur
     * @return std::vector<MidiFileEntry> Fichiers de ce compositeur
     * @note Thread-safe, insensible à la casse
     */
    std::vector<MidiFileEntry> filterByComposer(const std::string& composer);
    
    // ========================================================================
    // CONVERSION JSONMIDI
    // ========================================================================
    
    /**
     * @brief Convertit un fichier MIDI en JsonMidi
     * @param fileId ID du fichier
     * @return std::optional<json> JsonMidi (vide si échec)
     * @note Thread-safe, utilise cache si disponible
     */
    std::optional<json> convertToJsonMidi(const std::string& fileId);
    
    /**
     * @brief Sauvegarde un JsonMidi en fichier MIDI
     * @param jsonmidi Objet JsonMidi
     * @param filename Nom du fichier de sortie
     * @return std::optional<std::string> ID du fichier créé (vide si échec)
     * @note Thread-safe, indexe automatiquement le fichier créé
     */
    std::optional<std::string> saveFromJsonMidi(const json& jsonmidi, 
                                               const std::string& filename);
    
    // ========================================================================
    // CACHE
    // ========================================================================
    
    /**
     * @brief Vide le cache mémoire
     * @note Thread-safe
     */
    void clearCache();
    
    /**
     * @brief Précalcule et cache les JsonMidi de tous les fichiers
     * @note Thread-safe, processus très long
     */
    void warmupCache();
    
    /**
     * @brief Charge le cache depuis la base de données
     * @note Thread-safe, appelé automatiquement au démarrage
     */
    void loadCache();
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Nombre total de fichiers
     */
    size_t getFileCount() const;
    
    /**
     * @brief Taille totale de la bibliothèque en octets
     */
    uint64_t getTotalSize() const;
    
    /**
     * @brief Durée totale de tous les fichiers en ms
     */
    uint64_t getTotalDuration() const;
	
json getStatistics();

private:
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Base de données SQLite
    std::shared_ptr<Database> database_;
    
    /// Répertoire racine des fichiers MIDI
    std::string rootDirectory_;
    
    /// Analyseur de fichiers MIDI
    std::unique_ptr<MidiFileAnalyzer> analyzer_;
    
    /// Convertisseur MIDI/JsonMidi
    std::unique_ptr<JsonMidiConverter> converter_;
    
    /// Cache: fileId -> MidiFileEntry
    std::unordered_map<std::string, MidiFileEntry> cache_;
    
    /// Cache: filepath -> fileId (pour recherche rapide)
    std::unordered_map<std::string, std::string> pathToIdCache_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Génère un ID unique pour un fichier
     */
    std::string generateFileId() const;
    
    /**
     * @brief Met à jour le cache avec une entrée
     */
    void updateCache(const MidiFileEntry& entry);
    
    /**
     * @brief Supprime une entrée du cache
     */
    void removeFromCache(const std::string& fileId);
    
    /**
     * @brief Synchronise une entrée avec la base de données
     */
    void syncDatabase(const MidiFileEntry& entry);
    
    /**
     * @brief Charge une entrée depuis la base de données
     */
    std::optional<MidiFileEntry> loadFromDatabase(const std::string& fileId);
    
    /**
     * @brief Convertit une ligne SQL en MidiFileEntry
     */
    MidiFileEntry rowToEntry(const DatabaseRow& row) const;
    
    /**
     * @brief Calcule le chemin relatif depuis rootDirectory
     */
    std::string makeRelativePath(const std::string& absolutePath) const;
    
    /**
     * @brief Valide les données MIDI
     */
    bool isValidMidiData(const std::vector<uint8_t>& data) const;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiFileManager.h v3.0.4-COMPLET
// ============================================================================