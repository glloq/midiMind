// ============================================================================
// File: backend/src/midi/processing/ProcessorManager.cpp
// Version: 4.1.2 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.1.2:
//   - Fixed deadlock issues with callbacks
//   - Fixed double-locking issues with chain methods
//   - Removed unnecessary empty loop in resetStatistics
//   - process() no longer holds mutex during long operations
//
// ============================================================================

#include "ProcessorManager.h"
#include "../../core/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

ProcessorManager::ProcessorManager()
    : chainIdCounter_(0)
    , messagesProcessed_(0)
{
    Logger::info("ProcessorManager", "========================================");
    Logger::info("ProcessorManager", "  Initializing ProcessorManager");
    Logger::info("ProcessorManager", "========================================");
    
    // Safe to call without mutex - we're in constructor
    initializePresets();
    
    Logger::info("ProcessorManager", "✓ ProcessorManager initialized");
}

ProcessorManager::~ProcessorManager() {
    Logger::info("ProcessorManager", "ProcessorManager destroyed");
}

// ============================================================================
// MESSAGE PROCESSING
// ============================================================================

std::vector<MidiMessage> ProcessorManager::processMessage(
    const MidiMessage& input,
    const std::string& chainId)
{
    // FIX: Copy chain pointer under lock, then process without lock
    std::shared_ptr<ProcessorChain> chain;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = chains_.find(chainId);
        if (it == chains_.end()) {
            Logger::warning("ProcessorManager", "Chain not found: " + chainId);
            return {input};
        }
        
        chain = it->second;
        if (!chain) {
            return {input};
        }
    }
    
    // Process without holding mutex_ (chain->process is thread-safe)
    if (!chain->isEnabled()) {
        return {input};
    }
    
    auto outputs = chain->process(input);
    messagesProcessed_++;
    
    // FIX: Copy callback and call without holding any lock
    MessageOutputCallback callback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback = messageOutputCallback_;
    }
    
    if (callback) {
        for (const auto& output : outputs) {
            callback(output, chainId);
        }
    }
    
    return outputs;
}

std::map<std::string, std::vector<MidiMessage>> 
ProcessorManager::processMessageAllChains(const MidiMessage& input)
{
    // FIX: Copy all chains under lock, then process without lock
    std::vector<std::pair<std::string, std::shared_ptr<ProcessorChain>>> chainsCopy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        chainsCopy.reserve(chains_.size());
        for (const auto& [chainId, chain] : chains_) {
            if (chain) {
                chainsCopy.emplace_back(chainId, chain);
            }
        }
    }
    
    std::map<std::string, std::vector<MidiMessage>> results;
    
    for (const auto& [chainId, chain] : chainsCopy) {
        if (chain->isEnabled()) {
            results[chainId] = chain->process(input);
        }
    }
    
    messagesProcessed_++;
    
    return results;
}

// ============================================================================
// CHAIN MANAGEMENT
// ============================================================================

std::string ProcessorManager::createChain(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string chainId = generateChainId();
    
    auto chain = std::make_shared<ProcessorChain>(name);
    chains_[chainId] = chain;
    
    Logger::info("ProcessorManager", 
                "Created chain: " + name + " (ID: " + chainId + ")");
    
    return chainId;
}

bool ProcessorManager::deleteChain(const std::string& chainId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = chains_.find(chainId);
    if (it == chains_.end()) {
        Logger::warning("ProcessorManager", "Chain not found: " + chainId);
        return false;
    }
    
    std::string name = it->second->getName();
    chains_.erase(it);
    
    Logger::info("ProcessorManager", 
                "Deleted chain: " + name + " (ID: " + chainId + ")");
    
    return true;
}

std::shared_ptr<ProcessorChain> ProcessorManager::getChain(
    const std::string& chainId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = chains_.find(chainId);
    if (it == chains_.end()) {
        return nullptr;
    }
    
    return it->second;
}

std::vector<std::string> ProcessorManager::listChains() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> chainIds;
    chainIds.reserve(chains_.size());
    
    for (const auto& [id, chain] : chains_) {
        chainIds.push_back(id);
    }
    
    return chainIds;
}

bool ProcessorManager::renameChain(const std::string& chainId, 
                                   const std::string& newName)
{
    // FIX: Get chain pointer, then call setName without holding manager mutex
    // (setName has its own mutex in ProcessorChain)
    std::shared_ptr<ProcessorChain> chain;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = chains_.find(chainId);
        if (it == chains_.end()) {
            Logger::warning("ProcessorManager", "Chain not found: " + chainId);
            return false;
        }
        
        chain = it->second;
    }
    
    if (!chain) {
        return false;
    }
    
    chain->setName(newName);
    
    Logger::info("ProcessorManager", 
                "Renamed chain " + chainId + " to: " + newName);
    
    return true;
}

