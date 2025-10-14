// ============================================================================
// Fichier: src/midi/MidiFileStructures.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.3 - 2025-10-09 - STRUCTURES UNIFIÉES
// ============================================================================
// Description:
//   Structures de données unifiées pour les fichiers MIDI.
//   PHASE 4 - Harmonisation des structures entre Database et Manager.
//
// Problème résolu:
//   Avant: MidiFileEntry (database) ≠ MidiFileInfo (manager)
//   Après: Structure unique MidiFileEntry utilisée partout
//
// Modifications (v3.0.3):
//   ✅ Unification MidiFileEntry/MidiFileInfo
//   ✅ Harmonisation nommage timestamps
//   ✅ Cohérence types et champs
//
// Auteur: MidiMind Team
// Date: 2025-10-09
// Statut: ✅ COMPLET - Structures unifiées prêtes
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <ctime>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURE UNIFIÉE FICHIER MIDI
// ============================================================================

/**
 * @struct MidiFileEntry
 * @brief Structure unifiée pour représenter un fichier MIDI
 * 
 * Utilisée par:
 * - MidiFileDatabase (persistance SQLite)
 * - MidiFileManager (gestion fichiers)
 * - API (réponses JSON)
 * 
 * Nommage harmonisé:
 * - Tous les timestamps en suffixe "Timestamp" (std::time_t)
 * - Tous les compteurs en suffixe "Count" (uint32_t)
 * - Tous les chemins en suffixe "Path" (std::string)
 */
struct MidiFileEntry {
    // ========================================================================
    // IDENTIFICATION
    // ========================================================================
    
    std::string id;              ///< ID unique (UUID format: "file_...")
    std::string filename;        ///< Nom du fichier (ex: "beethoven.mid")
    std::string filepath;        ///< Chemin absolu complet
    std::string relativePath;    ///< Chemin relatif depuis rootDir
    
    // ========================================================================
    // MÉTADONNÉES MIDI
    // ========================================================================
    
    uint32_t durationMs;         ///< Durée en millisecondes
    uint16_t trackCount;         ///< Nombre de pistes MIDI
    uint16_t format;             ///< Format MIDI (0, 1 ou 2) - NOUVEAU
    uint16_t ticksPerQuarterNote; ///< Division (PPQN) - NOUVEAU
    
    std::string title;           ///< Titre extrait (meta-event 0x03)
    std::string composer;        ///< Compositeur (meta-event 0x02)
    double tempo;                ///< Tempo en BPM (défaut: 120.0) - NOUVEAU
    std::string timeSignature;   ///< Signature rythmique (ex: "4/4") - NOUVEAU
    
    std::vector<std::string> tags; ///< Tags utilisateur
    
    // ========================================================================
    // INFORMATIONS FICHIER
    // ========================================================================
    
    uint64_t fileSizeBytes;      ///< Taille du fichier en bytes
    std::time_t lastModifiedTimestamp; ///< Date modification fichier (renommé)
    std::time_t addedTimestamp;  ///< Date ajout en BDD (renommé)
    
    // ========================================================================
    // STATISTIQUES USAGE
    // ========================================================================
    
    uint32_t playCount;          ///< Nombre de lectures
    std::time_t lastPlayedTimestamp; ///< Date dernière lecture (renommé)
    uint8_t rating;              ///< Note 0-5 étoiles (0 = non noté) - NOUVEAU
    
    // ========================================================================
    // CONSTRUCTEUR
    // ========================================================================
    
    /**
     * @brief Constructeur par défaut avec valeurs initiales
     */
    MidiFileEntry()
        : durationMs(0)
        , trackCount(0)
        , format(1)
        , ticksPerQuarterNote(480)
        , tempo(120.0)
        , timeSignature("4/4")
        , fileSizeBytes(0)
        , lastModifiedTimestamp(0)
        , addedTimestamp(0)
        , playCount(0)
        , lastPlayedTimestamp(0)
        , rating(0) {}
    
    // ========================================================================
    // CONVERSION JSON
    // ========================================================================
    
