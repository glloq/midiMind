// ============================================================================
// Fichier: backend/src/midi/MidiValidator.cpp
// Version: 3.1.0
// Date: 2025-10-10
// Projet: MidiMind v3.1 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Implémentation du validateur de données MIDI et JsonMidi.
//
// Auteur: MidiMind Team
// Statut: ✅ PHASE 1 - COMPLET
// ============================================================================

#include "MidiValidator.h"
#include "../core/Logger.h"
#include <map>
#include <algorithm>

namespace midiMind {

// ============================================================================
// TYPES D'EVENTS VALIDES (static)
// ============================================================================

const std::set<std::string> MidiValidator::VALID_EVENT_TYPES = {
    "noteOn", "noteOff", "note",
    "cc", "controlChange",
    "programChange", "pc",
    "pitchBend", "pb",
    "aftertouch", "channelPressure", "polyPressure",
    "meta", "sysex",
    "tempo", "timeSignature", "keySignature",
    "marker", "text", "lyric", "cuePoint",
    "endOfTrack"
};

// ============================================================================
// CONSTRUCTION
// ============================================================================

MidiValidator::MidiValidator() {
    Logger::debug("MidiValidator", "Constructor called");
}

// ============================================================================
// VALIDATION PRINCIPALE
// ============================================================================

ValidationResult MidiValidator::validate(const json& jsonMidi) const {
    Logger::debug("MidiValidator", "Validating JsonMidi...");
    
    ValidationResult result;
    
    // Vérification structure de base
    if (!jsonMidi.is_object()) {
        addError(result, "structure", "JsonMidi must be an object");
        return result;
    }
    
    // Validation header
    if (jsonMidi.contains("header")) {
        auto headerResult = validateHeader(jsonMidi["header"]);
        result.errors.insert(result.errors.end(), 
                           headerResult.errors.begin(), 
                           headerResult.errors.end());
        result.warnings.insert(result.warnings.end(), 
                             headerResult.warnings.begin(), 
                             headerResult.warnings.end());
        if (!headerResult.isValid) {
            result.isValid = false;
        }
    } else {
        addError(result, "header", "Missing 'header' field");
    }
    
    // Validation tracks
    if (jsonMidi.contains("tracks")) {
        if (!jsonMidi["tracks"].is_array()) {
            addError(result, "tracks", "Field 'tracks' must be an array");
        } else {
            const auto& tracks = jsonMidi["tracks"];
            
            if (tracks.empty()) {
                addWarning(result, "tracks", "No tracks found in file");
            }
            
            for (size_t i = 0; i < tracks.size(); i++) {
                auto trackResult = validateTrack(tracks[i], i);
                result.errors.insert(result.errors.end(), 
                                   trackResult.errors.begin(), 
                                   trackResult.errors.end());
                result.warnings.insert(result.warnings.end(), 
                                     trackResult.warnings.begin(), 
                                     trackResult.warnings.end());
                if (!trackResult.isValid) {
                    result.isValid = false;
                }
            }
        }
    } else {
        addError(result, "tracks", "Missing 'tracks' field");
    }
    
    if (result.isValid) {
        Logger::info("MidiValidator", "Validation passed");
    } else {
        Logger::warn("MidiValidator", 
            "Validation failed with " + std::to_string(result.errors.size()) + " error(s)");
    }
    
    return result;
}

ValidationResult MidiValidator::validateHeader(const json& header) const {
    ValidationResult result;
    
    if (!header.is_object()) {
        addError(result, "header", "Header must be an object");
        return result;
    }
    
    // Vérifier champs requis
    std::vector<std::string> requiredFields = {"format", "tracks", "ppq"};
    if (!hasRequiredFields(header, requiredFields, result, "header")) {
        return result;
    }
    
    // Valider format (0, 1, ou 2)
    if (header.contains("format")) {
        int format = header["format"].get<int>();
        if (format < 0 || format > 2) {
            addError(result, "header", 
                "Invalid format: " + std::to_string(format) + " (must be 0, 1, or 2)",
                "header.format");
        }
    }
    
    // Valider tracks count
    if (header.contains("tracks")) {
        int tracks = header["tracks"].get<int>();
        if (tracks < 0) {
            addError(result, "header", 
                "Invalid track count: " + std::to_string(tracks),
                "header.tracks");
        }
        if (tracks > 256) {
            addWarning(result, "header", 
                "Unusually high track count: " + std::to_string(tracks),
                "header.tracks");
        }
    }
    
    // Valider PPQ (ticks per quarter note)
    if (header.contains("ppq")) {
        int ppq = header["ppq"].get<int>();
        if (ppq <= 0) {
            addError(result, "header", 
                "Invalid PPQ: " + std::to_string(ppq) + " (must be > 0)",
                "header.ppq");
        }
        if (ppq < 24) {
            addWarning(result, "header", 
                "Low PPQ value: " + std::to_string(ppq) + " (< 24 may cause timing issues)",
                "header.ppq");
        }
    }
    
    return result;
}

ValidationResult MidiValidator::validateTrack(const json& track, int trackIndex) const {
    ValidationResult result;
    
    std::string location = "track " + std::to_string(trackIndex);
    
    if (!track.is_object()) {
        addError(result, "track", "Track must be an object", location);
        return result;
    }
    
    // Valider events si présent
    if (track.contains("events")) {
        if (!track["events"].is_array()) {
            addError(result, "track", "Field 'events' must be an array", location);
        } else {
            const auto& events = track["events"];
            
            for (size_t i = 0; i < events.size(); i++) {
                std::string eventLocation = location + ", event " + std::to_string(i);
                auto eventResult = validateEvent(events[i], eventLocation);
                result.errors.insert(result.errors.end(), 
                                   eventResult.errors.begin(), 
                                   eventResult.errors.end());
                result.warnings.insert(result.warnings.end(), 
                                     eventResult.warnings.begin(), 
                                     eventResult.warnings.end());
                if (!eventResult.isValid) {
                    result.isValid = false;
                }
            }
            
            // Vérifier tri temporel
            auto sortResult = validateTimelineSorting(track, trackIndex);
            result.errors.insert(result.errors.end(), 
                               sortResult.errors.begin(), 
                               sortResult.errors.end());
            result.warnings.insert(result.warnings.end(), 
                                 sortResult.warnings.begin(), 
                                 sortResult.warnings.end());
            
            // Détecter overlaps de notes
            auto overlapResult = detectNoteOverlaps(track, trackIndex);
            result.warnings.insert(result.warnings.end(), 
                                 overlapResult.warnings.begin(), 
                                 overlapResult.warnings.end());
        }
    }
    
    return result;
}

ValidationResult MidiValidator::validateEvent(const json& event, const std::string& location) const {
    ValidationResult result;
    
    if (!event.is_object()) {
        addError(result, "event", "Event must be an object", location);
        return result;
    }
    
    // Vérifier type
    if (!event.contains("type")) {
        addError(result, "event", "Missing 'type' field", location);
        return result;
    }
    
    std::string type = event["type"].get<std::string>();
    
    if (!isValidEventType(type)) {
        addWarning(result, "event", 
            "Unknown event type: " + type, 
            location);
    }
    
    // Vérifier time
    if (!event.contains("time")) {
        addError(result, "event", "Missing 'time' field", location);
    } else if (!event["time"].is_number()) {
        addError(result, "event", "Field 'time' must be a number", location);
    } else {
        int time = event["time"].get<int>();
        if (time < 0) {
            addError(result, "event", 
                "Invalid time: " + std::to_string(time) + " (must be >= 0)", 
                location);
        }
    }
    
    // Validation spécifique par type
    if (type == "noteOn" || type == "noteOff" || type == "note") {
        auto noteResult = validateNote(event, location);
        result.errors.insert(result.errors.end(), 
                           noteResult.errors.begin(), 
                           noteResult.errors.end());
        result.warnings.insert(result.warnings.end(), 
                             noteResult.warnings.begin(), 
                             noteResult.warnings.end());
        if (!noteResult.isValid) {
            result.isValid = false;
        }
    } else if (type == "cc" || type == "controlChange") {
        auto ccResult = validateCC(event, location);
        result.errors.insert(result.errors.end(), 
                           ccResult.errors.begin(), 
                           ccResult.errors.end());
        result.warnings.insert(result.warnings.end(), 
                             ccResult.warnings.begin(), 
                             ccResult.warnings.end());
        if (!ccResult.isValid) {
            result.isValid = false;
        }
    }
    
    // Valider channel si présent
    if (event.contains("channel")) {
        int channel = event["channel"].get<int>();
        if (!isValidMidiChannel(channel)) {
            addError(result, "event", 
                "Invalid channel: " + std::to_string(channel) + " (must be 0-15)", 
                location);
        }
    }
    
    return result;
}

// ============================================================================
// VALIDATION SPÉCIFIQUE
// ============================================================================

ValidationResult MidiValidator::validateNote(const json& note, const std::string& location) const {
    ValidationResult result;
    
    // Vérifier note pitch
    if (!note.contains("note")) {
        addError(result, "note", "Missing 'note' field", location);
    } else {
        int pitch = note["note"].get<int>();
        if (!isValidMidiValue(pitch)) {
            addError(result, "note", 
                "Invalid note pitch: " + std::to_string(pitch) + " (must be 0-127)", 
                location);
        }
    }
    
    // Vérifier velocity
    if (!note.contains("velocity")) {
        addError(result, "note", "Missing 'velocity' field", location);
    } else {
        int velocity = note["velocity"].get<int>();
        if (!isValidMidiValue(velocity)) {
            addError(result, "note", 
                "Invalid velocity: " + std::to_string(velocity) + " (must be 0-127)", 
                location);
        }
        if (velocity == 0 && note["type"].get<std::string>() == "noteOn") {
            addWarning(result, "note", 
                "Note On with velocity 0 (equivalent to Note Off)", 
                location);
        }
    }
    
    // Vérifier duration si présent
    if (note.contains("duration")) {
        int duration = note["duration"].get<int>();
        if (duration < 0) {
            addError(result, "note", 
                "Invalid duration: " + std::to_string(duration) + " (must be >= 0)", 
                location);
        }
        if (duration == 0) {
            addWarning(result, "note", "Note with zero duration", location);
        }
    }
    
    return result;
}

ValidationResult MidiValidator::validateCC(const json& cc, const std::string& location) const {
    ValidationResult result;
    
    // Vérifier controller
    if (!cc.contains("controller")) {
        addError(result, "cc", "Missing 'controller' field", location);
    } else {
        int controller = cc["controller"].get<int>();
        if (!isValidMidiValue(controller)) {
            addError(result, "cc", 
                "Invalid controller: " + std::to_string(controller) + " (must be 0-127)", 
                location);
        }
    }
    
    // Vérifier value
    if (!cc.contains("value")) {
        addError(result, "cc", "Missing 'value' field", location);
    } else {
        int value = cc["value"].get<int>();
        if (!isValidMidiValue(value)) {
            addError(result, "cc", 
                "Invalid value: " + std::to_string(value) + " (must be 0-127)", 
                location);
        }
    }
    
    return result;
}

ValidationResult MidiValidator::detectNoteOverlaps(const json& track, int trackIndex) const {
    ValidationResult result;
    
    if (!track.contains("events") || !track["events"].is_array()) {
        return result;
    }
    
    const auto& events = track["events"];
    
    // Map: (channel, pitch) -> liste des notes actives
    std::map<std::pair<int, int>, std::vector<json>> activeNotes;
    
    for (size_t i = 0; i < events.size(); i++) {
        const auto& event = events[i];
        
        if (!event.contains("type") || !event.contains("time")) {
            continue;
        }
        
        std::string type = event["type"].get<std::string>();
        
        if (type == "noteOn" || type == "note") {
            int channel = event.value("channel", 0);
            int pitch = event.value("note", 0);
            int time = event["time"].get<int>();
            int duration = event.value("duration", 0);
            
            auto key = std::make_pair(channel, pitch);
            
            // Vérifier overlaps avec notes actives
            for (const auto& activeNote : activeNotes[key]) {
                int activeTime = activeNote["time"].get<int>();
                int activeDuration = activeNote.value("duration", 0);
                int activeEnd = activeTime + activeDuration;
                
                if (time < activeEnd) {
                    std::string location = "track " + std::to_string(trackIndex) + 
                                         ", event " + std::to_string(i);
                    addWarning(result, "note", 
                        "Note overlap detected: pitch " + std::to_string(pitch) + 
                        " on channel " + std::to_string(channel), 
                        location,
                        {{"note1_time", activeTime}, {"note2_time", time}});
                }
            }
            
            // Ajouter la note aux actives
            activeNotes[key].push_back(event);
        }
    }
    
    return result;
}

ValidationResult MidiValidator::validateTimelineSorting(const json& track, int trackIndex) const {
    ValidationResult result;
    
    if (!track.contains("events") || !track["events"].is_array()) {
        return result;
    }
    
    const auto& events = track["events"];
    
    if (events.empty()) {
        return result;
    }
    
    int lastTime = -1;
    
    for (size_t i = 0; i < events.size(); i++) {
        const auto& event = events[i];
        
        if (!event.contains("time")) {
            continue;
        }
        
        int time = event["time"].get<int>();
        
        if (time < lastTime) {
            std::string location = "track " + std::to_string(trackIndex) + 
                                 ", event " + std::to_string(i);
            addError(result, "timeline", 
                "Events not sorted by time: " + std::to_string(time) + 
                " < " + std::to_string(lastTime), 
                location);
        }
        
        lastTime = time;
    }
    
    return result;
}

// ============================================================================
// UTILITAIRES
// ============================================================================

bool MidiValidator::isValidMidiValue(int value) {
    return value >= 0 && value <= 127;
}

bool MidiValidator::isValidMidiChannel(int channel) {
    return channel >= 0 && channel <= 15;
}

bool MidiValidator::isValidEventType(const std::string& type) {
    return VALID_EVENT_TYPES.find(type) != VALID_EVENT_TYPES.end();
}

const std::set<std::string>& MidiValidator::getValidEventTypes() {
    return VALID_EVENT_TYPES;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

bool MidiValidator::hasRequiredFields(const json& obj, 
                                       const std::vector<std::string>& fields,
                                       ValidationResult& result,
                                       const std::string& location) const {
    bool allPresent = true;
    
    for (const auto& field : fields) {
        if (!obj.contains(field)) {
            addError(result, "structure", 
                "Missing required field: '" + field + "'", 
                location);
            allPresent = false;
        }
    }
    
    return allPresent;
}

void MidiValidator::addError(ValidationResult& result,
                             const std::string& category,
                             const std::string& message,
                             const std::string& location,
                             const json& context) const {
    result.addIssue(ValidationIssue(ValidationSeverity::ERROR, 
                                   category, message, location, context));
}

void MidiValidator::addWarning(ValidationResult& result,
                               const std::string& category,
                               const std::string& message,
                               const std::string& location,
                               const json& context) const {
    result.addIssue(ValidationIssue(ValidationSeverity::WARNING, 
                                   category, message, location, context));
}

void MidiValidator::addInfo(ValidationResult& result,
                            const std::string& category,
                            const std::string& message,
                            const std::string& location,
                            const json& context) const {
    result.addIssue(ValidationIssue(ValidationSeverity::INFO, 
                                   category, message, location, context));
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiValidator.cpp
// ============================================================================
