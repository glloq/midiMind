// ============================================================================
// Fichier: backend/src/storage/FileSystem.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.1 - 2025-10-09 - COMPLET
// ============================================================================
// Description:
//   Utilitaires pour manipuler le système de fichiers.
//   Wrappers C++ modernes pour les opérations filesystem.
//   VERSION COMPLÈTE avec toutes les méthodes nécessaires.
//
// Modifications apportées (v3.0.1):
//   ✅ Ajout listDirectories() - Lister sous-dossiers
//   ✅ Ajout getFileName() - Extraire nom de fichier
//   ✅ Ajout getFileSize() - Obtenir taille fichier
//   ✅ Ajout getModificationTime() - Date dernière modification
//
// Auteur: MidiMind Team
// Date: 2025-10-09
// Statut: ✅ COMPLET - Prêt pour production
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <ctime>
#include <algorithm>

#include "../core/Logger.h"

namespace midiMind {
namespace FileSystem {

// ============================================================================
// VÉRIFICATIONS D'EXISTENCE
// ============================================================================

/**
 * @brief Vérifie si un fichier ou dossier existe
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

// ============================================================================
// CRÉATION ET SUPPRESSION
// ============================================================================

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

// ============================================================================
// LISTAGE DE FICHIERS
// ============================================================================

/**
 * @brief Liste les fichiers d'un dossier
 * 
 * @param path Chemin du dossier
 * @param extension Filtrer par extension (optionnel, ex: ".mid")
 * @return std::vector<std::string> Liste des chemins complets
 */
inline std::vector<std::string> listFiles(const std::string& path, 
                                          const std::string& extension = "") {
    std::vector<std::string> files;
    
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        Logger::warn("FileSystem", "Cannot open directory: " + path);
        return files;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        
        // Ignorer . et ..
        if (name == "." || name == "..") continue;
        
        std::string fullPath = path + "/" + name;
        
        // Vérifier que c'est un fichier
        if (!isFile(fullPath)) continue;
        
        // Filtrer par extension si spécifié
        if (!extension.empty()) {
            if (name.length() < extension.length()) continue;
            
            std::string fileExt = name.substr(name.length() - extension.length());
            std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(), ::tolower);
            
            std::string targetExt = extension;
            std::transform(targetExt.begin(), targetExt.end(), targetExt.begin(), ::tolower);
            
            if (fileExt != targetExt) continue;
        }
        
        files.push_back(fullPath);
    }
    
    closedir(dir);
    
    return files;
}

/**
 * @brief Liste les sous-dossiers d'un dossier
 * 
 * @param path Chemin du dossier parent
 * @return std::vector<std::string> Liste des chemins complets des sous-dossiers
 * 
 * @note ✅ NOUVEAU - Implémenté pour Phase 1
 */
inline std::vector<std::string> listDirectories(const std::string& path) {
    std::vector<std::string> directories;
    
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        Logger::warn("FileSystem", "Cannot open directory: " + path);
        return directories;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        
        // Ignorer . et ..
        if (name == "." || name == "..") continue;
        
        std::string fullPath = path + "/" + name;
        
        // Vérifier que c'est un dossier
        if (isDirectory(fullPath)) {
            directories.push_back(fullPath);
        }
    }
    
    closedir(dir);
    
    return directories;
}

// ============================================================================
// OPÉRATIONS LECTURE/ÉCRITURE
// ============================================================================

/**
 * @brief Lit un fichier texte complet
 * 
 * @param path Chemin du fichier
 * @return std::string Contenu du fichier
 */
inline std::string readTextFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::error("FileSystem", "Cannot read file: " + path);
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

/**
 * @brief Lit un fichier binaire complet
 * 
 * @param path Chemin du fichier
 * @return std::vector<uint8_t> Contenu binaire du fichier
 */
