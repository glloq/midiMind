// ============================================================================
// File: backend/src/midi/JsonMidiConverter.cpp
// Version: 4.2.7 - UTF-8 SANITIZATION FIX
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of JsonMidiConverter.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.2.7:
//   - Added: UTF-8 sanitization for text fields to prevent JSON encoding errors
//   - Fixed: Meta event text fields with non-UTF-8 bytes now handled correctly
//
// Changes v4.2.2:
//   - Fixed bank extraction (MSB/LSB from CC 0/32)
//
// ============================================================================

#include "JsonMidiConverter.h"
#include "file/MidiFileReader.h"
#include "../core/Logger.h"
#include <algorithm>
#include <sstream>
#include <set> 
#include <iomanip>
#include <fstream>

namespace midiMind {

// ============================================================================
// UTF-8 SANITIZATION HELPER
// ============================================================================

/**
 * Sanitize a string to ensure it's valid UTF-8.
 * Invalid bytes are replaced with '?'.
 * Control characters (except \n \r \t) are replaced with spaces.
 */
static std::string sanitizeUtf8(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    
    for (size_t i = 0; i < str.size(); ) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        
        // ASCII 7-bit (0x00-0x7F)
        if (c < 0x80) {
            // Replace control characters with space (except \n \r \t)
            if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') {
                result += ' ';
            } else {
                result += c;
            }
            i++;
        }
        // UTF-8 2-byte (0xC0-0xDF)
        else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < str.size() && 
                (static_cast<unsigned char>(str[i+1]) & 0xC0) == 0x80) {
                result += str[i];
                result += str[i+1];
                i += 2;
            } else {
                result += '?';
                i++;
            }
        }
        // UTF-8 3-byte (0xE0-0xEF)
        else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < str.size() &&
                (static_cast<unsigned char>(str[i+1]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(str[i+2]) & 0xC0) == 0x80) {
                result += str[i];
                result += str[i+1];
                result += str[i+2];
                i += 3;
            } else {
                result += '?';
                i++;
            }
        }
        // UTF-8 4-byte (0xF0-0xF7)
        else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < str.size() &&
                (static_cast<unsigned char>(str[i+1]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(str[i+2]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(str[i+3]) & 0xC0) == 0x80) {
                result += str[i];
                result += str[i+1];
                result += str[i+2];
                result += str[i+3];
                i += 4;
            } else {
                result += '?';
                i++;
            }
        }
        // Invalid byte
        else {
            result += '?';
            i++;
        }
    }
    
    return result;
}

// ============================================================================
// JsonMidiEvent IMPLEMENTATION
// ============================================================================

json JsonMidiEvent::toJson() const {
    json j = {
        {"id", id},
        {"type", type},
        {"time", time},
        {"channel", channel}
    };
    
    if (note.has_value()) j["note"] = note.value();
    if (velocity.has_value()) j["velocity"] = velocity.value();
    if (duration.has_value()) j["duration"] = duration.value();
    if (controller.has_value()) j["controller"] = controller.value();
    if (value.has_value()) j["value"] = value.value();
    if (pitchBend.has_value()) j["pitchBend"] = pitchBend.value();
    if (program.has_value()) j["program"] = program.value();
    if (tempo.has_value()) j["tempo"] = tempo.value();
    
    // CORRECTION v4.2.7: Sanitize text to prevent UTF-8 encoding errors
    if (text.has_value()) {
        j["text"] = sanitizeUtf8(text.value());
    }
    
    if (data.has_value()) j["data"] = data.value();
    
    return j;
}

