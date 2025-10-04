// ============================================================================
// Fichier: src/network/rtpmidi/RtpPacket.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Structures et utilitaires pour les paquets RTP-MIDI (RFC 6295).
//   Définit le format des paquets RTP et les commandes du protocole.
//
// Thread-safety: Les structures sont POD (Plain Old Data)
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <cstdint>
#include <vector>
#include <cstring>

namespace midiMind {

// ============================================================================
// CONSTANTES RTP-MIDI
// ============================================================================

namespace RtpMidi {
    // Ports par défaut
    constexpr uint16_t DEFAULT_CONTROL_PORT = 5004;
    constexpr uint16_t DEFAULT_DATA_PORT = 5005;
    
    // Payload Type pour MIDI
    constexpr uint8_t PAYLOAD_TYPE_MIDI = 97;
    
    // Signatures de protocole
    constexpr uint16_t SIGNATURE = 0xFFFF;
    
    // Commandes du protocole
    constexpr uint16_t CMD_INVITATION = 0x494E;      // 'IN'
    constexpr uint16_t CMD_INVITATION_ACCEPTED = 0x4F4B;  // 'OK'
    constexpr uint16_t CMD_INVITATION_REJECTED = 0x4E4F;  // 'NO'
    constexpr uint16_t CMD_END_SESSION = 0x4259;     // 'BY'
    constexpr uint16_t CMD_SYNCHRONIZATION = 0x434B;  // 'CK'
    constexpr uint16_t CMD_RECEIVER_FEEDBACK = 0x5253; // 'RS'
    
    // Tailles maximales
    constexpr size_t MAX_PACKET_SIZE = 1500;
    constexpr size_t MAX_MIDI_PAYLOAD = 1400;
}

// ============================================================================
// STRUCTURES RTP
// ============================================================================

/**
 * @struct RtpHeader
 * @brief En-tête RTP standard (12 bytes)
 */
#pragma pack(push, 1)
struct RtpHeader {
    // Byte 0
    uint8_t version : 2;        ///< Version RTP (toujours 2)
    uint8_t padding : 1;        ///< Padding flag
    uint8_t extension : 1;      ///< Extension flag
    uint8_t csrcCount : 4;      ///< CSRC count
    
    // Byte 1
    uint8_t marker : 1;         ///< Marker bit
    uint8_t payloadType : 7;    ///< Payload type (97 pour MIDI)
    
    // Bytes 2-3
    uint16_t sequenceNumber;    ///< Sequence number (big endian)
    
    // Bytes 4-7
    uint32_t timestamp;         ///< Timestamp (big endian)
    
    // Bytes 8-11
    uint32_t ssrc;              ///< Synchronization source identifier
    
    /**
     * @brief Constructeur par défaut
     */
    RtpHeader() 
        : version(2)
        , padding(0)
        , extension(0)
        , csrcCount(0)
        , marker(0)
        , payloadType(RtpMidi::PAYLOAD_TYPE_MIDI)
        , sequenceNumber(0)
        , timestamp(0)
        , ssrc(0) {}
    
    /**
     * @brief Convertit en network byte order (big endian)
     */
    void toNetworkOrder() {
        sequenceNumber = htons(sequenceNumber);
        timestamp = htonl(timestamp);
        ssrc = htonl(ssrc);
    }
    
    /**
     * @brief Convertit depuis network byte order
     */
    void fromNetworkOrder() {
        sequenceNumber = ntohs(sequenceNumber);
        timestamp = ntohl(timestamp);
        ssrc = ntohl(ssrc);
    }
};
#pragma pack(pop)

static_assert(sizeof(RtpHeader) == 12, "RtpHeader must be 12 bytes");

/**
 * @struct RtpMidiHeader
 * @brief En-tête spécifique MIDI (après RTP header)
 */
#pragma pack(push, 1)
struct RtpMidiHeader {
    uint8_t flags;              ///< Flags MIDI (B, J, Z, P)
    uint8_t length;             ///< Longueur du payload MIDI
    
    // Flags individuels
    bool hasLongHeader() const { return (flags & 0x80) != 0; }
    bool hasJournal() const { return (flags & 0x40) != 0; }
    bool firstInGroup() const { return (flags & 0x20) != 0; }
    bool hasPayload() const { return (flags & 0x10) != 0; }
    
    void setLongHeader(bool value) {
        if (value) flags |= 0x80;
        else flags &= ~0x80;
    }
    
    void setJournal(bool value) {
        if (value) flags |= 0x40;
        else flags &= ~0x40;
    }
    
    void setFirstInGroup(bool value) {
        if (value) flags |= 0x20;
        else flags &= ~0x20;
    }
    
    void setPayload(bool value) {
        if (value) flags |= 0x10;
        else flags &= ~0x10;
    }
    
