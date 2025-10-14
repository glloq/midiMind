// ============================================================================
// Fichier: backend/src/midi/MidiTransforms.cpp
// Version: 3.1.0
// Date: 2025-10-10
// Projet: MidiMind v3.1 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Implémentation des transformations musicales MIDI.
//
// Auteur: MidiMind Team
// Statut: ✅ PHASE 2 - COMPLET
// ============================================================================

#include "MidiTransforms.h"
#include "../core/Logger.h"
#include <cmath>
#include <algorithm>

namespace midiMind {

// ============================================================================
// RNG THREAD-LOCAL
// ============================================================================

thread_local std::mt19937 MidiTransforms::rng_;

void MidiTransforms::initRng(unsigned int seed) {
    if (seed == 0) {
        std::random_device rd;
        rng_.seed(rd());
    } else {
        rng_.seed(seed);
    }
}

int MidiTransforms::randomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng_);
}

float MidiTransforms::randomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng_);
}

// ============================================================================
// QUANTIZATION
// ============================================================================

int MidiTransforms::quantize(json& jsonMidi,
                             const std::vector<std::string>& noteIds,
                             int division,
                             float strength,
                             int ppq) {
    Logger::debug("MidiTransforms", "Quantizing notes (division: " + 
        std::to_string(division) + ", strength: " + std::to_string(strength) + ")");
    
    // Validation
    if (division <= 0 || (division != 4 && division != 8 && division != 16 && 
                          division != 32 && division != 64)) {
        Logger::error("MidiTransforms", "Invalid division: " + std::to_string(division));
        return 0;
    }
    
    strength = clamp(strength, 0.0f, 1.0f);
    
    // Calculer la taille de la grille
    int gridSize = (ppq * 4) / division;  // 4 = nombre de beats par mesure
    
    Logger::debug("MidiTransforms", "Grid size: " + std::to_string(gridSize) + " ticks");
    
    // Trouver les notes à quantizer
    std::vector<json*> notes = noteIds.empty() ? 
        findAllNotes(jsonMidi) : 
        findEventsByIds(jsonMidi, noteIds);
    
    int quantizedCount = 0;
    
    for (json* note : notes) {
        if (!note || !note->contains("time")) {
            continue;
        }
        
        int oldTime = (*note)["time"].get<int>();
        int newTime = quantizeTime(oldTime, gridSize, strength);
        
        if (newTime != oldTime) {
            (*note)["time"] = newTime;
            quantizedCount++;
            
            Logger::debug("MidiTransforms", 
                "Quantized note: " + std::to_string(oldTime) + " → " + std::to_string(newTime));
        }
    }
    
    Logger::info("MidiTransforms", "Quantized " + std::to_string(quantizedCount) + " notes");
    return quantizedCount;
}

int MidiTransforms::quantizeTime(int time, int gridSize, float strength) {
    // Trouver la position la plus proche sur la grille
    int gridPosition = std::round(static_cast<float>(time) / gridSize) * gridSize;
    
    // Appliquer la force de quantization
    int quantizedTime = static_cast<int>(
        time + (gridPosition - time) * strength
    );
    
    return std::max(0, quantizedTime);
}

// ============================================================================
// TRANSPOSITION
// ============================================================================

int MidiTransforms::transpose(json& jsonMidi,
                              const std::vector<std::string>& noteIds,
                              int semitones) {
    Logger::debug("MidiTransforms", "Transposing notes: " + 
        std::to_string(semitones) + " semitones");
    
    if (semitones == 0) {
        return 0;
    }
    
    std::vector<json*> notes = noteIds.empty() ? 
        findAllNotes(jsonMidi) : 
        findEventsByIds(jsonMidi, noteIds);
    
    int transposedCount = 0;
    int clampedCount = 0;
    
    for (json* note : notes) {
        if (!note || !note->contains("note")) {
            continue;
        }
        
        int oldNote = (*note)["note"].get<int>();
        bool clamped = false;
        int newNote = transposeNote(oldNote, semitones, &clamped);
        
        if (newNote != oldNote) {
            (*note)["note"] = newNote;
            transposedCount++;
            
            if (clamped) {
                clampedCount++;
            }
            
            Logger::debug("MidiTransforms", 
                "Transposed note: " + std::to_string(oldNote) + " → " + 
                std::to_string(newNote) + (clamped ? " (clamped)" : ""));
        }
    }
    
    if (clampedCount > 0) {
        Logger::warn("MidiTransforms", std::to_string(clampedCount) + 
            " notes were clamped to valid range (0-127)");
    }
    
    Logger::info("MidiTransforms", "Transposed " + std::to_string(transposedCount) + " notes");
    return transposedCount;
}

