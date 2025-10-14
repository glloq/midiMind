// ============================================================================
// Fichier: backend/src/core/Config.h
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.1.1 - Mise à jour Logger
// ============================================================================
// Description:
//   Gestionnaire de configuration centralisé pour l'application.
//   Charge, sauvegarde et gère les paramètres depuis un fichier JSON.
//
// Fonctionnalités:
//   - Chargement/sauvegarde fichier JSON
//   - Configuration MIDI
//   - Configuration API
//   - Configuration Network
//   - Configuration Logger (NOUVEAU v3.1.1)
//   - Thread-safe
//   - Singleton pattern
//
// Auteur: midiMind Team
// Date: 2025-10-02
// Mise à jour: 2025-10-10 (ajout LoggerConfig)
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Logger.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURES DE CONFIGURATION
// ============================================================================

/**
 * @brief Configuration MIDI
 */
struct MidiConfig {
    std::string defaultDevice = "USB";
    int defaultChannel = 1;
    int bufferSize = 256;
    bool autoConnect = true;
    int maxLatencyMs = 10;
    
    json toJson() const {
        json j;
        j["defaultDevice"] = defaultDevice;
        j["defaultChannel"] = defaultChannel;
        j["bufferSize"] = bufferSize;
        j["autoConnect"] = autoConnect;
        j["maxLatencyMs"] = maxLatencyMs;
        return j;
    }
    
    void fromJson(const json& j) {
        if (j.contains("defaultDevice")) defaultDevice = j["defaultDevice"];
        if (j.contains("defaultChannel")) defaultChannel = j["defaultChannel"];
        if (j.contains("bufferSize")) bufferSize = j["bufferSize"];
        if (j.contains("autoConnect")) autoConnect = j["autoConnect"];
        if (j.contains("maxLatencyMs")) maxLatencyMs = j["maxLatencyMs"];
    }
};

/**
 * @brief Configuration API
 */
struct ApiConfig {
    int port = 8080;
    std::string host = "0.0.0.0";
    bool enableCors = true;
    int maxConnections = 100;
    int heartbeatIntervalMs = 5000;
    
    json toJson() const {
        json j;
        j["port"] = port;
        j["host"] = host;
        j["enableCors"] = enableCors;
        j["maxConnections"] = maxConnections;
        j["heartbeatIntervalMs"] = heartbeatIntervalMs;
        return j;
    }
    
    void fromJson(const json& j) {
        if (j.contains("port")) port = j["port"];
        if (j.contains("host")) host = j["host"];
        if (j.contains("enableCors")) enableCors = j["enableCors"];
        if (j.contains("maxConnections")) maxConnections = j["maxConnections"];
        if (j.contains("heartbeatIntervalMs")) heartbeatIntervalMs = j["heartbeatIntervalMs"];
    }
};

/**
 * @brief Configuration Network
 */
struct NetworkConfig {
    bool enableWifi = true;
    bool enableBluetooth = false;
    bool enableHotspot = false;
    std::string hotspotSsid = "midiMind";
    std::string hotspotPassword = "midimind2025";
    
    json toJson() const {
        json j;
        j["enableWifi"] = enableWifi;
        j["enableBluetooth"] = enableBluetooth;
        j["enableHotspot"] = enableHotspot;
        j["hotspotSsid"] = hotspotSsid;
        j["hotspotPassword"] = hotspotPassword;
        return j;
    }
    
    void fromJson(const json& j) {
        if (j.contains("enableWifi")) enableWifi = j["enableWifi"];
        if (j.contains("enableBluetooth")) enableBluetooth = j["enableBluetooth"];
        if (j.contains("enableHotspot")) enableHotspot = j["enableHotspot"];
        if (j.contains("hotspotSsid")) hotspotSsid = j["hotspotSsid"];
        if (j.contains("hotspotPassword")) hotspotPassword = j["hotspotPassword"];
    }
};

// ============================================================================
// AJOUT v3.1.1: Configuration Logger
// ============================================================================

/**
 * @brief Configuration du Logger
 * @version 3.1.1
 */
struct LoggerConfig {
    // Niveau de log
    std::string level = "INFO";  // "DEBUG", "INFO", "WARNING", "ERROR"
    
