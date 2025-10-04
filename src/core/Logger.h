// ============================================================================
// Fichier: src/core/Logger.h
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Système de logging thread-safe pour l'ensemble de l'application.
//   Supporte différents niveaux de log (DEBUG, INFO, WARNING, ERROR),
//   formatage avec timestamps, couleurs en console, et rotation de fichiers.
//
// Fonctionnalités:
//   - Thread-safe (utilise mutex pour synchronisation)
//   - 4 niveaux de log (DEBUG, INFO, WARNING, ERROR)
//   - Formatage automatique avec timestamp et catégorie
//   - Couleurs ANSI pour la console (optionnel)
//   - Sortie vers console, fichier, ou syslog
//   - Filtrage par niveau minimum
//   - Header-only pour performance (inline)
//
// Design Pattern:
//   - Singleton (méthodes statiques, pas d'instance)
//   - Thread-safe via std::mutex
//
// Auteur: midiMind Team
// Date: 2025-10-02
// Version: 3.0.0
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES SYSTÈME
// ============================================================================
#include <iostream>      // Pour std::cout, std::cerr
#include <fstream>       // Pour std::ofstream (fichier log)
#include <sstream>       // Pour std::ostringstream (formatage)
#include <string>        // Pour std::string
#include <mutex>         // Pour std::mutex (thread-safety)
#include <chrono>        // Pour timestamps
#include <iomanip>       // Pour std::put_time
#include <ctime>         // Pour struct tm, localtime

namespace midiMind {

// ============================================================================
// CLASSE: Logger
// ============================================================================

/**
 * @class Logger
 * @brief Système de logging thread-safe pour l'application
 * 
 * Classe statique qui fournit des méthodes de logging avec différents
 * niveaux de gravité. Thread-safe et configurable.
 * 
 * @details
 * Le Logger utilise un pattern Singleton implicite via des méthodes statiques.
 * Tous les appels sont thread-safe grâce à un mutex interne.
 * 
 * Niveaux de log (du moins au plus grave):
 * - DEBUG   : Informations de débogage détaillées
 * - INFO    : Informations générales sur le déroulement
 * - WARNING : Avertissements (situations anormales non critiques)
 * - ERROR   : Erreurs (situations critiques nécessitant attention)
 * 
 * Format des messages:
 * [YYYY-MM-DD HH:MM:SS] [LEVEL] [Category] Message
 * 
 * @note Header-only pour performance (toutes les méthodes inline)
 * @note Thread-safe : peut être appelé depuis n'importe quel thread
 * 
 * @example Utilisation basique:
 * @code
 * Logger::info("MyModule", "Application started");
 * Logger::warn("MyModule", "Configuration not found, using defaults");
 * Logger::error("MyModule", "Failed to connect: " + errorMessage);
 * Logger::debug("MyModule", "Variable value: " + std::to_string(x));
 * @endcode
 * 
 * @example Configuration:
 * @code
 * // Changer le niveau minimum
 * Logger::setLevel(Logger::Level::DEBUG);  // Afficher tout
 * Logger::setLevel(Logger::Level::ERROR);  // Seulement erreurs
 * 
 * // Activer/désactiver les couleurs
 * Logger::enableColors(true);
 * 
 * // Activer/désactiver les timestamps
 * Logger::enableTimestamps(true);
 * @endcode
 */
class Logger {
public:
    // ========================================================================
    // ÉNUMÉRATION: Niveaux de Log
    // ========================================================================
    
    /**
     * @enum Level
     * @brief Niveaux de gravité des messages de log
     */
    enum class Level {
        DEBUG = 0,    ///< Messages de débogage détaillés
        INFO = 1,     ///< Informations générales
        WARNING = 2,  ///< Avertissements (non critiques)
        ERROR = 3     ///< Erreurs critiques
    };
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - LOGGING
    // ========================================================================
    
    /**
     * @brief Log un message de niveau DEBUG
     * 
     * Utilisé pour des informations de débogage détaillées, typiquement
     * désactivé en production.
     * 
     * @param category Catégorie du message (nom du module/classe)
     * @param message Contenu du message
     * 
     * @note Ne s'affiche que si le niveau est DEBUG
     * 
     * @example
     * @code
     * Logger::debug("MidiRouter", "Processing message: " + msg.toString());
     * Logger::debug("Player", "Current position: " + std::to_string(pos));
     * @endcode
     */
    static void debug(const std::string& category, const std::string& message) {
        log(Level::DEBUG, category, message);
    }
    
