// ============================================================================
// File: backend/src/midi/JsonMidiConverter.cpp
// Version: 4.3.2 - FIXED MidiMessage API calls
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.3.2:
//   - FIXED: Use MidiMessage getData1()/getData2() instead of non-existent getters
//   - FIXED: Use getType() checks for channel/poly pressure
//
// Changes v4.3.1:
//   - FIXED: Extract real tempo from Meta events (0x51) instead of using default
//   - Proper µs/qn → BPM conversion: BPM = 60000000 / tempo_microseconds
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
#include <functional>

namespace midiMind {

// ============================================================================
// UTF-8 SANITIZATION HELPER
// ============================================================================

static std::string sanitizeUtf8(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    
    for (size_t i = 0; i < str.size(); ) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        
        if (c < 0x80) {
            if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') {
                result += ' ';
            } else {
                result += c;
            }
            i++;
        }
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
        {"instrument", instrument.toJson()}
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
    
    if (j.contains("instrument") && j["instrument"].is_object()) {
        track.instrument = JsonMidiInstrument::fromJson(j["instrument"]);
    }
    
    return track;
}

// ============================================================================
// JsonMidiInstrument IMPLEMENTATION
// ============================================================================

json JsonMidiInstrument::toJson() const {
    return {
        {"program", program},
        {"bank", bank},
        {"name", sanitizeUtf8(name)}
    };
}

JsonMidiInstrument JsonMidiInstrument::fromJson(const json& j) {
    JsonMidiInstrument inst;
    
    inst.program = j.value("program", 0);
    inst.bank = j.value("bank", 0);
    inst.name = j.value("name", "");
    
    return inst;
}

// ============================================================================
// JsonMidiData IMPLEMENTATION
// ============================================================================

json JsonMidiData::toJson() const {
    json j = {
        {"format", format},
        {"version", version},
        {"metadata", metadata.toJson()}
    };
    
    json tracksArray = json::array();
    for (const auto& track : tracks) {
        tracksArray.push_back(track.toJson());
    }
    j["tracks"] = tracksArray;
    
    json timelineArray = json::array();
    for (const auto& event : timeline) {
        timelineArray.push_back(event.toJson());
    }
    j["timeline"] = timelineArray;
    
    return j;
}

JsonMidiData JsonMidiData::fromJson(const json& j) {
    JsonMidiData data;
    
    data.format = j.value("format", "");
    data.version = j.value("version", 1);
    
    if (j.contains("metadata") && j["metadata"].is_object()) {
        data.metadata = JsonMidiMetadata::fromJson(j["metadata"]);
    }
    
    if (j.contains("tracks") && j["tracks"].is_array()) {
        for (const auto& trackJson : j["tracks"]) {
            if (trackJson.is_object()) {
                data.tracks.push_back(JsonMidiTrack::fromJson(trackJson));
            }
        }
    }
    
    if (j.contains("timeline") && j["timeline"].is_array()) {
        for (const auto& eventJson : j["timeline"]) {
            if (eventJson.is_object()) {
                data.timeline.push_back(JsonMidiEvent::fromJson(eventJson));
            }
        }
    }
    
    return data;
}

// ============================================================================
// JsonMidiConverter IMPLEMENTATION
// ============================================================================

JsonMidiConverter::JsonMidiConverter() {
}

JsonMidiConverter::~JsonMidiConverter() {
}

std::string JsonMidiConverter::generateEventId(
    const std::string& type, 
    uint32_t time, 
    int channel, 
    uint8_t value) const {
    
    std::ostringstream oss;
    oss << type << "_" << time << "_" << channel << "_" << static_cast<int>(value);
    return oss.str();
}

bool JsonMidiConverter::midiFileToJson(
    const std::string& midiFilePath, 
    const std::string& jsonFilePath,
    std::string& errorMessage) {
    
    MidiFile midiFile;
    
    if (!MidiFileReader::read(midiFilePath, midiFile, errorMessage)) {
        return false;
    }
    
    JsonMidiData jsonData = convertMidiFileToJson(midiFile);
    
    try {
        json j = jsonData.toJson();
        std::ofstream outFile(jsonFilePath);
        
        if (!outFile.is_open()) {
            errorMessage = "Cannot open output file: " + jsonFilePath;
            return false;
        }
        
        outFile << std::setw(2) << j << std::endl;
        outFile.close();
        
        return true;
        
    } catch (const std::exception& e) {
        errorMessage = "JSON generation failed: " + std::string(e.what());
        return false;
    }
}