    // Fichier de log (contrôle utilisateur)
    bool fileLoggingEnabled = false;
    std::string filePath = "/var/log/midimind/midimind.log";
    int maxFileSizeMB = 10;
    int maxFiles = 5;
    
    // Console
    bool colorsEnabled = true;
    bool timestampsEnabled = true;
    bool categoryEnabled = true;
    
    // Filtrage
    std::vector<std::string> categoryFilter;
    
    // Syslog (Linux)
    bool syslogEnabled = false;
    std::string syslogIdent = "midimind";
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        j["level"] = level;
        j["fileLogging"] = {
            {"enabled", fileLoggingEnabled},
            {"path", filePath},
            {"maxSizeMB", maxFileSizeMB},
            {"maxFiles", maxFiles}
        };
        j["console"] = {
            {"colors", colorsEnabled},
            {"timestamps", timestampsEnabled},
            {"category", categoryEnabled}
        };
        j["categoryFilter"] = categoryFilter;
        j["syslog"] = {
            {"enabled", syslogEnabled},
            {"ident", syslogIdent}
        };
        return j;
    }
    
    /**
     * @brief Charge depuis JSON
     */
    void fromJson(const json& j) {
        if (j.contains("level")) {
            level = j["level"];
        }
        
        if (j.contains("fileLogging")) {
            auto fl = j["fileLogging"];
            if (fl.contains("enabled")) fileLoggingEnabled = fl["enabled"];
            if (fl.contains("path")) filePath = fl["path"];
            if (fl.contains("maxSizeMB")) maxFileSizeMB = fl["maxSizeMB"];
            if (fl.contains("maxFiles")) maxFiles = fl["maxFiles"];
        }
        
        if (j.contains("console")) {
            auto c = j["console"];
            if (c.contains("colors")) colorsEnabled = c["colors"];
            if (c.contains("timestamps")) timestampsEnabled = c["timestamps"];
            if (c.contains("category")) categoryEnabled = c["category"];
        }
        
        if (j.contains("categoryFilter")) {
            categoryFilter = j["categoryFilter"].get<std::vector<std::string>>();
        }
        
        if (j.contains("syslog")) {
            auto s = j["syslog"];
            if (s.contains("enabled")) syslogEnabled = s["enabled"];
            if (s.contains("ident")) syslogIdent = s["ident"];
        }
    }
};

// ============================================================================
// CLASSE: Config (Singleton)
// ============================================================================

/**
 * @class Config
 * @brief Gestionnaire de configuration centralisé
 * 
 * @details
 * Classe singleton qui gère toute la configuration de l'application.
 * Thread-safe et persistante (fichier JSON).
 * 
 * @note Pattern Singleton avec Meyer's Singleton
 * @note Thread-safe via mutex
 * 
 * @example Utilisation
 * @code
 * // Charger la configuration
 * Config::instance().load("config/config.json");
 * 
 * // Accéder aux configs
 * int port = Config::instance().api.port;
 * std::string level = Config::instance().logger.level;
 * 
 * // Modifier et sauvegarder
 * Config::instance().logger.fileLoggingEnabled = true;
 * Config::instance().save();
 * @endcode
 */
class Config {
public:
    // ========================================================================
    // SINGLETON
    // ========================================================================
    
    /**
     * @brief Récupère l'instance unique
     * 
     * @return Config& Instance singleton
     * 
     * @note Thread-safe (Meyer's Singleton depuis C++11)
     */
    static Config& instance() {
        static Config instance;
        return instance;
    }
    
    // Désactiver copie et assignation
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    // ========================================================================
    // MEMBRES DE CONFIGURATION
    // ========================================================================
    
    MidiConfig midi;
    ApiConfig api;
    NetworkConfig network;
    LoggerConfig logger;  // ✅ NOUVEAU v3.1.1
    
    // ========================================================================
    // CHARGEMENT / SAUVEGARDE
    // ========================================================================
    
    /**
     * @brief Charge la configuration depuis un fichier JSON
     * 
     * @param filepath Chemin du fichier de configuration
     * @return true Si le chargement a réussi
     * 
     * @note Thread-safe
     * @note Si le fichier n'existe pas, utilise les valeurs par défaut
     * 
     * @example
     * @code
     * if (!Config::instance().load("config.json")) {
     *     Logger::warn("Config", "Using default configuration");
     * }
     * @endcode
     */
    bool load(const std::string& filepath) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Logger::info("Config", "Loading configuration from: " + filepath);
        