bool ProcessorManager::setChainEnabled(const std::string& chainId, bool enabled) {
    // FIX: Get chain pointer, then call setEnabled without holding manager mutex
    // (setEnabled is atomic in ProcessorChain)
    std::shared_ptr<ProcessorChain> chain;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = chains_.find(chainId);
        if (it == chains_.end()) {
            Logger::warning("ProcessorManager", "Chain not found: " + chainId);
            return false;
        }
        
        chain = it->second;
    }
    
    if (!chain) {
        return false;
    }
    
    chain->setEnabled(enabled);
    
    Logger::info("ProcessorManager", 
                "Chain " + chainId + " " + 
                (enabled ? "enabled" : "disabled"));
    
    return true;
}

// ============================================================================
// PROCESSOR MANAGEMENT
// ============================================================================

std::shared_ptr<MidiProcessor> ProcessorManager::createProcessor(
    ProcessorType type,
    const json& config)
{
    std::shared_ptr<MidiProcessor> processor;
    
    // NOTE: This is a stub implementation. Concrete processor classes
    // need to be implemented. For now, we log the attempt and return nullptr.
    
    std::string typeName;
    switch (type) {
        case ProcessorType::TRANSPOSE:
            typeName = "Transpose";
            // processor = std::make_shared<TransposeProcessor>();
            break;
        case ProcessorType::VELOCITY:
            typeName = "Velocity";
            // processor = std::make_shared<VelocityProcessor>();
            break;
        case ProcessorType::CHANNEL_FILTER:
            typeName = "ChannelFilter";
            // processor = std::make_shared<ChannelFilterProcessor>();
            break;
        case ProcessorType::NOTE_FILTER:
            typeName = "NoteFilter";
            // processor = std::make_shared<NoteFilterProcessor>();
            break;
        case ProcessorType::ARPEGGIATOR:
            typeName = "Arpeggiator";
            // processor = std::make_shared<ArpeggiatorProcessor>();
            break;
        case ProcessorType::DELAY:
            typeName = "Delay";
            // processor = std::make_shared<DelayProcessor>();
            break;
        case ProcessorType::CHORD:
            typeName = "Chord";
            // processor = std::make_shared<ChordProcessor>();
            break;
        case ProcessorType::HARMONIZER:
            typeName = "Harmonizer";
            // processor = std::make_shared<HarmonizerProcessor>();
            break;
        case ProcessorType::QUANTIZE:
            typeName = "Quantize";
            // processor = std::make_shared<QuantizeProcessor>();
            break;
        case ProcessorType::RANDOMIZE:
            typeName = "Randomize";
            // processor = std::make_shared<RandomizeProcessor>();
            break;
        default:
            Logger::error("ProcessorManager", "Unknown processor type");
            return nullptr;
    }
    
    Logger::warning("ProcessorManager", 
                "Processor creation not yet implemented: " + typeName);
    
    // Apply configuration if processor was created
    if (processor && !config.empty()) {
        try {
            processor->fromJson(config);
        } catch (const std::exception& e) {
            Logger::error("ProcessorManager", 
                "Failed to configure processor: " + std::string(e.what()));
            return nullptr;
        }
    }
    
    return processor;
}

bool ProcessorManager::addProcessorToChain(
    const std::string& chainId,
    std::shared_ptr<MidiProcessor> processor)
{
    if (!processor) {
        Logger::error("ProcessorManager", "Cannot add null processor");
        return false;
    }
    
    // FIX: Get chain pointer, then call addProcessor without holding manager mutex
    // (addProcessor has its own mutex in ProcessorChain)
    std::shared_ptr<ProcessorChain> chain;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = chains_.find(chainId);
        if (it == chains_.end()) {
            Logger::warning("ProcessorManager", "Chain not found: " + chainId);
            return false;
        }
        
        chain = it->second;
    }
    
    if (!chain) {
        return false;
    }
    
    bool success = chain->addProcessor(processor);
    
    if (success) {
        Logger::info("ProcessorManager", 
                    "Added processor to chain: " + chainId);
    }
    
    return success;
}

