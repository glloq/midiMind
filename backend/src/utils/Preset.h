// ============================================================================
// src/utils/Preset.h - Gestion des presets de routage
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <fstream>
#include "../core/Logger.h"

using json = nlohmann::json;

namespace midiMind {

struct PresetEntry {
    int channel;
    std::string fileId;
    std::string deviceName;
    int offsetMs;
    bool muted;
    bool solo;
    float volume;
};

class Preset {
public:
    Preset() = default;
    
    void setName(const std::string& name) { name_ = name; }
    std::string getName() const { return name_; }
    
    void addEntry(int channel, const std::string& fileId, 
                  const std::string& deviceName, int offsetMs = 0,
                  bool muted = false, bool solo = false, float volume = 1.0f) {
        PresetEntry entry;
        entry.channel = channel;
        entry.fileId = fileId;
        entry.deviceName = deviceName;
        entry.offsetMs = offsetMs;
        entry.muted = muted;
        entry.solo = solo;
        entry.volume = volume;
        entries_.push_back(entry);
    }
    
    const std::vector<PresetEntry>& getEntries() const { return entries_; }
    
    bool saveToFile(const std::string& filepath) const {
        try {
            json j;
            j["name"] = name_;
            j["entries"] = json::array();
            
            for (const auto& entry : entries_) {
                json e;
                e["channel"] = entry.channel;
                e["file_id"] = entry.fileId;
                e["device_name"] = entry.deviceName;
                e["offset_ms"] = entry.offsetMs;
                e["muted"] = entry.muted;
                e["solo"] = entry.solo;
                e["volume"] = entry.volume;
                j["entries"].push_back(e);
            }
            
            std::ofstream file(filepath);
            if (!file.is_open()) {
                Logger::error("Preset", "Failed to open file for writing: " + filepath);
                return false;
            }
            
            file << j.dump(2);
            Logger::info("Preset", "Saved preset to " + filepath);
            return true;
            
        } catch (const std::exception& e) {
            Logger::error("Preset", "Failed to save: " + std::string(e.what()));
            return false;
        }
    }
    
    bool loadFromFile(const std::string& filepath) {
        try {
            std::ifstream file(filepath);
            if (!file.is_open()) {
                Logger::error("Preset", "Failed to open file: " + filepath);
                return false;
            }
            
            json j;
            file >> j;
            
            name_ = j.value("name", "Unnamed");
            entries_.clear();
            
            if (j.contains("entries") && j["entries"].is_array()) {
                for (const auto& e : j["entries"]) {
                    PresetEntry entry;
                    entry.channel = e.value("channel", 0);
                    entry.fileId = e.value("file_id", "");
                    entry.deviceName = e.value("device_name", "");
                    entry.offsetMs = e.value("offset_ms", 0);
                    entry.muted = e.value("muted", false);
                    entry.solo = e.value("solo", false);
                    entry.volume = e.value("volume", 1.0f);
                    entries_.push_back(entry);
                }
            }
            
            Logger::info("Preset", "Loaded preset from " + filepath);
            return true;
            
        } catch (const std::exception& e) {
            Logger::error("Preset", "Failed to load: " + std::string(e.what()));
            return false;
        }
    }

private:
    std::string name_;
    std::vector<PresetEntry> entries_;
};

} // namespace midiMind