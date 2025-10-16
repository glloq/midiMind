// ============================================================================
// Fichier: src/midi/JsonMidiConverter.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Convertisseur bidirectionnel MIDI ↔ JsonMidi.
//   Permet de transformer des fichiers MIDI binaires en format JSON éditable
//   et vice-versa.
//
// Responsabilités:
//   - Conversion MidiFile → JsonMidi
//   - Conversion JsonMidi → MidiFile
//   - Fusion des tracks en timeline unifiée
//   - Calcul des durées de notes (noteOn → noteOff)
//   - Conversion temps (ticks ↔ millisecondes)
//   - Extraction des métadonnées MIDI
//
// Format JsonMidi:
//   - Timeline unifiée (tous événements chronologiques)
//   - Temps en millisecondes
//   - Notes avec durée intégrée
//   - Métadonnées enrichies
//
// Auteur: MidiMind Team
// Date: 2025-10-05
// Version: 3.0.0
// ============================================================================

#pragma once

#include "file/MidiFileReader.h"
#include "file/MidiFileWriter.h"
#include "MidiMessage.h"
#include "../core/Logger.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace midiMind {

using json = nlohmann::json;

// ============================================================================
// STRUCTURES JsonMidi
// ============================================================================

/**
 * @struct JsonMidiEvent
 * @brief Événement MIDI en format JSON
 */
struct JsonMidiEvent {
    std::string id;           // Identifiant unique
    std::string type;         // Type: noteOn, cc, programChange, etc.
    uint32_t time;           // Temps en ms
    uint8_t channel;         // Canal MIDI (1-16)
    
    // Données spécifiques selon le type
    std::optional<uint8_t> note;        // Pour noteOn/noteOff
    std::optional<uint8_t> velocity;    // Pour noteOn/noteOff
    std::optional<uint32_t> duration;   // Pour noteOn
    std::optional<uint8_t> controller;  // Pour CC
    std::optional<uint8_t> value;       // Pour CC, programChange
    std::optional<uint16_t> pitchBend;  // Pour pitchBend
    std::optional<uint32_t> tempo;      // Pour setTempo
    std::optional<std::string> text;    // Pour marker, text events
    std::optional<std::vector<uint8_t>> data; // Pour SysEx
    
    // Conversion vers JSON
    json toJson() const;
    
    // Création depuis JSON
    static JsonMidiEvent fromJson(const json& j);
};

/**
 * @struct JsonMidiMetadata
 * @brief Métadonnées du fichier MIDI
 */
struct JsonMidiMetadata {
    std::string title;
    std::string author;
    uint32_t tempo = 120;
    std::string timeSignature = "4/4";
    std::string keySignature = "C";
    uint32_t duration = 0;
    uint16_t ticksPerBeat = 480;
    uint16_t midiFormat = 1;
    uint16_t trackCount = 0;
    std::string createdAt;
    std::string modifiedAt;
    
    json toJson() const;
    static JsonMidiMetadata fromJson(const json& j);
};

/**
 * @struct JsonMidiTrack
 * @brief Information de piste
 */
struct JsonMidiTrack {
    uint16_t id;
    std::string name;
    uint8_t channel;
    bool muted = false;
    bool solo = false;
    uint8_t volume = 100;
    uint8_t pan = 64;
    int8_t transpose = 0;
    std::string color = "#667eea";
    
    struct {
        uint8_t program = 0;
        uint8_t bank = 0;
        std::string name;
    } instrument;
    
    json toJson() const;
    static JsonMidiTrack fromJson(const json& j);
};

/**
 * @struct JsonMidiMarker
 * @brief Marqueur de position
 */
struct JsonMidiMarker {
    std::string id;
    uint32_t time;
    std::string label;
    std::string color = "#667eea";
    
    json toJson() const;
    static JsonMidiMarker fromJson(const json& j);
};

/**
 * @struct JsonMidi
 * @brief Structure complète JsonMidi
 */
struct JsonMidi {
    std::string format = "jsonmidi-v1.0";
    std::string version = "1.0.0";
    JsonMidiMetadata metadata;
    std::vector<JsonMidiEvent> timeline;
    std::vector<JsonMidiTrack> tracks;
    std::vector<JsonMidiMarker> markers;
    
    json toJson() const;
    static JsonMidi fromJson(const json& j);
    static JsonMidi fromString(const std::string& jsonStr);
    std::string toString(int indent = 2) const;
};

