// ============================================================================
// Fichier: src/midi/MidiFileReader.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "MidiFileReader.h"
#include <cstring>

namespace midiMind {

// ============================================================================
// CONSTRUCTION
// ============================================================================

MidiFileReader::MidiFileReader() {
    Logger::debug("MidiFileReader", "MidiFileReader constructed");
}

MidiFileReader::~MidiFileReader() {
    Logger::debug("MidiFileReader", "MidiFileReader destroyed");
}

// ============================================================================
// LECTURE
// ============================================================================

MidiFile MidiFileReader::read(const std::string& filepath) {
    Logger::info("MidiFileReader", "Reading MIDI file: " + filepath);
    
    std::ifstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        THROW_ERROR(ErrorCode::MIDI_FILE_OPEN_FAILED, 
                   "Cannot open file: " + filepath);
    }
    
    try {
        MidiFile midiFile;
        
        // Parse header
        midiFile.header = parseHeader(file);
        
        Logger::info("MidiFileReader", "  Format: " + std::to_string(midiFile.header.format));
        Logger::info("MidiFileReader", "  Tracks: " + std::to_string(midiFile.header.numTracks));
        Logger::info("MidiFileReader", "  Division: " + std::to_string(midiFile.header.division));
        
        // Parse tracks
        midiFile.tracks.reserve(midiFile.header.numTracks);
        
        for (uint16_t i = 0; i < midiFile.header.numTracks; ++i) {
            Logger::debug("MidiFileReader", "  Parsing track " + std::to_string(i + 1));
            MidiTrack track = parseTrack(file);
            midiFile.tracks.push_back(track);
        }
        
        file.close();
        
        Logger::info("MidiFileReader", "✓ MIDI file read successfully");
        
        return midiFile;
        
    } catch (const std::exception& e) {
        file.close();
        throw;
    }
}

MidiFile MidiFileReader::readFromBuffer(const uint8_t* data, size_t size) {
    // TODO: Implémenter lecture depuis buffer
    // Pour l'instant, utiliser un fichier temporaire
    THROW_ERROR(ErrorCode::SYSTEM_NOT_SUPPORTED, 
               "readFromBuffer not yet implemented");
}

// ============================================================================
// VALIDATION
// ============================================================================

bool MidiFileReader::validate(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        return false;
    }
    
    try {
        // Vérifier la signature "MThd"
        char signature[4];
        file.read(signature, 4);
        
        if (std::strncmp(signature, "MThd", 4) != 0) {
            return false;
        }
        
        // Vérifier la longueur du header (doit être 6)
        uint32_t headerLength = 0;
        file.read(reinterpret_cast<char*>(&headerLength), 4);
        headerLength = __builtin_bswap32(headerLength); // Big endian
        
        if (headerLength != 6) {
            return false;
        }
        
        file.close();
        return true;
        
    } catch (...) {
        file.close();
        return false;
    }
}

// ============================================================================
// PARSING - HEADER
// ============================================================================

MidiFileHeader MidiFileReader::parseHeader(std::ifstream& file) {
    // Vérifier signature "MThd"
    verifySignature(file, "MThd");
    
    // Lire longueur du header (doit être 6)
    uint32_t headerLength = readUInt32(file);
    if (headerLength != 6) {
        THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT,
                   "Invalid header length: " + std::to_string(headerLength));
    }
    
    MidiFileHeader header;
    
    // Format
    header.format = readUInt16(file);
    if (header.format > 2) {
        THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT,
                   "Unsupported MIDI format: " + std::to_string(header.format));
    }
    
    // Nombre de pistes
    header.numTracks = readUInt16(file);
    
    // Division
    header.division = readUInt16(file);
    
    return header;
}

// ============================================================================
// PARSING - TRACK
// ============================================================================

MidiTrack MidiFileReader::parseTrack(std::ifstream& file) {
    // Vérifier signature "MTrk"
    verifySignature(file, "MTrk");
    
    // Lire longueur de la piste
    uint32_t trackLength = readUInt32(file);
    
    // Position de fin de la piste
    size_t startPos = file.tellg();
    size_t endPos = startPos + trackLength;
    
    MidiTrack track;
    uint8_t runningStatus = 0;
    uint32_t absoluteTime = 0;
    
    // Parser les événements
    while (file.tellg() < static_cast<std::streampos>(endPos)) {
        MidiEvent event = parseEvent(file, runningStatus);
        
        // Calculer le temps absolu
        absoluteTime += event.deltaTime;
        event.absoluteTime = absoluteTime;
        
        track.events.push_back(event);
        
        // Si c'est un meta-event "Track Name", l'extraire
        if (event.message.getSize() > 2 &&
            event.message.getData()[0] == 0xFF &&
            event.message.getData()[1] == 0x03) {
            // Track Name meta-event
            size_t nameLength = event.message.getSize() - 2;
            if (nameLength > 0) {
                track.name = std::string(
                    reinterpret_cast<const char*>(event.message.getData() + 2),
                    nameLength
                );
            }
        }
    }
    
    return track;
}

