// ============================================================================
// Fichier: src/midi/processing/creative/DelayProcessor.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "DelayProcessor.h"
#include "../../../core/Logger.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

DelayProcessor::DelayProcessor()
    : MidiProcessor("Delay", ProcessorType::DELAY)
    , delayMs_(250)
    , feedback_(0.5f)
    , mix_(0.5f)
    , maxRepeats_(4)
    , velocityDecay_(0.8f)
    , running_(false) {
    
    parameters_["delay_ms"] = delayMs_;
    parameters_["feedback"] = feedback_;
    parameters_["mix"] = mix_;
    parameters_["max_repeats"] = maxRepeats_;
    parameters_["velocity_decay"] = velocityDecay_;
}

DelayProcessor::~DelayProcessor() {
    stop();
}

// ============================================================================
// TRAITEMENT
// ============================================================================

std::vector<MidiMessage> DelayProcessor::process(const MidiMessage& input) {
    // Bypass
    if (!isEnabled() || isBypassed()) {
        return {input};
    }
    
    // Ne traiter que les Note On/Off
    if (!input.isNoteOn() && !input.isNoteOff()) {
        return {input};
    }
    
    std::lock_guard<std::mutex> lock(bufferMutex_);
    
    // Ajouter au buffer de délai
    uint64_t now = getCurrentTimestamp();
    uint64_t outputTime = now + delayMs_;
    
    // Ajouter les répétitions selon le feedback
    for (uint8_t repeat = 0; repeat < maxRepeats_ && feedback_ > 0.0f; ++repeat) {
        // Calculer le timestamp pour cette répétition
        uint64_t repeatTime = outputTime + (repeat * delayMs_);
        
        // Calculer la vélocité avec décroissance
        MidiMessage delayed = input;
        
        if (input.isNoteOn()) {
            float velocityFactor = std::pow(velocityDecay_, repeat + 1);
            uint8_t newVelocity = static_cast<uint8_t>(input.getVelocity() * velocityFactor);
            
            if (newVelocity < 10) {
                break; // Arrêter si la vélocité devient trop faible
            }
            
            delayed.setVelocity(newVelocity);
        }
        
        delayBuffer_.emplace_back(delayed, repeatTime, repeat);
        
        // Diminuer le feedback pour la prochaine répétition
        if (std::pow(feedback_, repeat + 1) < 0.1f) {
            break;
        }
    }
    
    Logger::debug("Delay", "Added message to delay buffer (buffer size: " + 
                 std::to_string(delayBuffer_.size()) + ")");
    
    // Transmettre le message original si mix < 1.0
    if (mix_ < 1.0f) {
        return {input};
    }
    
    return {}; // 100% wet
}

// ============================================================================
// CONTRÔLE
// ============================================================================

void DelayProcessor::start() {
    if (running_) {
        return;
    }
    
    Logger::info("Delay", "Starting delay processor");
    Logger::info("Delay", "  Delay: " + std::to_string(delayMs_) + "ms");
    Logger::info("Delay", "  Feedback: " + std::to_string(feedback_));
    Logger::info("Delay", "  Max repeats: " + std::to_string(maxRepeats_));
    
    running_ = true;
    
    delayThread_ = std::thread([this]() {
        delayThread();
    });
}

void DelayProcessor::stop() {
    if (!running_) {
        return;
    }
    
    Logger::info("Delay", "Stopping delay processor");
    
    running_ = false;
    
    if (delayThread_.joinable()) {
        delayThread_.join();
    }
}

bool DelayProcessor::isRunning() const {
    return running_;
}

void DelayProcessor::reset() {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    delayBuffer_.clear();
    Logger::info("Delay", "Delay buffer cleared");
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void DelayProcessor::setDelayTime(uint32_t delayMs) {
    delayMs_ = std::clamp(delayMs, uint32_t(1), uint32_t(5000));
    parameters_["delay_ms"] = delayMs_;
}

uint32_t DelayProcessor::getDelayTime() const {
    return delayMs_;
}

void DelayProcessor::setFeedback(float feedback) {
    feedback_ = std::clamp(feedback, 0.0f, 1.0f);
    parameters_["feedback"] = feedback_;
}

float DelayProcessor::getFeedback() const {
    return feedback_;
}

void DelayProcessor::setMix(float mix) {
    mix_ = std::clamp(mix, 0.0f, 1.0f);
    parameters_["mix"] = mix_;
}

float DelayProcessor::getMix() const {
    return mix_;
}

void DelayProcessor::setMaxRepeats(uint8_t maxRepeats) {
    maxRepeats_ = std::clamp(maxRepeats, uint8_t(1), uint8_t(16));
    parameters_["max_repeats"] = maxRepeats_;
}

uint8_t DelayProcessor::getMaxRepeats() const {
    return maxRepeats_;
}

void DelayProcessor::setVelocityDecay(float decay) {
    velocityDecay_ = std::clamp(decay, 0.0f, 1.0f);
    parameters_["velocity_decay"] = velocityDecay_;
}

float DelayProcessor::getVelocityDecay() const {
    return velocityDecay_;
}

void DelayProcessor::setMessageOutputCallback(MessageOutputCallback callback) {
    messageOutputCallback_ = callback;
}

bool DelayProcessor::setParameter(const std::string& name, const json& value) {
    if (name == "delay_ms") {
        setDelayTime(value.get<uint32_t>());
        return true;
    } else if (name == "feedback") {
        setFeedback(value.get<float>());
        return true;
    } else if (name == "mix") {
        setMix(value.get<float>());
        return true;
    } else if (name == "max_repeats") {
        setMaxRepeats(value.get<uint8_t>());
        return true;
    } else if (name == "velocity_decay") {
        setVelocityDecay(value.get<float>());
        return true;
    }
    
    return MidiProcessor::setParameter(name, value);
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void DelayProcessor::delayThread() {
    Logger::info("Delay", "Delay thread started");
    
    while (running_) {
        uint64_t now = getCurrentTimestamp();
        
        std::unique_lock<std::mutex> lock(bufferMutex_);
        
        // Traiter tous les messages dont le timestamp est passé
        while (!delayBuffer_.empty() && delayBuffer_.front().timestamp <= now) {
            DelayedMessage delayed = delayBuffer_.front();
            delayBuffer_.pop_front();
            
            lock.unlock();
            
            // Envoyer le message
            if (messageOutputCallback_) {
                messageOutputCallback_(delayed.message);
                
                Logger::debug("Delay", "Sent delayed message (repetition " + 
                             std::to_string(delayed.repetition) + ")");
            }
            
            lock.lock();
        }
        
        lock.unlock();
        
        // Attendre un peu avant de vérifier à nouveau
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    Logger::info("Delay", "Delay thread stopped");
}

uint64_t DelayProcessor::getCurrentTimestamp() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER DelayProcessor.cpp
// ============================================================================