    /**
     * @brief Convertit l'entrée en JSON
     * 
     * @return json Représentation JSON complète
     * 
     * Format de sortie:
     * ```json
     * {
     *   "id": "file_...",
     *   "filename": "beethoven.mid",
     *   "filepath": "/path/to/file.mid",
     *   "relative_path": "classical/beethoven.mid",
     *   "duration_ms": 180000,
     *   "duration_formatted": "3:00",
     *   "track_count": 4,
     *   "format": 1,
     *   "division": 480,
     *   "title": "Moonlight Sonata",
     *   "composer": "Ludwig van Beethoven",
     *   "tempo": 120.0,
     *   "time_signature": "4/4",
     *   "tags": ["classical", "piano"],
     *   "file_size_bytes": 52428,
     *   "file_size_mb": 0.05,
     *   "last_modified": 1696857600,
     *   "added_date": 1696857600,
     *   "play_count": 25,
     *   "last_played": 1696857600,
     *   "rating": 5
     * }
     * ```
     */
    json toJson() const {
        json j;
        
        // Identification
        j["id"] = id;
        j["filename"] = filename;
        j["filepath"] = filepath;
        j["relative_path"] = relativePath;
        
        // Métadonnées MIDI
        j["duration_ms"] = durationMs;
        j["duration_formatted"] = formatDuration(durationMs);
        j["track_count"] = trackCount;
        j["format"] = format;
        j["division"] = ticksPerQuarterNote;
        j["title"] = title;
        j["composer"] = composer;
        j["tempo"] = tempo;
        j["time_signature"] = timeSignature;
        j["tags"] = tags;
        
        // Informations fichier
        j["file_size_bytes"] = fileSizeBytes;
        j["file_size_mb"] = fileSizeBytes / (1024.0 * 1024.0);
        j["last_modified"] = lastModifiedTimestamp;
        j["added_date"] = addedTimestamp;
        
        // Statistiques
        j["play_count"] = playCount;
        j["last_played"] = lastPlayedTimestamp;
        j["rating"] = rating;
        
        return j;
    }
    
    /**
     * @brief Crée une entrée depuis JSON
     * 
     * @param j Objet JSON
     * @return MidiFileEntry Entrée créée
     */
    static MidiFileEntry fromJson(const json& j) {
        MidiFileEntry entry;
        
        if (j.contains("id")) entry.id = j["id"];
        if (j.contains("filename")) entry.filename = j["filename"];
        if (j.contains("filepath")) entry.filepath = j["filepath"];
        if (j.contains("relative_path")) entry.relativePath = j["relative_path"];
        
        if (j.contains("duration_ms")) entry.durationMs = j["duration_ms"];
        if (j.contains("track_count")) entry.trackCount = j["track_count"];
        if (j.contains("format")) entry.format = j["format"];
        if (j.contains("division")) entry.ticksPerQuarterNote = j["division"];
        if (j.contains("title")) entry.title = j["title"];
        if (j.contains("composer")) entry.composer = j["composer"];
        if (j.contains("tempo")) entry.tempo = j["tempo"];
        if (j.contains("time_signature")) entry.timeSignature = j["time_signature"];
        
        if (j.contains("tags") && j["tags"].is_array()) {
            entry.tags = j["tags"].get<std::vector<std::string>>();
        }
        
        if (j.contains("file_size_bytes")) entry.fileSizeBytes = j["file_size_bytes"];
        if (j.contains("last_modified")) entry.lastModifiedTimestamp = j["last_modified"];
        if (j.contains("added_date")) entry.addedTimestamp = j["added_date"];
        if (j.contains("play_count")) entry.playCount = j["play_count"];
        if (j.contains("last_played")) entry.lastPlayedTimestamp = j["last_played"];
        if (j.contains("rating")) entry.rating = j["rating"];
        
        return entry;
    }
    
private:
    /**
     * @brief Formate une durée en format MM:SS
     */
    static std::string formatDuration(uint32_t durationMs) {
        uint32_t totalSeconds = durationMs / 1000;
        uint32_t minutes = totalSeconds / 60;
        uint32_t seconds = totalSeconds % 60;
        
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%u:%02u", minutes, seconds);
        return std::string(buffer);
    }
};

