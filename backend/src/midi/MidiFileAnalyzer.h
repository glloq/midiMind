// ============================================================================
// src/midi/MidiFileAnalyzer.h
// ============================================================================
#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../core/Logger.h"

using json = nlohmann::json;

namespace midiMind {

struct TempoChange {
    uint32_t tick;
    uint32_t timeMs;
    double bpm;
};

struct TimeSignature {
    uint32_t tick;
    uint8_t numerator;
    uint8_t denominator;
};

struct KeySignature {
    uint32_t tick;
    int8_t sharpsFlats;
    bool isMinor;
    std::string keyName;
};

struct TrackInfo {
    uint16_t index;
    std::string name;
    uint8_t channel;
    uint8_t programChange;
    std::string instrumentName;
    
    uint32_t noteCount;
    uint8_t minNote;
    uint8_t maxNote;
    uint8_t avgVelocity;
    
    uint32_t firstEventMs;
    uint32_t lastEventMs;
    float noteDensity;
};

struct MidiFileAnalysis {
    std::string filename;
    uint32_t durationMs;
    uint16_t format;
    uint16_t ticksPerQuarterNote;
    
    std::vector<TempoChange> tempoMap;
    std::vector<TimeSignature> timeSignatures;
    std::vector<KeySignature> keySignatures;
    std::vector<TrackInfo> tracks;
    
    uint32_t totalNotes;
    uint32_t totalControlChanges;
    uint32_t totalProgramChanges;
    bool hasSysEx;
    bool hasLyrics;
    bool hasMarkers;
    
    json toJson() const {
        json j;
        j["filename"] = filename;
        j["duration_ms"] = durationMs;
        j["format"] = format;
        j["ticks_per_quarter_note"] = ticksPerQuarterNote;
        
        j["tempo_map"] = json::array();
        for (const auto& tempo : tempoMap) {
            j["tempo_map"].push_back({
                {"tick", tempo.tick},
                {"time_ms", tempo.timeMs},
                {"bpm", tempo.bpm}
            });
        }
        
        j["time_signatures"] = json::array();
        for (const auto& ts : timeSignatures) {
            j["time_signatures"].push_back({
                {"tick", ts.tick},
                {"numerator", ts.numerator},
                {"denominator", ts.denominator}
            });
        }
        
        j["key_signatures"] = json::array();
        for (const auto& ks : keySignatures) {
            j["key_signatures"].push_back({
                {"tick", ks.tick},
                {"sharps_flats", ks.sharpsFlats},
                {"is_minor", ks.isMinor},
                {"key_name", ks.keyName}
            });
        }
        
        j["tracks"] = json::array();
        for (const auto& track : tracks) {
            j["tracks"].push_back({
                {"index", track.index},
                {"name", track.name},
                {"channel", track.channel},
                {"program_change", track.programChange},
                {"instrument_name", track.instrumentName},
                {"note_count", track.noteCount},
                {"min_note", track.minNote},
                {"max_note", track.maxNote},
                {"avg_velocity", track.avgVelocity},
                {"first_event_ms", track.firstEventMs},
                {"last_event_ms", track.lastEventMs},
                {"note_density", track.noteDensity}
            });
        }
        
        j["total_notes"] = totalNotes;
        j["total_control_changes"] = totalControlChanges;
        j["total_program_changes"] = totalProgramChanges;
        j["has_sysex"] = hasSysEx;
        j["has_lyrics"] = hasLyrics;
        j["has_markers"] = hasMarkers;
        
        return j;
    }
};

class MidiFileAnalyzer {
public:
    static MidiFileAnalysis analyze(const std::string& filepath) {
        MidiFileAnalysis analysis;
        analysis.filename = filepath;
        
        smf::MidiFile midiFile;
        midiFile.read(filepath);
        
        if (!midiFile.status()) {
            Logger::error("Analyzer", "Failed to read file: " + filepath);
            return analysis;
        }
        
        midiFile.doTimeAnalysis();
        midiFile.linkNotePairs();
        
        analysis.format = midiFile.getTrackCount() == 1 ? 0 : 1;
        analysis.ticksPerQuarterNote = midiFile.getTicksPerQuarterNote();
        analysis.durationMs = (uint32_t)(midiFile.getFileDurationInSeconds() * 1000.0);
        
        // Construire tempo map
        buildTempoMap(midiFile, analysis);
        
        // Extraire time/key signatures
        extractTimeSignatures(midiFile, analysis);
        extractKeySignatures(midiFile, analysis);
        
        // Analyser chaque track
        for (int i = 0; i < midiFile.getTrackCount(); i++) {
            analysis.tracks.push_back(analyzeTrack(midiFile, i));
        }
        
        // Statistiques globales
        calculateGlobalStats(analysis);
        
        Logger::info("Analyzer", "Analysis complete for " + filepath);
        
        return analysis;
    }

private:
    static void buildTempoMap(const smf::MidiFile& file, MidiFileAnalysis& analysis) {
        for (int track = 0; track < file.getTrackCount(); track++) {
            for (int event = 0; event < file[track].size(); event++) {
                if (file[track][event].isTempo()) {
                    TempoChange tempo;
                    tempo.tick = file[track][event].tick;
                    tempo.timeMs = (uint32_t)(file[track][event].seconds * 1000.0);
                    tempo.bpm = file[track][event].getTempoBPM();
                    analysis.tempoMap.push_back(tempo);
                }
            }
        }
        
        if (analysis.tempoMap.empty()) {
            analysis.tempoMap.push_back({0, 0, 120.0});
        }
    }