bool ProcessorManager::removeProcessorFromChain(
    const std::string& chainId,
    size_t processorIndex)
{
    // FIX: Get chain pointer, then call removeProcessor without holding manager mutex
    std::shared_ptr<ProcessorChain> chain;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = chains_.find(chainId);
        if (it == chains_.end()) {
            Logger::warning("ProcessorManager", "Chain not found: " + chainId);
            return false;
        }
        
        chain = it->second;
    }
    
    if (!chain) {
        return false;
    }
    
    bool success = chain->removeProcessor(processorIndex);
    
    if (success) {
        Logger::info("ProcessorManager", 
                    "Removed processor from chain: " + chainId);
    }
    
    return success;
}

bool ProcessorManager::moveProcessor(
    const std::string& chainId,
    size_t fromIndex,
    size_t toIndex)
{
    // FIX: Get chain pointer, then call moveProcessor without holding manager mutex
    std::shared_ptr<ProcessorChain> chain;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = chains_.find(chainId);
        if (it == chains_.end()) {
            Logger::warning("ProcessorManager", "Chain not found: " + chainId);
            return false;
        }
        
        chain = it->second;
    }
    
    if (!chain) {
        return false;
    }
    
    bool success = chain->moveProcessor(fromIndex, toIndex);
    
    if (success) {
        Logger::info("ProcessorManager", 
                    "Moved processor in chain: " + chainId);
    }
    
    return success;
}

// ============================================================================
// PRESETS
// ============================================================================

std::string ProcessorManager::loadPreset(const std::string& presetName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = presets_.find(presetName);
    if (it == presets_.end()) {
        Logger::error("ProcessorManager", "Preset not found: " + presetName);
        return "";
    }
    
    const json& preset = it->second;
    
    std::string name = preset.value("name", presetName);
    std::string chainId = generateChainId();
    
    auto chain = std::make_shared<ProcessorChain>(name);
    
    // Load processors from preset
    if (preset.contains("processors") && preset["processors"].is_array()) {
        for (const auto& procConfig : preset["processors"]) {
            std::string type = procConfig.value("type", "");
            
            auto processor = createProcessorFromType(type);
            if (processor) {
                processor->fromJson(procConfig);
                chain->addProcessor(processor);
            }
        }
    }
    
    chains_[chainId] = chain;
    
    Logger::info("ProcessorManager", 
                "Loaded preset: " + presetName + " (ID: " + chainId + ")");
    
    return chainId;
}

std::vector<std::string> ProcessorManager::listPresets() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> presetNames;
    presetNames.reserve(presets_.size());
    
    for (const auto& [name, preset] : presets_) {
        presetNames.push_back(name);
    }
    
    return presetNames;
}

bool ProcessorManager::savePreset(const std::string& chainId, 
                                  const std::string& presetName)
{
    // FIX: Get chain pointer, call toJson, then store under lock
    std::shared_ptr<ProcessorChain> chain;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = chains_.find(chainId);
        if (it == chains_.end()) {
            Logger::error("ProcessorManager", "Chain not found: " + chainId);
            return false;
        }
        
        chain = it->second;
    }
    
    if (!chain) {
        return false;
    }
    
    // Call toJson without holding manager mutex (chain has its own mutex)
    json presetJson = chain->toJson();
    
    // Store preset
    {
        std::lock_guard<std::mutex> lock(mutex_);
        presets_[presetName] = presetJson;
    }
    
    Logger::info("ProcessorManager", 
                "Saved preset: " + presetName + " from chain: " + chainId);
    
    return true;
}

bool ProcessorManager::deletePreset(const std::string& presetName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = presets_.find(presetName);
    if (it == presets_.end()) {
        Logger::warning("ProcessorManager", "Preset not found: " + presetName);
        return false;
    }
    
    presets_.erase(it);
    
    Logger::info("ProcessorManager", "Deleted preset: " + presetName);
    
    return true;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

bool ProcessorManager::saveToFile(const std::string& filepath) const {
    // Get JSON without holding mutex during file I/O
    json j;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        j = toJson();
    }
    
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            Logger::error("ProcessorManager", 
                         "Failed to open file for writing: " + filepath);
            return false;
        }
        
        file << j.dump(2);
        file.close();
        
        Logger::info("ProcessorManager", 
                    "Saved configuration to: " + filepath);
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("ProcessorManager", 
                     "Failed to save configuration: " + std::string(e.what()));
        return false;
    }
}

bool ProcessorManager::loadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::error("ProcessorManager", 
                         "Failed to open file for reading: " + filepath);
            return false;
        }
        
        json j;
        file >> j;
        file.close();
        
        fromJson(j);
        
        Logger::info("ProcessorManager", 
                    "Loaded configuration from: " + filepath);
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("ProcessorManager", 
                     "Failed to load configuration: " + std::string(e.what()));
        return false;
    }
}

