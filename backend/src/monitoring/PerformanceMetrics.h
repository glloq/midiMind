// ============================================================================
// Fichier: src/monitoring/PerformanceMetrics.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Structures de données pour les métriques de performance.
//   Définit tous les types de métriques collectées par le système.
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @struct SystemMetrics
 * @brief Métriques système (CPU, RAM, etc.)
 */
struct SystemMetrics {
    /// CPU
    float cpuUsagePercent;          ///< Utilisation CPU globale (%)
    float cpuTemperature;           ///< Température CPU (°C)
    uint32_t cpuFrequencyMHz;       ///< Fréquence CPU (MHz)
    
    /// Mémoire
    uint64_t ramTotalBytes;         ///< RAM totale (bytes)
    uint64_t ramUsedBytes;          ///< RAM utilisée (bytes)
    uint64_t ramFreeBytes;          ///< RAM libre (bytes)
    float ramUsagePercent;          ///< Utilisation RAM (%)
    
    /// Disque
    uint64_t diskTotalBytes;        ///< Disque total (bytes)
    uint64_t diskUsedBytes;         ///< Disque utilisé (bytes)
    uint64_t diskFreeBytes;         ///< Disque libre (bytes)
    float diskUsagePercent;         ///< Utilisation disque (%)
    
    /// Réseau
    uint64_t networkBytesReceived;  ///< Bytes reçus
    uint64_t networkBytesSent;      ///< Bytes envoyés
    
    /// Timestamp
    uint64_t timestamp;             ///< Timestamp (ms)
    
    /**
     * @brief Constructeur par défaut
     */
    SystemMetrics()
        : cpuUsagePercent(0.0f)
        , cpuTemperature(0.0f)
        , cpuFrequencyMHz(0)
        , ramTotalBytes(0)
        , ramUsedBytes(0)
        , ramFreeBytes(0)
        , ramUsagePercent(0.0f)
        , diskTotalBytes(0)
        , diskUsedBytes(0)
        , diskFreeBytes(0)
        , diskUsagePercent(0.0f)
        , networkBytesReceived(0)
        , networkBytesSent(0)
        , timestamp(0) {}
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        
        j["cpu"]["usage_percent"] = cpuUsagePercent;
        j["cpu"]["temperature_celsius"] = cpuTemperature;
        j["cpu"]["frequency_mhz"] = cpuFrequencyMHz;
        
        j["ram"]["total_bytes"] = ramTotalBytes;
        j["ram"]["used_bytes"] = ramUsedBytes;
        j["ram"]["free_bytes"] = ramFreeBytes;
        j["ram"]["usage_percent"] = ramUsagePercent;
        
        j["disk"]["total_bytes"] = diskTotalBytes;
        j["disk"]["used_bytes"] = diskUsedBytes;
        j["disk"]["free_bytes"] = diskFreeBytes;
        j["disk"]["usage_percent"] = diskUsagePercent;
        
        j["network"]["bytes_received"] = networkBytesReceived;
        j["network"]["bytes_sent"] = networkBytesSent;
        
        j["timestamp"] = timestamp;
        
        return j;
    }
};

/**
 * @struct LatencyMetrics
 * @brief Métriques de latence MIDI
 */
struct LatencyMetrics {
    /// Latences (microsecondes)
    float currentLatencyUs;         ///< Latence actuelle (µs)
    float averageLatencyUs;         ///< Latence moyenne (µs)
    float minLatencyUs;             ///< Latence minimale (µs)
    float maxLatencyUs;             ///< Latence maximale (µs)
    float jitterUs;                 ///< Jitter (µs)
    
    /// Compteurs
    uint64_t messageCount;          ///< Nombre de messages traités
    uint64_t droppedMessages;       ///< Messages perdus
    
    /// Timestamp
    uint64_t timestamp;             ///< Timestamp (ms)
    
    /**
     * @brief Constructeur par défaut
     */
    LatencyMetrics()
        : currentLatencyUs(0.0f)
        , averageLatencyUs(0.0f)
        , minLatencyUs(0.0f)
        , maxLatencyUs(0.0f)
        , jitterUs(0.0f)
        , messageCount(0)
        , droppedMessages(0)
        , timestamp(0) {}
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        j["current_latency_us"] = currentLatencyUs;
        j["average_latency_us"] = averageLatencyUs;
        j["min_latency_us"] = minLatencyUs;
        j["max_latency_us"] = maxLatencyUs;
        j["jitter_us"] = jitterUs;
        j["message_count"] = messageCount;
        j["dropped_messages"] = droppedMessages;
        j["timestamp"] = timestamp;
        return j;
    }
};

/**
 * @struct MidiMetrics
 * @brief Métriques MIDI
 */
struct MidiMetrics {
    /// Compteurs globaux
    uint64_t messagesReceived;      ///< Messages reçus
    uint64_t messagesSent;          ///< Messages envoyés
    uint64_t messagesProcessed;     ///< Messages traités
    uint64_t messagesDropped;       ///< Messages perdus
    
    /// Par type de message
    uint64_t noteOnCount;           ///< Note On
    uint64_t noteOffCount;          ///< Note Off
    uint64_t controlChangeCount;    ///< Control Change
    uint64_t programChangeCount;    ///< Program Change
    uint64_t sysexCount;            ///< SysEx
    