// ============================================================================
// PARSING - EVENT
// ============================================================================

MidiEvent MidiFileReader::parseEvent(std::ifstream& file, uint8_t& runningStatus) {
    MidiEvent event;
    
    // Lire delta time
    event.deltaTime = readVariableLength(file);
    
    // Lire status byte
    uint8_t status = readUInt8(file);
    
    // Running status ?
    if ((status & 0x80) == 0) {
        // C'est un data byte, utiliser le running status
        file.seekg(-1, std::ios::cur); // Reculer d'un byte
        status = runningStatus;
    } else {
        // Nouveau status
        runningStatus = status;
    }
    
    // Parser selon le type
    if (status == 0xFF) {
        // Meta event
        return parseMetaEvent(file, event.deltaTime);
        
    } else if (status == 0xF0 || status == 0xF7) {
        // SysEx event
        return parseSysExEvent(file, event.deltaTime, status);
        
    } else {
        // Channel message
        uint8_t type = status & 0xF0;
        
        std::array<uint8_t, 3> data;
        data[0] = status;
        size_t dataSize = 1;
        
        // Lire les data bytes selon le type
        switch (type) {
            case 0x80: // Note Off
            case 0x90: // Note On
            case 0xA0: // Poly Aftertouch
            case 0xB0: // Control Change
            case 0xE0: // Pitch Bend
                data[1] = readUInt8(file);
                data[2] = readUInt8(file);
                dataSize = 3;
                break;
                
            case 0xC0: // Program Change
            case 0xD0: // Channel Aftertouch
                data[1] = readUInt8(file);
                dataSize = 2;
                break;
        }
        
        event.message = MidiMessage(data.data(), dataSize);
    }
    
    return event;
}

// ============================================================================
// PARSING - META EVENT
// ============================================================================

MidiEvent MidiFileReader::parseMetaEvent(std::ifstream& file, uint32_t deltaTime) {
    MidiEvent event;
    event.deltaTime = deltaTime;
    
    uint8_t metaType = readUInt8(file);
    uint32_t length = readVariableLength(file);
    
    // Créer le message avec 0xFF + type + data
    std::vector<uint8_t> data;
    data.push_back(0xFF);
    data.push_back(metaType);
    
    // Lire les données
    for (uint32_t i = 0; i < length; ++i) {
        data.push_back(readUInt8(file));
    }
    
    event.message = MidiMessage(data);
    
    return event;
}

// ============================================================================
// PARSING - SYSEX EVENT
// ============================================================================

MidiEvent MidiFileReader::parseSysExEvent(std::ifstream& file, uint32_t deltaTime, uint8_t status) {
    MidiEvent event;
    event.deltaTime = deltaTime;
    
    uint32_t length = readVariableLength(file);
    
    std::vector<uint8_t> data;
    data.push_back(status);
    
    // Lire les données
    for (uint32_t i = 0; i < length; ++i) {
        data.push_back(readUInt8(file));
    }
    
    event.message = MidiMessage(data);
    
    return event;
}

// ============================================================================
// UTILITAIRES DE LECTURE
// ============================================================================

uint32_t MidiFileReader::readVariableLength(std::ifstream& file) {
    uint32_t value = 0;
    uint8_t byte;
    
    do {
        byte = readUInt8(file);
        value = (value << 7) | (byte & 0x7F);
    } while (byte & 0x80);
    
    return value;
}

uint16_t MidiFileReader::readUInt16(std::ifstream& file) {
    uint16_t value;
    file.read(reinterpret_cast<char*>(&value), 2);
    return __builtin_bswap16(value); // Big endian
}

uint32_t MidiFileReader::readUInt32(std::ifstream& file) {
    uint32_t value;
    file.read(reinterpret_cast<char*>(&value), 4);
    return __builtin_bswap32(value); // Big endian
}

uint8_t MidiFileReader::readUInt8(std::ifstream& file) {
    uint8_t value;
    file.read(reinterpret_cast<char*>(&value), 1);
    return value;
}

void MidiFileReader::verifySignature(std::ifstream& file, const std::string& expected) {
    char signature[4];
    file.read(signature, 4);
    
    if (std::strncmp(signature, expected.c_str(), 4) != 0) {
        THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT,
                   "Invalid signature, expected: " + expected);
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiFileReader.cpp
// ============================================================================