// ============================================================================
// File: backend/src/midi/processing/MidiProcessor.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Abstract base class for all MIDI processors
//
// Author: MidiMind Team
// Date: 2025-10-17
//
// ============================================================================

#pragma once

#include "../MidiMessage.h"
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @class MidiProcessor
 * @brief Abstract base class for MIDI message processors
 * 
 * All MIDI processors must inherit from this class and implement
 * the process() method.
 */
class MidiProcessor {
public:
    /**
     * @brief Constructor
     */
    MidiProcessor(const std::string& name = "Processor")
        : name_(name)
        , enabled_(true)
    {}
    
    /**
     * @brief Virtual destructor
     */
    virtual ~MidiProcessor() = default;
    
    /**
     * @brief Process MIDI message
     * @param input Input message
     * @return Vector of output messages
     */
    virtual std::vector<MidiMessage> process(const MidiMessage& input) = 0;
    
    /**
     * @brief Get processor name
     */
    std::string getName() const { return name_; }
    
    /**
     * @brief Set processor name
     */
    void setName(const std::string& name) { name_ = name; }
    
    /**
     * @brief Check if enabled
     */
    bool isEnabled() const { return enabled_; }
    
    /**
     * @brief Enable/disable processor
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }
    
    /**
     * @brief Set parameter (generic)
     */
    virtual void setParameter(const std::string& name, double value) {
        // Default implementation - override in derived classes
    }
    
    /**
     * @brief Get parameter (generic)
     */
    virtual double getParameter(const std::string& name) const {
        // Default implementation - override in derived classes
        return 0.0;
    }
    
    /**
     * @brief Serialize to JSON
     */
    virtual json toJson() const {
        json j;
        j["name"] = name_;
        j["enabled"] = enabled_;
        return j;
    }
    
    /**
     * @brief Deserialize from JSON
     */
    virtual void fromJson(const json& j) {
        if (j.contains("name")) {
            name_ = j["name"].get<std::string>();
        }
        if (j.contains("enabled")) {
            enabled_ = j["enabled"].get<bool>();
        }
    }
    
    /**
     * @brief Reset processor state
     */
    virtual void reset() {
        // Default implementation - override if needed
    }

protected:
    std::string name_;      ///< Processor name
    bool enabled_;          ///< Enable state
};

} // namespace midiMind

// ============================================================================
// END OF FILE MidiProcessor.h v4.1.0
// ============================================================================