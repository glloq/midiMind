// ============================================================================
// File: backend/src/midi/processing/ProcessorChain.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Chain of MIDI processors
//
// Author: MidiMind Team
// Date: 2025-10-17
//
// ============================================================================

#pragma once

#include "MidiProcessor.h"
#include "../MidiMessage.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @class ProcessorChain
 * @brief Chain of MIDI processors
 * 
 * Processes MIDI messages through a sequence of processors.
 * Thread-safe implementation.
 */
class ProcessorChain {
public:
    /**
     * @brief Constructor
     */
    ProcessorChain(const std::string& name = "Chain")
        : name_(name)
        , enabled_(true)
    {}
    
    /**
     * @brief Destructor
     */
    ~ProcessorChain() = default;
    
    /**
     * @brief Process MIDI message through chain
     * @param input Input message
     * @return Vector of output messages
     */
    std::vector<MidiMessage> process(const MidiMessage& input) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!enabled_) {
            return {input};
        }
        
        std::vector<MidiMessage> messages = {input};
        
        for (auto& processor : processors_) {
            if (!processor || !processor->isEnabled()) {
                continue;
            }
            
            std::vector<MidiMessage> newMessages;
            
            for (const auto& msg : messages) {
                auto outputs = processor->process(msg);
                newMessages.insert(newMessages.end(), outputs.begin(), outputs.end());
            }
            
            messages = std::move(newMessages);
        }
        
        return messages;
    }
    
    /**
     * @brief Add processor to chain
     */
    bool addProcessor(std::shared_ptr<MidiProcessor> processor) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!processor) {
            return false;
        }
        
        processors_.push_back(processor);
        return true;
    }
    
    /**
     * @brief Remove processor from chain
     */
    bool removeProcessor(size_t index) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (index >= processors_.size()) {
            return false;
        }
        
        processors_.erase(processors_.begin() + index);
        return true;
    }
    
    /**
     * @brief Move processor within chain
     */
    bool moveProcessor(size_t fromIndex, size_t toIndex) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (fromIndex >= processors_.size() || toIndex >= processors_.size()) {
            return false;
        }
        
        auto processor = processors_[fromIndex];
        processors_.erase(processors_.begin() + fromIndex);
        processors_.insert(processors_.begin() + toIndex, processor);
        return true;
    }
    
    /**
     * @brief Get processor count
     */
    size_t getProcessorCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return processors_.size();
    }
    
    /**
     * @brief Get processor by index
     */
    std::shared_ptr<MidiProcessor> getProcessor(size_t index) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (index >= processors_.size()) {
            return nullptr;
        }
        
        return processors_[index];
    }
    
    /**
     * @brief Clear all processors
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        processors_.clear();
    }
    
    /**
     * @brief Get chain name
     */
    std::string getName() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return name_;
    }
    
    /**
     * @brief Set chain name
     */
    void setName(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        name_ = name;
    }
    
    /**
     * @brief Check if enabled
     */
    bool isEnabled() const {
        return enabled_;
    }
    
    /**
     * @brief Enable/disable chain
     */
    void setEnabled(bool enabled) {
        enabled_ = enabled;
    }
    
    /**
     * @brief Serialize to JSON
     */
    json toJson() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        json j;
        j["name"] = name_;
        j["enabled"] = enabled_;
        
        json processorsJson = json::array();
        for (const auto& processor : processors_) {
            if (processor) {
                processorsJson.push_back(processor->toJson());
            }
        }
        j["processors"] = processorsJson;
        
        return j;
    }
    
    /**
     * @brief Deserialize from JSON
     */
    void fromJson(const json& j) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (j.contains("name")) {
            name_ = j["name"].get<std::string>();
        }
        if (j.contains("enabled")) {
            enabled_ = j["enabled"].get<bool>();
        }
        
        // Note: Processors are created by ProcessorManager
        // This method just loads the configuration
    }

private:
    std::string name_;                                    ///< Chain name
    bool enabled_;                                        ///< Enable state
    std::vector<std::shared_ptr<MidiProcessor>> processors_;  ///< Processors
    mutable std::mutex mutex_;                            ///< Thread safety
};

} // namespace midiMind

// ============================================================================
// END OF FILE ProcessorChain.h v4.1.0
// ============================================================================