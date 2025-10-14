// ============================================================================
// Fichier: backend/src/midi/MidiTransforms.h
// Version: 3.1.0
// Date: 2025-10-10
// Projet: MidiMind v3.1 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Classe utilitaire pour transformations musicales sur données MIDI.
//   Fournit des algorithmes pour quantization, transposition, humanisation,
//   et autres opérations d'édition avancées.
//
// Fonctionnalités:
//   - Quantization rythmique (grille 4, 8, 16, 32, 64)
//   - Transposition (±semitones avec clamp)
//   - Scale velocity (facteur multiplicatif ou offset)
//   - Humanisation (variations aléatoires contrôlées)
//   - Manipulation timing et durées
//
// Utilisation:
//   MidiTransforms::quantize(jsonMidi, noteIds, division, strength);
//   MidiTransforms::transpose(jsonMidi, noteIds, semitones);
//
// Auteur: MidiMind Team
// Statut: ✅ PHASE 2 - COMPLET
// ============================================================================

#ifndef MIDIMIND_MIDI_TRANSFORMS_H
#define MIDIMIND_MIDI_TRANSFORMS_H

#include <string>
#include <vector>
#include <set>
#include <random>
#include "../core/json.hpp"

namespace midiMind {

using json = nlohmann::json;

// ============================================================================
// CLASSE: MidiTransforms
// Transformations musicales stateless
// ============================================================================
class MidiTransforms {
public:
    // ========================================================================
    // QUANTIZATION
    // ========================================================================
    
    /**
     * @brief Quantize les notes sur une grille rythmique
     * 
     * @param jsonMidi Données JsonMidi à modifier
     * @param noteIds Liste des IDs de notes à quantize (vide = toutes)
     * @param division Division rythmique (4, 8, 16, 32, 64)
     * @param strength Force de quantization (0.0 à 1.0)
     * @param ppq Ticks per quarter note (par défaut: 480)
     * @return int Nombre de notes quantizées
     * 
     * @note strength=1.0 : snap complet, strength=0.5 : 50% vers grille
     * @note Modifie directement jsonMidi
     */
    static int quantize(json& jsonMidi,
                       const std::vector<std::string>& noteIds,
                       int division,
                       float strength = 1.0f,
                       int ppq = 480);
    
    /**
     * @brief Calcule la position quantizée sur la grille
     * 
     * @param time Position originale (ticks)
     * @param gridSize Taille de la grille (ticks)
     * @param strength Force (0.0 à 1.0)
     * @return int Position quantizée
     */
    static int quantizeTime(int time, int gridSize, float strength = 1.0f);
    
    // ========================================================================
    // TRANSPOSITION
    // ========================================================================
    
    /**
     * @brief Transpose les notes de N semitones
     * 
     * @param jsonMidi Données JsonMidi à modifier
     * @param noteIds Liste des IDs de notes à transposer (vide = toutes)
     * @param semitones Nombre de semitones (positif = up, négatif = down)
     * @return int Nombre de notes transposées
     * 
     * @note Clamp automatique entre 0 et 127
     * @note Les notes hors range ne sont pas transposées
     */
    static int transpose(json& jsonMidi,
                        const std::vector<std::string>& noteIds,
                        int semitones);
    
    /**
     * @brief Transpose une note avec clamp
     * 
     * @param note Valeur note MIDI (0-127)
     * @param semitones Transposition
     * @param clamped [out] true si clampé
     * @return int Nouvelle valeur note
     */
    static int transposeNote(int note, int semitones, bool* clamped = nullptr);
    
    // ========================================================================
    // VELOCITY
    // ========================================================================
    
    /**
     * @brief Scale les vélocités par un facteur
     * 
     * @param jsonMidi Données JsonMidi à modifier
     * @param noteIds Liste des IDs de notes (vide = toutes)
     * @param factor Facteur multiplicatif (0.1 à 2.0)
     * @return int Nombre de notes modifiées
     * 
     * @note Clamp automatique entre 1 et 127
     * @note factor > 1.0 : augmente, factor < 1.0 : diminue
     */
    static int scaleVelocity(json& jsonMidi,
                            const std::vector<std::string>& noteIds,
                            float factor);
    
    /**
     * @brief Ajoute un offset aux vélocités
     * 
     * @param jsonMidi Données JsonMidi à modifier
     * @param noteIds Liste des IDs de notes (vide = toutes)
     * @param offset Offset à ajouter (-64 à +64)
     * @return int Nombre de notes modifiées
     * 
     * @note Clamp automatique entre 1 et 127
     */
    static int offsetVelocity(json& jsonMidi,
                             const std::vector<std::string>& noteIds,
                             int offset);
    
    /**
     * @brief Scale une vélocité avec clamp
     * 
     * @param velocity Vélocité originale (0-127)
     * @param factor Facteur ou offset
     * @param useOffset Si true, factor est un offset
     * @return int Nouvelle vélocité
     */
    static int scaleVelocityValue(int velocity, float factor, bool useOffset = false);
    
    // ========================================================================
    // HUMANISATION
    // ========================================================================
    
