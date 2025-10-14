// ============================================================================
// Fichier: src/midi/processing/creative/ArpeggiatorProcessor.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Processeur d'arpégiation MIDI.
//   Transforme les accords en arpèges selon différents patterns.
//
// Thread-safety: Oui
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include "../MidiProcessor.h"
#include <set>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

namespace midiMind {

/**
 * @enum ArpPattern
 * @brief Pattern d'arpégiation
 */
enum class ArpPattern {
    UP,             ///< Montant (1-2-3-4)
    DOWN,           ///< Descendant (4-3-2-1)
    UP_DOWN,        ///< Montant-descendant (1-2-3-4-3-2)
    DOWN_UP,        ///< Descendant-montant (4-3-2-1-2-3)
    RANDOM,         ///< Aléatoire
    AS_PLAYED       ///< Ordre de jeu
};

/**
 * @class ArpeggiatorProcessor
 * @brief Processeur d'arpégiation
 * 
 * @details
 * Transforme les accords joués en arpèges.
 * Maintient un buffer des notes enfoncées et les rejoue
 * selon un pattern et un tempo.
 * 
 * Paramètres:
 * - pattern: Pattern d'arpégiation
 * - rate: Vitesse (notes par beat)
 * - octaves: Nombre d'octaves (1-4)
 * - tempo: Tempo en BPM (pour calcul timing)
 * - gate: Durée de la note (0.0-1.0)
 * 
 * Thread-safety: Oui
 * 
 * @example Utilisation
 * ```cpp
 * auto arp = std::make_shared<ArpeggiatorProcessor>();
 * arp->setPattern(ArpPattern::UP);
 * arp->setRate(4); // 16ème notes
 * arp->setTempo(120);
 * arp->start();
 * ```
 */
class ArpeggiatorProcessor : public MidiProcessor {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback pour envoyer des notes générées
     */
    using NoteOutputCallback = std::function<void(const MidiMessage&)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    ArpeggiatorProcessor();
    
    /**
     * @brief Destructeur
     */
    ~ArpeggiatorProcessor();
    
    // ========================================================================
    // TRAITEMENT
    // ========================================================================
    
    /**
     * @brief Traite un message MIDI
     * 
     * Collecte les Note On/Off mais ne génère pas directement.
     * L'arpégiation se fait en temps réel dans un thread séparé.
     */
    std::vector<MidiMessage> process(const MidiMessage& input) override;
    
    // ========================================================================
    // CONTRÔLE
    // ========================================================================
    
    /**
     * @brief Démarre l'arpégiateur
     */
    void start();
    
    /**
     * @brief Arrête l'arpégiateur
     */
    void stop();
    
    /**
     * @brief Vérifie si l'arpégiateur est actif
     */
    bool isRunning() const;
    
    /**
     * @brief Réinitialise l'état
     */
    void reset() override;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit le pattern
     */
    void setPattern(ArpPattern pattern);
    
    /**
     * @brief Récupère le pattern
     */
    ArpPattern getPattern() const;
    
    /**
     * @brief Définit la vitesse (notes par beat)
     * 
     * @param rate 1=noires, 2=croches, 4=16èmes, 8=32èmes
     */
    void setRate(uint8_t rate);
    
    /**
     * @brief Récupère la vitesse
     */
    uint8_t getRate() const;
    
    /**
     * @brief Définit le nombre d'octaves
     * 
     * @param octaves 1-4 octaves
     */
    void setOctaves(uint8_t octaves);
    
    /**
     * @brief Récupère le nombre d'octaves
     */
    uint8_t getOctaves() const;
    
    /**
     * @brief Définit le tempo
     * 
     * @param bpm Tempo en BPM
     */
    void setTempo(float bpm);
    
    /**
     * @brief Récupère le tempo
     */
    float getTempo() const;
    
    /**
     * @brief Définit le gate (durée de note)
     * 
     * @param gate 0.0-1.0 (0.5 = 50% de la durée)
     */
    void setGate(float gate);
    
    /**
     * @brief Récupère le gate
     */
    float getGate() const;
    
    /**
     * @brief Définit le callback de sortie
     */
    void setNoteOutputCallback(NoteOutputCallback callback);
    
    /**
     * @brief Définit un paramètre
     */
    bool setParameter(const std::string& name, const json& value) override;

private:
    /**
     * @brief Thread d'arpégiation
     */
    void arpeggiatorThread();
    
    /**
     * @brief Génère la séquence d'arpège
     */
    std::vector<uint8_t> generateArpSequence();
    
    /**
     * @brief Calcule l'intervalle entre notes (ms)
     */
    uint32_t calculateInterval() const;
    
    /// Pattern d'arpégiation
    ArpPattern pattern_;
    
    /// Vitesse (notes par beat)
    uint8_t rate_;
    
    /// Nombre d'octaves
    uint8_t octaves_;
    
    /// Tempo en BPM
    float tempo_;
    
    /// Gate (durée de note)
    float gate_;
    
    /// Notes enfoncées (buffer)
    std::set<uint8_t> heldNotes_;
    
    /// Mutex pour protéger heldNotes_
    mutable std::mutex notesMutex_;
    
    /// Thread d'arpégiation
    std::thread arpThread_;
    
    /// Flag d'arrêt
    std::atomic<bool> running_;
    
    /// Index de position dans la séquence
    size_t sequencePosition_;
    
    /// Canal MIDI de sortie
    uint8_t outputChannel_;
    
    /// Vélocité de sortie
    uint8_t outputVelocity_;
    
    /// Callback de sortie
    NoteOutputCallback noteOutputCallback_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ArpeggiatorProcessor.h
// ============================================================================