json ProcessorManager::toJson() const {
    json j;
    
    // FIX: chains_ already protected by caller's mutex lock
    j["chains"] = json::object();
    for (const auto& [chainId, chain] : chains_) {
        // chain->toJson() is thread-safe (has its own mutex)
        j["chains"][chainId] = chain->toJson();
    }
    
    j["statistics"] = {
        {"messages_processed", messagesProcessed_.load()}
    };
    
    return j;
}

void ProcessorManager::fromJson(const json& j) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Clear existing chains
        chains_.clear();
        
        // Load chains
        if (j.contains("chains") && j["chains"].is_object()) {
            for (auto& [chainId, chainConfig] : j["chains"].items()) {
                
                auto chain = std::make_shared<ProcessorChain>();
                // chain->fromJson() is thread-safe (has its own mutex)
                chain->fromJson(chainConfig);
                
                chains_[chainId] = chain;
                
                Logger::debug("ProcessorManager", 
                             "Loaded chain: " + chainId);
            }
        }
        
        Logger::info("ProcessorManager", 
                    "Configuration loaded successfully (" + 
                    std::to_string(chains_.size()) + " chains)");
        
    } catch (const std::exception& e) {
        Logger::error("ProcessorManager", 
                     "Error loading configuration: " + std::string(e.what()));
        throw;
    }
}

// ============================================================================
// CALLBACKS
// ============================================================================

void ProcessorManager::setMessageOutputCallback(MessageOutputCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    messageOutputCallback_ = callback;
}

// ============================================================================
// STATISTICS
// ============================================================================

json ProcessorManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    stats["chain_count"] = chains_.size();
    stats["messages_processed"] = messagesProcessed_.load();
    
    json chainsStats = json::array();
    for (const auto& [id, chain] : chains_) {
        chainsStats.push_back({
            {"id", id},
            {"name", chain->getName()},
            {"enabled", chain->isEnabled()},
            {"processor_count", chain->getProcessorCount()}
        });
    }
    stats["chains"] = chainsStats;
    
    return stats;
}

void ProcessorManager::resetStatistics() {
    // FIX: Removed unnecessary empty loop
    std::lock_guard<std::mutex> lock(mutex_);
    
    messagesProcessed_ = 0;
    
    Logger::info("ProcessorManager", "Statistics reset");
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::string ProcessorManager::generateChainId() {
    uint32_t id = chainIdCounter_++;
    return "chain_" + std::to_string(id);
}

void ProcessorManager::initializePresets() {
    // Note: Called from constructor, no mutex needed
    Logger::info("ProcessorManager", "Initializing presets...");
    
    // Preset: Transpose Up
    presets_["transpose_up"] = {
        {"name", "Transpose Up Octave"},
        {"processors", json::array({
            {
                {"type", "transpose"},
                {"name", "Octave Up"},
                {"enabled", true},
                {"params", {
                    {"semitones", 12}
                }}
            },
            {
                {"type", "velocity"},
                {"name", "Velocity Boost"},
                {"enabled", true},
                {"params", {
                    {"multiplier", 1.2}
                }}
            }
        })}
    };
    
    // Preset: Piano Chords
    presets_["piano_chords"] = {
        {"name", "Piano Chords"},
        {"processors", json::array({
            {
                {"type", "chord"},
                {"name", "Major 7th"},
                {"enabled", true},
                {"params", {
                    {"chord_type", "major7"}
                }}
            }
        })}
    };
    
    // Preset: Arp Sequence
    presets_["arp_sequence"] = {
        {"name", "Arp Sequence"},
        {"processors", json::array({
            {
                {"type", "arpeggiator"},
                {"name", "Arpeggiator"},
                {"enabled", true},
                {"params", {
                    {"pattern", "up"},
                    {"rate", 16}
                }}
            },
            {
                {"type", "delay"},
                {"name", "Echo"},
                {"enabled", true},
                {"params", {
                    {"delay_ms", 250},
                    {"feedback", 0.5}
                }}
            }
        })}
    };
    
    Logger::info("ProcessorManager", 
                "✓ " + std::to_string(presets_.size()) + " presets initialized");
}

std::shared_ptr<MidiProcessor> ProcessorManager::createProcessorFromType(
    const std::string& type)
{
    // NOTE: This is a stub implementation. Will be implemented when
    // concrete processor classes are available.
    Logger::warning("ProcessorManager", 
                "Processor creation not yet implemented: " + type);
    return nullptr;
}

} // namespace midiMind

// ============================================================================
// END OF FILE ProcessorManager.cpp v4.1.2
// ============================================================================