// ============================================================================
// ALIAS POUR COMPATIBILITÉ (À SUPPRIMER PROGRESSIVEMENT)
// ============================================================================

/**
 * @typedef MidiFileInfo
 * @brief Alias pour compatibilité avec ancien code
 * @deprecated Utiliser MidiFileEntry directement
 */
using MidiFileInfo = MidiFileEntry;

// ============================================================================
// STRUCTURE PLAYLIST (DÉJÀ BIEN DÉFINIE)
// ============================================================================

/**
 * @struct Playlist
 * @brief Structure de playlist de fichiers MIDI
 * 
 * @note Structure déjà harmonisée dans PlaylistManager.h
 */
struct Playlist {
    std::string id;                      ///< ID unique (UUID)
    std::string name;                    ///< Nom de la playlist
    std::string description;             ///< Description
    std::vector<std::string> fileIds;    ///< Liste des IDs de fichiers
    std::time_t createdTimestamp;        ///< Date de création (renommé)
    std::time_t modifiedTimestamp;       ///< Date de modification (renommé)
    
    /**
     * @brief Constructeur par défaut
     */
    Playlist()
        : createdTimestamp(0)
        , modifiedTimestamp(0) {}
    
    /**
     * @brief Conversion en JSON
     */
    json toJson() const {
        json j;
        j["id"] = id;
        j["name"] = name;
        j["description"] = description;
        j["file_ids"] = fileIds;
        j["file_count"] = fileIds.size();
        j["created_date"] = createdTimestamp;
        j["modified_date"] = modifiedTimestamp;
        return j;
    }
};

// ============================================================================
// GUIDE DE MIGRATION
// ============================================================================

/*
 * GUIDE DE MIGRATION POUR PHASE 4
 * ================================
 * 
 * Remplacements à faire dans le code existant:
 * 
 * 1. NOMS DE CHAMPS (MidiFileEntry):
 *    lastModified      → lastModifiedTimestamp
 *    addedDate         → addedTimestamp
 *    lastPlayed        → lastPlayedTimestamp
 * 
 * 2. NOMS DE CHAMPS (Playlist):
 *    createdDate       → createdTimestamp
 *    modifiedDate      → modifiedTimestamp
 * 
 * 3. TYPES:
 *    MidiFileInfo      → MidiFileEntry (alias existe pour transition)
 * 
 * 4. NOUVEAUX CHAMPS AJOUTÉS:
 *    format            : uint16_t (0, 1 ou 2)
 *    ticksPerQuarterNote : uint16_t (division MIDI)
 *    tempo             : double (BPM)
 *    timeSignature     : std::string (ex: "4/4")
 *    rating            : uint8_t (0-5)
 * 
 * 5. SCHÉMA BDD À METTRE À JOUR:
 *    ALTER TABLE midi_files ADD COLUMN format INTEGER DEFAULT 1;
 *    ALTER TABLE midi_files ADD COLUMN division INTEGER DEFAULT 480;
 *    ALTER TABLE midi_files ADD COLUMN tempo REAL DEFAULT 120.0;
 *    ALTER TABLE midi_files ADD COLUMN time_signature TEXT DEFAULT '4/4';
 *    ALTER TABLE midi_files ADD COLUMN rating INTEGER DEFAULT 0;
 * 
 * 6. INCLUDES NÉCESSAIRES:
 *    #include "MidiFileStructures.h"  // Au lieu de définitions locales
 */

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiFileStructures.h
// Version: 3.0.3 - STRUCTURES UNIFIÉES ✅
// 
// Changements Phase 4:
// - ✅ Structure unique MidiFileEntry
// - ✅ Nommage harmonisé (Timestamp, Count, Path)
// - ✅ Nouveaux champs ajoutés (format, tempo, rating)
// - ✅ Méthodes toJson() / fromJson() complètes
// - ✅ Guide de migration inclus
// 
// PRÊT POUR INTÉGRATION
// ============================================================================