    RtpMidiHeader() : flags(0x80), length(0) {}
};
#pragma pack(pop)

/**
 * @struct ControlPacket
 * @brief Paquet de contrôle RTP-MIDI
 */
#pragma pack(push, 1)
struct ControlPacket {
    uint16_t signature;         ///< Toujours 0xFFFF
    uint16_t command;           ///< Commande (IN, OK, NO, BY, CK, RS)
    uint32_t protocolVersion;   ///< Version du protocole
    uint32_t initiatorToken;    ///< Token de l'initiateur
    uint32_t ssrc;              ///< SSRC
    
    // Nom du device (optionnel, taille variable)
    // Suit immédiatement la structure
    
    ControlPacket()
        : signature(RtpMidi::SIGNATURE)
        , command(0)
        , protocolVersion(2)
        , initiatorToken(0)
        , ssrc(0) {}
    
    void toNetworkOrder() {
        signature = htons(signature);
        command = htons(command);
        protocolVersion = htonl(protocolVersion);
        initiatorToken = htonl(initiatorToken);
        ssrc = htonl(ssrc);
    }
    
    void fromNetworkOrder() {
        signature = ntohs(signature);
        command = ntohs(command);
        protocolVersion = ntohl(protocolVersion);
        initiatorToken = ntohl(initiatorToken);
        ssrc = ntohl(ssrc);
    }
    
    bool isValid() const {
        return signature == RtpMidi::SIGNATURE;
    }
};
#pragma pack(pop)

/**
 * @struct SynchronizationPacket
 * @brief Paquet de synchronisation temporelle
 */
#pragma pack(push, 1)
struct SynchronizationPacket {
    uint16_t signature;         ///< 0xFFFF
    uint16_t command;           ///< 0x434B ('CK')
    uint32_t ssrc;              ///< SSRC
    uint8_t count;              ///< Compteur de sync
    uint8_t padding[3];         ///< Padding
    uint64_t timestamp1;        ///< Timestamp 1 (sender)
    uint64_t timestamp2;        ///< Timestamp 2 (receiver)
    uint64_t timestamp3;        ///< Timestamp 3 (sender)
    
    SynchronizationPacket()
        : signature(RtpMidi::SIGNATURE)
        , command(RtpMidi::CMD_SYNCHRONIZATION)
        , ssrc(0)
        , count(0)
        , timestamp1(0)
        , timestamp2(0)
        , timestamp3(0) {
        memset(padding, 0, sizeof(padding));
    }
    
    void toNetworkOrder() {
        signature = htons(signature);
        command = htons(command);
        ssrc = htonl(ssrc);
    }
    
    void fromNetworkOrder() {
        signature = ntohs(signature);
        command = ntohs(command);
        ssrc = ntohl(ssrc);
    }
};
#pragma pack(pop)

// ============================================================================
// CLASSES UTILITAIRES
// ============================================================================

/**
 * @class RtpPacketBuilder
 * @brief Constructeur de paquets RTP-MIDI
 */
class RtpPacketBuilder {
public:
    RtpPacketBuilder(uint32_t ssrc)
        : ssrc_(ssrc)
        , sequenceNumber_(0) {}
    
    /**
     * @brief Construit un paquet RTP-MIDI avec un message MIDI
     * 
     * @param midiData Données MIDI brutes
     * @param timestamp Timestamp RTP
     * @return std::vector<uint8_t> Paquet complet prêt à envoyer
     */
    std::vector<uint8_t> buildDataPacket(const std::vector<uint8_t>& midiData, 
                                         uint32_t timestamp) {
        std::vector<uint8_t> packet;
        
        // Préparer l'en-tête RTP
        RtpHeader rtpHeader;
        rtpHeader.version = 2;
        rtpHeader.payloadType = RtpMidi::PAYLOAD_TYPE_MIDI;
        rtpHeader.sequenceNumber = sequenceNumber_++;
        rtpHeader.timestamp = timestamp;
        rtpHeader.ssrc = ssrc_;
        rtpHeader.toNetworkOrder();
        
        // Copier l'en-tête RTP
        packet.resize(sizeof(RtpHeader));
        memcpy(packet.data(), &rtpHeader, sizeof(RtpHeader));
        
        // Ajouter l'en-tête MIDI
        RtpMidiHeader midiHeader;
        midiHeader.setPayload(true);
        midiHeader.length = static_cast<uint8_t>(midiData.size());
        
        packet.push_back(midiHeader.flags);
        packet.push_back(midiHeader.length);
        
        // Ajouter les données MIDI
        packet.insert(packet.end(), midiData.begin(), midiData.end());
        
        return packet;
    }
    
