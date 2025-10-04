// ============================================================================
// Fichier: src/midi/processing/ProcessorManager.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "ProcessorManager.h"
#include <fstream>
#include <sstream>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

ProcessorManager::ProcessorManager()
    : chainIdCounter_(0)
    , messagesProcessed_(0) {
    
    Logger::info("ProcessorManager", "═══════════════════════════════════════");
    Logger::info("ProcessorManager", "  Initializing ProcessorManager");
    Logger::info("ProcessorManager", "═══════════════════════════════════════");
    
    initializePresets();
    
    Logger::info("ProcessorManager", "✓ ProcessorManager initialized");
}

ProcessorManager::~ProcessorManager() {
    Logger::info("ProcessorManager", "ProcessorManager destroyed");
}

// ============================================================================
// TRAITEMENT
// ============================================================================

std::vector<MidiMessage> ProcessorManager::processMessage(
    const MidiMessage& input, 
    const std::string& chainId) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = chains_.find(chainId);
    if (it == chains_.end()) {
        Logger::warn("ProcessorManager", "Chain not found: " + chainId);
        return {input};
    }
    
    auto outputs = it->second->process(input);
    messagesProcessed_++;
    
    return outputs;
}

std::map<std::string, std::vector<MidiMessage>> ProcessorManager::processMessageAllChains(
    const MidiMessage& input) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::map<std::string, std::vector<MidiMessage>> results;
    
    for (const auto& [chainId, chain] : chains_) {
        if (chain && chain->isEnabled()) {
            results[chainId] = chain->process(input);
        }
    }
    
    messagesProcessed_++;
    
    return results;
}

// ============================================================================
// GESTION DES CHAÎNES
// ============================================================================

std::string ProcessorManager::createChain(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string chainId = generateChainId();
    
    auto chain = std::make_shared<ProcessorChain>(name);
    chains_[chainId] = chain;
    
    Logger::info("ProcessorManager", "Created chain: " + name + " (ID: " + chainId + ")");
    
    return chainId;
}

bool ProcessorManager::deleteChain(const std::string& chainId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = chains_.find(chainId);
    if (it == chains_.end()) {
        Logger::warn("ProcessorManager", "Chain not found: " + chainId);
        return false;
    }
    
    std::string name = it->second->getName();
    chains_.erase(it);
    
    Logger::info("ProcessorManager", "Deleted chain: " + name + " (ID: " + chainId + ")");
    
    return true;
}

ProcessorChainPtr ProcessorManager::getChain(const std::string& chainId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = chains_.find(chainId);
    if (it != chains_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::vector<std::string> ProcessorManager::listChains() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> chainIds;
    for (const auto& [id, chain] : chains_) {
        chainIds.push_back(id);
    }
    
    return chainIds;
}

bool ProcessorManager::renameChain(const std::string& chainId, const std::string& newName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = chains_.find(chainId);
    if (it == chains_.end()) {
        return false;
    }
    
    it->second->setName(newName);
    
    Logger::info("ProcessorManager", "Renamed chain " + chainId + " to: " + newName);
    
    return true;
}

// ============================================================================
// GESTION DES PROCESSORS
// ============================================================================

MidiProcessorPtr ProcessorManager::createProcessor(ProcessorType type, const json& config) {
    MidiProcessorPtr processor;
    
    switch (type) {
        case ProcessorType::TRANSPOSE:
            processor = std::make_shared<TransposeProcessor>();
            break;
            
        case ProcessorType::VELOCITY:
            processor = std::make_shared<VelocityProcessor>();
            break;
            
        case ProcessorType::CHANNEL_FILTER:
            processor = std::make_shared<ChannelFilterProcessor>();
            break;
            
        case ProcessorType::NOTE_FILTER:
            processor = std::make_shared<NoteFilterProcessor>();
            break;
            
        case ProcessorType::ARPEGGIATOR:
            processor = std::make_shared<ArpeggiatorProcessor>();
            break;
            
        case ProcessorType::DELAY:
            processor = std::make_shared<DelayProcessor>();
            break;
            
        case ProcessorType::CHORD:
            processor = std::make_shared<ChordProcessor>();
            break;
            
        case ProcessorType::HARMONIZER:
            processor = std::make_shared<HarmonizerProcessor>();
            break;
            
        default:
            Logger::error("ProcessorManager", "Unknown processor type");
            return nullptr;
    }
    
    if (processor && !config.empty()) {
        processor->fromJson(config);
    }
    
    Logger::info("ProcessorManager", "Created processor: " + processor->getName());
    
    return processor;
}

bool ProcessorManager::addProcessorToChain(const std::string& chainId, 
                                          MidiProcessorPtr processor) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = chains_.find(chainId);
    if (it == chains_.end()) {
        Logger::warn("ProcessorManager", "Chain not found: " + chainId);
        return false;
    }
    
    return it->second->addProcessor(processor);
}

bool ProcessorManager::removeProcessorFromChain(const std::string& chainId, 
                                               size_t processorIndex) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = chains_.find(chainId);
    if (it == chains_.end()) {
        Logger::warn("ProcessorManager", "Chain not found: " + chainId);
        return false;
    }
    
    return it->second->removeProcessor(processorIndex);
}