// ============================================================================
// CLASSE: JsonMidiConverter
// ============================================================================

/**
 * @class JsonMidiConverter
 * @brief Convertisseur bidirectionnel MIDI ↔ JsonMidi
 * 
 * @details
 * Cette classe permet de convertir des fichiers MIDI binaires (format SMF)
 * en format JSON éditable (JsonMidi) et vice-versa.
 * 
 * Conversions supportées:
 * - MidiFile → JsonMidi (parsing + fusion tracks)
 * - JsonMidi → MidiFile (split timeline + écriture binaire)
 * - Calcul automatique des durées de notes
 * - Conversion temps ticks ↔ millisecondes
 * - Extraction métadonnées (tempo, time signature, etc.)
 * 
 * @example Utilisation basique
 * @code
 * JsonMidiConverter converter;
 * 
 * // MIDI → JsonMidi
 * MidiFile midiFile = reader.read("input.mid");
 * JsonMidi jsonMidi = converter.midiToJson(midiFile);
 * std::string jsonStr = jsonMidi.toString();
 * 
 * // JsonMidi → MIDI
 * JsonMidi loaded = JsonMidi::fromString(jsonStr);
 * MidiFile output = converter.jsonToMidi(loaded);
 * writer.write("output.mid", output);
 * @endcode
 */
class JsonMidiConverter {
public:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    JsonMidiConverter();
    
    /**
     * @brief Destructeur
     */
    ~JsonMidiConverter() = default;
    
    // ========================================================================
    // CONVERSION MIDI → JsonMidi
    // ========================================================================
    
    /**
     * @brief Convertit un MidiFile en JsonMidi
     * 
     * @param midiFile Fichier MIDI à convertir
     * @return JsonMidi Structure JSON
     * 
     * @note Fusionne toutes les pistes en une timeline unique
     * @note Calcule automatiquement les durées des notes
     * @note Convertit les ticks en millisecondes
     */
    JsonMidi midiToJson(const MidiFile& midiFile);
    
    /**
     * @brief Convertit depuis un fichier MIDI
     * 
     * @param filepath Chemin du fichier MIDI
     * @return JsonMidi Structure JSON
     */
    JsonMidi midiFileToJson(const std::string& filepath);
    
    // ========================================================================
    // CONVERSION JsonMidi → MIDI
    // ========================================================================
    
    /**
     * @brief Convertit JsonMidi en MidiFile
     * 
     * @param jsonMidi Structure JSON
     * @return MidiFile Fichier MIDI binaire
     * 
     * @note Groupe la timeline par pistes/canaux
     * @note Convertit millisecondes en ticks
     * @note Crée noteOff depuis noteOn.duration
     */
    MidiFile jsonToMidi(const JsonMidi& jsonMidi);
    
    /**
     * @brief Convertit et écrit dans un fichier
     * 
     * @param jsonMidi Structure JSON
     * @param filepath Chemin de sortie
     */
    void jsonToMidiFile(const JsonMidi& jsonMidi, const std::string& filepath);
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    /**
     * @brief Valide une structure JsonMidi
     * 
     * @param jsonMidi Structure à valider
     * @param errorMessage Message d'erreur (output)
     * @return true Si valide
     */
    bool validate(const JsonMidi& jsonMidi, std::string& errorMessage) const;

private:
    // ========================================================================
    // CONVERSION INTERNE - MIDI → JsonMidi
    // ========================================================================
    
    /**
     * @brief Fusionne les pistes en timeline
     */
    std::vector<JsonMidiEvent> mergeTracksToTimeline(
        const std::vector<MidiTrack>& tracks,
        uint16_t ticksPerBeat,
        uint32_t tempo
    );
    
    /**
     * @brief Convertit un événement MIDI en JsonMidiEvent
     */
    JsonMidiEvent midiEventToJson(
        const MidiEvent& midiEvent,
        uint32_t absoluteTime,
        uint16_t ticksPerBeat,
        uint32_t tempo,
        uint8_t defaultChannel = 1
    );
    
    /**
     * @brief Calcule les durées des notes
     */
    void calculateNoteDurations(std::vector<JsonMidiEvent>& timeline);
    
