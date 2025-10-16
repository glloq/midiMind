// ============================================================================
// Fichier: backend/src/api/MessageEnvelope.h
// Version: 3.0.2 - AJOUT SIGNATURES MANQUANTES
// ============================================================================

// CORRECTIFS APPLIQUÉS:
// - Ajout toJsonString() avec paramètre indent
// - Ajout validate(std::vector<std::string>& errors)
// ============================================================================

#pragma once

#include "Protocol.h"
#include <optional>
#include <vector>
#include <string>

namespace midiMind {

/**
 * @class MessageEnvelope
 * @brief Enveloppe de message complète avec validation
 */
class MessageEnvelope {
public:
    // ========================================================================
    // CONSTRUCTEURS
    // ========================================================================
    
    MessageEnvelope();
    explicit MessageEnvelope(protocol::MessageType type);
    
    // ========================================================================
    // FACTORY METHODS
    // ========================================================================
    
    static MessageEnvelope createRequest(
        const std::string& command,
        const json& params = json::object());
    
    static MessageEnvelope createSuccessResponse(
        const std::string& requestId,
        const json& data = json::object(),
        int latency = 0);
    
    static MessageEnvelope createErrorResponse(
        const std::string& requestId,
        protocol::ErrorCode code,
        const std::string& message,
        const json& details = json::object(),
        bool retryable = false);
    
    static MessageEnvelope createEvent(
        const std::string& name,
        const json& data = json::object(),
        protocol::EventPriority priority = protocol::EventPriority::NORMAL);
    
    static MessageEnvelope createError(
        protocol::ErrorCode code,
        const std::string& message,
        const json& details = json::object());
    
    // ========================================================================
    // PARSING
    // ========================================================================
    
    static std::optional<MessageEnvelope> fromJson(const json& j);
    static std::optional<MessageEnvelope> fromJsonString(const std::string& str);
    
    // ========================================================================
    // SERIALIZATION
    // ========================================================================
    
    json toJson() const;
    
    /**
     * @brief ✅ AJOUT: Signature avec paramètre indent
     */
    std::string toJsonString(int indent = -1) const;
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    bool isValid() const;
    
    /**
     * @brief ✅ AJOUT: Méthode validate avec vecteur d'erreurs
     */
    bool validate(std::vector<std::string>& errors) const;
    
    std::vector<std::string> getValidationErrors() const;
    
    // ========================================================================
    // ACCESSEURS TYPE
    // ========================================================================
    
    bool isRequest() const;
    bool isResponse() const;
    bool isEvent() const;
    bool isError() const;
    
    // ========================================================================
    // ACCESSEURS CONTENU
    // ========================================================================
    
    const protocol::Envelope& getEnvelope() const { return envelope_; }
    const protocol::Request& getRequest() const;
    const protocol::Response& getResponse() const;
    const protocol::Event& getEvent() const;
    const protocol::Error& getError() const;
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    std::string getId() const { return envelope_.id; }
    int64_t getTimestamp() const { return envelope_.timestamp; }
    protocol::MessageType getType() const { return envelope_.type; }
    int getLatencySinceCreation() const;
    
private:
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    protocol::Envelope envelope_;
    
    std::optional<protocol::Request> request_;
    std::optional<protocol::Response> response_;
    std::optional<protocol::Event> event_;
    std::optional<protocol::Error> error_;
    
    mutable bool validationCached_;
    mutable bool validationResult_;
    mutable std::vector<std::string> validationErrors_;
    
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    bool validateEnvelope() const;
    bool validateContent() const;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MessageEnvelope.h
// ============================================================================