    /**
     * @brief Construit un paquet de contrôle
     * 
     * @param command Commande (IN, OK, NO, BY)
     * @param initiatorToken Token de l'initiateur
     * @param deviceName Nom du device (optionnel)
     * @return std::vector<uint8_t> Paquet de contrôle
     */
    std::vector<uint8_t> buildControlPacket(uint16_t command,
                                           uint32_t initiatorToken,
                                           const std::string& deviceName = "") {
        std::vector<uint8_t> packet;
        
        ControlPacket control;
        control.command = command;
        control.initiatorToken = initiatorToken;
        control.ssrc = ssrc_;
        control.toNetworkOrder();
        
        // Copier la structure
        packet.resize(sizeof(ControlPacket));
        memcpy(packet.data(), &control, sizeof(ControlPacket));
        
        // Ajouter le nom du device si fourni
        if (!deviceName.empty()) {
            packet.insert(packet.end(), deviceName.begin(), deviceName.end());
            packet.push_back(0); // Null terminator
        }
        
        return packet;
    }
    
    /**
     * @brief Construit un paquet de synchronisation
     */
    std::vector<uint8_t> buildSyncPacket(uint8_t count,
                                        uint64_t ts1,
                                        uint64_t ts2,
                                        uint64_t ts3) {
        std::vector<uint8_t> packet;
        
        SynchronizationPacket sync;
        sync.ssrc = ssrc_;
        sync.count = count;
        sync.timestamp1 = ts1;
        sync.timestamp2 = ts2;
        sync.timestamp3 = ts3;
        sync.toNetworkOrder();
        
        packet.resize(sizeof(SynchronizationPacket));
        memcpy(packet.data(), &sync, sizeof(SynchronizationPacket));
        
        return packet;
    }

private:
    uint32_t ssrc_;
    uint16_t sequenceNumber_;
};

/**
 * @class RtpPacketParser
 * @brief Parseur de paquets RTP-MIDI
 */
class RtpPacketParser {
public:
    /**
     * @brief Parse un paquet RTP de données
     * 
     * @param data Données brutes du paquet
     * @param size Taille des données
     * @param[out] midiData Données MIDI extraites
     * @param[out] timestamp Timestamp RTP
     * @param[out] sequenceNumber Numéro de séquence
     * @return true Si le parsing a réussi
     */
    static bool parseDataPacket(const uint8_t* data, size_t size,
                               std::vector<uint8_t>& midiData,
                               uint32_t& timestamp,
                               uint16_t& sequenceNumber) {
        if (size < sizeof(RtpHeader) + 2) {
            return false;
        }
        
        // Parser l'en-tête RTP
        RtpHeader rtpHeader;
        memcpy(&rtpHeader, data, sizeof(RtpHeader));
        rtpHeader.fromNetworkOrder();
        
        // Vérifier la version
        if (rtpHeader.version != 2) {
            return false;
        }
        
        timestamp = rtpHeader.timestamp;
        sequenceNumber = rtpHeader.sequenceNumber;
        
        // Parser l'en-tête MIDI
        const uint8_t* ptr = data + sizeof(RtpHeader);
        RtpMidiHeader midiHeader;
        midiHeader.flags = *ptr++;
        midiHeader.length = *ptr++;
        
        // Vérifier qu'il y a un payload
        if (!midiHeader.hasPayload() || midiHeader.length == 0) {
            return false;
        }
        
        // Vérifier la taille
        size_t remaining = size - sizeof(RtpHeader) - 2;
        if (remaining < midiHeader.length) {
            return false;
        }
        
        // Extraire les données MIDI
        midiData.assign(ptr, ptr + midiHeader.length);
        
        return true;
    }
    
    /**
     * @brief Parse un paquet de contrôle
     * 
     * @param data Données brutes
     * @param size Taille
     * @param[out] control Structure de contrôle
     * @param[out] deviceName Nom du device (si présent)
     * @return true Si le parsing a réussi
     */
    static bool parseControlPacket(const uint8_t* data, size_t size,
                                  ControlPacket& control,
                                  std::string& deviceName) {
        if (size < sizeof(ControlPacket)) {
            return false;
        }
        
        // Copier la structure
        memcpy(&control, data, sizeof(ControlPacket));
        control.fromNetworkOrder();
        
        // Vérifier la signature
        if (!control.isValid()) {
            return false;
        }
        
        // Extraire le nom du device si présent
        if (size > sizeof(ControlPacket)) {
            const char* namePtr = reinterpret_cast<const char*>(data + sizeof(ControlPacket));
            size_t nameLen = size - sizeof(ControlPacket);
            deviceName.assign(namePtr, nameLen);
            
            // Supprimer le null terminator
            if (!deviceName.empty() && deviceName.back() == '\0') {
                deviceName.pop_back();
            }
        }
        
        return true;
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER RtpPacket.h
// ============================================================================