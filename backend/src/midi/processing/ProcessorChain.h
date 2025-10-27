// ============================================================================
// File: backend/src/midi/processing/ProcessorChain.h
// Version: 4.1.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Chain of MIDI processors
//
// Author: MidiMind Team
// Date: 2025-10-17
//
// Changes v4.1.1:
//   - Enhanced documentation about thread-safety and processor lifecycle
//   - Added warning about recursive chain modifications
//
// ============================================================================

#pragma once

#include "MidiProcessor.h"
#include "../MidiMessage.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @class ProcessorChain
 * @brief Chain of MIDI processors
 * 
 * Processes MIDI messages through a sequence of processors.
 * Thread-safe implementation with minimal lock contention.
 * 
 * Performance: process() copies processor list under lock, then processes
 * without holding the lock to allow concurrent modifications.
 * 
 * Thread Safety Guarantees:
 * - All public methods are thread-safe
 * - Multiple threads can call process() concurrently
 * - Chain can be modified while processing (modifications take effect on next process())
 * - Processors are held via shared_ptr to ensure they remain valid during processing
 * 
 * IMPORTANT WARNINGS:
 * - Processors MUST NOT modify the chain during their process() method
 * - Recursive modifications (processor modifying its own chain) will deadlock
 * - If a processor needs to modify the chain, schedule it for later execution
 * 
 * @example Safe usage
 * @code
 * ProcessorChain chain("MyChain");
 * chain.addProcessor(std::make_shared<TransposeProcessor>(12));
 * chain.addProcessor(std::make_shared<VelocityScaler>(0.8));
 * 
 * // Process message - thread-safe
 * MidiMessage input = ...;
 * auto outputs = chain.process(input);
 * 
 * // Modify chain from another thread - safe
 * chain.removeProcessor(0);
 * @endcode
 * 
 * @example DANGEROUS - DO NOT DO THIS
 * @code
 * class BadProcessor : public MidiProcessor {
 *     ProcessorChain* chain_;
 *     
 *     std::vector<MidiMessage> process(const MidiMessage& input) override {
 *         // DEADLOCK! Never modify chain from within process()
 *         chain_->removeProcessor(0);  // BAD!
 *         return {input};
 *     }
 * };
 * @endcode
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
     * 
     * Thread Safety:
     * - Multiple threads can call process() concurrently
     * - Chain can be modified by other threads during processing
     * - Uses processor snapshot to avoid holding lock during processing
     * 
     * CRITICAL: Processors MUST NOT call any ProcessorChain methods
     * during their process() execution to avoid deadlock.
     */
    std::vector<MidiMessage> process(const MidiMessage& input) {
        // Copy processor list under lock, then process without lock
        // to avoid holding mutex during potentially long processing
        std::vector<std::shared_ptr<MidiProcessor>> processorsCopy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            processorsCopy = processors_;
        }
        
        if (!enabled_.load()) {
            return {input};
        }
        
        std::vector<MidiMessage> messages = {input};
        
        for (auto& processor : processorsCopy) {
            if (!processor || !processor->isEnabled()) {
                continue;
            }
            
            std::vector<MidiMessage> newMessages;
            
            for (const auto& msg : messages) {
                auto outputs = processor->process(msg);
                newMessages.insert(newMessages.end(), 
                                 std::make_move_iterator(outputs.begin()),
                                 std::make_move_iterator(outputs.end()));
            }
            
            messages = std::move(newMessages);
        }
        
        return messages;
    }
    
    /**
     * @brief Add processor to chain
     * @param processor Processor to add (must not be null)
     * @return true on success, false if processor is null
     * 
     * Thread Safety: Safe to call from any thread
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
     * @param index Processor index (0-based)
     * @return true on success, false if index out of bounds
     * 
     * Thread Safety: Safe to call from any thread
     * Note: Currently processing messages will complete with old chain
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
     * @param fromIndex Source index
     * @param toIndex Destination index
     * @return true on success, false if indices out of bounds
     * 
     * Thread Safety: Safe to call from any thread
     */
    bool moveProcessor(size_t fromIndex, size_t toIndex) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (fromIndex >= processors_.size() || toIndex >= processors_.size()) {
            return false;
        }
        
        if (fromIndex == toIndex) {
            return true;
        }
        
        // Use std::rotate for atomic move operation
        if (fromIndex < toIndex) {
            std::rotate(processors_.begin() + fromIndex,
                       processors_.begin() + fromIndex + 1,
                       processors_.begin() + toIndex + 1);
        } else {
            std::rotate(processors_.begin() + toIndex,
                       processors_.begin() + fromIndex,
                       processors_.begin() + fromIndex + 1);
        }
        
        return true;
    }
    
    /**
     * @brief Get processor count
     * @return Number of processors in chain
     * 
     * Thread Safety: Safe to call from any thread
     */
    size_t getProcessorCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return processors_.size();
    }
    
    /**
     * @brief Get processor by index
     * @param index Processor index
     * @return Processor shared_ptr or nullptr if index out of bounds
     * 
     * Thread Safety: Safe to call from any thread
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
     * 
     * Thread Safety: Safe to call from any thread
     * Note: Currently processing messages will complete with old chain
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
     * @brief Check if enabled (thread-safe)
     */
    bool isEnabled() const {
        return enabled_.load();
    }
    
    /**
     * @brief Enable/disable chain (thread-safe)
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
        j["enabled"] = enabled_.load();
        
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
    std::string name_;                                          ///< Chain name
    std::atomic<bool> enabled_;                                 ///< Enable state (atomic)
    std::vector<std::shared_ptr<MidiProcessor>> processors_;    ///< Processors
    mutable std::mutex mutex_;                                  ///< Thread safety for processors_ and name_
};

} // namespace midiMind

// ============================================================================
// END OF FILE ProcessorChain.h v4.1.1
// ============================================================================