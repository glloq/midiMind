// ============================================================================
// File: backend/src/midi/processing/MidiProcessor.h
// Version: 4.1.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Abstract base class for all MIDI processors
//
// Author: MidiMind Team
// Date: 2025-10-17
//
// Changes v4.1.1:
//   - Removed copy semantics to prevent race conditions with mutex
//   - Processors should be managed via shared_ptr only
//
// ============================================================================

#pragma once

#include "../MidiMessage.h"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @class MidiProcessor
 * @brief Abstract base class for MIDI message processors
 * 
 * All MIDI processors must inherit from this class and implement
 * the process() method.
 * 
 * Thread Safety: 
 * - enabled_ is atomic and thread-safe
 * - name_ is protected by mutex for read/write
 * - Derived classes must ensure their process() implementation is thread-safe
 * 
 * Ownership:
 * - Processors should be managed via shared_ptr
 * - Copy operations are disabled to prevent race conditions with mutex
 * - Use shared_ptr to share processor instances
 */
class MidiProcessor {
public:
    /**
     * @brief Constructor
     * @param name Processor name
     */
    explicit MidiProcessor(const std::string& name = "Processor")
        : name_(name)
        , enabled_(true)
    {}
    
    /**
     * @brief Virtual destructor
     */
    virtual ~MidiProcessor() = default;
    
    // Copy semantics disabled - processors contain mutex which is not copyable
    // Use shared_ptr to share processor instances
    MidiProcessor(const MidiProcessor&) = delete;
    MidiProcessor& operator=(const MidiProcessor&) = delete;
    
    // Move semantics
    MidiProcessor(MidiProcessor&& other) noexcept
        : name_(std::move(other.name_))
        , enabled_(other.enabled_.load())
    {}
    
    MidiProcessor& operator=(MidiProcessor&& other) noexcept {
        if (this != &other) {
            name_ = std::move(other.name_);
            enabled_ = other.enabled_.load();
        }
        return *this;
    }
    
    /**
     * @brief Process MIDI message
     * @param input Input message
     * @return Vector of output messages (uses move semantics)
     * 
     * Note: Returns vector by value to allow RVO/move optimization.
     * Typically returns 0-1 messages, making this efficient.
     * 
     * Thread Safety: Derived classes must implement thread-safe process()
     */
    virtual std::vector<MidiMessage> process(const MidiMessage& input) = 0;
    
    /**
     * @brief Get processor name (thread-safe)
     */
    std::string getName() const { 
        std::lock_guard<std::mutex> lock(nameMutex_);
        return name_; 
    }
    
    /**
     * @brief Set processor name (thread-safe)
     */
    void setName(const std::string& name) { 
        std::lock_guard<std::mutex> lock(nameMutex_);
        name_ = name; 
    }
    
    /**
     * @brief Check if enabled (thread-safe)
     */
    bool isEnabled() const { 
        return enabled_.load(); 
    }
    
    /**
     * @brief Enable/disable processor (thread-safe)
     */
    void setEnabled(bool enabled) { 
        enabled_ = enabled; 
    }
    
    /**
     * @brief Set parameter (generic)
     * @param name Parameter name
     * @param value Parameter value
     * 
     * Default implementation does nothing. Override in derived classes
     * to support parameters.
     */
    virtual void setParameter(const std::string& name, double value) {
        // Default implementation - override in derived classes
    }
    
    /**
     * @brief Get parameter (generic)
     * @param name Parameter name
     * @return Parameter value (0.0 if not found)
     * 
     * Default implementation returns 0.0. Override in derived classes
     * to support parameters.
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
        j["name"] = getName();  // Thread-safe getter
        j["enabled"] = isEnabled();  // Thread-safe getter
        return j;
    }
    
    /**
     * @brief Deserialize from JSON
     */
    virtual void fromJson(const json& j) {
        if (j.contains("name")) {
            setName(j["name"].get<std::string>());  // Thread-safe setter
        }
        if (j.contains("enabled")) {
            setEnabled(j["enabled"].get<bool>());  // Thread-safe setter
        }
    }
    
    /**
     * @brief Reset processor state
     * 
     * Default implementation does nothing. Override in derived classes
     * if state needs to be reset.
     */
    virtual void reset() {
        // Default implementation - override if needed
    }

protected:
    std::string name_;              ///< Processor name
    mutable std::mutex nameMutex_;  ///< Mutex for name_ access
    std::atomic<bool> enabled_;     ///< Enable state (atomic for thread-safety)
};

} // namespace midiMind

// ============================================================================
// END OF FILE MidiProcessor.h v4.1.1
// ============================================================================