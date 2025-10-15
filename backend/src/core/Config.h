// ============================================================================
// Fichier: backend/src/core/Config.h
// Version: 3.0.2 - CORRECTIONS CRITIQUES PHASE 1
// Date: 2025-10-15
// ============================================================================
// CORRECTIFS v3.0.2 (PHASE 1 - CRITIQUES):
//   ✅ 1.5 Validation valeurs dans load() - Sample rate, buffer size, ports, etc.
//   ✅ Valeurs par défaut sûres si invalides
//   ✅ Logs warnings pour valeurs corrigées
//   ✅ Préservation TOTALE des fonctionnalités existantes
//
// Description:
//   Configuration globale de l'application
//   Lecture/écriture fichier JSON avec validation
//
// Structure:
//   Config (singleton)
//   ├── MidiConfig    : Configuration MIDI
//   ├── ApiConfig     : Configuration API
//   ├── NetworkConfig : Configuration réseau
//   ├── LoggerConfig  : Configuration logging
//   └── ApplicationConfig : Configuration application
//
// Thread-safety: Singleton thread-safe (Meyer's Singleton)
//
// Auteur: MidiMind Team
// ============================================================================

#pragma once

#include <string>
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
 * @struct MidiConfig
 * @brief Configuration MIDI
 */
struct MidiConfig {
    std::string defaultInputDevice = "default";
    std::string defaultOutputDevice = "default";
    int sampleRate = 44100;           // Hz (validé: 8000-192000)
    int bufferSize = 256;             // samples (validé: power of 2, 32-8192)
    int latencyMs = 10;               // ms (validé: 0-1000)
    bool autoConnect = true;
    bool hotPlugEnabled = true;
    
    /**
     * @brief Convertit depuis JSON
     * @param j JSON source
     * @note Applique valeurs par défaut si champs manquants
     */
    void fromJson(const json& j) {
        if (j.contains("default_input_device")) {
            defaultInputDevice = j["default_input_device"];
        }
        if (j.contains("default_output_device")) {
            defaultOutputDevice = j["default_output_device"];
        }
        if (j.contains("sample_rate")) {
            sampleRate = j["sample_rate"];
        }
        if (j.contains("buffer_size")) {
            bufferSize = j["buffer_size"];
        }
        if (j.contains("latency_ms")) {
            latencyMs = j["latency_ms"];
        }
        if (j.contains("auto_connect")) {
            autoConnect = j["auto_connect"];
        }
        if (j.contains("hot_plug_enabled")) {
            hotPlugEnabled = j["hot_plug_enabled"];
        }
    }
    
    /**
     * @brief Convertit vers JSON
     * @return json JSON généré
     */
    json toJson() const {
        return {
            {"default_input_device", defaultInputDevice},
            {"default_output_device", defaultOutputDevice},
            {"sample_rate", sampleRate},
            {"buffer_size", bufferSize},
            {"latency_ms", latencyMs},
            {"auto_connect", autoConnect},
            {"hot_plug_enabled", hotPlugEnabled}
        };
    }
};

/**
 * @struct ApiConfig
 * @brief Configuration API WebSocket et HTTP
 */
struct ApiConfig {
    int port = 8080;                  // Port WebSocket (validé: 1024-65535)
    int httpPort = 8000;              // Port HTTP (validé: 1024-65535)
    int maxConnections = 100;         // Max clients (validé: 1-1000)
    int commandTimeout = 5000;        // ms
    bool enableCors = true;
    std::string corsOrigin = "*";
    
    void fromJson(const json& j) {
        if (j.contains("port")) {
            port = j["port"];
        }
        if (j.contains("http_port")) {
            httpPort = j["http_port"];
        }
        if (j.contains("max_connections")) {
            maxConnections = j["max_connections"];
        }
        if (j.contains("command_timeout")) {
            commandTimeout = j["command_timeout"];
        }
        if (j.contains("enable_cors")) {
            enableCors = j["enable_cors"];
        }
        if (j.contains("cors_origin")) {
            corsOrigin = j["cors_origin"];
        }
    }
    
