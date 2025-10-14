#include <gtest/gtest.h>
#include "../src/midi/JsonMidiConverter.h"

TEST(JsonMidiConverter, MidiToJsonBasic) {
    // Créer un MidiFile simple
    MidiFile midi;
    // ... ajouter événements ...
    
    JsonMidiConverter converter;
    JsonMidi json = converter.midiToJson(midi);
    
    EXPECT_EQ(json.format, "jsonmidi-v1.0");
    EXPECT_GT(json.timeline.size(), 0);
}

TEST(JsonMidiConverter, JsonToMidiBasic) {
    JsonMidi json;
    json.format = "jsonmidi-v1.0";
    // ... ajouter événements ...
    
    JsonMidiConverter converter;
    MidiFile midi = converter.jsonToMidi(json);
    
    EXPECT_GT(midi.tracks.size(), 0);
}

TEST(JsonMidiConverter, RoundTripConversion) {
    // MIDI → JSON → MIDI doit donner le même résultat
    MidiFile original = loadTestFile("test.mid");
    
    JsonMidiConverter converter;
    JsonMidi json = converter.midiToJson(original);
    MidiFile reconstructed = converter.jsonToMidi(json);
    
    // Vérifier équivalence
    EXPECT_EQ(original.tracks.size(), reconstructed.tracks.size());
    // ... autres assertions ...
}