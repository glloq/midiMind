// ============================================================================
// Fichier: backend/src/storage/FileManager.h
// Version: 3.0.4 - HYBRIDE (Sécurité Renforcée + Cohérence Projet)
// Date: 2025-10-15
// ============================================================================
// DESCRIPTION:
//   Gestionnaire de fichiers applicatifs avec sécurité maximale
//   et cohérence avec l'architecture du projet
//
// FONCTIONNALITÉS v3.0.4:
//   ✅ Gestion hiérarchique des fichiers (logs, backups, exports, uploads, temp)
//   ✅ Protection path traversal avancée (validation canonique)
//   ✅ Sanitization complète des noms de fichiers
//   ✅ Upload/Download sécurisés
//   ✅ Opérations fichiers (copy, move, delete)
//   ✅ Nettoyage automatique (logs anciens, backups)
//   ✅ Statistiques d'utilisation
//   ✅ Thread-safe (mutex interne)
//
// SÉCURITÉ:
//   - Validation stricte des chemins (pas de .., chemins absolus, etc.)
//   - Résolution canonique pour détecter symlinks malveillants
//   - Sanitization des noms (caractères interdits, noms réservés Windows)
//   - Limite de taille d'upload (100 MB par défaut)
//   - Tous les chemins relatifs à rootPath_
//
// Thread-safety: OUI (mutex interne sur toutes les opérations)
//
// Auteur: MidiMind Team
// ============================================================================

#ifndef MIDIMIND_FILE_MANAGER_H
#define MIDIMIND_FILE_MANAGER_H

#include "../core/Error.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>
#include <ctime>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// ÉNUMÉRATIONS
// ============================================================================

/**
 * @brief Types de répertoires gérés par FileManager
 */
enum class DirectoryType {
    LOGS,       ///< Fichiers de log applicatifs
    BACKUPS,    ///< Sauvegardes de données
    EXPORTS,    ///< Fichiers exportés par l'utilisateur
    UPLOADS,    ///< Fichiers uploadés par l'utilisateur
    TEMP        ///< Fichiers temporaires
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @brief Informations sur un fichier
 */
struct FileInfo {
    std::string path;           ///< Chemin complet du fichier
    std::string name;           ///< Nom du fichier (avec extension)
    std::string extension;      ///< Extension (.txt, .mid, etc.)
    uint64_t size;              ///< Taille en octets
    std::time_t modified;       ///< Date de dernière modification (timestamp)
    std::string directory;      ///< Type de répertoire (logs, backups, etc.)
    
    FileInfo() : size(0), modified(0) {}
};

// ============================================================================
// CLASSE FILEMANAGER
// ============================================================================

/**
 * @class FileManager
 * @brief Gestionnaire centralisé des fichiers applicatifs
 * 
 * Gère l'organisation hiérarchique des fichiers avec sécurité renforcée :
 * - Logs : fichiers de journalisation
 * - Backups : sauvegardes automatiques
 * - Exports : données exportées
 * - Uploads : fichiers utilisateur
 * - Temp : fichiers temporaires
 * 
 * SÉCURITÉ:
 * - Tous les chemins sont validés (pas de path traversal)
 * - Résolution canonique des chemins (détection symlinks)
 * - Sanitization des noms de fichiers
 * - Limite de taille d'upload
 * 
 * Thread-safety: Toutes les méthodes sont thread-safe (mutex interne)
 * 
 * @example
 * ```cpp
 * FileManager fm("./data");
 * fm.initializeDirectories();
 * 
 * // Upload sécurisé
 * std::vector<uint8_t> data = {...};
 * std::string path = fm.uploadFile(data, "song.mid", DirectoryType::UPLOADS);
 * 
 * // Liste des fichiers
 * auto files = fm.listFiles(DirectoryType::LOGS, ".log");
 * 
 * // Nettoyage automatique
 * fm.cleanOldLogs(30);  // Supprimer logs > 30 jours
 * ```
 */
class FileManager {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * @param rootPath Chemin racine pour tous les fichiers
     * 
     * Le chemin est automatiquement résolu en chemin canonique
     * pour éviter les problèmes de symlinks.
     */
    explicit FileManager(const std::string& rootPath);
    
    /**
     * @brief Destructeur
     */
    ~FileManager();
    
    // Pas de copie
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    /**
     * @brief Initialise la structure des répertoires
     * @return true si tous les répertoires ont été créés avec succès
     * 
     * Crée les répertoires suivants s'ils n'existent pas :
     * - logs/
     * - backups/
     * - exports/
     * - uploads/
     * - temp/
     */
    bool initializeDirectories();
    
