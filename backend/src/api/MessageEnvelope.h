// ============================================================================
// Fichier: backend/src/api/MessageEnvelope.h
// Version: 3.0.0-refonte
// Date: 2025-10-09
// ============================================================================
// Description:
//   Classe wrapper pour encapsuler tous les messages WebSocket.
//   Facilite la création, validation et parsing des messages.
//
// Usage:
//   // Créer une requête
//   auto msg = MessageEnvelope::createRequest("files.list", params);
//   std::string json = msg.toJsonString();
//
//   // Parser un message reçu
//   auto msg = MessageEnvelope::fromJsonString(jsonStr);
//   if (msg.isRequest()) { ... }
// ============================================================================

#pragma once

#include "Protocol.h"
#include "../core/Logger.h"
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @brief Classe enveloppe pour tous les messages WebSocket
 * 
 * Cette classe unifie la création, validation et parsing des messages.
 * Elle garantit que tous les messages respectent le protocole défini.
 * 
 * @example Créer et envoyer une requête
 * ```cpp
 * auto msg = MessageEnvelope::createRequest("files.list", {{"dir", "/midi"}});
 * ws.send(msg.toJsonString());
 * ```
 * 
 * @example Parser une réponse
 * ```cpp
 * auto msg = MessageEnvelope::fromJsonString(receivedJson);
 * if (msg.isResponse() && msg.getResponse().success) {
 *     auto data = msg.getResponse().data;
 * }
 * ```
 */
class MessageEnvelope {
public:
    // ========================================================================
    // CONSTRUCTEURS
    // ========================================================================
    
    /**
     * @brief Constructeur par défaut
     */
    MessageEnvelope();
    
    /**
     * @brief Constructeur avec type
     */
    explicit MessageEnvelope(protocol::MessageType type);
    
    // ========================================================================
    // FACTORY METHODS (création de messages)
    // ========================================================================
    
    /**
     * @brief Crée une requête
     * 
     * @param command Nom de la commande
     * @param params Paramètres (objet JSON)
     * @return Message enveloppe
     * 
     * @example
     * ```cpp
     * auto msg = MessageEnvelope::createRequest("files.list", 
     *     {{"directory", "/midi"}});
     * ```
     */
    static MessageEnvelope createRequest(
        const std::string& command,
        const json& params = json::object()
    );
    
    /**
     * @brief Crée une réponse de succès
     * 
     * @param requestId ID de la requête d'origine
     * @param data Données de la réponse
     * @param latency Latence en ms (optionnel)
     * @return Message enveloppe
     * 
     * @example
     * ```cpp
     * auto msg = MessageEnvelope::createSuccessResponse(
     *     "req-123", {{"files", fileList}}, 15);
     * ```
     */
    static MessageEnvelope createSuccessResponse(
        const std::string& requestId,
        const json& data,
        int latency = 0
    );
    
    /**
     * @brief Crée une réponse d'erreur
     * 
     * @param requestId ID de la requête d'origine
     * @param code Code d'erreur
     * @param message Message d'erreur
     * @param details Détails supplémentaires
     * @param retryable Peut-on réessayer ?
     * @return Message enveloppe
     * 
     * @example
     * ```cpp
     * auto msg = MessageEnvelope::createErrorResponse(
     *     "req-123", 
     *     protocol::ErrorCode::FILE_NOT_FOUND,
     *     "File not found",
     *     {{"path", "/midi/test.mid"}},
     *     false
     * );
     * ```
     */
    static MessageEnvelope createErrorResponse(
        const std::string& requestId,
        protocol::ErrorCode code,
        const std::string& message,
        const json& details = json::object(),
        bool retryable = false
    );
    
    /**
     * @brief Crée un événement
     * 
     * @param name Nom de l'événement
     * @param data Données
     * @param priority Priorité
     * @return Message enveloppe
     * 
     * @example
     * ```cpp
     * auto msg = MessageEnvelope::createEvent(
     *     "midi:message",
     *     {{"note", 60}, {"velocity", 100}},
     *     protocol::EventPriority::HIGH
     * );
     * ```
     */
    static MessageEnvelope createEvent(
        const std::string& name,
        const json& data,
        protocol::EventPriority priority = protocol::EventPriority::NORMAL
    );
    
