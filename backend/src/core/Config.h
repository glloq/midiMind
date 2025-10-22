// ============================================================================
// File: backend/src/core/Config.h
// Version: 4.1.1 - DEADLOCK FIX
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.1.1:
//   - Fixed deadlock in validate() by adding internal getters without mutex
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
        "level": "info",
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
 * @brief Global configuration management
 * 
 * Singleton class that loads and manages application configuration.
 * Thread-safe with mutex protection.
 */
class Config {
public:
    // ========================================================================
    // SINGLETON
    // ========================================================================
    
    static Config& instance() {
        static Config instance;
        return instance;
    }
    
    // Disable copy/move
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    /**
     * @brief Load configuration from JSON file
     * 
     * @param filepath Path to JSON config file
     * @return true if loaded successfully
     * 
     * @details
     * - Loads defaults first
     * - Merges file config (file takes precedence)
     * - Validates all values
     * - Falls back to defaults if file is invalid
     * 
     * @note Thread-safe
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
            json fileConfig;
            file >> fileConfig;
            file.close();
            
            // Load defaults first
            loadDefaults();
            
            // Merge file config with defaults (file config takes precedence)
            mergeJson(config_, fileConfig);
            
            configPath_ = filepath;
            
            // Validate configuration (uses internal methods without mutex)
            if (!validateInternal()) {
                Logger::warning("Config", "Configuration validation failed, using defaults");
                loadDefaults();
                return false;
            }
            
            Logger::info("Config", "✓ Configuration loaded successfully");
            return true;
            
        } catch (const json::exception& e) {
            Logger::error("Config", "JSON parse error: " + std::string(e.what()));
            Logger::warning("Config", "Using default configuration");
            loadDefaults();
            return false;
        }
    }
    
    /**
     * @brief Save configuration to JSON file
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
            
            file << config_.dump(2);
            file.close();
            
            Logger::info("Config", "Configuration saved to: " + path);
            return true;
            
        } catch (const std::exception& e) {
            Logger::error("Config", "Save error: " + std::string(e.what()));
            return false;
        }
    }
    
    // ========================================================================
    // GETTERS - TYPED (Thread-safe with mutex)
    // ========================================================================
    
    std::string getString(const std::string& path, 
                         const std::string& defaultValue = "") const {
        std::lock_guard<std::mutex> lock(mutex_);
        return getStringInternal(path, defaultValue);
    }
    
    int getInt(const std::string& path, int defaultValue = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return getIntInternal(path, defaultValue);
    }
    
    bool getBool(const std::string& path, bool defaultValue = false) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return getBoolInternal(path, defaultValue);
    }
    
    double getDouble(const std::string& path, double defaultValue = 0.0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return getDoubleInternal(path, defaultValue);
    }
    
    // ========================================================================
    // SETTERS
    // ========================================================================
    
    template<typename T>
    void set(const std::string& path, const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        setValueAtPath(path, value);
    }
    
    // ========================================================================
    // UTILITIES
    // ========================================================================
    
    bool has(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            getValueAtPath(path);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    json getAll() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }
    
    std::string getConfigPath() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return configPath_;
    }

private:
    // ========================================================================
    // CONSTRUCTOR (Private - Singleton)
    // ========================================================================
    
    Config() {
        loadDefaults();
    }
    
    // ========================================================================
    // INTERNAL GETTERS (No mutex - called from already-locked methods)
    // ========================================================================
    
    std::string getStringInternal(const std::string& path, 
                                  const std::string& defaultValue) const {
        try {
            auto value = getValueAtPath(path);
            if (value.is_string()) {
                return value.get<std::string>();
            }
        } catch (...) {}
        return defaultValue;
    }
    
    int getIntInternal(const std::string& path, int defaultValue) const {
        try {
            auto value = getValueAtPath(path);
            if (value.is_number_integer()) {
                return value.get<int>();
            }
        } catch (...) {}
        return defaultValue;
    }
    
    bool getBoolInternal(const std::string& path, bool defaultValue) const {
        try {
            auto value = getValueAtPath(path);
            if (value.is_boolean()) {
                return value.get<bool>();
            }
        } catch (...) {}
        return defaultValue;
    }
    
    double getDoubleInternal(const std::string& path, double defaultValue) const {
        try {
            auto value = getValueAtPath(path);
            if (value.is_number_float()) {
                return value.get<double>();
            } else if (value.is_number_integer()) {
                return static_cast<double>(value.get<int>());
            }
        } catch (...) {}
        return defaultValue;
    }
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    void loadDefaults() {
        try {
            config_ = json::parse(DEFAULT_CONFIG_JSON);
        } catch (const json::exception& e) {
            Logger::error("Config", "Failed to parse default config: " + 
                         std::string(e.what()));
            config_ = json::object();
        }
    }
    
    json getValueAtPath(const std::string& path) const {
        auto keys = splitPath(path);
        const json* current = &config_;
        
        for (const auto& key : keys) {
            if (!current->is_object() || !current->contains(key)) {
                throw std::runtime_error("Path not found: " + path);
            }
            current = &(*current)[key];
        }
        
        return *current;
    }
    
    template<typename T>
    void setValueAtPath(const std::string& path, const T& value) {
        auto keys = splitPath(path);
        json* current = &config_;
        
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
        
        if (!current->is_object()) {
            *current = json::object();
        }
        (*current)[keys.back()] = value;
    }
    
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
    
    void mergeJson(json& base, const json& overlay) {
        if (!overlay.is_object()) {
            base = overlay;
            return;
        }
        
        for (auto it = overlay.begin(); it != overlay.end(); ++it) {
            if (base.contains(it.key()) && base[it.key()].is_object() && 
                it.value().is_object()) {
                mergeJson(base[it.key()], it.value());
            } else {
                base[it.key()] = it.value();
            }
        }
    }
    
    /**
     * @brief Validate configuration (called with mutex already locked)
     */
    bool validateInternal() {
        bool valid = true;
        
        // Validate MIDI buffer size
        int bufferSize = getIntInternal("midi.buffer_size", 256);
        if (bufferSize < 32 || bufferSize > 8192 || 
            (bufferSize & (bufferSize - 1)) != 0) {
            Logger::warning("Config", "Invalid buffer_size: " + 
                          std::to_string(bufferSize) + ", using 256");
            setValueAtPath("midi.buffer_size", 256);
            valid = false;
        }
        
        // Validate sample rate
        int sampleRate = getIntInternal("midi.sample_rate", 44100);
        if (sampleRate < 8000 || sampleRate > 192000) {
            Logger::warning("Config", "Invalid sample_rate: " + 
                          std::to_string(sampleRate) + ", using 44100");
            setValueAtPath("midi.sample_rate", 44100);
            valid = false;
        }
        
        // Validate API port
        int apiPort = getIntInternal("api.port", 8080);
        if (apiPort < 1024 || apiPort > 65535) {
            Logger::warning("Config", "Invalid API port: " + 
                          std::to_string(apiPort) + ", using 8080");
            setValueAtPath("api.port", 8080);
            valid = false;
        }
        
        // Validate log level
        std::string logLevel = getStringInternal("logging.level", "info");
        if (logLevel != "debug" && logLevel != "info" && 
            logLevel != "warning" && logLevel != "error") {
            Logger::warning("Config", "Invalid log level: " + logLevel + 
                          ", using 'info'");
            setValueAtPath("logging.level", "info");
            valid = false;
        }
        
        return valid;
    }
    
    // ========================================================================
    // MEMBERS
    // ========================================================================
    
    mutable std::mutex mutex_;
    json config_;
    std::string configPath_;
};

} // namespace midiMind

// ============================================================================
// END OF FILE Config.h v4.1.1
// ============================================================================