    // ========================================================================
    // LISTAGE FICHIERS
    // ========================================================================
    
    /**
     * @brief Liste les fichiers d'un répertoire
     * @param dirType Type de répertoire à lister
     * @param extension Extension à filtrer (ex: ".log"), "" pour tous
     * @return Liste des fichiers triés par date (plus récent en premier)
     * 
     * @note Thread-safe
     */
    std::vector<FileInfo> listFiles(DirectoryType dirType, 
                                     const std::string& extension = "");
    
    /**
     * @brief Obtient les informations d'un fichier
     * @param filepath Chemin du fichier (relatif à rootPath_)
     * @return Informations du fichier
     * @throws Error si le fichier n'existe pas ou le chemin est invalide
     * 
     * @note Thread-safe
     * @note Le chemin est validé pour la sécurité
     */
    FileInfo getFileInfo(const std::string& filepath);
    
    // ========================================================================
    // UPLOAD / DOWNLOAD
    // ========================================================================
    
    /**
     * @brief Upload un fichier
     * @param data Données binaires du fichier
     * @param filename Nom du fichier (sera sécurisé automatiquement)
     * @param destDir Répertoire de destination
     * @param overwrite true pour écraser si existe
     * @return Chemin complet du fichier créé
     * @throws Error si échec (taille trop grande, nom invalide, etc.)
     * 
     * SÉCURITÉ:
     * - Taille limitée à MAX_UPLOAD_SIZE (100 MB)
     * - Nom de fichier automatiquement sécurisé
     * - Chemin validé contre path traversal
     * 
     * @note Thread-safe
     */
    std::string uploadFile(const std::vector<uint8_t>& data,
                          const std::string& filename,
                          DirectoryType destDir = DirectoryType::UPLOADS,
                          bool overwrite = false);
    
    /**
     * @brief Télécharge un fichier
     * @param filepath Chemin du fichier (relatif à rootPath_)
     * @return Données binaires du fichier
     * @throws Error si le fichier n'existe pas ou le chemin est invalide
     * 
     * @note Thread-safe
     * @note Le chemin est validé pour la sécurité
     */
    std::vector<uint8_t> downloadFile(const std::string& filepath);
    
    // ========================================================================
    // OPÉRATIONS FICHIERS
    // ========================================================================
    
    /**
     * @brief Copie un fichier
     * @param source Chemin source (relatif à rootPath_)
     * @param destination Chemin destination (relatif à rootPath_)
     * @param overwrite true pour écraser si existe
     * @return true si succès
     * 
     * @note Thread-safe
     * @note Les chemins sont validés pour la sécurité
     */
    bool copyFile(const std::string& source, 
                  const std::string& destination,
                  bool overwrite = false);
    
    /**
     * @brief Déplace un fichier
     * @param source Chemin source (relatif à rootPath_)
     * @param destination Chemin destination (relatif à rootPath_)
     * @return true si succès
     * 
     * @note Thread-safe
     * @note Les chemins sont validés pour la sécurité
     */
    bool moveFile(const std::string& source, 
                  const std::string& destination);
    
    /**
     * @brief Supprime un fichier
     * @param filepath Chemin du fichier (relatif à rootPath_)
     * @return true si succès ou si le fichier n'existait pas
     * 
     * @note Thread-safe
     * @note Le chemin est validé pour la sécurité
     */
    bool deleteFile(const std::string& filepath);
    
    /**
     * @brief Vide un répertoire (supprime tous les fichiers)
     * @param dirType Type de répertoire à vider
     * @return Nombre de fichiers supprimés
     * 
     * @note Thread-safe
     * @warning Cette opération est irréversible !
     */
    int clearDirectory(DirectoryType dirType);
    
    // ========================================================================
    // NETTOYAGE AUTOMATIQUE
    // ========================================================================
    
    /**
     * @brief Nettoie les logs anciens
     * @param maxAgeDays Age maximum en jours (défaut: 30)
     * @return Nombre de fichiers supprimés
     * 
     * Garde toujours au moins les 5 logs les plus récents, même s'ils
     * sont plus vieux que maxAgeDays.
     * 
     * @note Thread-safe
     */
    int cleanOldLogs(int maxAgeDays = 30);
    
