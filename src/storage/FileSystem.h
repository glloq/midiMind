// ============================================================================
// Fichier: src/storage/FileSystem.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Utilitaires pour manipuler le système de fichiers.
//   Wrappers C++ modernes pour les opérations filesystem.
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "../core/Logger.h"
#include "../core/StringUtils.h"

namespace midiMind {
namespace FileSystem {

/**
 * @brief Vérifie si un fichier existe
 */
inline bool exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

/**
 * @brief Vérifie si c'est un fichier
 */
inline bool isFile(const std::string& path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) return false;
    return S_ISREG(buffer.st_mode);
}

/**
 * @brief Vérifie si c'est un dossier
 */
inline bool isDirectory(const std::string& path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) return false;
    return S_ISDIR(buffer.st_mode);
}

/**
 * @brief Crée un dossier
 * 
 * @param path Chemin du dossier
 * @param recursive true pour créer les parents
 * @return true Si succès
 */
inline bool createDirectory(const std::string& path, bool recursive = false) {
    if (exists(path)) {
        return isDirectory(path);
    }
    
    if (recursive) {
        // Créer les parents
        size_t pos = 0;
        while ((pos = path.find('/', pos + 1)) != std::string::npos) {
            std::string parent = path.substr(0, pos);
            if (!exists(parent)) {
                mkdir(parent.c_str(), 0755);
            }
        }
    }
    
    return mkdir(path.c_str(), 0755) == 0;
}

/**
 * @brief Supprime un fichier
 */
inline bool removeFile(const std::string& path) {
    return unlink(path.c_str()) == 0;
}

/**
 * @brief Supprime un dossier (vide)
 */
inline bool removeDirectory(const std::string& path) {
    return rmdir(path.c_str()) == 0;
}

/**
 * @brief Liste les fichiers d'un dossier
 * 
 * @param path Chemin du dossier
 * @param extension Filtrer par extension (optionnel, ex: ".mid")
 * @return std::vector<std::string> Liste des fichiers
 */
inline std::vector<std::string> listFiles(const std::string& path, 
                                          const std::string& extension = "") {
    std::vector<std::string> files;
    
    DIR* dir = opendir(path.c_str());
    if (!dir) return files;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        
        // Ignorer . et ..
        if (name == "." || name == "..") continue;
        
        std::string fullPath = path + "/" + name;
        
        // Vérifier si c'est un fichier
        if (!isFile(fullPath)) continue;
        
        // Filtrer par extension
        if (!extension.empty() && !StringUtils::endsWith(name, extension)) {
            continue;
        }
        
        files.push_back(fullPath);
    }
    
    closedir(dir);
    
    return files;
}

/**
 * @brief Récupère la taille d'un fichier
 * 
 * @param path Chemin du fichier
 * @return size_t Taille en bytes (0 si erreur)
 */
inline size_t getFileSize(const std::string& path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) return 0;
    return buffer.st_size;
}

/**
 * @brief Lit un fichier texte complet
 * 
 * @param path Chemin du fichier
 * @return std::string Contenu du fichier
 */
inline std::string readTextFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

/**
 * @brief Écrit un fichier texte
 * 
 * @param path Chemin du fichier
 * @param content Contenu
 * @return true Si succès
 */
inline bool writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    
    file << content;
    file.close();
    return true;
}

/**
 * @brief Copie un fichier
 * 
 * @param source Fichier source
 * @param destination Fichier destination
 * @return true Si succès
 */
inline bool copyFile(const std::string& source, const std::string& destination) {
    std::ifstream src(source, std::ios::binary);
    if (!src.is_open()) return false;
    
    std::ofstream dst(destination, std::ios::binary);
    if (!dst.is_open()) return false;
    
    dst << src.rdbuf();
    
    src.close();
    dst.close();
    
    return true;
}

/**
 * @brief Déplace un fichier
 * 
 * @param source Fichier source
 * @param destination Fichier destination
 * @return true Si succès
 */
inline bool moveFile(const std::string& source, const std::string& destination) {
    return rename(source.c_str(), destination.c_str()) == 0;
}

/**
 * @brief Récupère l'extension d'un fichier
 * 
 * @param path Chemin du fichier
 * @return std::string Extension (avec le point, ex: ".mid")
 */
inline std::string getExtension(const std::string& path) {
    size_t pos = path.find_last_of('.');
    if (pos == std::string::npos) return "";
    return path.substr(pos);
}

/**
 * @brief Récupère le nom de fichier sans chemin
 * 
 * @param path Chemin complet
 * @return std::string Nom du fichier
 */
inline std::string getFileName(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

/**
 * @brief Récupère le dossier parent
 * 
 * @param path Chemin complet
 * @return std::string Dossier parent
 */
inline std::string getParentDirectory(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

/**
 * @brief Joint des chemins
 * 
 * @param parts Parties du chemin
 * @return std::string Chemin complet
 * 
 * @example
 * ```cpp
 * auto path = FileSystem::joinPath({"/home", "user", "file.txt"});
 * // "/home/user/file.txt"
 * ```
 */
inline std::string joinPath(const std::vector<std::string>& parts) {
    if (parts.empty()) return "";
    
    std::string result = parts[0];
    
    for (size_t i = 1; i < parts.size(); ++i) {
        if (!result.empty() && result.back() != '/') {
            result += '/';
        }
        result += parts[i];
    }
    
    return result;
}

} // namespace FileSystem
} // namespace midiMind

// ============================================================================
// FIN DU FICHIER FileSystem.h
// ============================================================================