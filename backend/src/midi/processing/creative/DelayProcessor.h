// ============================================================================
// Fichier: src/midi/processing/creative/DelayProcessor.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Processeur de délai MIDI.
//   Retarde et répète les messages MIDI avec feedback.
//
// Thread-safety: Oui
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include "../MidiProcessor.h"
#include <deque>
#include <thread>
#include <atomic>
#include <functional>

namespace midiMind {

/**
 * @struct DelayedMessage
 * @brief Message MIDI retardé
 */
struct DelayedMessage {
    MidiMessage message;
    uint64_t timestamp;     ///< Timestamp de sortie (ms)
    uint8_t repetition;     ///< Numéro de répétition
    
    DelayedMessage(const MidiMessage& msg, uint64_t ts, uint8_t rep = 0)
        : message(msg), timestamp(ts), repetition(rep) {}
};

/**
 * @class DelayProcessor
 * @brief Processeur de délai MIDI
 * 
 * @details
 * Retarde les messages MIDI et les répète avec feedback.
 * Peut simuler un delay classique avec décroissance de vélocité.
 * 
 * Paramètres:
 * - delay_ms: Temps de délai en millisecondes
 * - feedback: Feedback (0.0-1.0)
 * - mix: Mix dry/wet (0.0-1.0)
 * - max_repeats: Nombre maximum de répétitions
 * - velocity_decay: Décroissance de vélocité par répétition
 * 
 * Thread-safety: Oui
 * 
 * @example Utilisation
 * ```cpp
 * auto delay = std::make_shared<DelayProcessor>();
 * delay->setDelayTime(250); // 250ms
 * delay->setFeedback(0.6f); // 60% feedback
 * delay->start();
 * ```
 */
class DelayProcessor : public MidiProcessor {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback pour envoyer des messages retardés
     */
    using MessageOutputCallback = std::function<void(const MidiMessage&)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    DelayProcessor();
    
    /**
     * @brief Destructeur
     */
    ~DelayProcessor();
    
    // ========================================================================
    // TRAITEMENT
    // ========================================================================
    
    /**
     * @brief Traite un message MIDI
     * 
     * Transmet le message original (dry) et ajoute au buffer de délai.
     */
    std::vector<MidiMessage> process(const MidiMessage& input) override;
    
    // ========================================================================
    // CONTRÔLE
    // ========================================================================
    
    /**
     * @brief Démarre le processeur de délai
     */
    void start();
    
    /**
     * @brief Arrête le processeur de délai
     */
    void stop();
    
    /**
     * @brief Vérifie si le processeur est actif
     */
    bool isRunning() const;
    
    /**
     * @brief Réinitialise l'état (vide le buffer)
     */
    void reset() override;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit le temps de délai
     * 
     * @param delayMs Délai en millisecondes (1-5000ms)
     */
    void setDelayTime(uint32_t delayMs);
    
    /**
     * @brief Récupère le temps de délai
     */
    uint32_t getDelayTime() const;
    
    /**
     * @brief Définit le feedback
     * 
     * @param feedback Feedback (0.0-1.0)
     */
    void setFeedback(float feedback);
    
    /**
     * @brief Récupère le feedback
     */
    float getFeedback() const;
    
    /**
     * @brief Définit le mix dry/wet
     * 
     * @param mix Mix (0.0=dry, 1.0=wet)
     */
    void setMix(float mix);
    
    /**
     * @brief Récupère le mix
     */
    float getMix() const;
    
    /**
     * @brief Définit le nombre maximum de répétitions
     * 
     * @param maxRepeats Nombre de répétitions (1-16)
     */
    void setMaxRepeats(uint8_t maxRepeats);
    
    /**
     * @brief Récupère le nombre maximum de répétitions
     */
    uint8_t getMaxRepeats() const;
    
    /**
     * @brief Définit la décroissance de vélocité
     * 
     * @param decay Facteur de décroissance (0.0-1.0)
     */
    void setVelocityDecay(float decay);
    
    /**
     * @brief Récupère la décroissance de vélocité
     */
    float getVelocityDecay() const;
    
    /**
     * @brief Définit le callback de sortie
     */
    void setMessageOutputCallback(MessageOutputCallback callback);
    
    /**
     * @brief Définit un paramètre
     */
    bool setParameter(const std::string& name, const json& value) override;

private:
    /**
     * @brief Thread de traitement du délai
     */
    void delayThread();
    
    /**
     * @brief Récupère le timestamp actuel (ms)
     */
    uint64_t getCurrentTimestamp() const;
    
    /// Temps de délai (ms)
    uint32_t delayMs_;
    
    /// Feedback (0.0-1.0)
    float feedback_;
    
    /// Mix dry/wet (0.0-1.0)
    float mix_;
    
    /// Nombre maximum de répétitions
    uint8_t maxRepeats_;
    
    /// Décroissance de vélocité
    float velocityDecay_;
    
    /// Buffer de messages retardés
    std::deque<DelayedMessage> delayBuffer_;
    
    /// Mutex pour protéger le buffer
    mutable std::mutex bufferMutex_;
    
    /// Thread de traitement
    std::thread delayThread_;
    
    /// Flag d'arrêt
    std::atomic<bool> running_;
    
    /// Callback de sortie
    MessageOutputCallback messageOutputCallback_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER DelayProcessor.h
// ============================================================================