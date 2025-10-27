// ============================================================================
// File: backend/src/midi/processing/ProcessorManager.h
// Version: 4.1.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Central manager for MIDI processor chains (fixed with missing methods)
//
// Author: MidiMind Team
// Date: 2025-10-17
//
// Changes v4.1.1:
//   - Added missing savePreset() method
//   - Added missing deletePreset() method
//   - Thread-safe callback handling
//
// ============================================================================

#pragma once

#include "../MidiMessage.h"
#include "ProcessorChain.h"
#include "MidiProcessor.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// ENUMS
// ============================================================================

/**
 * @enum ProcessorType
 * @brief Type of MIDI processor
 */
enum class ProcessorType {
    TRANSPOSE,          ///< Transpose notes
    VELOCITY,           ///< Modify velocity
    CHANNEL_FILTER,     ///< Filter by channel
    NOTE_FILTER,        ///< Filter by note range
    ARPEGGIATOR,        ///< Arpeggiator
    DELAY,              ///< MIDI delay
    CHORD,              ///< Chord generator
    HARMONIZER,         ///< Harmonizer
    QUANTIZE,           ///< Timing quantize
    RANDOMIZE           ///< Randomize parameters
};

// ============================================================================
// CLASS: ProcessorManager
// ============================================================================

/**
 * @class ProcessorManager
 * @brief Central manager for MIDI processor chains
 * 
 * Thread Safety: Methods are thread-safe. Callbacks are invoked without
 * holding internal locks to prevent deadlocks.
 * 
 * Note: Processor creation methods (createProcessor, createProcessorFromType)
 * are currently stubs returning nullptr. They will be implemented when
 * concrete processor classes are available.
 */
class ProcessorManager {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    using MessageOutputCallback = std::function<void(const MidiMessage&, 
                                                     const std::string& chainId)>;
    
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    ProcessorManager();
    ~ProcessorManager();
    
    // Disable copy
    ProcessorManager(const ProcessorManager&) = delete;
    ProcessorManager& operator=(const ProcessorManager&) = delete;
    
    // ========================================================================
    // MESSAGE PROCESSING
    // ========================================================================
    
    std::vector<MidiMessage> processMessage(const MidiMessage& input,
                                           const std::string& chainId);
    
    std::map<std::string, std::vector<MidiMessage>> processMessageAllChains(
        const MidiMessage& input);
    
    // ========================================================================
    // CHAIN MANAGEMENT
    // ========================================================================
    
    std::string createChain(const std::string& name);
    bool deleteChain(const std::string& chainId);
    std::shared_ptr<ProcessorChain> getChain(const std::string& chainId) const;
    std::vector<std::string> listChains() const;
    bool renameChain(const std::string& chainId, const std::string& newName);
    bool setChainEnabled(const std::string& chainId, bool enabled);
    
    // ========================================================================
    // PROCESSOR MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Create processor instance
     * @param type Processor type
     * @param config Configuration JSON
     * @return Processor instance or nullptr if type not implemented yet
     * 
     * NOTE: This is currently a stub returning nullptr. Will be implemented
     * when concrete processor classes are available.
     */
    std::shared_ptr<MidiProcessor> createProcessor(ProcessorType type,
                                                   const json& config = json());
    
    bool addProcessorToChain(const std::string& chainId,
                            std::shared_ptr<MidiProcessor> processor);
    
    bool removeProcessorFromChain(const std::string& chainId,
                                  size_t processorIndex);
    
    bool moveProcessor(const std::string& chainId, 
                      size_t fromIndex, 
                      size_t toIndex);
    
    // ========================================================================
    // PRESETS
    // ========================================================================
    
    /**
     * @brief Load preset and create chain
     * @param presetName Preset name
     * @return Chain ID or empty string on error
     */
    std::string loadPreset(const std::string& presetName);
    
    /**
     * @brief List available presets
     * @return Vector of preset names
     */
    std::vector<std::string> listPresets() const;
    
    /**
     * @brief Save chain configuration as preset
     * @param chainId Chain ID to save
     * @param presetName Name for the preset
     * @return true if saved successfully
     */
    bool savePreset(const std::string& chainId, const std::string& presetName);
    
    /**
     * @brief Delete a preset
     * @param presetName Preset name to delete
     * @return true if deleted successfully
     */
    bool deletePreset(const std::string& presetName);
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    bool saveToFile(const std::string& filepath) const;
    bool loadFromFile(const std::string& filepath);
    json toJson() const;
    void fromJson(const json& j);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    void setMessageOutputCallback(MessageOutputCallback callback);
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    json getStatistics() const;
    void resetStatistics();

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    std::string generateChainId();
    void initializePresets();
    
    /**
     * @brief Create processor from type string
     * @param type Type string (e.g. "transpose", "velocity")
     * @return Processor instance or nullptr if not implemented yet
     * 
     * NOTE: Currently a stub returning nullptr.
     */
    std::shared_ptr<MidiProcessor> createProcessorFromType(const std::string& type);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    std::map<std::string, std::shared_ptr<ProcessorChain>> chains_;
    std::map<std::string, json> presets_;
    mutable std::mutex mutex_;
    std::atomic<uint32_t> chainIdCounter_;
    std::atomic<uint64_t> messagesProcessed_;
    
    // Callback protected by separate mutex to avoid deadlock
    std::mutex callbackMutex_;
    MessageOutputCallback messageOutputCallback_;
};

} // namespace midiMind

// ============================================================================
// END OF FILE ProcessorManager.h v4.1.1
// ============================================================================