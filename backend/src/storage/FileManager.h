// ============================================================================
// Fichier: backend/src/storage/FileManager.h
// Version: 3.0.0
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestionnaire de fichiers applicatifs (logs, backups, exports, temp).
//   Distinct de MidiFileManager qui gère les fichiers MIDI de la bibliothèque.
//
// Fonctionnalités:
//   - Gestion structure de répertoires
//   - Upload/Download fichiers
//   - Opérations fichiers (copy, move, delete)
//   - Nettoyage automatique (vieux logs, backups)
//   - Validation chemins (sécurité path traversal)
//   - Thread-safe
//
// Dépendances:
//   - FileSystem.h (utilitaires filesystem)
//   - Logger.h (logging)
//
// Auteur: MidiMind Team
// Date: 2025-10-13
// Statut: ✅ COMPLET
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <nlohmann/json.hpp>

#include "../core/FileSystem.h"
#include "../core/Logger.h"
#include "../core/Error.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct FileInfo
 * @brief Informations sur un fichier applicatif
 */
struct FileInfo {
    std::string path;           ///< Chemin absolu complet
    std::string name;           ///< Nom du fichier
    std::string extension;      ///< Extension (avec le point)
    uint64_t size;              ///< Taille en bytes
    std::time_t modified;       ///< Date de modification (timestamp)
    std::string directory;      ///< Répertoire parent (logs/backups/exports/etc)
    
    /**
     * @brief Constructeur par défaut
     */
    FileInfo()
        : size(0)
        , modified(0) {}
    
    /**
     * @brief Conversion en JSON
     */
    json toJson() const {
        json j;
        j["path"] = path;
        j["name"] = name;
        j["extension"] = extension;
        j["size"] = size;
        j["modified"] = modified;
        j["directory"] = directory;
        return j;
    }
};

/**
 * @enum DirectoryType
 * @brief Types de répertoires gérés
 */
enum class DirectoryType {
    LOGS,       ///< Fichiers de logs
    BACKUPS,    ///< Backups de base de données
    EXPORTS,    ///< Exports MIDI/JSON
    UPLOADS,    ///< Fichiers uploadés
    TEMP        ///< Fichiers temporaires
};

// ============================================================================
// CLASSE: FileManager
// ============================================================================

/**
 * @class FileManager
 * @brief Gestionnaire de fichiers applicatifs
 * 
 * @details
 * Gère les fichiers système de l'application (logs, backups, exports, temp).
 * Distinct de MidiFileManager qui gère les fichiers MIDI.
 * 
 * Structure des répertoires:
 * @code
 * rootPath/
 *   ├── logs/         (fichiers de logs)
 *   ├── backups/      (backups BDD)
 *   ├── exports/      (exports MIDI/JSON)
 *   ├── uploads/      (fichiers uploadés)
 *   └── temp/         (fichiers temporaires)
 * @endcode
 * 
 * Sécurité:
 * - Validation path traversal (.. interdit)
 * - Vérification extension fichiers
 * - Limitation taille uploads
 * 
 * @example Utilisation
 * @code
 * FileManager fm("/home/pi/midimind");
 * fm.initializeDirectories();
 * 
 * // Lister les logs
 * auto logs = fm.listFiles(DirectoryType::LOGS);
 * 
 * // Nettoyer vieux logs
 * int deleted = fm.cleanOldLogs(30); // 30 jours
 * 
 * // Upload un fichier
 * std::vector<uint8_t> data = {...};
 * auto path = fm.uploadFile(data, "config.json", DirectoryType::UPLOADS);
 * @endcode
 */
class FileManager {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param rootPath Chemin racine pour tous les répertoires
     * 
     * @throws MidiMindException Si rootPath invalide
     * 
     * @note Le répertoire racine doit exister et être accessible
     */
    explicit FileManager(const std::string& rootPath);
    
    /**
     * @brief Destructeur
     */
    ~FileManager();
    