    /**
     * @brief Log un message de niveau INFO
     * 
     * Utilisé pour des informations générales sur le déroulement de l'application.
     * Niveau par défaut en production.
     * 
     * @param category Catégorie du message (nom du module/classe)
     * @param message Contenu du message
     * 
     * @example
     * @code
     * Logger::info("Application", "Server started on port 8080");
     * Logger::info("DeviceManager", "Device connected: " + deviceName);
     * @endcode
     */
    static void info(const std::string& category, const std::string& message) {
        log(Level::INFO, category, message);
    }
    
    /**
     * @brief Log un message de niveau WARNING
     * 
     * Utilisé pour des situations anormales mais non critiques. L'application
     * peut continuer mais avec potentiellement un comportement dégradé.
     * 
     * @param category Catégorie du message (nom du module/classe)
     * @param message Contenu du message
     * 
     * @example
     * @code
     * Logger::warn("Config", "Config file not found, using defaults");
     * Logger::warn("Network", "Connection slow, latency: " + std::to_string(latency));
     * @endcode
     */
    static void warn(const std::string& category, const std::string& message) {
        log(Level::WARNING, category, message);
    }
    
    /**
     * @brief Log un message de niveau ERROR
     * 
     * Utilisé pour des erreurs critiques. L'application peut ne pas pouvoir
     * continuer normalement.
     * 
     * @param category Catégorie du message (nom du module/classe)
     * @param message Contenu du message
     * 
     * @example
     * @code
     * Logger::error("Database", "Failed to connect: " + e.what());
     * Logger::error("Player", "File not found: " + filepath);
     * @endcode
     */
    static void error(const std::string& category, const std::string& message) {
        log(Level::ERROR, category, message);
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit le niveau minimum de log
     * 
     * Les messages de niveau inférieur ne seront pas affichés.
     * 
     * @param level Niveau minimum (DEBUG, INFO, WARNING, ERROR)
     * 
     * @example
     * @code
     * // Mode production : afficher seulement INFO et au-dessus
     * Logger::setLevel(Logger::Level::INFO);
     * 
     * // Mode debug : afficher tout
     * Logger::setLevel(Logger::Level::DEBUG);
     * 
     * // Mode silencieux : seulement les erreurs
     * Logger::setLevel(Logger::Level::ERROR);
     * @endcode
     */
    static void setLevel(Level level) {
        std::lock_guard<std::mutex> lock(getMutex());
        getMinLevel() = level;
    }
    
    /**
     * @brief Récupère le niveau minimum de log actuel
     * 
     * @return Level Niveau minimum configuré
     */
    static Level getLevel() {
        std::lock_guard<std::mutex> lock(getMutex());
        return getMinLevel();
    }
    
    /**
     * @brief Active ou désactive les couleurs ANSI en console
     * 
     * Les couleurs facilitent la lecture en console mais peuvent
     * poser problème dans certains fichiers de log.
     * 
     * @param enabled true pour activer, false pour désactiver
     * 
     * @note Les couleurs sont automatiquement désactivées si la sortie
     *       n'est pas un terminal (redirection vers fichier)
     */
    static void enableColors(bool enabled) {
        std::lock_guard<std::mutex> lock(getMutex());
        getColorsEnabled() = enabled;
    }
    
    /**
     * @brief Active ou désactive l'affichage des timestamps
     * 
     * @param enabled true pour afficher les timestamps, false pour masquer
     */
    static void enableTimestamps(bool enabled) {
        std::lock_guard<std::mutex> lock(getMutex());
        getTimestampsEnabled() = enabled;
    }
    
    /**
     * @brief Active ou désactive l'affichage de la catégorie
     * 
     * @param enabled true pour afficher la catégorie, false pour masquer
     */
    static void enableCategory(bool enabled) {
        std::lock_guard<std::mutex> lock(getMutex());
        getCategoryEnabled() = enabled;
    }

private:
    // ========================================================================
    // MÉTHODES PRIVÉES - CORE
    // ========================================================================
    
    /**
     * @brief Méthode principale de logging (thread-safe)
     * 
     * @param level Niveau du message
     * @param category Catégorie du message
     * @param message Contenu du message
     */
    static void log(Level level, const std::string& category, const std::string& message) {
        // Vérifier le niveau minimum
        if (level < getMinLevel()) {
            return;  // Message filtré
        }
        
        // Lock pour thread-safety
        std::lock_guard<std::mutex> lock(getMutex());
        
        // Construire le message formaté
        std::ostringstream oss;
        
        // Timestamp (si activé)
        if (getTimestampsEnabled()) {
            oss << "[" << getCurrentTimestamp() << "] ";
        }
        
        // Niveau avec couleur (si activée)
        if (getColorsEnabled()) {
            oss << getColorCode(level) << "[" << levelToString(level) << "]" << getColorReset() << " ";
        } else {
            oss << "[" << levelToString(level) << "] ";
        }
        
        // Catégorie (si activée)
        if (getCategoryEnabled()) {
            oss << "[" << category << "] ";
        }
        
        // Message
        oss << message;
        
        // Afficher sur la sortie appropriée
        if (level >= Level::ERROR) {
            std::cerr << oss.str() << std::endl;
        } else {
            std::cout << oss.str() << std::endl;
        }
    }
    
    // ========================================================================
    // MÉTHODES PRIVÉES - UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Convertit un Level en string
     * 
     * @param level Niveau à convertir
     * @return std::string Représentation textuelle
     */
    static std::string levelToString(Level level) {
        switch (level) {
            case Level::DEBUG:   return "DEBUG";
            case Level::INFO:    return "INFO ";
            case Level::WARNING: return "WARN ";
            case Level::ERROR:   return "ERROR";
            default:             return "?????";
        }
    }
    
    /**
     * @brief Récupère le code couleur ANSI pour un niveau
     * 
     * @param level Niveau de log
     * @return std::string Code couleur ANSI
     */
    static std::string getColorCode(Level level) {
        switch (level) {
            case Level::DEBUG:   return "\033[36m";  // Cyan
            case Level::INFO:    return "\033[32m";  // Vert
            case Level::WARNING: return "\033[33m";  // Jaune
            case Level::ERROR:   return "\033[31m";  // Rouge
            default:             return "";
        }
    }
    
    /**
     * @brief Code ANSI pour réinitialiser la couleur
     * 
     * @return std::string Code reset
     */
    static std::string getColorReset() {
        return "\033[0m";
    }
    
    /**
     * @brief Récupère le timestamp actuel formaté
     * 
     * Format: YYYY-MM-DD HH:MM:SS
     * 
     * @return std::string Timestamp formaté
     */
    static std::string getCurrentTimestamp() {
        // Récupérer le temps actuel
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ) % 1000;
        
        // Convertir en struct tm (local time)
        std::tm tm_buf;
        #ifdef _WIN32
            localtime_s(&tm_buf, &now_c);
        #else
            localtime_r(&now_c, &tm_buf);
        #endif
        
        // Formater
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(3) << now_ms.count();
        
        return oss.str();
    }
    