JsonMidiEvent JsonMidiEvent::fromJson(const json& j) {
    JsonMidiEvent event;
    
    event.id = j.value("id", "");
    event.type = j.value("type", "");
    event.time = j.value("time", 0);
    event.channel = j.value("channel", 1);
    
    if (j.contains("note") && j["note"].is_number_unsigned()) {
        event.note = j["note"].get<uint8_t>();
    }
    if (j.contains("velocity") && j["velocity"].is_number_unsigned()) {
        event.velocity = j["velocity"].get<uint8_t>();
    }
    if (j.contains("duration") && j["duration"].is_number_unsigned()) {
        event.duration = j["duration"].get<uint32_t>();
    }
    if (j.contains("controller") && j["controller"].is_number_unsigned()) {
        event.controller = j["controller"].get<uint8_t>();
    }
    if (j.contains("value") && j["value"].is_number_unsigned()) {
        event.value = j["value"].get<uint8_t>();
    }
    if (j.contains("pitchBend") && j["pitchBend"].is_number_integer()) {
        event.pitchBend = j["pitchBend"].get<int16_t>();
    }
    if (j.contains("program") && j["program"].is_number_unsigned()) {
        event.program = j["program"].get<uint8_t>();
    }
    if (j.contains("tempo") && j["tempo"].is_number_unsigned()) {
        event.tempo = j["tempo"].get<uint32_t>();
    }
    if (j.contains("text") && j["text"].is_string()) {
        event.text = j["text"].get<std::string>();
    }
    if (j.contains("data") && j["data"].is_array()) {
        try {
            event.data = j["data"].get<std::vector<uint8_t>>();
        } catch (const json::exception&) {
            // Invalid array content, skip
        }
    }
    
    return event;
}

// ============================================================================
// JsonMidiMetadata IMPLEMENTATION
// ============================================================================

json JsonMidiMetadata::toJson() const {
    return {
        {"title", sanitizeUtf8(title)},
        {"artist", sanitizeUtf8(artist)},
        {"album", sanitizeUtf8(album)},
        {"genre", sanitizeUtf8(genre)},
        {"copyright", sanitizeUtf8(copyright)},
        {"comment", sanitizeUtf8(comment)},
        {"tempo", tempo},
        {"timeSignature", sanitizeUtf8(timeSignature)},
        {"keySignature", sanitizeUtf8(keySignature)},
        {"duration", duration},
        {"ticksPerBeat", ticksPerBeat},
        {"midiFormat", midiFormat},
        {"trackCount", trackCount},
        {"createdAt", sanitizeUtf8(createdAt)},
        {"modifiedAt", sanitizeUtf8(modifiedAt)}
    };
}

JsonMidiMetadata JsonMidiMetadata::fromJson(const json& j) {
    JsonMidiMetadata meta;
    
    meta.title = j.value("title", "");
    meta.artist = j.value("artist", "");
    meta.album = j.value("album", "");
    meta.genre = j.value("genre", "");
    meta.copyright = j.value("copyright", "");
    meta.comment = j.value("comment", "");
    meta.tempo = j.value("tempo", 120);
    meta.timeSignature = j.value("timeSignature", "4/4");
    meta.keySignature = j.value("keySignature", "C");
    meta.duration = j.value("duration", 0);
    meta.ticksPerBeat = j.value("ticksPerBeat", 480);
    meta.midiFormat = j.value("midiFormat", 1);
    meta.trackCount = j.value("trackCount", 0);
    meta.createdAt = j.value("createdAt", "");
    meta.modifiedAt = j.value("modifiedAt", "");
    
    return meta;
}

// ============================================================================
// JsonMidiTrack IMPLEMENTATION
// ============================================================================

json JsonMidiTrack::toJson() const {
    return {
        {"id", id},
        {"name", sanitizeUtf8(name)},
        {"channel", channel},
        {"muted", muted},
        {"solo", solo},
        {"volume", volume},
        {"pan", pan},
        {"transpose", transpose},
        {"color", sanitizeUtf8(color)},
        {"instrument", {
            {"program", instrument.program},
            {"bank", instrument.bank},
            {"name", sanitizeUtf8(instrument.name)}
        }}
    };
}

JsonMidiTrack JsonMidiTrack::fromJson(const json& j) {
    JsonMidiTrack track;
    
    track.id = j.value("id", 0);
    track.name = j.value("name", "");
    track.channel = j.value("channel", 1);
    track.muted = j.value("muted", false);
    track.solo = j.value("solo", false);
    track.volume = j.value("volume", 100);
    track.pan = j.value("pan", 64);
    track.transpose = j.value("transpose", 0);
    track.color = j.value("color", "#667eea");
    
    if (j.contains("instrument")) {
        auto inst = j["instrument"];
        track.instrument.program = inst.value("program", 0);
        track.instrument.bank = inst.value("bank", 0);
        track.instrument.name = inst.value("name", "");
    }
    
    return track;
}

