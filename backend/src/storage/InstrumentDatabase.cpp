// ============================================================================
// File: backend/src/storage/InstrumentDatabase.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of InstrumentDatabase class.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Initial implementation
//   - CRUD operations with cache
//   - Thread-safe operations
//
// ============================================================================

#include "InstrumentDatabase.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

InstrumentDatabase::InstrumentDatabase(Database& database)
    : database_(database)
{
    Logger::info("InstrumentDatabase", "InstrumentDatabase created");
    loadCache();
}

InstrumentDatabase::~InstrumentDatabase() {
    Logger::debug("InstrumentDatabase", "InstrumentDatabase destroyed");
}

// ============================================================================
// CRUD OPERATIONS
// ============================================================================

bool InstrumentDatabase::createInstrument(const InstrumentLatencyEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("InstrumentDatabase", "Creating instrument: " + entry.id);
    
    try {
        std::string sql = R"(
            INSERT INTO instruments_latency (
                id, device_id, channel, name, instrument_type,
                avg_latency, min_latency, max_latency,
                jitter, std_deviation, measurement_count,
                calibration_confidence, last_calibration, calibration_method,
                compensation_offset, auto_calibration, enabled,
                measurement_history
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";
        
        auto result = database_.execute(sql, {
            entry.id,
            entry.deviceId,
            std::to_string(entry.channel),
            entry.name,
            entry.instrumentType,
            std::to_string(entry.avgLatency),
            std::to_string(entry.minLatency),
            std::to_string(entry.maxLatency),
            std::to_string(entry.jitter),
            std::to_string(entry.stdDeviation),
            std::to_string(entry.measurementCount),
            std::to_string(entry.calibrationConfidence),
            entry.lastCalibration,
            entry.calibrationMethod,
            std::to_string(entry.compensationOffset),
            entry.autoCalibration ? "1" : "0",
            entry.enabled ? "1" : "0",
            entry.measurementHistory
        });
        
        if (result.success) {
            // Update cache
            cache_[entry.id] = entry;
            Logger::info("InstrumentDatabase", "✓ Instrument created: " + entry.id);
            return true;
        } else {
            Logger::error("InstrumentDatabase", "Failed to create: " + result.error);
            return false;
        }
        
    } catch (const std::exception& e) {
        Logger::error("InstrumentDatabase", "Exception: " + std::string(e.what()));
        return false;
    }
}

std::optional<InstrumentLatencyEntry> InstrumentDatabase::getInstrument(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check cache first
    auto it = cache_.find(id);
    if (it != cache_.end()) {
        return it->second;
    }
    
    // Not in cache, query database
    try {
        std::string sql = "SELECT * FROM instruments_latency WHERE id = ?";
        auto result = database_.query(sql, {id});
        
        if (result.success && !result.rows.empty()) {
            auto entry = parseRow(result.rows[0]);
            cache_[id] = entry; // Update cache
            return entry;
        }
        
    } catch (const std::exception& e) {
        Logger::error("InstrumentDatabase", "Failed to get instrument: " + std::string(e.what()));
    }
    
    return std::nullopt;
}

bool InstrumentDatabase::updateInstrument(const InstrumentLatencyEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("InstrumentDatabase", "Updating instrument: " + entry.id);
    
    try {
        std::string sql = R"(
            UPDATE instruments_latency SET
                device_id = ?, channel = ?, name = ?, instrument_type = ?,
                avg_latency = ?, min_latency = ?, max_latency = ?,
                jitter = ?, std_deviation = ?, measurement_count = ?,
                calibration_confidence = ?, last_calibration = ?, calibration_method = ?,
                compensation_offset = ?, auto_calibration = ?, enabled = ?,
                measurement_history = ?
            WHERE id = ?
        )";
        
        auto result = database_.execute(sql, {
            entry.deviceId,
            std::to_string(entry.channel),
            entry.name,
            entry.instrumentType,
            std::to_string(entry.avgLatency),
            std::to_string(entry.minLatency),
            std::to_string(entry.maxLatency),
            std::to_string(entry.jitter),
            std::to_string(entry.stdDeviation),
            std::to_string(entry.measurementCount),
            std::to_string(entry.calibrationConfidence),
            entry.lastCalibration,
            entry.calibrationMethod,
            std::to_string(entry.compensationOffset),
            entry.autoCalibration ? "1" : "0",
            entry.enabled ? "1" : "0",
            entry.measurementHistory,
            entry.id
        });
        
        if (result.success) {
            // Update cache
            cache_[entry.id] = entry;
            Logger::info("InstrumentDatabase", "✓ Instrument updated: " + entry.id);
            return true;
        } else {
            Logger::error("InstrumentDatabase", "Failed to update: " + result.error);
            return false;
        }
        
    } catch (const std::exception& e) {
        Logger::error("InstrumentDatabase", "Exception: " + std::string(e.what()));
        return false;
    }
}