    static void extractTimeSignatures(const smf::MidiFile& file, MidiFileAnalysis& analysis) {
        for (int track = 0; track < file.getTrackCount(); track++) {
            for (int event = 0; event < file[track].size(); event++) {
                if (file[track][event].isTimeSignature()) {
                    TimeSignature ts;
                    ts.tick = file[track][event].tick;
                    ts.numerator = file[track][event][3];
                    ts.denominator = 1 << file[track][event][4];
                    analysis.timeSignatures.push_back(ts);
                }
            }
        }
        
        if (analysis.timeSignatures.empty()) {
            analysis.timeSignatures.push_back({0, 4, 4});
        }
    }

    static void extractKeySignatures(const smf::MidiFile& file, MidiFileAnalysis& analysis) {
        for (int track = 0; track < file.getTrackCount(); track++) {
            for (int event = 0; event < file[track].size(); event++) {
                if (file[track][event].isKeySignature()) {
                    KeySignature ks;
                    ks.tick = file[track][event].tick;
                    ks.sharpsFlats = (int8_t)file[track][event][3];
                    ks.isMinor = (file[track][event][4] == 1);
                    ks.keyName = getKeyName(ks.sharpsFlats, ks.isMinor);
                    analysis.keySignatures.push_back(ks);
                }
            }
        }
        
        if (analysis.keySignatures.empty()) {
            analysis.keySignatures.push_back({0, 0, false, "C major"});
        }
    }

    static TrackInfo analyzeTrack(const smf::MidiFile& file, int trackIndex) {
        TrackInfo info;
        info.index = trackIndex;
        info.channel = 255;
        info.programChange = 0;
        info.noteCount = 0;
        info.minNote = 127;
        info.maxNote = 0;
        info.avgVelocity = 0;
        info.firstEventMs = UINT32_MAX;
        info.lastEventMs = 0;
        
        uint32_t totalVelocity = 0;
        
        for (int event = 0; event < file[trackIndex].size(); event++) {
            const auto& ev = file[trackIndex][event];
            uint32_t timeMs = (uint32_t)(ev.seconds * 1000.0);
            
            if (ev.isTrackName()) {
                for (int i = 0; i < ev.getMetaContent().size(); i++) {
                    info.name += (char)ev.getMetaContent()[i];
                }
            }
            
            if (ev.isNoteOn()) {
                info.noteCount++;
                info.channel = ev.getChannel();
                
                uint8_t note = ev.getKeyNumber();
                uint8_t velocity = ev.getVelocity();
                
                info.minNote = std::min(info.minNote, note);
                info.maxNote = std::max(info.maxNote, note);
                totalVelocity += velocity;
                
                info.firstEventMs = std::min(info.firstEventMs, timeMs);
                info.lastEventMs = std::max(info.lastEventMs, timeMs);
            }
            
            if (ev.isProgramChange()) {
                info.programChange = ev[1];
                info.channel = ev.getChannel();
            }
        }
        
        if (info.noteCount > 0) {
            info.avgVelocity = totalVelocity / info.noteCount;
            
            uint32_t durationMs = info.lastEventMs - info.firstEventMs;
            if (durationMs > 0) {
                info.noteDensity = (float)info.noteCount / (durationMs / 1000.0f);
            }
        }
        
        info.instrumentName = getInstrumentName(info.programChange);
        
        if (info.name.empty()) {
            info.name = "Track " + std::to_string(trackIndex + 1);
        }
        
        return info;
    }