// ============================================================================
// PRESETS
// ============================================================================

std::string ProcessorManager::loadPreset(const std::string& presetName) {
    Logger::info("ProcessorManager", "Loading preset: " + presetName);
    
    std::string chainId;
    
    if (presetName == "transpose_up") {
        // Transpose +7 demi-tons (quinte)
        chainId = createChain("Transpose Up");
        auto transpose = std::make_shared<TransposeProcessor>(7);
        addProcessorToChain(chainId, transpose);
    }
    else if (presetName == "lead_synth") {
        // Lead synth: Transpose + Velocity boost
        chainId = createChain("Lead Synth");
        
        auto transpose = std::make_shared<TransposeProcessor>(12); // +1 octave
        addProcessorToChain(chainId, transpose);
        
        auto velocity = std::make_shared<VelocityProcessor>(VelocityMode::MULTIPLY, 1.3f);
        addProcessorToChain(chainId, velocity);
    }
    else if (presetName == "piano_chords") {
        // Piano chords: Générateur d'accords
        chainId = createChain("Piano Chords");
        
        auto chord = std::make_shared<ChordProcessor>(ChordType::MAJOR7);
        chord->setVelocityScale(0.7f);
        addProcessorToChain(chainId, chord);
    }
    else if (presetName == "arp_sequence") {
        // Arpégiateur + Delay
        chainId = createChain("Arp Sequence");
        
        auto arp = std::make_shared<ArpeggiatorProcessor>();
        arp->setPattern(ArpPattern::UP);
        arp->setRate(4); // 16èmes
        arp->setTempo(120);
        addProcessorToChain(chainId, arp);
        
        auto delay = std::make_shared<DelayProcessor>();
        delay->setDelayTime(250);
        delay->setFeedback(0.5f);
        addProcessorToChain(chainId, delay);
    }
    else {
        Logger::error("ProcessorManager", "Unknown preset: " + presetName);
        return "";
    }
    
    Logger::info("ProcessorManager", "✓ Preset loaded: " + presetName);
    
    return chainId;
}

std::vector<std::string> ProcessorManager::listPresets() const {
    return {
        "transpose_up",
        "lead_synth",
        "piano_chords",
        "arp_sequence"
    };
}

// ============================================================================
// SÉRIALISATION
// ============================================================================

bool ProcessorManager::saveToFile(const std::string& filepath) const {
    Logger::info("ProcessorManager", "Saving to file: " + filepath);
    
    try {
        json j = toJson();
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            Logger::error("ProcessorManager", "Cannot open file: " + filepath);
            return false;
        }
        
        file << j.dump(2); // Indentation de 2 espaces
        file.close();
        
        Logger::info("ProcessorManager", "✓ Saved to file");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("ProcessorManager", "Save failed: " + std::string(e.what()));
        return false;
    }
}

bool ProcessorManager::loadFromFile(const std::string& filepath) {
    Logger::info("ProcessorManager", "Loading from file: " + filepath);
    
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::error("ProcessorManager", "Cannot open file: " + filepath);
            return false;
        }
        
        json j;
        file >> j;
        file.close();
        
        fromJson(j);
        
        Logger::info("ProcessorManager", "✓ Loaded from file");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("ProcessorManager", "Load failed: " + std::string(e.what()));
        return false;
    }
}

json ProcessorManager::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json j;
    j["chains"] = json::object();
    
    for (const auto& [id, chain] : chains_) {
        j["chains"][id] = chain->toJson();
    }
    
    return j;
}

void ProcessorManager::fromJson(const json& j) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Note: Cette implémentation est simplifiée
    // Une vraie implémentation devrait recréer les processors depuis le JSON
    
    if (j.contains("chains") && j["chains"].is_object()) {
        for (auto& [id, chainJson] : j["chains"].items()) {
            auto it = chains_.find(id);
            if (it != chains_.end()) {
                it->second->fromJson(chainJson);
            }
        }
    }
}

// ============================================================================
// CALLBACKS
// ============================================================================

void ProcessorManager::setMessageOutputCallback(MessageOutputCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    messageOutputCallback_ = callback;
}

// ============================================================================
// STATISTIQUES
// ============================================================================

json ProcessorManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    stats["chain_count"] = chains_.size();
    stats["messages_processed"] = messagesProcessed_.load();
    
    stats["chains"] = json::array();
    for (const auto& [id, chain] : chains_) {
        json chainStats = chain->getStatistics();
        chainStats["id"] = id;
        stats["chains"].push_back(chainStats);
    }
    
    return stats;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

std::string ProcessorManager::generateChainId() const {
    uint32_t id = chainIdCounter_++;
    return "chain_" + std::to_string(id);
}

void ProcessorManager::initializePresets() {
    Logger::info("ProcessorManager", "Initializing presets...");
    
    // Les presets sont chargés dynamiquement via loadPreset()
    
    Logger::info("ProcessorManager", "✓ " + std::to_string(listPresets().size()) + 
                " presets available");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ProcessorManager.cpp
// ============================================================================