    // Désactiver copie
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    /**
     * @brief Crée la structure de répertoires
     * 
     * Crée tous les sous-répertoires nécessaires :
     * - logs/
     * - backups/
     * - exports/
     * - uploads/
     * - temp/
     * 
     * @return bool True si succès
     * 
     * @note Thread-safe
     * @note Ne génère pas d'erreur si les répertoires existent déjà
     */
    bool initializeDirectories();
    
    // ========================================================================
    // LISTAGE FICHIERS
    // ========================================================================
    
    /**
     * @brief Liste les fichiers d'un répertoire
     * 
     * @param dirType Type de répertoire
     * @param extension Filtrer par extension (optionnel, ex: ".log")
     * @return std::vector<FileInfo> Liste des fichiers
     * 
     * @note Thread-safe
     * @note Triés par date de modification (plus récent en premier)
     */
    std::vector<FileInfo> listFiles(DirectoryType dirType, 
                                    const std::string& extension = "");
    
    /**
     * @brief Obtient le chemin complet d'un répertoire
     * 
     * @param dirType Type de répertoire
     * @return std::string Chemin complet absolu
     * 
     * @example
     * @code
     * auto logsDir = fm.getDirectoryPath(DirectoryType::LOGS);
     * // Retourne: "/home/pi/midimind/logs"
     * @endcode
     */
    std::string getDirectoryPath(DirectoryType dirType) const;
    
    /**
     * @brief Obtient les infos d'un fichier
     * 
     * @param filepath Chemin complet du fichier
     * @return std::optional<FileInfo> Infos ou nullopt si erreur
     * 
     * @note Thread-safe
     */
    std::optional<FileInfo> getFileInfo(const std::string& filepath);
    
    // ========================================================================
    // UPLOAD/DOWNLOAD
    // ========================================================================
    
    /**
     * @brief Upload un fichier
     * 
     * @param data Données binaires
     * @param filename Nom du fichier
     * @param destDir Répertoire destination
     * @param overwrite Écraser si existe (défaut: false)
     * @return std::string Chemin du fichier créé
     * 
     * @throws MidiMindException Si erreur ou fichier existe (et !overwrite)
     * 
     * @note Thread-safe
     * @note Le nom de fichier est automatiquement sécurisé
     * @note Limite de taille: 100MB par défaut
     * 
     * @example
     * @code
     * std::vector<uint8_t> data = {0x01, 0x02, 0x03};
     * auto path = fm.uploadFile(data, "config.json", DirectoryType::UPLOADS);
     * @endcode
     */
    std::string uploadFile(const std::vector<uint8_t>& data,
                          const std::string& filename,
                          DirectoryType destDir,
                          bool overwrite = false);
    
    /**
     * @brief Download un fichier
     * 
     * @param filepath Chemin complet du fichier
     * @return std::vector<uint8_t> Données binaires
     * 
     * @throws MidiMindException Si fichier non trouvé ou erreur lecture
     * 
     * @note Thread-safe
     * @note Valide le chemin (pas d'accès hors rootPath)
     */
    std::vector<uint8_t> downloadFile(const std::string& filepath);
    
    // ========================================================================
    // OPÉRATIONS FICHIERS
    // ========================================================================
    
    /**
     * @brief Copie un fichier
     * 
     * @param source Chemin source
     * @param destination Chemin destination
     * @param overwrite Écraser si existe (défaut: false)
     * @return bool True si succès
     * 
     * @note Thread-safe
     * @note Valide les chemins (sécurité)
     */
    bool copyFile(const std::string& source, 
                  const std::string& destination,
                  bool overwrite = false);
    
    /**
     * @brief Déplace un fichier
     * 
     * @param source Chemin source
     * @param destination Chemin destination
     * @return bool True si succès
     * 
     * @note Thread-safe
     * @note Plus rapide que copy + delete
     */
    bool moveFile(const std::string& source, const std::string& destination);
    
    /**
     * @brief Supprime un fichier
     * 
     * @param filepath Chemin du fichier
     * @return bool True si succès
     * 
     * @note Thread-safe
     * @note Ne génère pas d'erreur si le fichier n'existe pas
     */
    bool deleteFile(const std::string& filepath);
    