    static void calculateGlobalStats(MidiFileAnalysis& analysis) {
        analysis.totalNotes = 0;
        analysis.totalControlChanges = 0;
        analysis.totalProgramChanges = 0;
        analysis.hasSysEx = false;
        analysis.hasLyrics = false;
        analysis.hasMarkers = false;
        
        for (const auto& track : analysis.tracks) {
            analysis.totalNotes += track.noteCount;
        }
    }

    static std::string getKeyName(int8_t sharpsFlats, bool isMinor) {
        static const char* majorKeys[] = {
            "C", "G", "D", "A", "E", "B", "F#", "C#",
            "F", "Bb", "Eb", "Ab", "Db", "Gb", "Cb"
        };
        static const char* minorKeys[] = {
            "A", "E", "B", "F#", "C#", "G#", "D#", "A#",
            "D", "G", "C", "F", "Bb", "Eb", "Ab"
        };
        
        int index = sharpsFlats >= 0 ? sharpsFlats : 7 - sharpsFlats;
        std::string key = isMinor ? minorKeys[index] : majorKeys[index];
        key += isMinor ? " minor" : " major";
        
        return key;
    }

    static std::string getInstrumentName(uint8_t program) {
        static const std::string instruments[128] = {
            "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano",
            "Honky-tonk Piano", "Electric Piano 1", "Electric Piano 2", "Harpsichord",
            "Clavinet", "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
            "Marimba", "Xylophone", "Tubular Bells", "Dulcimer", "Drawbar Organ",
            "Percussive Organ", "Rock Organ", "Church Organ", "Reed Organ",
            "Accordion", "Harmonica", "Tango Accordion", "Acoustic Guitar (nylon)",
            "Acoustic Guitar (steel)", "Electric Guitar (jazz)", "Electric Guitar (clean)",
            "Electric Guitar (muted)", "Overdriven Guitar", "Distortion Guitar",
            "Guitar Harmonics", "Acoustic Bass", "Electric Bass (finger)",
            "Electric Bass (pick)", "Fretless Bass", "Slap Bass 1", "Slap Bass 2",
            "Synth Bass 1", "Synth Bass 2", "Violin", "Viola", "Cello",
            "Contrabass", "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp",
            "Timpani", "String Ensemble 1", "String Ensemble 2", "Synth Strings 1",
            "Synth Strings 2", "Choir Aahs", "Voice Oohs", "Synth Choir",
            "Orchestra Hit", "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
            "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
            "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax", "Oboe",
            "English Horn", "Bassoon", "Clarinet", "Piccolo", "Flute", "Recorder",
            "Pan Flute", "Blown bottle", "Shakuhachi", "Whistle", "Ocarina",
            "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)",
            "Lead 4 (chiff)", "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)",
            "Lead 8 (bass + lead)", "Pad 1 (new age)", "Pad 2 (warm)",
            "Pad 3 (polysynth)", "Pad 4 (choir)", "Pad 5 (bowed)", "Pad 6 (metallic)",
            "Pad 7 (halo)", "Pad 8 (sweep)", "FX 1 (rain)", "FX 2 (soundtrack)",
            "FX 3 (crystal)", "FX 4 (atmosphere)", "FX 5 (brightness)",
            "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)", "Sitar", "Banjo",
            "Shamisen", "Koto", "Kalimba", "Bagpipe", "Fiddle", "Shanai",
            "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock", "Taiko Drum",
            "Melodic Tom", "Synth Drum", "Reverse Cymbal", "Guitar Fret Noise",
            "Breath Noise", "Seashore", "Bird Tweet", "Telephone Ring", "Helicopter",
            "Applause", "Gunshot"
        };
        return instruments[program];
    }
};

} // namespace midiMind