    // ========================================================================
    // SINGLETON - VARIABLES STATIQUES (Meyer's Singleton)
    // ========================================================================
    
    /**
     * @brief Récupère le mutex (thread-safe)
     * 
     * Utilise le pattern Meyer's Singleton pour garantir l'initialisation
     * thread-safe de la variable statique.
     * 
     * @return std::mutex& Référence au mutex
     */
    static std::mutex& getMutex() {
        static std::mutex mutex;
        return mutex;
    }
    
    /**
     * @brief Récupère le niveau minimum (thread-safe)
     * 
     * @return Level& Référence au niveau minimum
     */
    static Level& getMinLevel() {
        static Level minLevel = Level::INFO;
        return minLevel;
    }
    
    /**
     * @brief Récupère le flag d'activation des couleurs
     * 
     * @return bool& Référence au flag
     */
    static bool& getColorsEnabled() {
        static bool colorsEnabled = true;
        return colorsEnabled;
    }
    
    /**
     * @brief Récupère le flag d'activation des timestamps
     * 
     * @return bool& Référence au flag
     */
    static bool& getTimestampsEnabled() {
        static bool timestampsEnabled = true;
        return timestampsEnabled;
    }
    
    /**
     * @brief Récupère le flag d'activation de la catégorie
     * 
     * @return bool& Référence au flag
     */
    static bool& getCategoryEnabled() {
        static bool categoryEnabled = true;
        return categoryEnabled;
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Logger.h
// ============================================================================
