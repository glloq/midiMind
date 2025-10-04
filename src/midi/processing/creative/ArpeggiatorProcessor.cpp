// ============================================================================
// Fichier: src/midi/processing/creative/ArpeggiatorProcessor.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "ArpeggiatorProcessor.h"
#include "../../../core/Logger.h"
#include <algorithm>
#include <random>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

ArpeggiatorProcessor::ArpeggiatorProcessor()
    : MidiProcessor("Arpeggiator", ProcessorType::ARPEGGIATOR)
    , pattern_(ArpPattern::UP)
    , rate_(4)
    , octaves_(1)
    , tempo_(120.0f)
    , gate_(0.8f)
    , running_(false)
    , sequencePosition_(0)
    , outputChannel_(1)
    , outputVelocity_(100) {
    
    parameters_["pattern"] = static_cast<int>(pattern_);
    parameters_["rate"] = rate_;
    parameters_["octaves"] = octaves_;
    parameters_["tempo"] = tempo_;
    parameters_["gate"] = gate_;
}

ArpeggiatorProcessor::~ArpeggiatorProcessor() {
    stop();
}

// ============================================================================
// TRAITEMENT
// ============================================================================

std::vector<MidiMessage> ArpeggiatorProcessor::process(const MidiMessage& input) {
    // Bypass
    if (!isEnabled() || isBypassed()) {
        return {input};
    }
    
    // Ne traiter que les Note On/Off
    if (!input.isNoteOn() && !input.isNoteOff()) {
        return {input};
    }
    
    std::lock_guard<std::mutex> lock(notesMutex_);
    
    uint8_t note = input.getNote();
    
    if (input.isNoteOn()) {
        // Ajouter au buffer
        heldNotes_.insert(note);
        outputChannel_ = input.getChannel();
        outputVelocity_ = input.getVelocity();
        
        Logger::debug("Arpeggiator", "Note added: " + std::to_string(note) + 
                     " (total: " + std::to_string(heldNotes_.size()) + ")");
    } else {
        // Retirer du buffer
        heldNotes_.erase(note);
        
        Logger::debug("Arpeggiator", "Note removed: " + std::to_string(note) + 
                     " (total: " + std::to_string(heldNotes_.size()) + ")");
    }
    
    // Ne pas transmettre les messages (l'arpégiateur génère ses propres notes)
    return {};
}

// ============================================================================
// CONTRÔLE
// ============================================================================

void ArpeggiatorProcessor::start() {
    if (running_) {
        return;
    }
    
    Logger::info("Arpeggiator", "Starting arpeggiator");
    Logger::info("Arpeggiator", "  Pattern: " + std::to_string(static_cast<int>(pattern_)));
    Logger::info("Arpeggiator", "  Rate: " + std::to_string(rate_));
    Logger::info("Arpeggiator", "  Tempo: " + std::to_string(tempo_) + " BPM");
    
    running_ = true;
    sequencePosition_ = 0;
    
    arpThread_ = std::thread([this]() {
        arpeggiatorThread();
    });
}

void ArpeggiatorProcessor::stop() {
    if (!running_) {
        return;
    }
    
    Logger::info("Arpeggiator", "Stopping arpeggiator");
    
    running_ = false;
    
    if (arpThread_.joinable()) {
        arpThread_.join();
    }
}

bool ArpeggiatorProcessor::isRunning() const {
    return running_;
}

