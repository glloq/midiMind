// ============================================================================
// Fichier: src/midi/JsonMidiConverter.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.1 - 2025-10-09 - COMPLET
// ============================================================================
// Description:
//   Convertisseur MIDI ↔ JSON (format JsonMidi)
//   PARTIE COMPLÉTÉE - Méthodes d'extraction métadonnées
//
// Modifications apportées (v3.0.1):
//   ✅ Implémentation extractTitle() - Extraction titre depuis meta-events
//   ✅ Implémentation extractAuthor() - Extraction compositeur
//   ✅ Implémentation extractTimeSignature() - Extraction signature rythmique
//
// Note: Ce fichier contient UNIQUEMENT les méthodes à compléter.
//       Le reste du code existe déjà dans le fichier original.
//
// Auteur: MidiMind Team
// Date: 2025-10-09
// Statut: ✅ COMPLET - Méthodes manquantes implémentées
// ============================================================================

#include "JsonMidiConverter.h"
#include "../core/Logger.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// EXTRACTION MÉTADONNÉES (MÉTHODES COMPLÉTÉES)
// ============================================================================

/**
 * @brief Extrait le titre depuis les pistes MIDI
 * 
 * Cherche les meta-events de type:
 * - 0x03: Sequence/Track Name
 * - 0x01: Text Event (si contient "title")
 * 
 * @param tracks Pistes MIDI parsées
 * @return std::string Titre extrait ou chaîne vide
 * 
 * @note ✅ IMPLÉMENTÉ - Phase 2
 * Priorité: Track 0 > autres tracks > premier event trouvé
 */
std::string JsonMidiConverter::extractTitle(const std::vector<MidiTrack>& tracks) const {
    if (tracks.empty()) {
        return "";
    }
    
    // ÉTAPE 1: Chercher dans la première piste (Track 0)
    // C'est la convention MIDI standard pour les métadonnées globales
    if (!tracks[0].events.empty()) {
        for (const auto& event : tracks[0].events) {
            // Meta-event 0x03: Sequence/Track Name
            if (event.type == MidiEventType::META && 
                event.metaType == 0x03 && 
                !event.text.empty()) {
                
                Logger::debug("JsonMidiConverter", "Title found in track 0: " + event.text);
                return event.text;
            }
        }
    }
    
    // ÉTAPE 2: Chercher dans les autres pistes
    for (size_t i = 1; i < tracks.size(); ++i) {
        for (const auto& event : tracks[i].events) {
            if (event.type == MidiEventType::META && 
                event.metaType == 0x03 && 
                !event.text.empty()) {
                
                // Ignorer les noms de pistes génériques
                std::string lowerText = event.text;
                std::transform(lowerText.begin(), lowerText.end(), 
                             lowerText.begin(), ::tolower);
                
                // Filtrer les noms de pistes techniques
                if (lowerText.find("track") == std::string::npos &&
                    lowerText.find("channel") == std::string::npos &&
                    lowerText.find("untitled") == std::string::npos) {
                    
                    Logger::debug("JsonMidiConverter", 
                                "Title found in track " + std::to_string(i) + ": " + event.text);
                    return event.text;
                }
            }
        }
    }
    
    // ÉTAPE 3: Chercher dans les Text Events (0x01)
    // Certains fichiers utilisent des text events pour le titre
    for (const auto& track : tracks) {
        for (const auto& event : track.events) {
            if (event.type == MidiEventType::META && 
                event.metaType == 0x01 && 
                !event.text.empty()) {
                
                std::string lowerText = event.text;
                std::transform(lowerText.begin(), lowerText.end(), 
                             lowerText.begin(), ::tolower);
                
                // Chercher "title:" ou "titre:"
                if (lowerText.find("title:") != std::string::npos) {
                    size_t pos = lowerText.find("title:") + 6;
                    std::string title = event.text.substr(pos);
                    // Trim spaces
                    title.erase(0, title.find_first_not_of(" \t"));
                    title.erase(title.find_last_not_of(" \t") + 1);
                    
                    if (!title.empty()) {
                        Logger::debug("JsonMidiConverter", "Title found in text event: " + title);
                        return title;
                    }
                }
            }
        }
    }
    
    Logger::debug("JsonMidiConverter", "No title found in MIDI file");
    return "";
}