JsonMidiData JsonMidiConverter::convertMidiFileToJson(const MidiFile& midiFile) {
    JsonMidiData jsonData;
    jsonData.format = "jsonmidi-v1.0";
    jsonData.version = 1;
    
    jsonData.metadata.ticksPerBeat = midiFile.ticksPerBeat;
    jsonData.metadata.midiFormat = midiFile.format;
    jsonData.metadata.trackCount = static_cast<uint16_t>(midiFile.tracks.size());
    jsonData.metadata.tempo = 120;
    jsonData.metadata.timeSignature = "4/4";
    jsonData.metadata.keySignature = "C";
    
    std::set<std::string> usedTrackNames;
    
    for (size_t trackIdx = 0; trackIdx < midiFile.tracks.size(); ++trackIdx) {
        const auto& track = midiFile.tracks[trackIdx];
        JsonMidiTrack jsonTrack = convertMidiTrackToJsonTrack(track, trackIdx);
        
        if (jsonTrack.name.empty() || usedTrackNames.count(jsonTrack.name) > 0) {
            jsonTrack.name = "Track " + std::to_string(trackIdx + 1);
        }
        usedTrackNames.insert(jsonTrack.name);
        
        jsonData.tracks.push_back(jsonTrack);
    }
    
    jsonData.timeline = convertMidiEventsToTimeline(midiFile, midiFile.ticksPerBeat);
    
    std::sort(jsonData.timeline.begin(), jsonData.timeline.end(),
        [](const JsonMidiEvent& a, const JsonMidiEvent& b) {
            return a.time < b.time;
        });
    
    computeNoteDurations(jsonData.timeline);
    
    if (!jsonData.timeline.empty()) {
        uint32_t lastEventTime = jsonData.timeline.back().time;
        jsonData.metadata.duration = lastEventTime;
    }
    
    return jsonData;
}

void JsonMidiConverter::computeNoteDurations(std::vector<JsonMidiEvent>& timeline) {
    std::map<uint8_t, uint32_t> noteOnTimes;
    
    for (auto& event : timeline) {
        if (event.type == "noteOn" && event.note.has_value()) {
            uint8_t note = event.note.value();
            noteOnTimes[note] = event.time;
        }
        else if (event.type == "noteOff" && event.note.has_value()) {
            uint8_t note = event.note.value();
            
            if (noteOnTimes.find(note) != noteOnTimes.end()) {
                uint32_t duration = event.time - noteOnTimes[note];
                
                for (auto& e : timeline) {
                    if (e.type == "noteOn" && 
                        e.note.has_value() && 
                        e.note.value() == note &&
                        e.time == noteOnTimes[note]) {
                        e.duration = duration;
                        break;
                    }
                }
                
                noteOnTimes.erase(note);
            }
        }
    }
}

JsonMidiEvent JsonMidiConverter::messageToEvent(const MidiMessage& message, uint32_t timeMs) {
    JsonMidiEvent event;
    event.time = timeMs;
    event.channel = message.getChannel();
    
    if (message.isNoteOn()) {
        event.type = "noteOn";
        event.note = message.getData1();        // Note number
        event.velocity = message.getData2();    // Velocity
        event.id = generateEventId("noteOn", timeMs, event.channel, message.getData1());
        
    } else if (message.isNoteOff()) {
        event.type = "noteOff";
        event.note = message.getData1();        // Note number
        event.velocity = message.getData2();    // Release velocity
        event.id = generateEventId("noteOff", timeMs, event.channel, message.getData1());
        
    } else if (message.isControlChange()) {
        event.type = "controlChange";
        event.controller = message.getData1();  // Controller number
        event.value = message.getData2();       // Controller value
        event.id = generateEventId("controlChange", timeMs, event.channel, message.getData1());
        
    } else if (message.isProgramChange()) {
        event.type = "programChange";
        event.program = message.getData1();     // Program number
        event.id = generateEventId("programChange", timeMs, event.channel, message.getData1());
        
    } else if (message.isPitchBend()) {
        event.type = "pitchBend";
        // Pitch bend: combine LSB (data1) and MSB (data2) into 14-bit signed value
        uint16_t raw14bit = (static_cast<uint16_t>(message.getData2()) << 7) | message.getData1();
        event.pitchBend = static_cast<int16_t>(raw14bit) - 8192;  // Convert to -8192..8191
        event.id = generateEventId("pitchBend", timeMs, event.channel, 0);
        
    } else if (message.getType() == MidiMessageType::CHANNEL_PRESSURE) {
        event.type = "channelPressure";
        event.value = message.getData1();       // Pressure value
        event.id = generateEventId("channelPressure", timeMs, event.channel, 0);
        
    } else if (message.getType() == MidiMessageType::POLY_PRESSURE) {
        event.type = "polyPressure";
        event.note = message.getData1();        // Note number
        event.value = message.getData2();       // Pressure value
        event.id = generateEventId("polyPressure", timeMs, event.channel, message.getData1());
    }
    
    return event;
}