void ArpeggiatorProcessor::reset() {
    std::lock_guard<std::mutex> lock(notesMutex_);
    heldNotes_.clear();
    sequencePosition_ = 0;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void ArpeggiatorProcessor::setPattern(ArpPattern pattern) {
    pattern_ = pattern;
    parameters_["pattern"] = static_cast<int>(pattern);
    sequencePosition_ = 0;
}

ArpPattern ArpeggiatorProcessor::getPattern() const {
    return pattern_;
}

void ArpeggiatorProcessor::setRate(uint8_t rate) {
    rate_ = std::clamp(rate, uint8_t(1), uint8_t(16));
    parameters_["rate"] = rate_;
}

uint8_t ArpeggiatorProcessor::getRate() const {
    return rate_;
}

void ArpeggiatorProcessor::setOctaves(uint8_t octaves) {
    octaves_ = std::clamp(octaves, uint8_t(1), uint8_t(4));
    parameters_["octaves"] = octaves_;
}

uint8_t ArpeggiatorProcessor::getOctaves() const {
    return octaves_;
}

void ArpeggiatorProcessor::setTempo(float bpm) {
    tempo_ = std::clamp(bpm, 20.0f, 300.0f);
    parameters_["tempo"] = tempo_;
}

float ArpeggiatorProcessor::getTempo() const {
    return tempo_;
}

void ArpeggiatorProcessor::setGate(float gate) {
    gate_ = std::clamp(gate, 0.1f, 1.0f);
    parameters_["gate"] = gate_;
}

float ArpeggiatorProcessor::getGate() const {
    return gate_;
}

void ArpeggiatorProcessor::setNoteOutputCallback(NoteOutputCallback callback) {
    noteOutputCallback_ = callback;
}

bool ArpeggiatorProcessor::setParameter(const std::string& name, const json& value) {
    if (name == "pattern") {
        setPattern(static_cast<ArpPattern>(value.get<int>()));
        return true;
    } else if (name == "rate") {
        setRate(value.get<uint8_t>());
        return true;
    } else if (name == "octaves") {
        setOctaves(value.get<uint8_t>());
        return true;
    } else if (name == "tempo") {
        setTempo(value.get<float>());
        return true;
    } else if (name == "gate") {
        setGate(value.get<float>());
        return true;
    }
    
    return MidiProcessor::setParameter(name, value);
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void ArpeggiatorProcessor::arpeggiatorThread() {
    Logger::info("Arpeggiator", "Arpeggiator thread started");
    
    uint8_t lastNote = 0;
    bool noteIsOn = false;
    
    while (running_) {
        // Générer la séquence d'arpège
        auto sequence = generateArpSequence();
        
        if (sequence.empty()) {
            // Pas de notes, attendre un peu
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // Éteindre la dernière note si elle est encore allumée
            if (noteIsOn && noteOutputCallback_) {
                MidiMessage noteOff = MidiMessage::noteOff(outputChannel_, lastNote, 0);
                noteOutputCallback_(noteOff);
                noteIsOn = false;
            }
            
            continue;
        }
        
        // Jouer la note courante
        uint8_t note = sequence[sequencePosition_ % sequence.size()];
        
        // Note On
        if (noteOutputCallback_) {
            MidiMessage noteOn = MidiMessage::noteOn(outputChannel_, note, outputVelocity_);
            noteOutputCallback_(noteOn);
            noteIsOn = true;
            lastNote = note;
        }
        
        // Calculer le timing
        uint32_t intervalMs = calculateInterval();
        uint32_t gateMs = static_cast<uint32_t>(intervalMs * gate_);
        uint32_t restMs = intervalMs - gateMs;
        
        // Attendre pendant le gate
        std::this_thread::sleep_for(std::chrono::milliseconds(gateMs));
        
        // Note Off
        if (noteIsOn && noteOutputCallback_) {
            MidiMessage noteOff = MidiMessage::noteOff(outputChannel_, note, 0);
            noteOutputCallback_(noteOff);
            noteIsOn = false;
        }
        
        // Attendre le reste de l'intervalle
        if (restMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(restMs));
        }
        
        // Passer à la note suivante
        sequencePosition_++;
    }
    
    // Éteindre la dernière note
    if (noteIsOn && noteOutputCallback_) {
        MidiMessage noteOff = MidiMessage::noteOff(outputChannel_, lastNote, 0);
        noteOutputCallback_(noteOff);
    }
    
    Logger::info("Arpeggiator", "Arpeggiator thread stopped");
}

std::vector<uint8_t> ArpeggiatorProcessor::generateArpSequence() {
    std::lock_guard<std::mutex> lock(notesMutex_);
    
    if (heldNotes_.empty()) {
        return {};
    }
    
    std::vector<uint8_t> baseNotes(heldNotes_.begin(), heldNotes_.end());
    std::vector<uint8_t> sequence;
    
    // Générer selon le pattern
    switch (pattern_) {
        case ArpPattern::UP:
            // Trier croissant
            std::sort(baseNotes.begin(), baseNotes.end());
            sequence = baseNotes;
            break;
            
        case ArpPattern::DOWN:
            // Trier décroissant
            std::sort(baseNotes.begin(), baseNotes.end(), std::greater<uint8_t>());
            sequence = baseNotes;
            break;
            
        case ArpPattern::UP_DOWN:
            // Montant puis descendant
            std::sort(baseNotes.begin(), baseNotes.end());
            sequence = baseNotes;
            for (int i = baseNotes.size() - 2; i > 0; --i) {
                sequence.push_back(baseNotes[i]);
            }
            break;
            
        case ArpPattern::DOWN_UP:
            // Descendant puis montant
            std::sort(baseNotes.begin(), baseNotes.end(), std::greater<uint8_t>());
            sequence = baseNotes;
            for (int i = baseNotes.size() - 2; i > 0; --i) {
                sequence.push_back(baseNotes[i]);
            }
            break;
            
        case ArpPattern::RANDOM: {
            // Aléatoire
            sequence = baseNotes;
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::shuffle(sequence.begin(), sequence.end(), gen);
            break;
        }
        
        case ArpPattern::AS_PLAYED:
            // Ordre de jeu (ordre d'insertion)
            sequence = baseNotes;
            break;
    }
    
    // Ajouter les octaves
    if (octaves_ > 1) {
        std::vector<uint8_t> expanded;
        
        for (uint8_t octave = 0; octave < octaves_; ++octave) {
            for (uint8_t note : sequence) {
                uint8_t transposed = note + (octave * 12);
                if (transposed <= 127) {
                    expanded.push_back(transposed);
                }
            }
        }
        
        sequence = expanded;
    }
    
    return sequence;
}

uint32_t ArpeggiatorProcessor::calculateInterval() const {
    // Intervalle en ms = (60000 / tempo) / rate
    // Ex: 120 BPM, rate 4 (16èmes) = (60000 / 120) / 4 = 125ms
    
    float beatDurationMs = 60000.0f / tempo_;
    float intervalMs = beatDurationMs / rate_;
    
    return static_cast<uint32_t>(intervalMs);
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ArpeggiatorProcessor.cpp
// ============================================================================