    /**
     * @brief Humanise les notes (variations aléatoires)
     * 
     * @param jsonMidi Données JsonMidi à modifier
     * @param noteIds Liste des IDs de notes (vide = toutes)
     * @param timingVarianceMs Variance timing en ms (±)
     * @param velocityVariance Variance vélocité (±)
     * @param seed Seed pour random (0 = aléatoire)
     * @param ppq Ticks per quarter note
     * @param tempo BPM pour conversion ms → ticks
     * @return int Nombre de notes humanisées
     * 
     * @note Ajoute des variations aléatoires dans les limites
     * @note Seed permet reproductibilité
     */
    static int humanize(json& jsonMidi,
                       const std::vector<std::string>& noteIds,
                       int timingVarianceMs,
                       int velocityVariance,
                       unsigned int seed = 0,
                       int ppq = 480,
                       int tempo = 120);
    
    // ========================================================================
    // DURÉES
    // ========================================================================
    
    /**
     * @brief Scale les durées de notes
     * 
     * @param jsonMidi Données JsonMidi à modifier
     * @param noteIds Liste des IDs de notes (vide = toutes)
     * @param factor Facteur multiplicatif (0.1 à 4.0)
     * @return int Nombre de notes modifiées
     * 
     * @note factor > 1.0 : allonge, factor < 1.0 : raccourcit
     */
    static int scaleDuration(json& jsonMidi,
                            const std::vector<std::string>& noteIds,
                            float factor);
    
    /**
     * @brief Définit une durée fixe pour toutes les notes
     * 
     * @param jsonMidi Données JsonMidi à modifier
     * @param noteIds Liste des IDs de notes (vide = toutes)
     * @param duration Durée en ticks
     * @return int Nombre de notes modifiées
     */
    static int setDuration(json& jsonMidi,
                          const std::vector<std::string>& noteIds,
                          int duration);
    
    // ========================================================================
    // TIMING
    // ========================================================================
    
    /**
     * @brief Déplace les notes dans le temps
     * 
     * @param jsonMidi Données JsonMidi à modifier
     * @param noteIds Liste des IDs de notes (vide = toutes)
     * @param deltaTime Offset temporel (ticks)
     * @return int Nombre de notes déplacées
     * 
     * @note Les temps négatifs sont clampés à 0
     */
    static int moveNotes(json& jsonMidi,
                        const std::vector<std::string>& noteIds,
                        int deltaTime);
    
    /**
     * @brief Déplace et transpose les notes
     * 
     * @param jsonMidi Données JsonMidi à modifier
     * @param noteIds Liste des IDs de notes (vide = toutes)
     * @param deltaTime Offset temporel (ticks)
     * @param deltaPitch Transposition (semitones)
     * @return int Nombre de notes déplacées
     */
    static int moveAndTranspose(json& jsonMidi,
                               const std::vector<std::string>& noteIds,
                               int deltaTime,
                               int deltaPitch);
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Trouve un event par ID dans le JsonMidi
     * 
     * @param jsonMidi Données JsonMidi
     * @param noteId ID de la note à trouver
     * @return json* Pointeur vers l'event ou nullptr
     */
    static json* findEventById(json& jsonMidi, const std::string& noteId);
    
    /**
     * @brief Trouve tous les events correspondant aux IDs
     * 
     * @param jsonMidi Données JsonMidi
     * @param noteIds Liste des IDs
     * @return std::vector<json*> Pointeurs vers les events
     */
    static std::vector<json*> findEventsByIds(json& jsonMidi,
                                               const std::vector<std::string>& noteIds);
    
    /**
     * @brief Trouve toutes les notes (noteOn) dans le JsonMidi
     * 
     * @param jsonMidi Données JsonMidi
     * @return std::vector<json*> Pointeurs vers les notes
     */
    static std::vector<json*> findAllNotes(json& jsonMidi);
    
    /**
     * @brief Convertit millisecondes → ticks
     * 
     * @param ms Temps en millisecondes
     * @param ppq Ticks per quarter note
     * @param tempo BPM
     * @return int Ticks
     */
    static int msToTicks(int ms, int ppq, int tempo);
    
    /**
     * @brief Convertit ticks → millisecondes
     * 
     * @param ticks Temps en ticks
     * @param ppq Ticks per quarter note
     * @param tempo BPM
     * @return int Millisecondes
     */
    static int ticksToMs(int ticks, int ppq, int tempo);
    
    /**
     * @brief Clamp une valeur entre min et max
     */
    template<typename T>
    static T clamp(T value, T min, T max) {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }
    
private:
    // ========================================================================
    // RANDOM NUMBER GENERATOR (thread-local)
    // ========================================================================
    
    static thread_local std::mt19937 rng_;
    
    /**
     * @brief Initialise le RNG avec un seed
     */
    static void initRng(unsigned int seed);
    
    /**
     * @brief Génère un nombre aléatoire entre min et max
     */
    static int randomInt(int min, int max);
    
    /**
     * @brief Génère un float aléatoire entre min et max
     */
    static float randomFloat(float min, float max);
};

} // namespace midiMind

#endif // MIDIMIND_MIDI_TRANSFORMS_H

// ============================================================================
// FIN DU FICHIER MidiTransforms.h
// ============================================================================