MidiMessage JsonMidiConverter::eventToMessage(const JsonMidiEvent& event) {
    uint8_t channel = event.channel;
    
    if (event.type == "noteOn") {
        if (event.note.has_value() && event.velocity.has_value()) {
            return MidiMessage::noteOn(channel, event.note.value(), event.velocity.value());
        }
        
    } else if (event.type == "noteOff") {
        if (event.note.has_value() && event.velocity.has_value()) {
            return MidiMessage::noteOff(channel, event.note.value(), event.velocity.value());
        }
        
    } else if (event.type == "controlChange") {
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
    
    return MidiMessage();
}

bool JsonMidiConverter::jsonToMidiFile(
    const std::string& jsonFilePath, 
    const std::string& midiFilePath, 
    std::string& errorMessage) {
    
    try {
        std::ifstream inFile(jsonFilePath);
        if (!inFile.is_open()) {
            errorMessage = "Cannot open JSON file: " + jsonFilePath;
            return false;
        }
        
        json j;
        inFile >> j;
        inFile.close();
        
        JsonMidiData jsonMidi = JsonMidiData::fromJson(j);
        
        if (!validateJsonMidiData(jsonMidi, errorMessage)) {
            return false;
        }
        
        errorMessage = "JSON to MIDI conversion not yet implemented";
        return false;
        
    } catch (const json::exception& e) {
        errorMessage = "JSON parsing failed: " + std::string(e.what());
        return false;
    } catch (const std::exception& e) {
        errorMessage = "Conversion failed: " + std::string(e.what());
        return false;
    }
}

bool JsonMidiConverter::validateJsonMidiData(
    const JsonMidiData& jsonMidi, 
    std::string& errorMessage) const {
    
    if (jsonMidi.format != "jsonmidi-v1.0") {
        errorMessage = "Invalid format: " + jsonMidi.format;
        return false;
    }
    
    if (jsonMidi.tracks.empty()) {
        errorMessage = "No tracks defined";
        return false;
    }
    
    if (jsonMidi.timeline.empty()) {
        errorMessage = "No events in timeline";
        return false;
    }
    
    for (const auto& event : jsonMidi.timeline) {
        if (event.type.empty()) {
            errorMessage = "Event with empty type";
            return false;
        }
        if (event.channel > 16) {
            errorMessage = "Invalid channel: " + std::to_string(event.channel);
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// TIME CONVERSION UTILITIES
// ============================================================================

uint32_t JsonMidiConverter::ticksToMs(uint32_t ticks, uint16_t ticksPerBeat, uint32_t tempo) {
    if (ticksPerBeat == 0 || tempo == 0) {
        return 0;
    }
    double beatsPerMinute = tempo;
    double minutesPerBeat = 1.0 / beatsPerMinute;
    double secondsPerBeat = minutesPerBeat * 60.0;
    double millisecondsPerBeat = secondsPerBeat * 1000.0;
    double millisecondsPerTick = millisecondsPerBeat / ticksPerBeat;
    return static_cast<uint32_t>(ticks * millisecondsPerTick);
}

uint32_t JsonMidiConverter::msToTicks(uint32_t ms, uint16_t ticksPerBeat, uint32_t tempo) {
    if (ticksPerBeat == 0 || tempo == 0) {
        return 0;
    }
    double beatsPerMinute = tempo;
    double minutesPerBeat = 1.0 / beatsPerMinute;
    double secondsPerBeat = minutesPerBeat * 60.0;
    double millisecondsPerBeat = secondsPerBeat * 1000.0;
    double millisecondsPerTick = millisecondsPerBeat / ticksPerBeat;
    return static_cast<uint32_t>(ms / millisecondsPerTick);
}

// ============================================================================
// MIDI FILE CONVERSION HELPERS
// ============================================================================

std::string JsonMidiConverter::formatTimeSignature(const TimeSignature& ts) const {
    std::ostringstream oss;
    oss << static_cast<int>(ts.numerator) << "/" << static_cast<int>(ts.denominator);
    return oss.str();
}

JsonMidiTrack JsonMidiConverter::convertMidiTrackToJsonTrack(
    const MidiTrack& track, 
    uint16_t trackId) {
    
    JsonMidiTrack jsonTrack;
    jsonTrack.id = trackId;
    jsonTrack.name = track.name.empty() ? ("Track " + std::to_string(trackId + 1)) : track.name;
    jsonTrack.channel = track.channel;
    jsonTrack.muted = false;
    jsonTrack.solo = false;
    jsonTrack.volume = 100;
    jsonTrack.pan = 64;
    jsonTrack.transpose = 0;
    jsonTrack.color = "#667eea";
    jsonTrack.instrument.program = 0;
    jsonTrack.instrument.bank = 0;
    jsonTrack.instrument.name = "Acoustic Grand Piano";
    
    return jsonTrack;
}

std::vector<JsonMidiEvent> JsonMidiConverter::convertMidiEventsToTimeline(
    const MidiFile& midiFile, 
    uint16_t ticksPerBeat) {
    
    std::vector<JsonMidiEvent> timeline;
    uint32_t currentTempo = 500000;
    
    for (size_t trackIdx = 0; trackIdx < midiFile.tracks.size(); ++trackIdx) {
        const auto& track = midiFile.tracks[trackIdx];
        
        for (const auto& event : track.events) {
            if (event.type == MidiEventType::META && event.metaType == 0x51) {
                if (event.data.size() >= 3) {
                    currentTempo = (event.data[0] << 16) | 
                                  (event.data[1] << 8) | 
                                   event.data[2];
                }
            }
            
            uint32_t timeMs = ticksToMilliseconds(
                event.absoluteTime, 
                ticksPerBeat, 
                currentTempo
            );
            
            JsonMidiEvent jsonEvent = convertMidiEventToJsonEvent(
                event, 
                timeMs, 
                track.channel
            );
            
            if (!jsonEvent.id.empty() && !jsonEvent.type.empty()) {
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
    
    if (event.type == MidiEventType::MIDI_CHANNEL) {
        jsonEvent.type = event.messageType;
        jsonEvent.channel = event.channel > 0 ? event.channel : trackChannel;
        
        if (jsonEvent.type == "noteOn" || jsonEvent.type == "noteOff") {
            jsonEvent.note = event.note;
            jsonEvent.velocity = event.velocity;
        } 
        else if (jsonEvent.type == "controlChange") {
            jsonEvent.controller = event.controller;
            jsonEvent.value = event.value;
        }
        else if (jsonEvent.type == "programChange") {
            jsonEvent.program = event.program;
        }
        else if (jsonEvent.type == "pitchBend") {
            jsonEvent.pitchBend = static_cast<int16_t>(event.pitchBend - 8192);
        }
        else if (jsonEvent.type == "channelPressure") {
            jsonEvent.value = event.pressure;
        }
        else if (jsonEvent.type == "polyPressure") {
            jsonEvent.note = event.note;
            jsonEvent.value = event.pressure;
        }
    }
    else if (event.type == MidiEventType::META) {
        jsonEvent.channel = 0;
        jsonEvent.type = event.messageType;
        
        if (!event.text.empty()) {
            jsonEvent.text = event.text;
        } else if (!event.trackName.empty()) {
            jsonEvent.text = event.trackName;
        }
        
        if (event.metaType == 0x51 && event.data.size() >= 3) {
            uint32_t usPerQuarter = (event.data[0] << 16) | 
                                   (event.data[1] << 8) | 
                                    event.data[2];
            jsonEvent.tempo = 60000000 / usPerQuarter;
        }
        else if (event.metaType == 0x58 && event.data.size() >= 4) {
            std::ostringstream tsStream;
            tsStream << static_cast<int>(event.data[0]) << "/" 
                    << (1 << event.data[1]);
            jsonEvent.text = tsStream.str();
        }
        else if (event.metaType == 0x59 && event.data.size() >= 2) {
            int8_t sharpsFlats = static_cast<int8_t>(event.data[0]);
            uint8_t majorMinor = event.data[1];
            std::ostringstream ksStream;
            ksStream << (majorMinor == 0 ? "Major" : "Minor") << " ";
            if (sharpsFlats > 0) ksStream << "+" << static_cast<int>(sharpsFlats);
            else if (sharpsFlats < 0) ksStream << static_cast<int>(sharpsFlats);
            else ksStream << "C";
            jsonEvent.text = ksStream.str();
        }
    }
    else if (event.type == MidiEventType::SYSEX) {
        jsonEvent.type = event.messageType;
        jsonEvent.channel = 0;
        if (!event.data.empty()) {
            jsonEvent.data = event.data;
        }
    }
    
    if (!jsonEvent.type.empty()) {
        jsonEvent.id = generateEventId(jsonEvent.type, timeMs, jsonEvent.channel, 
                                       jsonEvent.note.value_or(0));
    }
    
    return jsonEvent;
}

uint32_t JsonMidiConverter::ticksToMilliseconds(
    uint32_t ticks, 
    uint16_t ticksPerBeat, 
    uint32_t tempo) const {
    
    uint64_t microseconds = (static_cast<uint64_t>(ticks) * tempo) / ticksPerBeat;
    return static_cast<uint32_t>(microseconds / 1000);
}

} // namespace midiMind

// ============================================================================
// END OF FILE JsonMidiConverter.cpp v4.3.2
// ============================================================================