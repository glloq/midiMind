// ============================================================================
// File: backend/src/core/Config.h
// Version: 4.1.0 - FIXED (2025-10-21)
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Global configuration management system. Loads configuration from JSON,
//   provides typed getters with defaults, validates on load. Header-only
//   singleton with thread-safe access.
//
// Features:
//   - JSON-based configuration file
//   - Typed getters with default values
//   - Nested path support with dot notation (e.g., "midi.buffer_size")
//   - Validation on load
//   - Runtime updates
//   - Thread-safe access
//
// Dependencies:
//   - nlohmann/json
//   - Logger.h
//   - Error.h
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Simplified structure (removed complex nested structs)
//   - Added nested path support with dot notation
//   - Improved validation
//   - Added merge capability
//   - Focused on essential configuration only
//
// FIX 2025-10-21:
//   - Added catch for std::exception in load() method (line 222)
//   - Prevents silent crashes when std::runtime_error is thrown from getValueAtPath()
//   - Now catches both json::exception AND std::exception
//
// ============================================================================

#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

#include "Logger.h"
#include "Error.h"

namespace midiMind {

using json = nlohmann::json;

// ============================================================================
// DEFAULT CONFIGURATION VALUES
// ============================================================================

/**
 * @brief Default configuration as JSON string
 * 
 * @details
 * This is the fallback configuration used when:
 * - No config file is found
 * - Config file is corrupted
 * - Specific values are missing
 */
const char* DEFAULT_CONFIG_JSON = R"({
    "application": {
        "version": "4.1.0",
        "name": "MidiMind",
        "data_dir": "/var/lib/midimind",
        "log_dir": "/var/log/midimind"
    },
    "midi": {
        "buffer_size": 256,
        "sample_rate": 44100,
        "max_devices": 32,
        "alsa_client_name": "MidiMind"
    },
    "timing": {
        "latency_compensation": true,
        "auto_calibration": true,
        "calibration_duration_ms": 5000,
        "calibration_iterations": 100,
        "max_jitter_ms": 5.0
    },
    "api": {
        "port": 8080,
        "host": "0.0.0.0",
        "max_connections": 10,
        "timeout_ms": 30000
    },
    "storage": {
        "database_path": "/var/lib/midimind/midimind.db",
        "auto_backup": true,
        "backup_interval_hours": 24,
        "max_backups": 7
    },
    "logging": {
        "level": "debug",
        "file_enabled": true,
        "console_enabled": true,
        "max_file_size_mb": 10,
        "max_backups": 5
    }
})";

// ============================================================================
// CONFIG CLASS
// ============================================================================

/**
 * @class Config
 * @brief Global configuration manager (Singleton)
 * 
 * @details
 * Thread-safe singleton that manages application configuration.
 * Configuration is stored as JSON and can be accessed using:
 * - Direct JSON access: config.get()
 * - Typed getters: getString(), getInt(), getBool()
 * - Nested paths: "midi.buffer_size", "api.port"
 * 
 * @example Basic usage
 * @code
 * auto& config = Config::instance();
 * config.load("/etc/midimind/config.json");
 * 
 * int bufferSize = config.getInt("midi.buffer_size", 256);
 * std::string logLevel = config.getString("logging.level", "info");
 * bool autoCalib = config.getBool("timing.auto_calibration", true);
 * @endcode
 * 
 * @example Nested path access
 * @code
 * // Access nested values using dot notation
 * std::string dbPath = config.getString("storage.database_path");
 * int apiPort = config.getInt("api.port");
 * @endcode
 */
class Config {
public:
    // ========================================================================
    // SINGLETON
    // ========================================================================
    
    /**
     * @brief Get singleton instance (thread-safe)
     * @return Reference to Config singleton
     */
    static Config& instance() {
        static Config instance;
        return instance;
    }
    
    // Disable copy and move
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;
    
    // ========================================================================
    // LOAD / SAVE
    // ========================================================================
    
    /**
     * @brief Load configuration from JSON file
     * 
     * @param filepath Path to configuration file
     * @return true if loaded successfully, false otherwise
     * 
     * @note If file doesn't exist or is invalid, uses default configuration
     * @note Thread-safe
     * 
     * @example
     * @code
     * if (!Config::instance().load("/etc/midimind/config.json")) {
     *     Logger::warning("Config", "Using default configuration");
     * }
     * @endcode
     */
    bool load(const std::string& filepath) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Logger::info("Config", "Loading configuration from: " + filepath);
        