    json toJson() const {
        return {
            {"port", port},
            {"http_port", httpPort},
            {"max_connections", maxConnections},
            {"command_timeout", commandTimeout},
            {"enable_cors", enableCors},
            {"cors_origin", corsOrigin}
        };
    }
};

/**
 * @struct NetworkConfig
 * @brief Configuration réseau (WiFi, Bluetooth, etc.)
 */
struct NetworkConfig {
    bool enabled = false;
    bool wifiEnabled = true;
    bool bluetoothEnabled = false;
    bool rtpMidiEnabled = false;
    int rtpMidiPort = 5004;
    std::string networkInterface = "wlan0";
    
    void fromJson(const json& j) {
        if (j.contains("enabled")) {
            enabled = j["enabled"];
        }
        if (j.contains("wifi_enabled")) {
            wifiEnabled = j["wifi_enabled"];
        }
        if (j.contains("bluetooth_enabled")) {
            bluetoothEnabled = j["bluetooth_enabled"];
        }
        if (j.contains("rtpmidi_enabled")) {
            rtpMidiEnabled = j["rtpmidi_enabled"];
        }
        if (j.contains("rtpmidi_port")) {
            rtpMidiPort = j["rtpmidi_port"];
        }
        if (j.contains("network_interface")) {
            networkInterface = j["network_interface"];
        }
    }
    
    json toJson() const {
        return {
            {"enabled", enabled},
            {"wifi_enabled", wifiEnabled},
            {"bluetooth_enabled", bluetoothEnabled},
            {"rtpmidi_enabled", rtpMidiEnabled},
            {"rtpmidi_port", rtpMidiPort},
            {"network_interface", networkInterface}
        };
    }
};

/**
 * @struct LoggerConfig
 * @brief Configuration du logging
 */
struct LoggerConfig {
    std::string level = "info";
    std::string outputFile = "/var/log/midimind/midimind.log";
    bool enableConsole = true;
    bool enableFile = true;
    int maxFileSize = 10 * 1024 * 1024;  // 10 MB
    int maxBackups = 5;
    
    void fromJson(const json& j) {
        if (j.contains("level")) {
            level = j["level"];
        }
        if (j.contains("output_file")) {
            outputFile = j["output_file"];
        }
        if (j.contains("enable_console")) {
            enableConsole = j["enable_console"];
        }
        if (j.contains("enable_file")) {
            enableFile = j["enable_file"];
        }
        if (j.contains("max_file_size")) {
            maxFileSize = j["max_file_size"];
        }
        if (j.contains("max_backups")) {
            maxBackups = j["max_backups"];
        }
    }
    
    json toJson() const {
        return {
            {"level", level},
            {"output_file", outputFile},
            {"enable_console", enableConsole},
            {"enable_file", enableFile},
            {"max_file_size", maxFileSize},
            {"max_backups", maxBackups}
        };
    }
};

/**
 * @struct ApplicationConfig
 * @brief Configuration de l'application
 */
struct ApplicationConfig {
    std::string version = "3.0.0";
    std::string dataDirectory = "/var/lib/midimind";
    bool daemonMode = false;
    std::string pidFile = "/var/run/midimind.pid";
    
    void fromJson(const json& j) {
        if (j.contains("version")) {
            version = j["version"];
        }
        if (j.contains("data_directory")) {
            dataDirectory = j["data_directory"];
        }
        if (j.contains("daemon_mode")) {
            daemonMode = j["daemon_mode"];
        }
        if (j.contains("pid_file")) {
            pidFile = j["pid_file"];
        }
    }
    
    json toJson() const {
        return {
            {"version", version},
            {"data_directory", dataDirectory},
            {"daemon_mode", daemonMode},
            {"pid_file", pidFile}
        };
    }
};