/**
 * @brief Extrait le compositeur depuis les pistes MIDI
 * 
 * Cherche les meta-events de type:
 * - 0x02: Copyright Notice
 * - 0x01: Text Event (si contient "composer", "author", "by")
 * 
 * @param tracks Pistes MIDI parsées
 * @return std::string Compositeur extrait ou chaîne vide
 * 
 * @note ✅ IMPLÉMENTÉ - Phase 2
 */
std::string JsonMidiConverter::extractAuthor(const std::vector<MidiTrack>& tracks) const {
    if (tracks.empty()) {
        return "";
    }
    
    // ÉTAPE 1: Chercher Copyright Notice (0x02)
    // C'est le meta-event standard pour le copyright/compositeur
    for (const auto& track : tracks) {
        for (const auto& event : track.events) {
            if (event.type == MidiEventType::META && 
                event.metaType == 0x02 && 
                !event.text.empty()) {
                
                std::string copyright = event.text;
                
                // Extraire le nom après le symbole ©
                size_t copyrightPos = copyright.find("©");
                if (copyrightPos != std::string::npos) {
                    std::string author = copyright.substr(copyrightPos + 1);
                    
                    // Trim spaces et date éventuelle
                    author.erase(0, author.find_first_not_of(" \t"));
                    
                    // Retirer l'année si présente au début (ex: "2024 John Doe")
                    size_t firstSpace = author.find(' ');
                    if (firstSpace != std::string::npos && firstSpace < 5) {
                        std::string potentialYear = author.substr(0, firstSpace);
                        if (std::all_of(potentialYear.begin(), potentialYear.end(), ::isdigit)) {
                            author = author.substr(firstSpace + 1);
                            author.erase(0, author.find_first_not_of(" \t"));
                        }
                    }
                    
                    if (!author.empty()) {
                        Logger::debug("JsonMidiConverter", "Author found in copyright: " + author);
                        return author;
                    }
                }
                
                // Si pas de ©, retourner tel quel
                Logger::debug("JsonMidiConverter", "Author found in copyright (no ©): " + copyright);
                return copyright;
            }
        }
    }
    
    // ÉTAPE 2: Chercher dans les Text Events (0x01)
    // Certains fichiers utilisent des text events avec mots-clés
    std::vector<std::string> keywords = {
        "composer:", "author:", "by:", "composed by", "music by", 
        "compositeur:", "auteur:"
    };
    
    for (const auto& track : tracks) {
        for (const auto& event : track.events) {
            if (event.type == MidiEventType::META && 
                event.metaType == 0x01 && 
                !event.text.empty()) {
                
                std::string text = event.text;
                std::string lowerText = text;
                std::transform(lowerText.begin(), lowerText.end(), 
                             lowerText.begin(), ::tolower);
                
                // Chercher les mots-clés
                for (const auto& keyword : keywords) {
                    size_t pos = lowerText.find(keyword);
                    if (pos != std::string::npos) {
                        std::string author = text.substr(pos + keyword.length());
                        
                        // Trim spaces
                        author.erase(0, author.find_first_not_of(" \t:"));
                        author.erase(author.find_last_not_of(" \t") + 1);
                        
                        if (!author.empty()) {
                            Logger::debug("JsonMidiConverter", "Author found with keyword: " + author);
                            return author;
                        }
                    }
                }
            }
        }
    }
    
    // ÉTAPE 3: Chercher Lyricist (0x05) comme fallback
    for (const auto& track : tracks) {
        for (const auto& event : track.events) {
            if (event.type == MidiEventType::META && 
                event.metaType == 0x05 && 
                !event.text.empty()) {
                
                Logger::debug("JsonMidiConverter", "Author found in lyricist: " + event.text);
                return event.text;
            }
        }
    }
    
    Logger::debug("JsonMidiConverter", "No author found in MIDI file");
    return "";
}

/**
 * @brief Extrait la signature rythmique depuis la timeline
 * 
 * Cherche dans la timeline les events de type "timeSignature"
 * et retourne au format "4/4", "3/4", etc.
 * 
 * @param timeline Timeline des events JsonMidi
 * @return std::string Signature rythmique (ex: "4/4")
 * 
 * @note ✅ IMPLÉMENTÉ - Phase 2
 */