    /// Débit
    float messagesPerSecond;        ///< Messages/seconde
    float bytesPerSecond;           ///< Bytes/seconde
    
    /// Timestamp
    uint64_t timestamp;             ///< Timestamp (ms)
    
    /**
     * @brief Constructeur par défaut
     */
    MidiMetrics()
        : messagesReceived(0)
        , messagesSent(0)
        , messagesProcessed(0)
        , messagesDropped(0)
        , noteOnCount(0)
        , noteOffCount(0)
        , controlChangeCount(0)
        , programChangeCount(0)
        , sysexCount(0)
        , messagesPerSecond(0.0f)
        , bytesPerSecond(0.0f)
        , timestamp(0) {}
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        j["messages_received"] = messagesReceived;
        j["messages_sent"] = messagesSent;
        j["messages_processed"] = messagesProcessed;
        j["messages_dropped"] = messagesDropped;
        
        j["by_type"]["note_on"] = noteOnCount;
        j["by_type"]["note_off"] = noteOffCount;
        j["by_type"]["control_change"] = controlChangeCount;
        j["by_type"]["program_change"] = programChangeCount;
        j["by_type"]["sysex"] = sysexCount;
        
        j["throughput"]["messages_per_second"] = messagesPerSecond;
        j["throughput"]["bytes_per_second"] = bytesPerSecond;
        
        j["timestamp"] = timestamp;
        
        return j;
    }
};

/**
 * @struct ApplicationMetrics
 * @brief Métriques de l'application
 */
struct ApplicationMetrics {
    /// Uptime
    uint64_t uptimeSeconds;         ///< Uptime (secondes)
    
    /// Composants
    uint32_t activeMidiDevices;     ///< Devices MIDI actifs
    uint32_t activeNetworkSessions; ///< Sessions réseau actives
    uint32_t activeProcessorChains; ///< Chaînes de processors actives
    uint32_t activeMidiRoutes;      ///< Routes MIDI actives
    
    /// Files MIDI
    uint32_t loadedMidiFiles;       ///< Fichiers MIDI chargés
    bool playerActive;              ///< Player actif
    
    /// Réseau
    bool rtpMidiActive;             ///< RTP-MIDI actif
    bool bleMidiActive;             ///< BLE MIDI actif
    bool wifiHotspotActive;         ///< WiFi Hotspot actif
    
    /// Timestamp
    uint64_t timestamp;             ///< Timestamp (ms)
    
    /**
     * @brief Constructeur par défaut
     */
    ApplicationMetrics()
        : uptimeSeconds(0)
        , activeMidiDevices(0)
        , activeNetworkSessions(0)
        , activeProcessorChains(0)
        , activeMidiRoutes(0)
        , loadedMidiFiles(0)
        , playerActive(false)
        , rtpMidiActive(false)
        , bleMidiActive(false)
        , wifiHotspotActive(false)
        , timestamp(0) {}
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        j["uptime_seconds"] = uptimeSeconds;
        
        j["components"]["midi_devices"] = activeMidiDevices;
        j["components"]["network_sessions"] = activeNetworkSessions;
        j["components"]["processor_chains"] = activeProcessorChains;
        j["components"]["midi_routes"] = activeMidiRoutes;
        
        j["midi_files"]["loaded"] = loadedMidiFiles;
        j["midi_files"]["player_active"] = playerActive;
        
        j["network"]["rtp_midi_active"] = rtpMidiActive;
        j["network"]["ble_midi_active"] = bleMidiActive;
        j["network"]["wifi_hotspot_active"] = wifiHotspotActive;
        
        j["timestamp"] = timestamp;
        
        return j;
    }
};

/**
 * @struct HealthStatus
 * @brief État de santé du système
 */
enum class HealthLevel {
    HEALTHY,        ///< Système sain
    WARNING,        ///< Avertissement
    CRITICAL,       ///< Critique
    ERROR           ///< Erreur
};

struct HealthStatus {
    HealthLevel level;              ///< Niveau de santé
    std::string message;            ///< Message descriptif
    std::vector<std::string> issues;///< Liste des problèmes
    uint64_t timestamp;             ///< Timestamp (ms)
    
    /**
     * @brief Constructeur par défaut
     */
    HealthStatus()
        : level(HealthLevel::HEALTHY)
        , timestamp(0) {}
    
    /**
     * @brief Constructeur
     */
    HealthStatus(HealthLevel lvl, const std::string& msg)
        : level(lvl)
        , message(msg)
        , timestamp(getCurrentTimestamp()) {}
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        j["level"] = healthLevelToString(level);
        j["message"] = message;
        j["issues"] = issues;
        j["timestamp"] = timestamp;
        return j;
    }
    
    /**
     * @brief Vérifie si le système est sain
     */
    bool isHealthy() const {
        return level == HealthLevel::HEALTHY;
    }

private:
    static uint64_t getCurrentTimestamp() {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
    
    static std::string healthLevelToString(HealthLevel level) {
        switch (level) {
            case HealthLevel::HEALTHY: return "healthy";
            case HealthLevel::WARNING: return "warning";
            case HealthLevel::CRITICAL: return "critical";
            case HealthLevel::ERROR: return "error";
            default: return "unknown";
        }
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER PerformanceMetrics.h
// ============================================================================