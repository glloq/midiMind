// ============================================================================
// File: backend/src/midi/processing/ProcessorManager.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Central manager for MIDI processor chains
//
// Features:
//   - Create and manage processor chains
//   - Factory for processor creation
//   - Route messages through chains
//   - Save/load configurations
//   - Preset management
//   - Statistics and monitoring
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Simplified architecture
//   - Better chain management
//   - Enhanced error handling
//   - Improved preset system
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
 * Manages multiple processor chains and routes MIDI messages.
 * Acts as a facade for all MIDI processing operations.
 * 
 * Architecture:
 * ```
 * MidiRouter → ProcessorManager → ProcessorChain → Processors → Output
 *                              ↓
 *                      [Chain1, Chain2, ...]
 * ```
 * 
 * Thread Safety: YES (all public methods are thread-safe)
 * 
 * Example:
 * ```cpp
 * auto manager = std::make_shared<ProcessorManager>();
 * 
 * // Create chain
 * auto chainId = manager->createChain("Lead Synth");
 * 
 * // Add processors
 * auto transpose = manager->createProcessor(ProcessorType::TRANSPOSE);
 * transpose->setParameter("semitones", 12);
 * manager->addProcessorToChain(chainId, transpose);
 * 
 * // Process message
 * auto outputs = manager->processMessage(noteOn, chainId);
 * 
 * // Save configuration
 * manager->saveToFile("config.json");
 * ```
 */
class ProcessorManager {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback for processed messages
     */
    using MessageOutputCallback = std::function<void(const MidiMessage&, 
                                                     const std::string& chainId)>;
    
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     */
    ProcessorManager();
    
    /**
     * @brief Destructor
     */
    ~ProcessorManager();
    
    // Disable copy
    ProcessorManager(const ProcessorManager&) = delete;
    ProcessorManager& operator=(const ProcessorManager&) = delete;
    
    // ========================================================================
    // MESSAGE PROCESSING
    // ========================================================================
    
    /**
     * @brief Process message through a chain
     * @param input Input message
     * @param chainId Chain ID
     * @return Vector of output messages
     * @note Thread-safe
     */
    std::vector<MidiMessage> processMessage(const MidiMessage& input,
                                           const std::string& chainId);
    
    /**
     * @brief Process message through all active chains
     * @param input Input message
     * @return Map of chain ID to output messages
     * @note Thread-safe
     */
    std::map<std::string, std::vector<MidiMessage>> processMessageAllChains(
        const MidiMessage& input);
    
    // ========================================================================
    // CHAIN MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Create new processor chain
     * @param name Chain name
     * @return Chain ID
     * @note Thread-safe
     */
    std::string createChain(const std::string& name);
    
    /**
     * @brief Delete chain
     * @param chainId Chain ID
     * @return true if deleted
     * @note Thread-safe
     */
    bool deleteChain(const std::string& chainId);
    
    /**
     * @brief Get chain
     * @param chainId Chain ID
     * @return Shared pointer to chain or nullptr
     * @note Thread-safe
     */
    std::shared_ptr<ProcessorChain> getChain(const std::string& chainId) const;
    
    /**
     * @brief List all chains
     * @return Vector of chain IDs
     * @note Thread-safe
     */
    std::vector<std::string> listChains() const;
    
    /**
     * @brief Rename chain
     * @param chainId Chain ID
     * @param newName New name
     * @return true if renamed
     * @note Thread-safe
     */
    bool renameChain(const std::string& chainId, const std::string& newName);
    
    /**
     * @brief Enable/disable chain
     * @param chainId Chain ID
     * @param enabled Enable state
     * @return true if changed
     * @note Thread-safe
     */
    bool setChainEnabled(const std::string& chainId, bool enabled);
    
    // ========================================================================
    // PROCESSOR MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Create processor (factory)
     * @param type Processor type
     * @param config Initial configuration (optional)
     * @return Shared pointer to processor
     * @note Thread-safe
     */
    std::shared_ptr<MidiProcessor> createProcessor(ProcessorType type,
                                                   const json& config = json());
    
    /**
     * @brief Add processor to chain
     * @param chainId Chain ID
     * @param processor Processor to add
     * @return true if added
     * @note Thread-safe
     */
    bool addProcessorToChain(const std::string& chainId,
                            std::shared_ptr<MidiProcessor> processor);
    
    /**
     * @brief Remove processor from chain
     * @param chainId Chain ID
     * @param processorIndex Processor index
     * @return true if removed
     * @note Thread-safe
     */
    bool removeProcessorFromChain(const std::string& chainId,
                                  size_t processorIndex);
    
    /**
     * @brief Move processor within chain
     * @param chainId Chain ID
     * @param fromIndex Source index
     * @param toIndex Target index
     * @return true if moved
     * @note Thread-safe
     */
    bool moveProcessor(const std::string& chainId, 
                      size_t fromIndex, 
                      size_t toIndex);
    
    // ========================================================================
    // PRESETS
    // ========================================================================
    
    /**
     * @brief Load preset chain
     * 
     * Available presets:
     * - "transpose_up": Transpose +7 semitones
     * - "lead_synth": Transpose + Velocity boost
     * - "piano_chords": Chord processor (Major7)
     * - "arp_sequence": Arpeggiator + Delay
     * 
     * @param presetName Preset name
     * @return Chain ID
     * @note Thread-safe
     */
    std::string loadPreset(const std::string& presetName);
    
    /**
     * @brief List available presets
     * @return Vector of preset names
     */
    std::vector<std::string> listPresets() const;
    
    /**
     * @brief Save chain as preset
     * @param chainId Chain ID
     * @param presetName Preset name
     * @return true if saved
     * @note Thread-safe
     */
    bool saveAsPreset(const std::string& chainId, 
                     const std::string& presetName);
    
    // ========================================================================
    // SERIALIZATION
    // ========================================================================
    
    /**
     * @brief Save all chains to file
     * @param filepath File path
     * @return true if saved
     * @note Thread-safe
     */
    bool saveToFile(const std::string& filepath) const;
    
    /**
     * @brief Load chains from file
     * @param filepath File path
     * @return true if loaded
     * @note Thread-safe
     */
    bool loadFromFile(const std::string& filepath);
    
    /**
     * @brief Export to JSON
     * @return JSON configuration
     * @note Thread-safe
     */
    json toJson() const;
    
    /**
     * @brief Import from JSON
     * @param j JSON configuration
     * @note Thread-safe
     */
    void fromJson(const json& j);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Set message output callback
     * @param callback Callback function
     * @note Thread-safe
     */
    void setMessageOutputCallback(MessageOutputCallback callback);
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get statistics
     * @return JSON statistics
     * @note Thread-safe
     */
    json getStatistics() const;
    
    /**
     * @brief Reset statistics
     * @note Thread-safe
     */
    void resetStatistics();

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Generate unique chain ID
     */
    std::string generateChainId();
    
    /**
     * @brief Initialize built-in presets
     */
    void initializePresets();
    
    /**
     * @brief Create processor from type string
     */
    std::shared_ptr<MidiProcessor> createProcessorFromType(
        const std::string& type);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Processor chains (chainId -> chain)
    std::map<std::string, std::shared_ptr<ProcessorChain>> chains_;
    
    /// Preset configurations
    std::map<std::string, json> presets_;
    
    /// Thread safety
    mutable std::mutex mutex_;
    
    /// Chain ID counter
    std::atomic<uint32_t> chainIdCounter_;
    
    /// Statistics
    std::atomic<uint64_t> messagesProcessed_;
    
    /// Message output callback
    MessageOutputCallback messageOutputCallback_;
};

} // namespace midiMind