int MidiTransforms::transposeNote(int note, int semitones, bool* clamped) {
    int transposed = note + semitones;
    
    bool wasClamped = false;
    
    if (transposed < 0) {
        transposed = 0;
        wasClamped = true;
    } else if (transposed > 127) {
        transposed = 127;
        wasClamped = true;
    }
    
    if (clamped) {
        *clamped = wasClamped;
    }
    
    return transposed;
}

// ============================================================================
// VELOCITY
// ============================================================================

int MidiTransforms::scaleVelocity(json& jsonMidi,
                                  const std::vector<std::string>& noteIds,
                                  float factor) {
    Logger::debug("MidiTransforms", "Scaling velocity by factor: " + 
        std::to_string(factor));
    
    factor = clamp(factor, 0.1f, 2.0f);
    
    std::vector<json*> notes = noteIds.empty() ? 
        findAllNotes(jsonMidi) : 
        findEventsByIds(jsonMidi, noteIds);
    
    int modifiedCount = 0;
    
    for (json* note : notes) {
        if (!note || !note->contains("velocity")) {
            continue;
        }
        
        int oldVelocity = (*note)["velocity"].get<int>();
        int newVelocity = scaleVelocityValue(oldVelocity, factor, false);
        
        if (newVelocity != oldVelocity) {
            (*note)["velocity"] = newVelocity;
            modifiedCount++;
            
            Logger::debug("MidiTransforms", 
                "Scaled velocity: " + std::to_string(oldVelocity) + " → " + 
                std::to_string(newVelocity));
        }
    }
    
    Logger::info("MidiTransforms", "Scaled velocity for " + 
        std::to_string(modifiedCount) + " notes");
    return modifiedCount;
}

int MidiTransforms::offsetVelocity(json& jsonMidi,
                                   const std::vector<std::string>& noteIds,
                                   int offset) {
    Logger::debug("MidiTransforms", "Offsetting velocity by: " + 
        std::to_string(offset));
    
    offset = clamp(offset, -64, 64);
    
    std::vector<json*> notes = noteIds.empty() ? 
        findAllNotes(jsonMidi) : 
        findEventsByIds(jsonMidi, noteIds);
    
    int modifiedCount = 0;
    
    for (json* note : notes) {
        if (!note || !note->contains("velocity")) {
            continue;
        }
        
        int oldVelocity = (*note)["velocity"].get<int>();
        int newVelocity = scaleVelocityValue(oldVelocity, offset, true);
        
        if (newVelocity != oldVelocity) {
            (*note)["velocity"] = newVelocity;
            modifiedCount++;
            
            Logger::debug("MidiTransforms", 
                "Offset velocity: " + std::to_string(oldVelocity) + " → " + 
                std::to_string(newVelocity));
        }
    }
    
    Logger::info("MidiTransforms", "Offset velocity for " + 
        std::to_string(modifiedCount) + " notes");
    return modifiedCount;
}

int MidiTransforms::scaleVelocityValue(int velocity, float factor, bool useOffset) {
    int newVelocity;
    
    if (useOffset) {
        // factor est en fait un offset
        newVelocity = velocity + static_cast<int>(factor);
    } else {
        // factor est un multiplicateur
        newVelocity = static_cast<int>(velocity * factor);
    }
    
    // Clamp entre 1 et 127 (0 = noteOff)
    return clamp(newVelocity, 1, 127);
}

// ============================================================================
// HUMANISATION
// ============================================================================