    /**
     * @brief Vide un répertoire
     * 
     * @param dirType Type de répertoire
     * @return int Nombre de fichiers supprimés
     * 
     * @note Thread-safe
     * @warning Opération irréversible !
     */
    int clearDirectory(DirectoryType dirType);
    
    // ========================================================================
    // NETTOYAGE AUTOMATIQUE
    // ========================================================================
    
    /**
     * @brief Nettoie les vieux logs
     * 
     * Supprime les fichiers .log plus vieux que maxAgeDays.
     * 
     * @param maxAgeDays Âge maximum en jours (défaut: 30)
     * @return int Nombre de fichiers supprimés
     * 
     * @note Thread-safe
     * @note Garde toujours les 5 logs les plus récents
     */
    int cleanOldLogs(int maxAgeDays = 30);
    
    /**
     * @brief Nettoie les vieux backups
     * 
     * Garde seulement les N backups les plus récents.
     * 
     * @param maxCount Nombre maximum de backups à garder (défaut: 10)
     * @return int Nombre de fichiers supprimés
     * 
     * @note Thread-safe
     * @note Trie par date, garde les plus récents
     */
    int cleanOldBackups(int maxCount = 10);
    
    /**
     * @brief Vide le répertoire temp
     * 
     * Supprime tous les fichiers temporaires.
     * 
     * @return int Nombre de fichiers supprimés
     * 
     * @note Thread-safe
     * @note Appeler régulièrement pour libérer l'espace disque
     */
    int cleanTemp();
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Taille totale d'un répertoire
     * 
     * @param dirType Type de répertoire
     * @return uint64_t Taille en bytes
     * 
     * @note Thread-safe
     */
    uint64_t getDirectorySize(DirectoryType dirType);
    
    /**
     * @brief Nombre de fichiers dans un répertoire
     * 
     * @param dirType Type de répertoire
     * @return int Nombre de fichiers
     * 
     * @note Thread-safe
     */
    int getFileCount(DirectoryType dirType);
    
    /**
     * @brief Statistiques complètes
     * 
     * @return json Stats JSON
     * 
     * Format:
     * @code{.json}
     * {
     *   "root_path": "/home/pi/midimind",
     *   "logs": {"files": 15, "size": 2048000},
     *   "backups": {"files": 5, "size": 10240000},
     *   "exports": {"files": 3, "size": 512000},
     *   "uploads": {"files": 0, "size": 0},
     *   "temp": {"files": 2, "size": 1024},
     *   "total_size": 12800024
     * }
     * @endcode
     */
    json getStatistics();

private:
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Chemin racine
    std::string rootPath_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Taille max upload (100MB)
    static constexpr uint64_t MAX_UPLOAD_SIZE = 100 * 1024 * 1024;
    
    /// Noms des sous-répertoires
    const std::string DIR_LOGS = "logs";
    const std::string DIR_BACKUPS = "backups";
    const std::string DIR_EXPORTS = "exports";
    const std::string DIR_UPLOADS = "uploads";
    const std::string DIR_TEMP = "temp";
    
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Valide un chemin (sécurité path traversal)
     * 
     * @param path Chemin à valider
     * @return bool True si valide
     * 
     * Vérifie:
     * - Pas de ".." dans le chemin
     * - Le chemin est sous rootPath_
     * - Pas de caractères dangereux
     */
    bool validatePath(const std::string& path) const;
    
    /**
     * @brief Sécurise un nom de fichier
     * 
     * @param filename Nom à sécuriser
     * @return std::string Nom sécurisé
     * 
     * Remplace les caractères dangereux par '_'
     */
    std::string sanitizeFilename(const std::string& filename) const;
    
    /**
     * @brief Convertit DirectoryType en nom de dossier
     * 
     * @param dirType Type
     * @return std::string Nom du dossier
     */
    std::string directoryTypeToString(DirectoryType dirType) const;
    
    /**
     * @brief Parse un fichier en FileInfo
     * 
     * @param filepath Chemin du fichier
     * @param dirType Type de répertoire
     * @return FileInfo Infos parsées
     */
    FileInfo parseFileInfo(const std::string& filepath, DirectoryType dirType);
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER FileManager.h
// ============================================================================