    /**
     * @brief Crée une erreur standalone (sans requête associée)
     * 
     * @param code Code d'erreur
     * @param message Message
     * @param details Détails
     * @return Message enveloppe
     */
    static MessageEnvelope createError(
        protocol::ErrorCode code,
        const std::string& message,
        const json& details = json::object()
    );
    
    // ========================================================================
    // PARSING (depuis JSON)
    // ========================================================================
    
    /**
     * @brief Parse un message depuis JSON string
     * 
     * @param jsonStr String JSON
     * @return Message enveloppe ou std::nullopt si parsing échoue
     * 
     * @example
     * ```cpp
     * auto msgOpt = MessageEnvelope::fromJsonString(receivedJson);
     * if (msgOpt) {
     *     auto msg = *msgOpt;
     *     // Traiter le message
     * }
     * ```
     */
    static std::optional<MessageEnvelope> fromJsonString(const std::string& jsonStr);
    
    /**
     * @brief Parse un message depuis objet JSON
     * 
     * @param j Objet JSON
     * @return Message enveloppe ou std::nullopt si parsing échoue
     */
    static std::optional<MessageEnvelope> fromJson(const json& j);
    
    // ========================================================================
    // SÉRIALISATION (vers JSON)
    // ========================================================================
    
    /**
     * @brief Convertit en objet JSON
     * 
     * @return Objet JSON
     */
    json toJson() const;
    
    /**
     * @brief Convertit en string JSON
     * 
     * @return String JSON
     */
    std::string toJsonString() const;
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    /**
     * @brief Valide le message
     * 
     * @return true si valide, false sinon
     */
    bool isValid() const;
    
    /**
     * @brief Récupère les erreurs de validation
     * 
     * @return Liste des erreurs
     */
    std::vector<std::string> getValidationErrors() const;
    
    // ========================================================================
    // ACCESSEURS TYPE
    // ========================================================================
    
    /**
     * @brief Vérifie si c'est une requête
     */
    bool isRequest() const;
    
    /**
     * @brief Vérifie si c'est une réponse
     */
    bool isResponse() const;
    
    /**
     * @brief Vérifie si c'est un événement
     */
    bool isEvent() const;
    
    /**
     * @brief Vérifie si c'est une erreur
     */
    bool isError() const;
    
    // ========================================================================
    // ACCESSEURS CONTENU
    // ========================================================================
    
    /**
     * @brief Récupère l'enveloppe
     */
    const protocol::Envelope& getEnvelope() const { return envelope_; }
    
    /**
     * @brief Récupère la requête (lance exception si pas une requête)
     */
    const protocol::Request& getRequest() const;
    
    /**
     * @brief Récupère la réponse (lance exception si pas une réponse)
     */
    const protocol::Response& getResponse() const;
    
    /**
     * @brief Récupère l'événement (lance exception si pas un événement)
     */
    const protocol::Event& getEvent() const;
    
    /**
     * @brief Récupère l'erreur (lance exception si pas une erreur)
     */
    const protocol::Error& getError() const;
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    /**
     * @brief Récupère l'ID du message
     */
    std::string getId() const { return envelope_.id; }
    
    /**
     * @brief Récupère le timestamp
     */
    int64_t getTimestamp() const { return envelope_.timestamp; }
    
    /**
     * @brief Récupère le type
     */
    protocol::MessageType getType() const { return envelope_.type; }
    
    /**
     * @brief Calcule la latence depuis l'envoi
     * 
     * @return Latence en millisecondes
     */
    int getLatencySinceCreation() const;
    
private:
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    protocol::Envelope envelope_;
    
    // Union-like storage (un seul sera utilisé selon le type)
    std::optional<protocol::Request> request_;
    std::optional<protocol::Response> response_;
    std::optional<protocol::Event> event_;
    std::optional<protocol::Error> error_;
    
    // Cache de validation
    mutable bool validationCached_;
    mutable bool validationResult_;
    mutable std::vector<std::string> validationErrors_;
    
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Valide l'enveloppe
     */
    bool validateEnvelope() const;
    
    /**
     * @brief Valide le contenu selon le type
     */
    bool validateContent() const;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MessageEnvelope.h
// ============================================================================