int MidiTransforms::humanize(json& jsonMidi,
                             const std::vector<std::string>& noteIds,
                             int timingVarianceMs,
                             int velocityVariance,
                             unsigned int seed,
                             int ppq,
                             int tempo) {
    Logger::debug("MidiTransforms", "Humanizing notes (timing: ±" + 
        std::to_string(timingVarianceMs) + "ms, velocity: ±" + 
        std::to_string(velocityVariance) + ")");
    
    // Initialiser RNG
    initRng(seed);
    
    // Convertir timing variance en ticks
    int timingVarianceTicks = msToTicks(timingVarianceMs, ppq, tempo);
    
    std::vector<json*> notes = noteIds.empty() ? 
        findAllNotes(jsonMidi) : 
        findEventsByIds(jsonMidi, noteIds);
    
    int humanizedCount = 0;
    
    for (json* note : notes) {
        if (!note) {
            continue;
        }
        
        bool modified = false;
        
        // Humaniser le timing
        if (note->contains("time") && timingVarianceTicks > 0) {
            int oldTime = (*note)["time"].get<int>();
            int timeOffset = randomInt(-timingVarianceTicks, timingVarianceTicks);
            int newTime = std::max(0, oldTime + timeOffset);
            
            if (newTime != oldTime) {
                (*note)["time"] = newTime;
                modified = true;
            }
        }
        
        // Humaniser la vélocité
        if (note->contains("velocity") && velocityVariance > 0) {
            int oldVelocity = (*note)["velocity"].get<int>();
            int velocityOffset = randomInt(-velocityVariance, velocityVariance);
            int newVelocity = clamp(oldVelocity + velocityOffset, 1, 127);
            
            if (newVelocity != oldVelocity) {
                (*note)["velocity"] = newVelocity;
                modified = true;
            }
        }
        
        if (modified) {
            humanizedCount++;
        }
    }
    
    Logger::info("MidiTransforms", "Humanized " + std::to_string(humanizedCount) + " notes");
    return humanizedCount;
}

// ============================================================================
// DURÉES
// ============================================================================

int MidiTransforms::scaleDuration(json& jsonMidi,
                                  const std::vector<std::string>& noteIds,
                                  float factor) {
    Logger::debug("MidiTransforms", "Scaling duration by factor: " + 
        std::to_string(factor));
    
    factor = clamp(factor, 0.1f, 4.0f);
    
    std::vector<json*> notes = noteIds.empty() ? 
        findAllNotes(jsonMidi) : 
        findEventsByIds(jsonMidi, noteIds);
    
    int modifiedCount = 0;
    
    for (json* note : notes) {
        if (!note || !note->contains("duration")) {
            continue;
        }
        
        int oldDuration = (*note)["duration"].get<int>();
        int newDuration = std::max(1, static_cast<int>(oldDuration * factor));
        
        if (newDuration != oldDuration) {
            (*note)["duration"] = newDuration;
            modifiedCount++;
            
            Logger::debug("MidiTransforms", 
                "Scaled duration: " + std::to_string(oldDuration) + " → " + 
                std::to_string(newDuration));
        }
    }
    
    Logger::info("MidiTransforms", "Scaled duration for " + 
        std::to_string(modifiedCount) + " notes");
    return modifiedCount;
}

int MidiTransforms::setDuration(json& jsonMidi,
                               const std::vector<std::string>& noteIds,
                               int duration) {
    Logger::debug("MidiTransforms", "Setting duration to: " + 
        std::to_string(duration));
    
    duration = std::max(1, duration);
    
    std::vector<json*> notes = noteIds.empty() ? 
        findAllNotes(jsonMidi) : 
        findEventsByIds(jsonMidi, noteIds);
    
    int modifiedCount = 0;
    
    for (json* note : notes) {
        if (!note || !note->contains("duration")) {
            continue;
        }
        
        (*note)["duration"] = duration;
        modifiedCount++;
    }
    
    Logger::info("MidiTransforms", "Set duration for " + 
        std::to_string(modifiedCount) + " notes");
    return modifiedCount;
}

// ============================================================================
// TIMING
// ============================================================================