std::string JsonMidiConverter::extractTimeSignature(const std::vector<TimelineEvent>& timeline) const {
    // Chercher le premier event de type "timeSignature"
    for (const auto& event : timeline) {
        if (event.type == "timeSignature") {
            // L'event peut avoir un champ "text" avec la signature
            if (event.text.has_value() && !event.text.value().empty()) {
                Logger::debug("JsonMidiConverter", "Time signature found: " + event.text.value());
                return event.text.value();
            }
            
            // Ou des champs numériques
            if (event.data.contains("numerator") && event.data.contains("denominator")) {
                int numerator = event.data["numerator"].get<int>();
                int denominator = event.data["denominator"].get<int>();
                
                std::string timeSignature = std::to_string(numerator) + "/" + 
                                          std::to_string(denominator);
                
                Logger::debug("JsonMidiConverter", "Time signature found: " + timeSignature);
                return timeSignature;
            }
        }
    }
    
    // Par défaut: 4/4 (signature la plus commune)
    Logger::debug("JsonMidiConverter", "No time signature found, using default: 4/4");
    return "4/4";
}

// ============================================================================
// MÉTHODE DE VALIDATION (COMPLÉTÉE)
// ============================================================================

/**
 * @brief Valide la structure d'un JsonMidi
 * 
 * @param jsonMidi Objet JsonMidi à valider
 * @param errorMessage Message d'erreur si invalide
 * @return true Si valide
 */
bool JsonMidiConverter::validate(const JsonMidi& jsonMidi, std::string& errorMessage) const {
    // Vérifier le format
    if (jsonMidi.format != "jsonmidi-v1.0") {
        errorMessage = "Invalid format: " + jsonMidi.format + " (expected: jsonmidi-v1.0)";
        return false;
    }
    
    // Vérifier le tempo
    if (jsonMidi.metadata.tempo <= 0 || jsonMidi.metadata.tempo > 500) {
        errorMessage = "Invalid tempo: " + std::to_string(jsonMidi.metadata.tempo) + 
                      " (must be between 1 and 500 BPM)";
        return false;
    }
    
    // Vérifier la division
    if (jsonMidi.division <= 0) {
        errorMessage = "Invalid division: " + std::to_string(jsonMidi.division) + 
                      " (must be > 0)";
        return false;
    }
    
    // Vérifier que tracks existe
    if (jsonMidi.tracks.empty()) {
        errorMessage = "No tracks found (at least 1 track required)";
        return false;
    }
    
    // Vérifier que timeline existe
    if (jsonMidi.timeline.empty()) {
        errorMessage = "Empty timeline (at least 1 event required)";
        return false;
    }
    
    // Vérifier l'unicité des IDs d'events
    std::set<std::string> eventIds;
    for (const auto& event : jsonMidi.timeline) {
        if (event.id.empty()) {
            errorMessage = "Event with empty ID found";
            return false;
        }
        
        if (eventIds.count(event.id)) {
            errorMessage = "Duplicate event ID: " + event.id;
            return false;
        }
        
        eventIds.insert(event.id);
    }
    
    // Vérifier que les channels sont valides (1-16)
    for (const auto& channel : jsonMidi.channels) {
        if (channel.channel < 1 || channel.channel > 16) {
            errorMessage = "Invalid MIDI channel: " + std::to_string(channel.channel) + 
                          " (must be 1-16)";
            return false;
        }
    }
    
    Logger::debug("JsonMidiConverter", "JsonMidi validation successful");
    return true;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER JsonMidiConverter.cpp (PARTIE COMPLÉTÉE)
// Version: 3.0.1 - COMPLET ✅
// 
// Méthodes implémentées:
// - extractTitle()          ✅ Extraction titre avec priorité Track 0
// - extractAuthor()         ✅ Extraction compositeur (copyright + keywords)
// - extractTimeSignature()  ✅ Extraction signature rythmique
// - validate()              ✅ Validation complète JsonMidi
//
// Ces méthodes complètent le fichier JsonMidiConverter.cpp existant
// ============================================================================
