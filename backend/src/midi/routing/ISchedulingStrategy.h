// ============================================================================
// src/midi/routing/ISchedulingStrategy.h
// Pattern Strategy pour le scheduling des messages MIDI
// ============================================================================

#include "../MidiMessage.h"

struct ScheduledMidiMessage {
    uint32_t timeMs;
    std::string deviceId;
    MidiMessage message;
    
    bool operator>(const ScheduledMidiMessage& other) const {
        return timeMs > other.timeMs;
    }
};

/**
 * @brief Interface Strategy pour le scheduling
 */
class ISchedulingStrategy {
public:
    virtual ~ISchedulingStrategy() = default;
    
    /**
     * @brief Ajoute un message au scheduler
     */
    virtual void schedule(const ScheduledMidiMessage& msg) = 0;
    
    /**
     * @brief Récupère le prochain message à envoyer
     * @return Message si disponible, std::nullopt sinon
     */
    virtual std::optional<ScheduledMidiMessage> getNext() = 0;
    
    /**
     * @brief Vérifie s'il y a des messages prêts
     */
    virtual bool hasReady(uint32_t currentTimeMs) const = 0;
    
    /**
     * @brief Vide tous les messages
     */
    virtual void clear() = 0;
    
    /**
     * @brief Nombre de messages en attente
     */
    virtual size_t size() const = 0;
    
    /**
     * @brief Nom de la stratégie
     */
    virtual std::string getName() const = 0;
};
