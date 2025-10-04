// ============================================================================
// Fichier: src/midi/MidiFileReader.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Lecteur de fichiers MIDI Standard (format SMF 0/1/2).
//   Parse les fichiers .mid et extrait tous les événements.
//
// Responsabilités:
//   - Lecture fichiers MIDI Standard
//   - Parsing header et tracks
//   - Extraction des événements
//   - Gestion des formats 0, 1 et 2
//
// Thread-safety: NON (utiliser par un seul thread)
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <memory>

#include "MidiMessage.h"
#include "../core/Logger.h"
#include "../core/Error.h"

namespace midiMind {

/**
 * @struct MidiFileHeader
 * @brief Header d'un fichier MIDI Standard
 */
struct MidiFileHeader {
    uint16_t format;        ///< Format (0, 1, ou 2)
    uint16_t numTracks;     ///< Nombre de pistes
    uint16_t division;      ///< Division (ticks par quarter note)
    
    MidiFileHeader() : format(1), numTracks(1), division(480) {}
    
    json toJson() const {
        json j;
        j["format"] = format;
        j["num_tracks"] = numTracks;
        j["division"] = division;
        return j;
    }
};

/**
 * @struct MidiEvent
 * @brief Événement MIDI avec timestamp delta
 */
struct MidiEvent {
    uint32_t deltaTime;     ///< Delta time en ticks
    uint32_t absoluteTime;  ///< Temps absolu en ticks
    MidiMessage message;    ///< Message MIDI
    
    MidiEvent() : deltaTime(0), absoluteTime(0) {}
    
    json toJson() const {
        json j;
        j["delta_time"] = deltaTime;
        j["absolute_time"] = absoluteTime;
        j["message"] = message.toJson();
        return j;
    }
};

/**
 * @struct MidiTrack
 * @brief Piste MIDI avec ses événements
 */
struct MidiTrack {
    std::string name;               ///< Nom de la piste (optionnel)
    std::vector<MidiEvent> events;  ///< Événements de la piste
    
    json toJson() const {
        json j;
        j["name"] = name;
        j["num_events"] = events.size();
        
        json eventsJson = json::array();
        for (const auto& event : events) {
            eventsJson.push_back(event.toJson());
        }
        j["events"] = eventsJson;
        
        return j;
    }
};

/**
 * @struct MidiFile
 * @brief Structure complète d'un fichier MIDI
 */
struct MidiFile {
    MidiFileHeader header;          ///< Header
    std::vector<MidiTrack> tracks;  ///< Pistes
    
    json toJson() const {
        json j;
        j["header"] = header.toJson();
        j["num_tracks"] = tracks.size();
        
        json tracksJson = json::array();
        for (const auto& track : tracks) {
            tracksJson.push_back(track.toJson());
        }
        j["tracks"] = tracksJson;
        
        return j;
    }
};

/**
 * @class MidiFileReader
 * @brief Lecteur de fichiers MIDI Standard
 * 
 * @details
 * Parse les fichiers MIDI Standard Format (SMF) 0, 1 et 2.
 * 
 * Formats supportés:
 * - Format 0: Une seule piste multi-canal
 * - Format 1: Pistes multiples, synchronisées
 * - Format 2: Pistes indépendantes (rare)
 * 
 * @example Utilisation
 * ```cpp
 * MidiFileReader reader;
 * 
 * // Lire un fichier
 * MidiFile midiFile = reader.read("song.mid");
 * 
 * // Parcourir les pistes
 * for (const auto& track : midiFile.tracks) {
 *     for (const auto& event : track.events) {
 *         // Traiter l'événement
 *     }
 * }
 * ```
 */
class MidiFileReader {
public:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    MidiFileReader();
    
    /**
     * @brief Destructeur
     */
    ~MidiFileReader();
    
    // ========================================================================
    // LECTURE
    // ========================================================================
    
    /**
     * @brief Lit un fichier MIDI
     * 
     * @param filepath Chemin du fichier
     * @return MidiFile Structure du fichier
     * 
     * @throws MidiMindException Si erreur de lecture
     */
    MidiFile read(const std::string& filepath);
    
    /**
     * @brief Lit depuis un buffer
     * 
     * @param data Données brutes
     * @param size Taille des données
     * @return MidiFile Structure du fichier
     * 
     * @throws MidiMindException Si erreur de parsing
     */
    MidiFile readFromBuffer(const uint8_t* data, size_t size);
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    /**
     * @brief Valide un fichier MIDI sans le parser complètement
     * 
     * @param filepath Chemin du fichier
     * @return true Si le fichier est valide
     */
    static bool validate(const std::string& filepath);

private:
    // ========================================================================
    // MÉTHODES PRIVÉES DE PARSING
    // ========================================================================
    
    /**
     * @brief Parse le header
     */
    MidiFileHeader parseHeader(std::ifstream& file);
    
    /**
     * @brief Parse une piste
     */
    MidiTrack parseTrack(std::ifstream& file);
    
    /**
     * @brief Parse un événement
     */
    MidiEvent parseEvent(std::ifstream& file, uint8_t& runningStatus);
    
    /**
     * @brief Lit un nombre variable length
     */
    uint32_t readVariableLength(std::ifstream& file);
    
    /**
     * @brief Lit un uint16 big-endian
     */
    uint16_t readUInt16(std::ifstream& file);
    
    /**
     * @brief Lit un uint32 big-endian
     */
    uint32_t readUInt32(std::ifstream& file);
    
    /**
     * @brief Lit un uint8
     */
    uint8_t readUInt8(std::ifstream& file);
    
    /**
     * @brief Vérifie une signature (ex: "MThd")
     */
    void verifySignature(std::ifstream& file, const std::string& expected);
    
    /**
     * @brief Parse un meta-event
     */
    MidiEvent parseMetaEvent(std::ifstream& file, uint32_t deltaTime);
    
    /**
     * @brief Parse un SysEx event
     */
    MidiEvent parseSysExEvent(std::ifstream& file, uint32_t deltaTime, uint8_t status);
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiFileReader.h
// ============================================================================