inline std::vector<uint8_t> readBinaryFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        Logger::error("FileSystem", "Cannot read binary file: " + path);
        return {};
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }
    
    return {};
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
    if (!file.is_open()) {
        Logger::error("FileSystem", "Cannot write file: " + path);
        return false;
    }
    
    file << content;
    file.close();
    return true;
}

/**
 * @brief Écrit un fichier binaire
 * 
 * @param path Chemin du fichier
 * @param data Données binaires
 * @return true Si succès
 */
inline bool writeBinaryFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        Logger::error("FileSystem", "Cannot write binary file: " + path);
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();
    return true;
}

// ============================================================================
// COPIE ET DÉPLACEMENT
// ============================================================================

/**
 * @brief Copie un fichier
 * 
 * @param source Fichier source
 * @param destination Fichier destination
 * @return true Si succès
 */
inline bool copyFile(const std::string& source, const std::string& destination) {
    std::ifstream src(source, std::ios::binary);
    if (!src.is_open()) {
        Logger::error("FileSystem", "Cannot open source: " + source);
        return false;
    }
    
    std::ofstream dst(destination, std::ios::binary);
    if (!dst.is_open()) {
        Logger::error("FileSystem", "Cannot create destination: " + destination);
        return false;
    }
    
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

// ============================================================================
// INFORMATIONS FICHIERS (NOUVELLES MÉTHODES)
// ============================================================================

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
 * @brief Extrait le nom de fichier depuis un chemin complet
 * 
 * @param path Chemin complet (/path/to/file.mid)
 * @return std::string Nom du fichier (file.mid)
 * 
 * @note ✅ NOUVEAU - Implémenté pour Phase 1
 */
inline std::string getFileName(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return path; // Pas de slash, c'est déjà juste le nom
    }
    return path.substr(pos + 1);
}

/**
 * @brief Récupère la taille d'un fichier en bytes
 * 
 * @param path Chemin du fichier
 * @return uint64_t Taille en bytes (0 si erreur)
 * 
 * @note ✅ NOUVEAU - Implémenté pour Phase 1
 */
inline uint64_t getFileSize(const std::string& path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        Logger::warn("FileSystem", "Cannot get size for: " + path);
        return 0;
    }
    
    if (!S_ISREG(buffer.st_mode)) {
        Logger::warn("FileSystem", "Not a regular file: " + path);
        return 0;
    }
    
    return static_cast<uint64_t>(buffer.st_size);
}

/**
 * @brief Récupère le timestamp de dernière modification
 * 
 * @param path Chemin du fichier
 * @return std::time_t Timestamp Unix (0 si erreur)
 * 
 * @note ✅ NOUVEAU - Implémenté pour Phase 1
 */
inline std::time_t getModificationTime(const std::string& path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        Logger::warn("FileSystem", "Cannot get modification time for: " + path);
        return 0;
    }
    
    return buffer.st_mtime;
}

// ============================================================================
// VALIDATION CHEMINS
// ============================================================================

/**
 * @brief Vérifie si un chemin est sûr (pas de ..)
 * 
 * @param path Chemin à vérifier
 * @return true Si le chemin est sûr
 */
inline bool isSafePath(const std::string& path) {
    return path.find("..") == std::string::npos;
}

/**
 * @brief Normalise un chemin (enlève les / multiples)
 * 
 * @param path Chemin à normaliser
 * @return std::string Chemin normalisé
 */
inline std::string normalizePath(const std::string& path) {
    std::string result = path;
    
    // Remplacer // par /
    size_t pos = 0;
    while ((pos = result.find("//", pos)) != std::string::npos) {
        result.replace(pos, 2, "/");
    }
    
    // Enlever le / final si présent
    if (!result.empty() && result.back() == '/') {
        result.pop_back();
    }
    
    return result;
}

} // namespace FileSystem
} // namespace midiMind

// ============================================================================
// FIN DU FICHIER FileSystem.h
// Version: 3.0.1 - COMPLET ✅
// Toutes les méthodes nécessaires sont implémentées
// ============================================================================