// ============================================================================
// JsonMidiMarker IMPLEMENTATION
// ============================================================================

json JsonMidiMarker::toJson() const {
    return {
        {"id", id},
        {"time", time},
        {"label", label},
        {"color", sanitizeUtf8(color)}
    };
}

JsonMidiMarker JsonMidiMarker::fromJson(const json& j) {
    JsonMidiMarker marker;
    
    marker.id = j.value("id", "");
    marker.time = j.value("time", 0);
    marker.label = j.value("label", "");
    marker.color = j.value("color", "#667eea");
    
    return marker;
}

// ============================================================================
// JsonMidi IMPLEMENTATION
// ============================================================================

json JsonMidi::toJson() const {
    json j = {
        {"format", format},
        {"version", version},
        {"metadata", metadata.toJson()},
        {"timeline", json::array()},
        {"tracks", json::array()},
        {"markers", json::array()}
    };
    
    for (const auto& event : timeline) {
        j["timeline"].push_back(event.toJson());
    }
    
    for (const auto& track : tracks) {
        j["tracks"].push_back(track.toJson());
    }
    
    for (const auto& marker : markers) {
        j["markers"].push_back(marker.toJson());
    }
    
    return j;
}

JsonMidi JsonMidi::fromJson(const json& j) {
    JsonMidi jsonMidi;
    
    jsonMidi.format = j.value("format", "jsonmidi-v1.0");
    jsonMidi.version = j.value("version", "1.0.0");
    
    if (j.contains("metadata") && j["metadata"].is_object()) {
        jsonMidi.metadata = JsonMidiMetadata::fromJson(j["metadata"]);
    }
    
    if (j.contains("timeline") && j["timeline"].is_array()) {
        for (const auto& eventJson : j["timeline"]) {
            jsonMidi.timeline.push_back(JsonMidiEvent::fromJson(eventJson));
        }
    }
    
    if (j.contains("tracks") && j["tracks"].is_array()) {
        for (const auto& trackJson : j["tracks"]) {
            jsonMidi.tracks.push_back(JsonMidiTrack::fromJson(trackJson));
        }
    }
    
    if (j.contains("markers") && j["markers"].is_array()) {
        for (const auto& markerJson : j["markers"]) {
            jsonMidi.markers.push_back(JsonMidiMarker::fromJson(markerJson));
        }
    }
    
    return jsonMidi;
}

JsonMidi JsonMidi::fromString(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        return fromJson(j);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }
}

std::string JsonMidi::toString(int indent) const {
    return toJson().dump(indent);
}

// ============================================================================
// JsonMidiConverter CONSTRUCTOR
// ============================================================================

JsonMidiConverter::JsonMidiConverter() {
    Logger::debug("JsonMidiConverter", "Converter created");
}

// ============================================================================
// CONVERSION: MIDI â†’ JsonMidi
// ============================================================================