        // Try to open file
        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::warning("Config", "Cannot open config file, using defaults");
            loadDefaults();
            configPath_ = filepath;
            return false;
        }
        
        try {
            // Parse JSON
            Logger::debug("Config", "Step 1: Parsing JSON...");
            json fileConfig;
            file >> fileConfig;
            file.close();
            Logger::debug("Config", "✓ JSON parsed OK");
            
            // Load defaults first
            Logger::debug("Config", "Step 2: Loading defaults...");
            loadDefaults();
            Logger::debug("Config", "✓ Defaults loaded OK");
            
            // Merge file config with defaults (file config takes precedence)
            Logger::debug("Config", "Step 3: Merging configurations...");
            mergeJson(config_, fileConfig);
            Logger::debug("Config", "✓ Merge OK");
            
            configPath_ = filepath;
            
            // Validate configuration
            Logger::debug("Config", "Step 4: Validating...");
            if (!validate()) {
                Logger::warning("Config", "Configuration validation failed, using defaults");
                loadDefaults();
                return false;
            }
            Logger::debug("Config", "✓ Validation OK");
            
            Logger::info("Config", "✓ Configuration loaded successfully");
            return true;
            
        } catch (const json::exception& e) {
            Logger::error("Config", "JSON parse error: " + std::string(e.what()));
            Logger::warning("Config", "Using default configuration");
            loadDefaults();
            return false;
        } catch (const std::exception& e) {
            Logger::error("Config", "Error loading config: " + std::string(e.what()));
            Logger::warning("Config", "Using default configuration");
            loadDefaults();
            return false;
        }
    }
    
    /**
     * @brief Save configuration to JSON file
     * 
     * @param filepath Path to save config (optional, uses loaded path if empty)
     * @return true if saved successfully
     * 
     * @note Thread-safe
     */
    bool save(const std::string& filepath = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string path = filepath.empty() ? configPath_ : filepath;
        
        if (path.empty()) {
            Logger::error("Config", "No config path specified");
            return false;
        }
        
        try {
            std::ofstream file(path);
            if (!file.is_open()) {
                Logger::error("Config", "Cannot open file for writing: " + path);
                return false;
            }
            
            file << config_.dump(2);  // Pretty print with 2 spaces
            file.close();
            
            Logger::info("Config", "Configuration saved to: " + path);
            return true;
            
        } catch (const std::exception& e) {
            Logger::error("Config", "Save error: " + std::string(e.what()));
            return false;
        }
    }
    
    // ========================================================================
    // GETTERS - TYPED
    // ========================================================================
    
    /**
     * @brief Get string value
     * 
     * @param path Nested path (e.g., "midi.alsa_client_name")
     * @param defaultValue Default if path doesn't exist
     * @return String value or default
     * 
     * @note Thread-safe
     */
    std::string getString(const std::string& path, 
                         const std::string& defaultValue = "") const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            auto value = getValueAtPath(path);
            if (value.is_string()) {
                return value.get<std::string>();
            }
        } catch (...) {
            // Path not found or wrong type
        }
        
        return defaultValue;
    }
    
    /**
     * @brief Get integer value
     * 
     * @param path Nested path (e.g., "midi.buffer_size")
     * @param defaultValue Default if path doesn't exist
     * @return Integer value or default
     * 
     * @note Thread-safe
     */
    int getInt(const std::string& path, int defaultValue = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            auto value = getValueAtPath(path);
            if (value.is_number_integer()) {
                return value.get<int>();
            }
        } catch (...) {
            // Path not found or wrong type
        }
        
        return defaultValue;
    }
    
    /**
     * @brief Get boolean value
     * 
     * @param path Nested path (e.g., "timing.latency_compensation")
     * @param defaultValue Default if path doesn't exist
     * @return Boolean value or default
     * 
     * @note Thread-safe
     */
    bool getBool(const std::string& path, bool defaultValue = false) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            auto value = getValueAtPath(path);
            if (value.is_boolean()) {
                return value.get<bool>();
            }
        } catch (...) {
            // Path not found or wrong type
        }
        
        return defaultValue;
    }
    
    /**
     * @brief Get double value
     * 
     * @param path Nested path (e.g., "timing.max_jitter_ms")
     * @param defaultValue Default if path doesn't exist
     * @return Double value or default
     * 
     * @note Thread-safe
     */
    double getDouble(const std::string& path, double defaultValue = 0.0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            auto value = getValueAtPath(path);
            if (value.is_number()) {
                return value.get<double>();
            }
        } catch (...) {
            // Path not found or wrong type
        }
        
        return defaultValue;
    }
    
    /**
     * @brief Get JSON object/array
     * 
     * @param path Nested path (e.g., "midi" returns entire midi section)
     * @return JSON value or null if not found
     * 
     * @note Thread-safe
     */
    json getJson(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            return getValueAtPath(path);
        } catch (...) {
            return json();  // Return null
        }
    }
    
    // ========================================================================
    // SETTERS
    // ========================================================================
    
    /**
     * @brief Set value at path
     * 
     * @param path Nested path (e.g., "midi.buffer_size")
     * @param value Value to set (any JSON-compatible type)
     * 
     * @note Thread-safe
     * @note Creates intermediate objects if they don't exist
     * 
     * @example
     * @code
     * config.set("midi.buffer_size", 512);
     * config.set("logging.level", "debug");
     * config.set("timing.auto_calibration", false);
     * @endcode
     */
    template<typename T>
    void set(const std::string& path, const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        setValueAtPath(path, value);
    }
    
    // ========================================================================
    // UTILITIES
    // ========================================================================
    
    /**
     * @brief Get entire configuration as JSON
     * @return Copy of configuration JSON
     * @note Thread-safe
     */
    json getAll() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }
    
    /**
     * @brief Check if path exists in configuration
     * @param path Nested path to check
     * @return true if path exists
     * @note Thread-safe
     */
    bool has(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            getValueAtPath(path);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /**
     * @brief Reset to default configuration
     * @note Thread-safe
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        loadDefaults();
        Logger::info("Config", "Configuration reset to defaults");
    }

private:
    // ========================================================================
    // PRIVATE CONSTRUCTOR
    // ========================================================================
    
    Config() {
        loadDefaults();
    }
    
    ~Config() = default;
    
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Load default configuration
     */
    void loadDefaults() {
        try {
            config_ = json::parse(DEFAULT_CONFIG_JSON);
        } catch (const json::exception& e) {
            Logger::error("Config", "Failed to parse default config: " + 
                         std::string(e.what()));
            config_ = json::object();  // Empty object as last resort
        }
    }
    
    /**
     * @brief Get value at nested path
     * @param path Dot-separated path (e.g., "midi.buffer_size")
     * @return JSON value at path
     * @throws json::exception if path not found
     */
    json getValueAtPath(const std::string& path) const {
        auto keys = splitPath(path);
        json current = config_;
        
        for (const auto& key : keys) {
            if (!current.is_object() || !current.contains(key)) {
                throw std::runtime_error("Invalid JSON path: " + path);
            }
            current = current[key];
        }
        
        return current;
    }
    
    /**
     * @brief Set value at nested path
     * @param path Dot-separated path
     * @param value Value to set
     */
    template<typename T>
    void setValueAtPath(const std::string& path, const T& value) {
        auto keys = splitPath(path);
        json* current = &config_;
        
        // Navigate/create path
        for (size_t i = 0; i < keys.size() - 1; ++i) {
            const auto& key = keys[i];
            
            if (!current->is_object()) {
                *current = json::object();
            }
            
            if (!current->contains(key)) {
                (*current)[key] = json::object();
            }
            
            current = &(*current)[key];
        }
        
        // Set final value
        if (!current->is_object()) {
            *current = json::object();
        }
        (*current)[keys.back()] = value;
    }
    
    /**
     * @brief Split path into keys
     * @param path Dot-separated path
     * @return Vector of keys
     */
    std::vector<std::string> splitPath(const std::string& path) const {
        std::vector<std::string> keys;
        std::stringstream ss(path);
        std::string key;
        
        while (std::getline(ss, key, '.')) {
            if (!key.empty()) {
                keys.push_back(key);
            }
        }
        
        return keys;
    }
    
    /**
     * @brief Merge two JSON objects recursively
     * @param base Base object (modified in place)
     * @param overlay Overlay object (takes precedence)
     */
    void mergeJson(json& base, const json& overlay) {
        if (!overlay.is_object()) {
            base = overlay;
            return;
        }
        
        for (auto it = overlay.begin(); it != overlay.end(); ++it) {
            if (base.contains(it.key()) && base[it.key()].is_object() && 
                it.value().is_object()) {
                // Recursive merge for nested objects
                mergeJson(base[it.key()], it.value());
            } else {
                // Direct override
                base[it.key()] = it.value();
            }
        }
    }
    
    /**
     * @brief Validate configuration values
     * @return true if valid
     */
    bool validate() {
        bool valid = true;
        
        // Validate MIDI buffer size (must be power of 2, 32-8192)
        int bufferSize = getInt("midi.buffer_size", 256);
        if (bufferSize < 32 || bufferSize > 8192 || 
            (bufferSize & (bufferSize - 1)) != 0) {
            Logger::warning("Config", "Invalid buffer_size: " + 
                          std::to_string(bufferSize) + ", using 256");
            set("midi.buffer_size", 256);
            valid = false;
        }
        
        // Validate sample rate (8000-192000 Hz)
        int sampleRate = getInt("midi.sample_rate", 44100);
        if (sampleRate < 8000 || sampleRate > 192000) {
            Logger::warning("Config", "Invalid sample_rate: " + 
                          std::to_string(sampleRate) + ", using 44100");
            set("midi.sample_rate", 44100);
            valid = false;
        }
        
        // Validate API port (1024-65535)
        int apiPort = getInt("api.port", 8080);
        if (apiPort < 1024 || apiPort > 65535) {
            Logger::warning("Config", "Invalid API port: " + 
                          std::to_string(apiPort) + ", using 8080");
            set("api.port", 8080);
            valid = false;
        }
        
        // Validate log level
        std::string logLevel = getString("logging.level", "info");
        if (logLevel != "debug" && logLevel != "info" && 
            logLevel != "warning" && logLevel != "error") {
            Logger::warning("Config", "Invalid log level: " + logLevel + 
                          ", using 'info'");
            set("logging.level", "info");
            valid = false;
        }
        
        return valid;
    }
    
    // ========================================================================
    // MEMBERS
    // ========================================================================
    
    mutable std::mutex mutex_;     ///< Thread-safety
    json config_;                   ///< Configuration data
    std::string configPath_;        ///< Path to config file
};

} // namespace midiMind

// ============================================================================
// END OF FILE Config.h v4.1.0
// ============================================================================