int MidiTransforms::moveNotes(json& jsonMidi,
                              const std::vector<std::string>& noteIds,
                              int deltaTime) {
    Logger::debug("MidiTransforms", "Moving notes by: " + 
        std::to_string(deltaTime) + " ticks");
    
    if (deltaTime == 0) {
        return 0;
    }
    
    std::vector<json*> notes = noteIds.empty() ? 
        findAllNotes(jsonMidi) : 
        findEventsByIds(jsonMidi, noteIds);
    
    int movedCount = 0;
    
    for (json* note : notes) {
        if (!note || !note->contains("time")) {
            continue;
        }
        
        int oldTime = (*note)["time"].get<int>();
        int newTime = std::max(0, oldTime + deltaTime);
        
        (*note)["time"] = newTime;
        movedCount++;
        
        Logger::debug("MidiTransforms", 
            "Moved note: " + std::to_string(oldTime) + " → " + 
            std::to_string(newTime));
    }
    
    Logger::info("MidiTransforms", "Moved " + std::to_string(movedCount) + " notes");
    return movedCount;
}

int MidiTransforms::moveAndTranspose(json& jsonMidi,
                                     const std::vector<std::string>& noteIds,
                                     int deltaTime,
                                     int deltaPitch) {
    Logger::debug("MidiTransforms", "Moving and transposing notes (time: " + 
        std::to_string(deltaTime) + ", pitch: " + std::to_string(deltaPitch) + ")");
    
    std::vector<json*> notes = noteIds.empty() ? 
        findAllNotes(jsonMidi) : 
        findEventsByIds(jsonMidi, noteIds);
    
    int modifiedCount = 0;
    
    for (json* note : notes) {
        if (!note) {
            continue;
        }
        
        // Déplacer dans le temps
        if (note->contains("time") && deltaTime != 0) {
            int oldTime = (*note)["time"].get<int>();
            int newTime = std::max(0, oldTime + deltaTime);
            (*note)["time"] = newTime;
        }
        
        // Transposer
        if (note->contains("note") && deltaPitch != 0) {
            int oldNote = (*note)["note"].get<int>();
            int newNote = transposeNote(oldNote, deltaPitch);
            (*note)["note"] = newNote;
        }
        
        modifiedCount++;
    }
    
    Logger::info("MidiTransforms", "Moved and transposed " + 
        std::to_string(modifiedCount) + " notes");
    return modifiedCount;
}

// ============================================================================
// UTILITAIRES
// ============================================================================

json* MidiTransforms::findEventById(json& jsonMidi, const std::string& noteId) {
    if (!jsonMidi.contains("tracks") || !jsonMidi["tracks"].is_array()) {
        return nullptr;
    }
    
    for (auto& track : jsonMidi["tracks"]) {
        if (!track.contains("events") || !track["events"].is_array()) {
            continue;
        }
        
        for (auto& event : track["events"]) {
            if (event.contains("id") && event["id"].get<std::string>() == noteId) {
                return &event;
            }
        }
    }
    
    return nullptr;
}

std::vector<json*> MidiTransforms::findEventsByIds(json& jsonMidi,
                                                    const std::vector<std::string>& noteIds) {
    std::vector<json*> result;
    
    for (const auto& noteId : noteIds) {
        json* event = findEventById(jsonMidi, noteId);
        if (event) {
            result.push_back(event);
        }
    }
    
    return result;
}

std::vector<json*> MidiTransforms::findAllNotes(json& jsonMidi) {
    std::vector<json*> result;
    
    if (!jsonMidi.contains("tracks") || !jsonMidi["tracks"].is_array()) {
        return result;
    }
    
    for (auto& track : jsonMidi["tracks"]) {
        if (!track.contains("events") || !track["events"].is_array()) {
            continue;
        }
        
        for (auto& event : track["events"]) {
            std::string type = event.value("type", "");
            
            if (type == "noteOn" || type == "note") {
                result.push_back(&event);
            }
        }
    }
    
    return result;
}

int MidiTransforms::msToTicks(int ms, int ppq, int tempo) {
    // Formula: ticks = (ms * ppq * tempo) / 60000
    return static_cast<int>((ms * ppq * tempo) / 60000.0);
}

int MidiTransforms::ticksToMs(int ticks, int ppq, int tempo) {
    // Formula: ms = (ticks * 60000) / (ppq * tempo)
    return static_cast<int>((ticks * 60000.0) / (ppq * tempo));
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiTransforms.cpp
// ============================================================================