JsonMidi JsonMidiConverter::fromMidiMessages(
    const std::vector<MidiMessage>& messages,
    uint16_t ticksPerBeat,
    uint32_t tempo) {
    
    Logger::info("JsonMidiConverter", 
                "Converting " + std::to_string(messages.size()) + " MIDI messages to JsonMidi");
    
    JsonMidi jsonMidi;
    jsonMidi.format = "jsonmidi-v1.0";
    jsonMidi.version = "1.0.0";
    
    // Initialize metadata
    jsonMidi.metadata.tempo = tempo;
    jsonMidi.metadata.ticksPerBeat = ticksPerBeat;
    jsonMidi.metadata.timeSignature = defaultTimeSignature_;
    jsonMidi.metadata.midiFormat = 1;
    jsonMidi.metadata.trackCount = 1;
    
    // Convert messages to events
    for (const auto& message : messages) {
        // Convert timestamp from microseconds to milliseconds
        uint32_t timeMs = static_cast<uint32_t>(message.getTimestamp() / 1000);
        
        JsonMidiEvent event = messageToEvent(message, timeMs);
        if (!event.id.empty()) {
            jsonMidi.timeline.push_back(event);
        }
    }
    
    // Sort timeline by time
    std::sort(jsonMidi.timeline.begin(), jsonMidi.timeline.end(),
             [](const JsonMidiEvent& a, const JsonMidiEvent& b) {
                 return a.time < b.time;
             });
    
    // Calculate note durations
    calculateNoteDurations(jsonMidi.timeline);
    
    // Calculate total duration
    if (!jsonMidi.timeline.empty()) {
        jsonMidi.metadata.duration = jsonMidi.timeline.back().time;
    }
    
    // Extract metadata from timeline
    jsonMidi.metadata.tempo = extractTempo(jsonMidi.timeline);
    
    Logger::info("JsonMidiConverter", 
                "âœ“ Converted to " + std::to_string(jsonMidi.timeline.size()) + " events");
    
    return jsonMidi;
}