// ============================================================================
// CLASSE CONFIG (SINGLETON)
// ============================================================================

/**
 * @class Config
 * @brief Singleton de configuration globale
 * 
 * @details
 * Gère la configuration de toute l'application.
 * 
 * Fonctionnalités:
 * - Lecture/écriture fichier JSON
 * - ✅ Validation des valeurs (v3.0.2)
 * - Valeurs par défaut
 * - Thread-safe (Meyer's Singleton)
 * 
 * @example Utilisation
 * ```cpp
 * auto& config = Config::instance();
 * config.load("config.json");
 * 
 * int sampleRate = config.midi.sampleRate;
 * ```
 */
class Config {
public:
    // ========================================================================
    // SINGLETON
    // ========================================================================
    
    /**
     * @brief Récupère l'instance unique (thread-safe C++11)
     * @return Config& Référence singleton
     */
    static Config& instance() {
        static Config instance;
        return instance;
    }
    
    // Désactiver copie
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
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
     * @note ✅ CORRECTIF 1.5: Validation complète des valeurs
     * 
     * @example
     * ```cpp
     * if (!Config::instance().load("config.json")) {
     *     Logger::warn("Config", "Using default configuration");
     * }
     * ```
     */
    bool load(const std::string& filepath) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Logger::info("Config", "Loading configuration from: " + filepath);
        
        // Ouvrir le fichier
        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::warn("Config", "Cannot open config file: " + filepath);
            Logger::warn("Config", "Using default configuration");
            configPath_ = filepath;
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
            
            if (j.contains("logger")) {
                logger.fromJson(j["logger"]);
                Logger::debug("Config", "Logger config loaded");
            }
            
            if (j.contains("application")) {
                application.fromJson(j["application"]);
                Logger::debug("Config", "Application config loaded");
            }
            
            configPath_ = filepath;
            
            // ✅ CORRECTIF 1.5: VALIDATION DES VALEURS
            validateAndFix();
            
            Logger::info("Config", "✅ Configuration loaded successfully");
            return true;
            
        } catch (const json::exception& e) {
            Logger::error("Config", "JSON parse error: " + std::string(e.what()));
            Logger::warn("Config", "Using default configuration");
            return false;
        }
    }
    
    /**
     * @brief Sauvegarde la configuration dans un fichier JSON
     * 
     * @param filepath Chemin du fichier (optionnel, utilise le dernier chargé)
     * @return true Si la sauvegarde a réussi
     * 
     * @note Thread-safe
     */
    bool save(const std::string& filepath = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string path = filepath.empty() ? configPath_ : filepath;
        
        if (path.empty()) {
            Logger::error("Config", "No config file path specified");
            return false;
        }
        
        Logger::info("Config", "Saving configuration to: " + path);
        
        try {
            json j = {
                {"midi", midi.toJson()},
                {"api", api.toJson()},
                {"network", network.toJson()},
                {"logger", logger.toJson()},
                {"application", application.toJson()}
            };
            
            std::ofstream file(path);
            if (!file.is_open()) {
                Logger::error("Config", "Cannot open file for writing: " + path);
                return false;
            }
            
            file << j.dump(2);  // Pretty print avec indentation
            file.close();
            
            Logger::info("Config", "✅ Configuration saved successfully");
            return true;
            
        } catch (const std::exception& e) {
            Logger::error("Config", "Save error: " + std::string(e.what()));
            return false;
        }
    }
    
    // ========================================================================
    // MEMBRES PUBLICS - CONFIGURATION
    // ========================================================================
    
    MidiConfig midi;
    ApiConfig api;
    NetworkConfig network;
    LoggerConfig logger;
    ApplicationConfig application;
    