        // Ouvrir le fichier
        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::warn("Config", "Cannot open config file: " + filepath);
            Logger::warn("Config", "Using default configuration");
            configPath_ = filepath;  // Sauvegarder le chemin pour save()
            return false;
        }
        
        try {
            // Parser JSON
            json j;
            file >> j;
            file.close();
            
            // Charger les différentes sections
            if (j.contains("midi")) {
                midi.fromJson(j["midi"]);
                Logger::debug("Config", "MIDI config loaded");
            }
            
            if (j.contains("api")) {
                api.fromJson(j["api"]);
                Logger::debug("Config", "API config loaded");
            }
            
            if (j.contains("network")) {
                network.fromJson(j["network"]);
                Logger::debug("Config", "Network config loaded");
            }
            
            // ✅ NOUVEAU v3.1.1: Charger logger config
            if (j.contains("logger")) {
                logger.fromJson(j["logger"]);
                Logger::debug("Config", "Logger config loaded");
            }
            
            configPath_ = filepath;
            
            Logger::info("Config", "✓ Configuration loaded successfully");
            return true;
            
        } catch (const std::exception& e) {
            Logger::error("Config", "Failed to parse config file: " + 
                         std::string(e.what()));
            Logger::warn("Config", "Using default configuration");
            return false;
        }
    }
    
    /**
     * @brief Sauvegarde la configuration dans un fichier JSON
     * 
     * @param filepath Chemin du fichier (optionnel, utilise le chemin de load() par défaut)
     * @return true Si la sauvegarde a réussi
     * 
     * @note Thread-safe
     * 
     * @example
     * @code
     * // Modifier une config
     * Config::instance().logger.fileLoggingEnabled = true;
     * 
     * // Sauvegarder
     * Config::instance().save();
     * @endcode
     */
    bool save(const std::string& filepath = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string path = filepath.empty() ? configPath_ : filepath;
        
        if (path.empty()) {
            Logger::error("Config", "No config path specified");
            return false;
        }
        
        Logger::info("Config", "Saving configuration to: " + path);
        
        try {
            // Créer le JSON
            json j = toJson();
            
            // Écrire dans le fichier
            std::ofstream file(path);
            if (!file.is_open()) {
                Logger::error("Config", "Cannot open file for writing: " + path);
                return false;
            }
            
            file << j.dump(2);  // Indentation de 2 espaces
            file.close();
            
            Logger::info("Config", "✓ Configuration saved successfully");
            return true;
            
        } catch (const std::exception& e) {
            Logger::error("Config", "Failed to save config: " + 
                         std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief Convertit toute la configuration en JSON
     * 
     * @return json Configuration complète
     * 
     * @note Thread-safe
     */
    json toJson() const {
        // Pas besoin de lock ici car appelé depuis save() qui lock déjà
        
        json j;
        
        j["application"] = {
            {"name", "midiMind"},
            {"version", "3.1.1"}
        };
        
        j["midi"] = midi.toJson();
        j["api"] = api.toJson();
        j["network"] = network.toJson();
        j["logger"] = logger.toJson();  // ✅ NOUVEAU v3.1.1
        
        return j;
    }
    
    /**
     * @brief Réinitialise à la configuration par défaut
     * 
     * @note Thread-safe
     */
    void resetToDefaults() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Logger::info("Config", "Resetting to default configuration");
        
        midi = MidiConfig();
        api = ApiConfig();
        network = NetworkConfig();
        logger = LoggerConfig();  // ✅ NOUVEAU v3.1.1
        
        Logger::info("Config", "✓ Configuration reset to defaults");
    }
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    /**
     * @brief Récupère le chemin du fichier de configuration
     * 
     * @return std::string Chemin du fichier
     */
    std::string getConfigPath() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return configPath_;
    }

private:
    // ========================================================================
    // CONSTRUCTEUR PRIVÉ (Singleton)
    // ========================================================================
    
    Config() : configPath_("") {
        // Configuration par défaut déjà initialisée dans les structs
    }
    
    ~Config() = default;
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    mutable std::mutex mutex_;
    std::string configPath_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Config.h v3.1.1
// ============================================================================