    /**
     * @brief Extrait les métadonnées d'un MidiFile
     */
    JsonMidiMetadata extractMetadata(
        const MidiFile& midiFile,
        const std::vector<JsonMidiEvent>& timeline
    );
    
    /**
     * @brief Extrait les informations de pistes
     */
    std::vector<JsonMidiTrack> extractTracks(
        const MidiFile& midiFile,
        const std::vector<JsonMidiEvent>& timeline
    );
    
    // ========================================================================
    // CONVERSION INTERNE - JsonMidi → MIDI
    // ========================================================================
    
    /**
     * @brief Groupe la timeline par pistes
     */
    std::vector<MidiTrack> splitTimelineToTracks(
        const std::vector<JsonMidiEvent>& timeline,
        const JsonMidiMetadata& metadata
    );
    
    /**
     * @brief Convertit JsonMidiEvent en MidiEvent
     */
    std::vector<MidiEvent> jsonEventToMidi(
        const JsonMidiEvent& jsonEvent,
        uint16_t ticksPerBeat,
        uint32_t tempo
    );
    
    /**
     * @brief Crée la piste de tempo (track 0)
     */
    MidiTrack createTempoTrack(const JsonMidiMetadata& metadata);
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Convertit ticks → millisecondes
     * 
     * @param ticks Ticks MIDI
     * @param ticksPerBeat Division
     * @param tempo BPM
     * @return uint32_t Temps en ms
     */
    uint32_t ticksToMs(uint32_t ticks, uint16_t ticksPerBeat, uint32_t tempo) const;
    
    /**
     * @brief Convertit millisecondes → ticks
     * 
     * @param ms Temps en ms
     * @param ticksPerBeat Division
     * @param tempo BPM
     * @return uint32_t Ticks MIDI
     */
    uint32_t msToTicks(uint32_t ms, uint16_t ticksPerBeat, uint32_t tempo) const;
    
    /**
     * @brief Génère un ID unique pour un événement
     */
    std::string generateEventId(
        const std::string& type,
        uint32_t time,
        uint8_t channel,
        uint8_t data1 = 0
    ) const;
    
    /**
     * @brief Extrait le tempo depuis la timeline
     */
    uint32_t extractTempo(const std::vector<JsonMidiEvent>& timeline) const;
    
    /**
     * @brief Extrait la time signature
     */
    std::string extractTimeSignature(const std::vector<JsonMidiEvent>& timeline) const;
    
    /**
     * @brief Extrait le titre depuis meta-events
     */
    std::string extractTitle(const std::vector<MidiTrack>& tracks) const;
    
    /**
     * @brief Extrait l'auteur depuis meta-events
     */
    std::string extractAuthor(const std::vector<MidiTrack>& tracks) const;
    
    // ========================================================================
    // MEMBRES
    // ========================================================================
    
    /// Tempo par défaut (BPM)
    uint32_t defaultTempo_ = 120;
    
    /// Time signature par défaut
    std::string defaultTimeSignature_ = "4/4";
};

// ============================================================================
// FONCTIONS UTILITAIRES GLOBALES
// ============================================================================

/**
 * @brief Charge un JsonMidi depuis un fichier JSON
 */
inline JsonMidi loadJsonMidi(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        THROW_ERROR(ErrorCode::MIDI_FILE_OPEN_FAILED,
                   "Cannot open JSON file: " + filepath);
    }
    
    json j;
    file >> j;
    return JsonMidi::fromJson(j);
}

/**
 * @brief Sauvegarde un JsonMidi dans un fichier JSON
 */
inline void saveJsonMidi(const JsonMidi& jsonMidi, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        THROW_ERROR(ErrorCode::MIDI_FILE_OPEN_FAILED,
                   "Cannot create JSON file: " + filepath);
    }
    
    file << jsonMidi.toString(2);
    file.close();
}

/**
 * @brief Conversion rapide MIDI → JSON
 */
inline JsonMidi quickMidiToJson(const std::string& midiPath) {
    JsonMidiConverter converter;
    return converter.midiFileToJson(midiPath);
}

/**
 * @brief Conversion rapide JSON → MIDI
 */
inline void quickJsonToMidi(const std::string& jsonPath, const std::string& midiPath) {
    JsonMidi jsonMidi = loadJsonMidi(jsonPath);
    JsonMidiConverter converter;
    converter.jsonToMidiFile(jsonMidi, midiPath);
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER JsonMidiConverter.h
// ============================================================================