    /**
     * @brief Nettoie les anciennes sauvegardes
     * @param maxCount Nombre maximum de sauvegardes à conserver (défaut: 10)
     * @return Nombre de fichiers supprimés
     * 
     * Garde les maxCount sauvegardes les plus récentes.
     * 
     * @note Thread-safe
     */
    int cleanOldBackups(int maxCount = 10);
    
    /**
     * @brief Vide le répertoire temporaire
     * @return Nombre de fichiers supprimés
     * 
     * @note Thread-safe
     */
    int clearTemp();
    
    // ========================================================================
    // STATISTIQUES (v3.0.4 - BONUS)
    // ========================================================================
    
    /**
     * @brief Calcule la taille totale d'un répertoire
     * @param dirType Type de répertoire
     * @return Taille en octets
     * 
     * @note Thread-safe
     */
    uint64_t getDirectorySize(DirectoryType dirType);
    
    /**
     * @brief Compte les fichiers dans un répertoire
     * @param dirType Type de répertoire
     * @return Nombre de fichiers
     * 
     * @note Thread-safe
     */
    int getFileCount(DirectoryType dirType);
    
    /**
     * @brief Obtient les statistiques globales
     * @return JSON avec les statistiques de tous les répertoires
     * 
     * Format:
     * ```json
     * {
     *   "root_path": "/path/to/data",
     *   "logs": {"files": 10, "size": 102400},
     *   "backups": {"files": 5, "size": 2048000},
     *   "exports": {"files": 3, "size": 512000},
     *   "uploads": {"files": 20, "size": 5120000},
     *   "temp": {"files": 0, "size": 0},
     *   "total_size": 7782400
     * }
     * ```
     * 
     * @note Thread-safe
     */
    json getStatistics();
    
    // ========================================================================
    // MÉTHODES DE SÉCURITÉ (v3.0.4)
    // ========================================================================
    
    /**
     * @brief Valide un chemin pour prévenir path traversal
     * @param relativePath Chemin relatif à valider
     * @return true si le chemin est sûr
     * 
     * Vérifie:
     * - Pas de ".." (path traversal)
     * - Pas de chemin absolu (/, \, ~, C:, etc.)
     * - Pas de null bytes
     * - Pas de chemins UNC (\\server\share)
     * - Résolution canonique (détecte symlinks malveillants)
     * 
     * @note Thread-safe (lecture seule de rootPath_)
     */
    bool validatePath(const std::string& relativePath) const;
    
    /**
     * @brief Nettoie un nom de fichier des caractères dangereux
     * @param filename Nom de fichier à nettoyer
     * @return Nom de fichier sécurisé
     * 
     * Effectue:
     * - Suppression des null bytes
     * - Remplacement des / et \ par _
     * - Suppression des caractères interdits (<>:"|?*, contrôles)
     * - Suppression des points au début/fin
     * - Vérification des noms réservés Windows (CON, PRN, etc.)
     * - Limitation à MAX_FILENAME_LENGTH (255 caractères)
     * 
     * @note Thread-safe (pas d'état partagé)
     */
    std::string sanitizeFilename(const std::string& filename) const;
    
    /**
     * @brief Vérifie si un chemin est sûr (wrapper pour validatePath)
     * @param path Chemin à vérifier (absolu ou relatif)
     * @return true si le chemin est sûr
     * 
     * Si le chemin est absolu et commence par rootPath_, le convertit
     * en chemin relatif avant validation.
     * 
     * @note Thread-safe (lecture seule de rootPath_)
     */
    bool isPathSafe(const std::string& path) const;

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Obtient le chemin complet d'un répertoire
     * @param dirType Type de répertoire
     * @return Chemin complet (rootPath_ + type)
     */
    std::string getDirectoryPath(DirectoryType dirType) const;
    
    /**
     * @brief Convertit DirectoryType en string
     * @param dirType Type de répertoire
     * @return Nom du répertoire ("logs", "backups", etc.)
     */
    std::string directoryTypeToString(DirectoryType dirType) const;
    
    /**
     * @brief Parse les informations d'un fichier
     * @param filepath Chemin complet du fichier
     * @param dirType Type de répertoire
     * @return Structure FileInfo remplie
     */
    FileInfo parseFileInfo(const std::string& filepath, 
                          DirectoryType dirType);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    std::string rootPath_;      ///< Chemin racine (canonique)
    mutable std::mutex mutex_;  ///< Mutex pour thread-safety
};

} // namespace midiMind

#endif // MIDIMIND_FILE_MANAGER_H