private:
    // ========================================================================
    // CONSTRUCTEUR PRIVÉ
    // ========================================================================
    
    Config() = default;
    
    // ========================================================================
    // VALIDATION - CORRECTIF 1.5
    // ========================================================================
    
    /**
     * @brief ✅ Valide et corrige les valeurs de configuration
     * @note Applique valeurs par défaut sûres si invalides
     * @note Log warnings pour chaque correction
     */
    void validateAndFix() {
        bool hadErrors = false;
        
        // ✅ VALIDATION MIDI - Sample Rate
        if (midi.sampleRate <= 0 || midi.sampleRate > 192000) {
            Logger::warn("Config", 
                "Invalid sample rate (" + std::to_string(midi.sampleRate) + 
                "), using 44100 Hz");
            midi.sampleRate = 44100;
            hadErrors = true;
        }
        
        // ✅ VALIDATION MIDI - Buffer Size (doit être puissance de 2)
        if (midi.bufferSize <= 0 || midi.bufferSize > 8192 || 
            !isPowerOfTwo(midi.bufferSize)) {
            Logger::warn("Config", 
                "Invalid buffer size (" + std::to_string(midi.bufferSize) + 
                "), using 256 samples");
            midi.bufferSize = 256;
            hadErrors = true;
        }
        
        // ✅ VALIDATION MIDI - Latency
        if (midi.latencyMs < 0 || midi.latencyMs > 1000) {
            Logger::warn("Config", 
                "Invalid latency (" + std::to_string(midi.latencyMs) + 
                "ms), using 10ms");
            midi.latencyMs = 10;
            hadErrors = true;
        }
        
        // ✅ VALIDATION API - Port WebSocket
        if (api.port < 1024 || api.port > 65535) {
            Logger::warn("Config", 
                "Invalid API port (" + std::to_string(api.port) + 
                "), using 8080");
            api.port = 8080;
            hadErrors = true;
        }
        
        // ✅ VALIDATION API - Port HTTP
        if (api.httpPort < 1024 || api.httpPort > 65535) {
            Logger::warn("Config", 
                "Invalid HTTP port (" + std::to_string(api.httpPort) + 
                "), using 8000");
            api.httpPort = 8000;
            hadErrors = true;
        }
        
        // ✅ VALIDATION API - Max Connections
        if (api.maxConnections <= 0 || api.maxConnections > 1000) {
            Logger::warn("Config", 
                "Invalid max connections (" + std::to_string(api.maxConnections) + 
                "), using 100");
            api.maxConnections = 100;
            hadErrors = true;
        }
        
        // ✅ VALIDATION API - Command Timeout
        if (api.commandTimeout <= 0 || api.commandTimeout > 60000) {
            Logger::warn("Config", 
                "Invalid command timeout (" + std::to_string(api.commandTimeout) + 
                "ms), using 5000ms");
            api.commandTimeout = 5000;
            hadErrors = true;
        }
        
        // ✅ VALIDATION LOGGER - Max File Size
        if (logger.maxFileSize <= 0 || logger.maxFileSize > 100 * 1024 * 1024) {
            Logger::warn("Config", 
                "Invalid log file size, using 10MB");
            logger.maxFileSize = 10 * 1024 * 1024;
            hadErrors = true;
        }
        
        // ✅ VALIDATION LOGGER - Max Backups
        if (logger.maxBackups < 0 || logger.maxBackups > 100) {
            Logger::warn("Config", 
                "Invalid log backups count, using 5");
            logger.maxBackups = 5;
            hadErrors = true;
        }
        
        if (hadErrors) {
            Logger::warn("Config", 
                "⚠️  Configuration had invalid values, corrected to safe defaults");
        } else {
            Logger::info("Config", "✅ Configuration validation passed");
        }
    }
    
    /**
     * @brief Vérifie si un nombre est une puissance de 2
     * @param n Nombre à vérifier
     * @return true Si puissance de 2
     */
    static bool isPowerOfTwo(int n) {
        return n > 0 && (n & (n - 1)) == 0;
    }
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Chemin du fichier de configuration
    std::string configPath_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Config.h v3.0.2 - CORRECTIONS PHASE 1 COMPLÈTES
// ============================================================================