JsonMidi JsonMidiConverter::fromMidiFile(const std::string& filepath) {
    Logger::info("JsonMidiConverter", "Loading MIDI file: " + filepath);
    
    // 1. Utiliser MidiFileReader pour parser le fichier
    MidiFileReader reader;
    MidiFile midiFile;
    
    try {
        midiFile = reader.readFromFile(filepath);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to read MIDI file: " + std::string(e.what()));
    }
    
    // 2. Valider le fichier MIDI
    if (!midiFile.isValid()) {
        throw std::runtime_error("Invalid MIDI file structure");
    }
    
    Logger::info("JsonMidiConverter", 
        "Parsed MIDI: format=" + std::to_string(midiFile.header.format) +
        ", tracks=" + std::to_string(midiFile.tracks.size()) +
        ", division=" + std::to_string(midiFile.header.division));
    
    // 3. Initialiser structure JsonMidi
    JsonMidi jsonMidi;
    jsonMidi.format = "jsonmidi-v1.0";
    jsonMidi.version = "1.0.0";
    
    // 4. Remplir les mÃ©tadonnÃ©es
    jsonMidi.metadata.tempo = midiFile.tempo;
    jsonMidi.metadata.duration = midiFile.durationMs;
    jsonMidi.metadata.ticksPerBeat = midiFile.header.division;
    jsonMidi.metadata.midiFormat = midiFile.header.format;
    jsonMidi.metadata.trackCount = static_cast<uint16_t>(midiFile.tracks.size());
    jsonMidi.metadata.timeSignature = formatTimeSignature(midiFile.timeSignature);
    
    // Horodatage
    auto now = std::time(nullptr);
    std::tm tm;
    localtime_r(&now, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    jsonMidi.metadata.createdAt = oss.str();
    jsonMidi.metadata.modifiedAt = oss.str();
    
    // 5. Convertir les tracks
    uint16_t trackId = 0;
    for (const auto& midiTrack : midiFile.tracks) {
        JsonMidiTrack jsonTrack = convertMidiTrackToJsonTrack(midiTrack, trackId++);
        jsonMidi.tracks.push_back(jsonTrack);
    }
    
    // 6. Convertir les events en timeline unifiÃ©e
    jsonMidi.timeline = convertMidiEventsToTimeline(midiFile, midiFile.header.division);
    
    // 7. Trier la timeline par temps
    std::sort(jsonMidi.timeline.begin(), jsonMidi.timeline.end(),
             [](const JsonMidiEvent& a, const JsonMidiEvent& b) {
                 return a.time < b.time;
             });
    
    // 8. Calculer les durÃ©es des notes
    calculateNoteDurations(jsonMidi.timeline);
    
    Logger::info("JsonMidiConverter", 
        "âœ“ Converted to " + std::to_string(jsonMidi.timeline.size()) + 
        " events across " + std::to_string(jsonMidi.tracks.size()) + " tracks");
    
    return jsonMidi;
}

// ============================================================================
// CONVERSION: JsonMidi â†’ MIDI
// ============================================================================

std::vector<MidiMessage> JsonMidiConverter::toMidiMessages(const JsonMidi& jsonMidi) {
    Logger::info("JsonMidiConverter", 
                "Converting JsonMidi to MIDI messages (" + 
                std::to_string(jsonMidi.timeline.size()) + " events)");
    
    std::vector<MidiMessage> messages;
    messages.reserve(jsonMidi.timeline.size());
    
    for (const auto& event : jsonMidi.timeline) {
        MidiMessage message = eventToMessage(event);
        
        if (message.isValid()) {
            // Convert time from milliseconds to microseconds for timestamp
            message.setTimestamp(static_cast<uint64_t>(event.time) * 1000);
            messages.push_back(message);
        }
    }
    
    Logger::info("JsonMidiConverter", 
                "âœ“ Converted to " + std::to_string(messages.size()) + " MIDI messages");
    
    return messages;
}

bool JsonMidiConverter::toMidiFile(const JsonMidi& jsonMidi, const std::string& filepath) {
    Logger::info("JsonMidiConverter", "Writing MIDI file: " + filepath);
    
    // TODO: Implement actual MIDI file writing
    // For now, just return false
    
    Logger::warning("JsonMidiConverter", "MIDI file writing not yet implemented");
    
    return false;
}

// ============================================================================
// VALIDATION
// ============================================================================

bool JsonMidiConverter::validate(const JsonMidi& jsonMidi, std::string& errorMessage) const {
    // Check format
    if (jsonMidi.format != "jsonmidi-v1.0") {
        errorMessage = "Invalid format: " + jsonMidi.format;
        return false;
    }
    
    // Check tempo
    if (jsonMidi.metadata.tempo == 0) {
        errorMessage = "Invalid tempo: 0";
        return false;
    }
    
    // Check for duplicate IDs
    std::set<std::string> ids;
    for (const auto& event : jsonMidi.timeline) {
        if (!event.id.empty()) {
            if (ids.count(event.id) > 0) {
                errorMessage = "Duplicate event ID: " + event.id;
                return false;
            }
            ids.insert(event.id);
        }
    }
    
    // Check timeline is sorted
    for (size_t i = 1; i < jsonMidi.timeline.size(); ++i) {
        if (jsonMidi.timeline[i].time < jsonMidi.timeline[i-1].time) {
            errorMessage = "Timeline not sorted at index " + std::to_string(i);
            return false;
        }
    }
    
    // Check event validity
    for (const auto& event : jsonMidi.timeline) {
        if (event.channel < 1 || event.channel > 16) {
            errorMessage = "Invalid channel: " + std::to_string(event.channel);
            return false;
        }
        
        if (event.type == "noteOn" || event.type == "noteOff") {
            if (!event.note.has_value() || !event.velocity.has_value()) {
                errorMessage = "Note event missing note or velocity";
                return false;
            }
        }
    }
    
    return true;
}

// ============================================================================
// UTILITIES
// ============================================================================

void JsonMidiConverter::calculateNoteDurations(std::vector<JsonMidiEvent>& timeline) {
    Logger::debug("JsonMidiConverter", "Calculating note durations");
    
    // Map to track active notes: (channel, note) -> event index
    std::map<std::pair<uint8_t, uint8_t>, size_t> activeNotes;
    
    for (size_t i = 0; i < timeline.size(); ++i) {
        auto& event = timeline[i];
        
        if (event.type == "noteOn" && event.note.has_value()) {
            // Start tracking this note
            auto key = std::make_pair(event.channel, event.note.value());
            activeNotes[key] = i;
            
        } else if (event.type == "noteOff" && event.note.has_value()) {
            // Find matching Note On
            auto key = std::make_pair(event.channel, event.note.value());
            auto it = activeNotes.find(key);
            
            if (it != activeNotes.end()) {
                size_t noteOnIndex = it->second;
                auto& noteOnEvent = timeline[noteOnIndex];
                
                // Calculate duration
                uint32_t duration = event.time - noteOnEvent.time;
                noteOnEvent.duration = duration;
                
                // Remove from active notes
                activeNotes.erase(it);
            }
        }
    }
    
    // Handle orphaned notes (no matching Note Off)
    for (const auto& [key, index] : activeNotes) {
        auto& event = timeline[index];
        if (!event.duration.has_value()) {
            // Set default duration (100ms)
            event.duration = 100;
        }
    }
    
    Logger::debug("JsonMidiConverter", "âœ“ Note durations calculated");
}

uint32_t JsonMidiConverter::ticksToMs(uint32_t ticks, uint16_t ticksPerBeat, uint32_t tempo) {
    // Calculate milliseconds per tick
    // tempo is in BPM (beats per minute)
    // ticksPerBeat is ticks per quarter note
    
    double msPerBeat = 60000.0 / tempo;  // milliseconds per beat
    double msPerTick = msPerBeat / ticksPerBeat;
    
    return static_cast<uint32_t>(ticks * msPerTick);
}

uint32_t JsonMidiConverter::msToTicks(uint32_t ms, uint16_t ticksPerBeat, uint32_t tempo) {
    double msPerBeat = 60000.0 / tempo;
    double msPerTick = msPerBeat / ticksPerBeat;
    
    return static_cast<uint32_t>(ms / msPerTick);
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

JsonMidiEvent JsonMidiConverter::messageToEvent(const MidiMessage& message, uint32_t timeMs) {
    JsonMidiEvent event;
    event.time = timeMs;
    
    int channel = message.getChannel();
    if (channel >= 0) {
        event.channel = channel + 1;  // Convert to 1-16
    } else {
        event.channel = 1;
    }
    
    // Convert based on message type
    if (message.isNoteOn()) {
        event.type = "noteOn";
        event.note = message.getData1();
        event.velocity = message.getData2();
        event.id = generateEventId("noteOn", timeMs, event.channel, event.note.value());
        
    } else if (message.isNoteOff()) {
        event.type = "noteOff";
        event.note = message.getData1();
        event.velocity = message.getData2();
        event.id = generateEventId("noteOff", timeMs, event.channel, event.note.value());
        
    } else if (message.isControlChange()) {
        event.type = "cc";
        event.controller = message.getData1();
        event.value = message.getData2();
        event.id = generateEventId("cc", timeMs, event.channel, event.controller.value());
        
    } else if (message.isProgramChange()) {
        event.type = "programChange";
        event.program = message.getData1();
        event.id = generateEventId("pc", timeMs, event.channel, event.program.value());
        
    } else if (message.isPitchBend()) {
        event.type = "pitchBend";
        int16_t bend = (message.getData2() << 7) | message.getData1();
        event.pitchBend = bend - 8192;
        event.id = generateEventId("pb", timeMs, event.channel, 0);
        
    } else if (message.getType() == MidiMessageType::CHANNEL_PRESSURE) {
        event.type = "channelPressure";
        event.value = message.getData1();
        event.id = generateEventId("cp", timeMs, event.channel, 0);
        
    } else if (message.getType() == MidiMessageType::POLY_PRESSURE) {
        event.type = "polyPressure";
        event.note = message.getData1();
        event.value = message.getData2();
        event.id = generateEventId("pp", timeMs, event.channel, event.note.value());
        
    } else {
        // Unknown or unsupported message type
        return JsonMidiEvent();  // Return empty event
    }
    
    return event;
}

MidiMessage JsonMidiConverter::eventToMessage(const JsonMidiEvent& event) {
    uint8_t channel = event.channel - 1;  // Convert from 1-16 to 0-15
    
    if (event.type == "noteOn") {
        if (event.note.has_value() && event.velocity.has_value()) {
            return MidiMessage::noteOn(channel, event.note.value(), event.velocity.value());
        }
        
    } else if (event.type == "noteOff") {
        if (event.note.has_value()) {
            uint8_t velocity = event.velocity.value_or(0);
            return MidiMessage::noteOff(channel, event.note.value(), velocity);
        }
        
    } else if (event.type == "cc") {
        if (event.controller.has_value() && event.value.has_value()) {
            return MidiMessage::controlChange(channel, 
                                             event.controller.value(), 
                                             event.value.value());
        }
        
    } else if (event.type == "programChange") {
        if (event.program.has_value()) {
            return MidiMessage::programChange(channel, event.program.value());
        }
        
    } else if (event.type == "pitchBend") {
        if (event.pitchBend.has_value()) {
            return MidiMessage::pitchBend(channel, event.pitchBend.value());
        }
        
    } else if (event.type == "channelPressure") {
        if (event.value.has_value()) {
            return MidiMessage::channelPressure(channel, event.value.value());
        }
        
    } else if (event.type == "polyPressure") {
        if (event.note.has_value() && event.value.has_value()) {
            return MidiMessage::polyPressure(channel, 
                                            event.note.value(), 
                                            event.value.value());
        }
    }
    
    // Return empty message if conversion failed
    return MidiMessage();
}

std::string JsonMidiConverter::generateEventId(
    const std::string& type,
    uint32_t time,
    uint8_t channel,
    uint8_t data1) {
    
    std::ostringstream oss;
    oss << type << "_" << time << "_" << static_cast<int>(channel) << "_" << static_cast<int>(data1);
    return oss.str();
}

JsonMidiMetadata JsonMidiConverter::extractMetadata(const std::vector<MidiMessage>& messages) {
    JsonMidiMetadata metadata;
    
    metadata.tempo = defaultTempo_;
    metadata.timeSignature = defaultTimeSignature_;
    metadata.keySignature = "C";
    metadata.ticksPerBeat = 480;
    metadata.midiFormat = 1;
    metadata.trackCount = 1;
    
    // Calculate duration from messages
    if (!messages.empty()) {
        uint64_t lastTimestamp = 0;
        for (const auto& msg : messages) {
            if (msg.getTimestamp() > lastTimestamp) {
                lastTimestamp = msg.getTimestamp();
            }
        }
        // Convert timestamp from microseconds to milliseconds
        metadata.duration = static_cast<uint32_t>(lastTimestamp / 1000);
    }
    
    // Get current time for timestamps
    auto now = std::time(nullptr);
    std::tm tm;
    localtime_r(&now, &tm);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    metadata.createdAt = oss.str();
    metadata.modifiedAt = oss.str();
    
    return metadata;
}

uint32_t JsonMidiConverter::extractTempo(const std::vector<JsonMidiEvent>& timeline) const {
    // Look for tempo events in timeline
    for (const auto& event : timeline) {
        if (event.type == "setTempo" && event.tempo.has_value()) {
            return event.tempo.value();
        }
    }
    
    return defaultTempo_;
}


// ============================================================================
// PHASE 2: MIDI FILE CONVERSION HELPER METHODS
// ============================================================================

std::string JsonMidiConverter::formatTimeSignature(const TimeSignature& ts) const {
    return std::to_string(ts.numerator) + "/" + std::to_string(ts.denominator);
}

JsonMidiTrack JsonMidiConverter::convertMidiTrackToJsonTrack(
    const MidiTrack& track, 
    uint16_t trackId) {
    
    JsonMidiTrack jsonTrack;
    jsonTrack.id = trackId;
    jsonTrack.name = track.name.empty() ? "Track " + std::to_string(trackId + 1) : track.name;
    jsonTrack.channel = track.channel;
    jsonTrack.muted = false;
    jsonTrack.solo = false;
    jsonTrack.volume = 100;
    jsonTrack.pan = 64;
    jsonTrack.transpose = 0;
    jsonTrack.color = "#667eea";  // Couleur par dÃ©faut
    
    // Extraire Bank Select (CC 0/32) et Program Change
    uint8_t bankMSB = 0;
    uint8_t bankLSB = 0;
    uint8_t program = 0;
    
    for (const auto& event : track.events) {
        if (event.type == MidiEventType::MIDI_CHANNEL) {
            if (event.messageType == "controlChange") {
                if (event.controller == 0) {  // Bank Select MSB
                    bankMSB = event.value;
                } else if (event.controller == 32) {  // Bank Select LSB
                    bankLSB = event.value;
                }
            } else if (event.messageType == "programChange") {
                program = event.program;
                jsonTrack.instrument.program = program;
                jsonTrack.instrument.bank = (bankMSB << 7) | bankLSB;
                jsonTrack.instrument.name = "Program " + std::to_string(program);
                break;
            }
        }
    }
    
    return jsonTrack;
}

std::vector<JsonMidiEvent> JsonMidiConverter::convertMidiEventsToTimeline(
    const MidiFile& midiFile, 
    uint16_t ticksPerBeat) {
    
    std::vector<JsonMidiEvent> timeline;
    uint32_t currentTempo = 500000;  // DÃ©faut: 120 BPM
    
    // Parcourir tous les tracks
    for (size_t trackIdx = 0; trackIdx < midiFile.tracks.size(); trackIdx++) {
        const auto& track = midiFile.tracks[trackIdx];
        
        for (const auto& event : track.events) {
            // Mettre Ã  jour le tempo si meta-event tempo
            if (event.type == MidiEventType::META && event.metaType == 0x51) {
                if (event.data.size() >= 3) {
                    currentTempo = (event.data[0] << 16) | 
                                  (event.data[1] << 8) | 
                                   event.data[2];
                }
            }
            
            // Convertir ticks â†’ millisecondes
            uint32_t timeMs = ticksToMilliseconds(
                event.absoluteTime, 
                ticksPerBeat, 
                currentTempo
            );
            
            // Convertir l'event
            JsonMidiEvent jsonEvent = convertMidiEventToJsonEvent(
                event, 
                timeMs, 
                track.channel
            );
            
            if (!jsonEvent.id.empty()) {
                timeline.push_back(jsonEvent);
            }
        }
    }
    
    return timeline;
}

JsonMidiEvent JsonMidiConverter::convertMidiEventToJsonEvent(
    const MidiEvent& event, 
    uint32_t timeMs, 
    uint8_t trackChannel) {
    
    JsonMidiEvent jsonEvent;
    jsonEvent.time = timeMs;
    
    // GÃ©nÃ©rer ID unique
    std::ostringstream idStream;
    idStream << event.messageType << "_" << timeMs << "_" 
             << std::hex << std::setfill('0') << std::setw(8) 
             << static_cast<uint32_t>(std::rand());
    jsonEvent.id = idStream.str();
    
    // Type et channel
    jsonEvent.type = event.messageType;
    jsonEvent.channel = event.channel > 0 ? event.channel : trackChannel;
    
    // DonnÃ©es spÃ©cifiques selon le type
    if (event.messageType == "noteOn" || event.messageType == "noteOff") {
        jsonEvent.note = event.note;
        jsonEvent.velocity = event.velocity;
    } 
    else if (event.messageType == "controlChange") {
        jsonEvent.controller = event.controller;
        jsonEvent.value = event.value;
    }
    else if (event.messageType == "programChange") {
        jsonEvent.program = event.program;
    }
    else if (event.messageType == "pitchBend") {
        jsonEvent.pitchBend = static_cast<int16_t>(event.pitchBend - 8192);
    }
    else if (event.type == MidiEventType::META && event.metaType == 0x51) {
        jsonEvent.type = "setTempo";
        if (event.data.size() >= 3) {
            uint32_t usPerQuarter = (event.data[0] << 16) | 
                                   (event.data[1] << 8) | 
                                    event.data[2];
            jsonEvent.tempo = 60000000 / usPerQuarter;  // Convertir en BPM
        }
    }
    
    return jsonEvent;
}

uint32_t JsonMidiConverter::ticksToMilliseconds(
    uint32_t ticks, 
    uint16_t ticksPerBeat, 
    uint32_t tempo) const {
    
    // tempo = microseconds per quarter note
    // ticksPerBeat = ticks per quarter note
    // milliseconds = (ticks * tempo) / (ticksPerBeat * 1000)
    
    uint64_t microseconds = (static_cast<uint64_t>(ticks) * tempo) / ticksPerBeat;
    return static_cast<uint32_t>(microseconds / 1000);
}


} // namespace midiMind