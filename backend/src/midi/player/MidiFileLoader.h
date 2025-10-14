// ============================================================================
// src/midi/player/MidiFileLoader.h
// Responsabilité: Chargement et parsing des fichiers MIDI
// ============================================================================
#pragma once

#include <string>
#include <memory>
#include <midifile/MidiFile.h>
#include "../../core/Logger.h"

namespace midiMind {

struct LoadedMidiFile {
    std::string filepath;
    smf::MidiFile midiFile;
    uint32_t durationMs;
    uint16_t trackCount;
    uint16_t ticksPerQuarterNote;
    bool isValid;
};

class MidiFileLoader {
public:
    MidiFileLoader() = default;
    
    /**
     * @brief Charge un fichier MIDI depuis le disque
     * @param filepath Chemin du fichier
     * @return Structure avec fichier chargé et métadonnées
     */
    std::unique_ptr<LoadedMidiFile> load(const std::string& filepath) {
        auto result = std::make_unique<LoadedMidiFile>();
        result->filepath = filepath;
        result->isValid = false;
        
        try {
            result->midiFile.read(filepath);
            
            if (!result->midiFile.status()) {
                Logger::error("MidiFileLoader", "Failed to read: " + filepath);
                return result;
            }
            
            // Analyse temporelle
            result->midiFile.doTimeAnalysis();
            result->midiFile.linkNotePairs();
            
            // Extraire métadonnées
            result->durationMs = (uint32_t)(result->midiFile.getFileDurationInSeconds() * 1000.0);
            result->trackCount = result->midiFile.getTrackCount();
            result->ticksPerQuarterNote = result->midiFile.getTicksPerQuarterNote();
            result->isValid = true;
            
            Logger::info("MidiFileLoader", 
                "✓ Loaded: " + filepath + " (" + 
                std::to_string(result->trackCount) + " tracks, " +
                std::to_string(result->durationMs) + "ms)");
            
            return result;
            
        } catch (const std::exception& e) {
            Logger::error("MidiFileLoader", "Exception: " + std::string(e.what()));
            return result;
        }
    }
    
    /**
     * @brief Valide un fichier sans le charger complètement
     */
    bool validate(const std::string& filepath) {
        try {
            smf::MidiFile temp;
            temp.read(filepath);
            return temp.status();
        } catch (...) {
            return false;
        }
    }
};