bool InstrumentDatabase::deleteInstrument(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("InstrumentDatabase", "Deleting instrument: " + id);
    
    try {
        std::string sql = "DELETE FROM instruments_latency WHERE id = ?";
        auto result = database_.execute(sql, {id});
        
        if (result.success) {
            // Remove from cache
            cache_.erase(id);
            Logger::info("InstrumentDatabase", "✓ Instrument deleted: " + id);
            return true;
        } else {
            Logger::error("InstrumentDatabase", "Failed to delete: " + result.error);
            return false;
        }
        
    } catch (const std::exception& e) {
        Logger::error("InstrumentDatabase", "Exception: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// QUERY OPERATIONS
// ============================================================================

std::vector<InstrumentLatencyEntry> InstrumentDatabase::listAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<InstrumentLatencyEntry> instruments;
    
    try {
        std::string sql = "SELECT * FROM instruments_latency ORDER BY name";
        auto result = database_.query(sql);
        
        if (result.success) {
            for (const auto& row : result.rows) {
                instruments.push_back(parseRow(row));
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("InstrumentDatabase", "Failed to list all: " + std::string(e.what()));
    }
    
    return instruments;
}

std::vector<InstrumentLatencyEntry> InstrumentDatabase::listByDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<InstrumentLatencyEntry> instruments;
    
    try {
        std::string sql = "SELECT * FROM instruments_latency WHERE device_id = ? ORDER BY channel";
        auto result = database_.query(sql, {deviceId});
        
        if (result.success) {
            for (const auto& row : result.rows) {
                instruments.push_back(parseRow(row));
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("InstrumentDatabase", "Failed to list by device: " + std::string(e.what()));
    }
    
    return instruments;
}

std::vector<InstrumentLatencyEntry> InstrumentDatabase::listByChannel(int channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<InstrumentLatencyEntry> instruments;
    
    try {
        std::string sql = "SELECT * FROM instruments_latency WHERE channel = ? ORDER BY name";
        auto result = database_.query(sql, {std::to_string(channel)});
        
        if (result.success) {
            for (const auto& row : result.rows) {
                instruments.push_back(parseRow(row));
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("InstrumentDatabase", "Failed to list by channel: " + std::string(e.what()));
    }
    
    return instruments;
}

std::vector<InstrumentLatencyEntry> InstrumentDatabase::listEnabled() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<InstrumentLatencyEntry> instruments;
    
    try {
        std::string sql = "SELECT * FROM instruments_latency WHERE enabled = 1 ORDER BY name";
        auto result = database_.query(sql);
        
        if (result.success) {
            for (const auto& row : result.rows) {
                instruments.push_back(parseRow(row));
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("InstrumentDatabase", "Failed to list enabled: " + std::string(e.what()));
    }
    
    return instruments;
}

std::optional<InstrumentLatencyEntry> InstrumentDatabase::getByDeviceAndChannel(
    const std::string& deviceId, int channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::string sql = "SELECT * FROM instruments_latency WHERE device_id = ? AND channel = ?";
        auto result = database_.query(sql, {deviceId, std::to_string(channel)});
        
        if (result.success && !result.rows.empty()) {
            return parseRow(result.rows[0]);
        }
        
    } catch (const std::exception& e) {
        Logger::error("InstrumentDatabase", "Failed to get by device/channel: " + std::string(e.what()));
    }
    
    return std::nullopt;
}

// ============================================================================
// CACHE MANAGEMENT
// ============================================================================

void InstrumentDatabase::refreshCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("InstrumentDatabase", "Refreshing cache...");
    
    cache_.clear();
    loadCache();
}

void InstrumentDatabase::clearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("InstrumentDatabase", "Clearing cache...");
    cache_.clear();
}

// ============================================================================
// STATISTICS
// ============================================================================

json InstrumentDatabase::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int total = cache_.size();
    int enabled = 0;
    int calibrated = 0;
    int highConfidence = 0;
    
    for (const auto& [id, entry] : cache_) {
        if (entry.enabled) enabled++;
        if (entry.measurementCount > 0) calibrated++;
        if (entry.calibrationConfidence >= 0.8) highConfidence++;
    }
    
    return {
        {"total_instruments", total},
        {"enabled_instruments", enabled},
        {"calibrated_instruments", calibrated},
        {"high_confidence_instruments", highConfidence},
        {"cache_size", cache_.size()}
    };
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void InstrumentDatabase::loadCache() {
    Logger::info("InstrumentDatabase", "Loading cache from database...");
    
    try {
        std::string sql = "SELECT * FROM instruments_latency";
        auto result = database_.query(sql);
        
        if (result.success) {
            for (const auto& row : result.rows) {
                auto entry = parseRow(row);
                cache_[entry.id] = entry;
            }
            
            Logger::info("InstrumentDatabase", 
                        "✓ Loaded " + std::to_string(cache_.size()) + " instruments into cache");
        }
        
    } catch (const std::exception& e) {
        Logger::error("InstrumentDatabase", "Failed to load cache: " + std::string(e.what()));
    }
}

InstrumentLatencyEntry InstrumentDatabase::parseRow(const DatabaseRow& row) {
    InstrumentLatencyEntry entry;
    
    entry.id = row.at("id");
    entry.deviceId = row.at("device_id");
    entry.channel = std::stoi(row.at("channel"));
    entry.name = row.count("name") ? row.at("name") : "";
    entry.instrumentType = row.count("instrument_type") ? row.at("instrument_type") : "unknown";
    
    entry.avgLatency = std::stoll(row.at("avg_latency"));
    entry.minLatency = std::stoll(row.at("min_latency"));
    entry.maxLatency = std::stoll(row.at("max_latency"));
    
    entry.jitter = std::stod(row.at("jitter"));
    entry.stdDeviation = std::stod(row.at("std_deviation"));
    entry.measurementCount = std::stoi(row.at("measurement_count"));
    
    entry.calibrationConfidence = std::stod(row.at("calibration_confidence"));
    entry.lastCalibration = row.count("last_calibration") ? row.at("last_calibration") : "";
    entry.calibrationMethod = row.count("calibration_method") ? row.at("calibration_method") : "";
    
    entry.compensationOffset = std::stoll(row.at("compensation_offset"));
    entry.autoCalibration = row.at("auto_calibration") == "1";
    entry.enabled = row.at("enabled") == "1";
    
    entry.measurementHistory = row.count("measurement_history") ? row.at("measurement_history") : "";
    
    entry.createdAt = row.count("created_at") ? row.at("created_at") : "";
    entry.updatedAt = row.count("updated_at") ? row.at("updated_at") : "";
    
    return entry;
}

} // namespace midiMind

// ============================================================================
// END OF FILE InstrumentDatabase.cpp
// ============================================================================