// ============================================================================
// Fichier: backend/src/api/commands/editor.cpp
// Version: 3.1.0-corrections
// Date: 2025-10-15
// ============================================================================
// Description:
//   Handlers pour les commandes d'édition MIDI.
//   Édition de fichiers MIDI au format JsonMidi.
//
// CORRECTIONS v3.1.0:
//   ✅ Suppression de la struct EditorState locale obsolète
//   ✅ Utilisation de la vraie classe EditorState via EditorStateManager
//   ✅ Cohérence avec EditorState.h/.cpp existant
//   ✅ Gestion propre des états multiples avec singleton
//   ✅ Format de retour harmonisé
//   ✅ Logging amélioré
//
// Commandes implémentées (7 commandes):
//   - editor.load      : Charger un fichier en JsonMidi pour édition
//   - editor.save      : Sauvegarder les modifications
//   - editor.addNote   : Ajouter une note MIDI
//   - editor.deleteNote : Supprimer des notes
//   - editor.addCC     : Ajouter un Control Change
//   - editor.undo      : Annuler la dernière action
//   - editor.redo      : Refaire une action annulée
//
// Auteur: MidiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../midi/MidiFileManager.h"
#include "../editor/EditorState.h"  
#include "../../core/Logger.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <unordered_map>
#include <mutex>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// GESTIONNAIRE D'ÉTATS D'ÉDITION (Singleton)
// ============================================================================

/**
 * @class EditorStateManager
 * @brief Gestionnaire centralisé des états d'édition par fichier
 * 
 * Maintient une map des EditorState actifs, un par fichier ouvert.
 * Thread-safe avec mutex interne.
 */
class EditorStateManager {
private:
    std::unordered_map<std::string, std::shared_ptr<EditorState>> states_;
    mutable std::mutex mutex_;
    
    // Constructeur privé (Singleton)
    EditorStateManager() = default;
    
public:
    // Désactiver copie
    EditorStateManager(const EditorStateManager&) = delete;
    EditorStateManager& operator=(const EditorStateManager&) = delete;
    
    /**
     * @brief Accès à l'instance unique
     */
    static EditorStateManager& instance() {
        static EditorStateManager manager;
        return manager;
    }
    
    /**
     * @brief Récupère ou crée un état pour un fichier
     * 
     * @param fileId ID unique du fichier
     * @return Pointeur partagé vers EditorState
     */
    std::shared_ptr<EditorState> getOrCreate(const std::string& fileId) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = states_.find(fileId);
        if (it != states_.end()) {
            return it->second;
        }
        
        // Créer nouvel état
        auto state = std::make_shared<EditorState>();
        states_[fileId] = state;
        
        Logger::debug("EditorStateManager", 
            "Created new EditorState for: " + fileId);
        
        return state;
    }
    
    /**
     * @brief Vérifie si un état existe pour un fichier
     */
    bool has(const std::string& fileId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return states_.find(fileId) != states_.end();
    }
    
    /**
     * @brief Supprime l'état d'un fichier
     * 
     * @param fileId ID du fichier
     * @param saveIfModified Sauvegarder avant suppression si modifié
     */
    void remove(const std::string& fileId, bool saveIfModified = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = states_.find(fileId);
        if (it != states_.end()) {
            if (saveIfModified && it->second->isModified()) {
                // Unlock temporairement pour save()
                mutex_.unlock();
                it->second->save();
                mutex_.lock();
            }
            
            states_.erase(it);
            
            Logger::debug("EditorStateManager", 
                "Removed EditorState for: " + fileId);
        }
    }
    
    /**
     * @brief Nombre d'états actifs
     */
    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return states_.size();
    }
    
    /**
     * @brief Liste des fichiers actuellement en édition
     */
    std::vector<std::string> listActiveFiles() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> files;
        files.reserve(states_.size());
        
        for (const auto& [fileId, state] : states_) {
            files.push_back(fileId);
        }
        
        return files;
    }
};

// ============================================================================
// FONCTION: registerEditorCommands()
// Enregistre toutes les commandes d'édition (7 commandes)
// ============================================================================

void registerEditorCommands(CommandFactory& factory, 
                            std::shared_ptr<MidiFileManager> fileManager) {
    
    Logger::info("EditorHandlers", "Registering editor commands...");
    
    // ========================================================================
    // editor.load - Charger un fichier en mode édition
    // ========================================================================
    
    factory.registerCommand("editor.load",
        [fileManager](const json& params) -> json {
            Logger::debug("EditorAPI", "Loading file for editing...");
            
            try {
                // Validation
                if (!params.contains("file_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: file_id"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                Logger::info("EditorAPI", "Loading file: " + fileId);
                
                // Récupérer ou créer l'état d'édition
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                // Charger le fichier via MidiFileManager
                auto jsonMidiOpt = fileManager->loadAsJsonMidi(fileId);
                
                if (!jsonMidiOpt) {
                    Logger::error("EditorAPI", "Failed to load JsonMidi for: " + fileId);
                    return {
                        {"success", false},
                        {"error", "Failed to load file"}
                    };
                }
                
                json jsonMidi = *jsonMidiOpt;
                
                // Obtenir le chemin du fichier
                std::string filepath = fileManager->getFilePath(fileId);
                
                // Charger dans EditorState
                state->load(fileId, filepath, jsonMidi);
                
                Logger::info("EditorAPI", "✓ File loaded successfully: " + fileId);
                
                return {
                    {"success", true},
                    {"data", {
                        {"file_id", fileId},
                        {"filepath", filepath},
                        {"jsonmidi", jsonMidi}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to load file: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to load file: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // editor.save - Sauvegarder les modifications
    // ========================================================================
    
    factory.registerCommand("editor.save",
        [](const json& params) -> json {
            Logger::debug("EditorAPI", "Saving file...");
            
            try {
                // Validation
                if (!params.contains("file_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: file_id"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                // Vérifier que le fichier est chargé
                if (!EditorStateManager::instance().has(fileId)) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"}
                    };
                }
                
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                // Vérifier si modifié
                if (!state->isModified()) {
                    Logger::debug("EditorAPI", "File not modified, skip save");
                    return {
                        {"success", true},
                        {"message", "File not modified (nothing to save)"}
                    };
                }
                
                Logger::info("EditorAPI", "Saving file: " + fileId);
                
                // Sauvegarder via EditorState (qui utilise MidiFileManager)
                bool success = state->save();
                
                if (!success) {
                    Logger::error("EditorAPI", "Save failed for: " + fileId);
                    return {
                        {"success", false},
                        {"error", "Failed to save file"}
                    };
                }
                
                Logger::info("EditorAPI", "✓ File saved successfully: " + fileId);
                
                return {
                    {"success", true},
                    {"message", "File saved successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to save: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to save: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // editor.addNote - Ajouter une note MIDI
    // ========================================================================
    
    factory.registerCommand("editor.addNote",
        [](const json& params) -> json {
            Logger::debug("EditorAPI", "Adding note...");
            
            try {
                // Validation des paramètres requis
                std::vector<std::string> required = {
                    "file_id", "track", "tick", "note", "velocity", "duration"
                };
                
                for (const auto& field : required) {
                    if (!params.contains(field)) {
                        return {
                            {"success", false},
                            {"error", "Missing required parameter: " + field}
                        };
                    }
                }
                
                std::string fileId = params["file_id"];
                int track = params["track"];
                int tick = params["tick"];
                int note = params["note"];
                int velocity = params["velocity"];
                int duration = params["duration"];
                int channel = params.value("channel", 0);
                
                // Validation des valeurs
                if (note < 0 || note > 127 || velocity < 0 || velocity > 127) {
                    return {
                        {"success", false},
                        {"error", "Invalid note or velocity value (0-127)"}
                    };
                }
                
                if (duration <= 0) {
                    return {
                        {"success", false},
                        {"error", "Duration must be positive"}
                    };
                }
                
                // Récupérer l'état d'édition
                if (!EditorStateManager::instance().has(fileId)) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"}
                    };
                }
                
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                // ✅ Sauvegarder l'état actuel pour undo
                state->pushUndo("Add note");
                
                // Modifier le JsonMidi
                json& data = state->getData();
                json& tracks = data["tracks"];
                
                // Validation du numéro de track
                if (track < 0 || track >= (int)tracks.size()) {
                    return {
                        {"success", false},
                        {"error", "Invalid track number"}
                    };
                }
                
                // Créer les événements Note On et Note Off
                json noteOn = {
                    {"tick", tick},
                    {"type", "noteOn"},
                    {"note", note},
                    {"velocity", velocity},
                    {"channel", channel}
                };
                
                json noteOff = {
                    {"tick", tick + duration},
                    {"type", "noteOff"},
                    {"note", note},
                    {"velocity", 0},
                    {"channel", channel}
                };
                
                // Ajouter aux événements du track
                tracks[track]["events"].push_back(noteOn);
                tracks[track]["events"].push_back(noteOff);
                
                // Marquer comme modifié
                state->markModified();
                
                Logger::info("EditorAPI", 
                    "Note added: " + std::to_string(note) + 
                    " at tick " + std::to_string(tick));
                
                return {
                    {"success", true},
                    {"message", "Note added successfully"},
                    {"data", {
                        {"track", track},
                        {"tick", tick},
                        {"note", note},
                        {"velocity", velocity},
                        {"duration", duration}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to add note: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to add note: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // editor.deleteNote - Supprimer des notes
    // ========================================================================
    
    factory.registerCommand("editor.deleteNote",
        [](const json& params) -> json {
            Logger::debug("EditorAPI", "Deleting note(s)...");
            
            try {
                // Validation
                if (!params.contains("file_id") || !params.contains("track")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameters"}
                    };
                }
                
                std::string fileId = params["file_id"];
                int track = params["track"];
                
                // Paramètres optionnels de filtre
                int tick = params.value("tick", -1);
                int note = params.value("note", -1);
                
                // Récupérer l'état d'édition
                if (!EditorStateManager::instance().has(fileId)) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"}
                    };
                }
                
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                // Sauvegarder pour undo
                state->pushUndo("Delete notes");
                
                json& data = state->getData();
                json& tracks = data["tracks"];
                
                // Validation du track
                if (track < 0 || track >= (int)tracks.size()) {
                    return {
                        {"success", false},
                        {"error", "Invalid track number"}
                    };
                }
                
                // Supprimer les notes correspondantes
                json& events = tracks[track]["events"];
                json newEvents = json::array();
                int deletedCount = 0;
                
                for (const auto& event : events) {
                    bool shouldDelete = false;
                    
                    if (event["type"] == "noteOn" || event["type"] == "noteOff") {
                        bool matchTick = (tick < 0 || event["tick"] == tick);
                        bool matchNote = (note < 0 || event["note"] == note);
                        shouldDelete = matchTick && matchNote;
                    }
                    
                    if (!shouldDelete) {
                        newEvents.push_back(event);
                    } else {
                        deletedCount++;
                    }
                }
                
                tracks[track]["events"] = newEvents;
                state->markModified();
                
                Logger::info("EditorAPI", 
                    "Deleted " + std::to_string(deletedCount) + " note events");
                
                return {
                    {"success", true},
                    {"message", "Notes deleted successfully"},
                    {"deleted_count", deletedCount}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to delete notes: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to delete notes: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // editor.addCC - Ajouter un Control Change
    // ========================================================================
    
    factory.registerCommand("editor.addCC",
        [](const json& params) -> json {
            Logger::debug("EditorAPI", "Adding Control Change...");
            
            try {
                // Validation
                std::vector<std::string> required = {
                    "file_id", "track", "tick", "controller", "value"
                };
                
                for (const auto& field : required) {
                    if (!params.contains(field)) {
                        return {
                            {"success", false},
                            {"error", "Missing required parameter: " + field}
                        };
                    }
                }
                
                std::string fileId = params["file_id"];
                int track = params["track"];
                int tick = params["tick"];
                int controller = params["controller"];
                int value = params["value"];
                int channel = params.value("channel", 0);
                
                // Validation des valeurs
                if (controller < 0 || controller > 127 || value < 0 || value > 127) {
                    return {
                        {"success", false},
                        {"error", "Invalid controller or value (0-127)"}
                    };
                }
                
                // Récupérer l'état
                if (!EditorStateManager::instance().has(fileId)) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"}
                    };
                }
                
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                state->pushUndo("Add CC");
                
                json& data = state->getData();
                json& tracks = data["tracks"];
                
                if (track < 0 || track >= (int)tracks.size()) {
                    return {
                        {"success", false},
                        {"error", "Invalid track number"}
                    };
                }
                
                // Créer l'événement CC
                json ccEvent = {
                    {"tick", tick},
                    {"type", "controller"},
                    {"controller", controller},
                    {"value", value},
                    {"channel", channel}
                };
                
                tracks[track]["events"].push_back(ccEvent);
                state->markModified();
                
                Logger::info("EditorAPI", "Control Change added: CC" + 
                    std::to_string(controller) + "=" + std::to_string(value));
                
                return {
                    {"success", true},
                    {"message", "Control Change added successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to add CC: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to add CC: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // editor.undo - Annuler la dernière action
    // ========================================================================
    
    factory.registerCommand("editor.undo",
        [](const json& params) -> json {
            Logger::debug("EditorAPI", "Undo...");
            
            try {
                if (!params.contains("file_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: file_id"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                if (!EditorStateManager::instance().has(fileId)) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"}
                    };
                }
                
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                // Vérifier si undo possible
                if (!state->canUndo()) {
                    return {
                        {"success", false},
                        {"error", "Nothing to undo"}
                    };
                }
                
                // Effectuer undo
                bool success = state->undo();
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Undo operation failed"}
                    };
                }
                
                Logger::info("EditorAPI", "Undo performed");
                
                return {
                    {"success", true},
                    {"message", "Undo performed successfully"},
                    {"data", {
                        {"jsonmidi", state->getDataCopy()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to undo: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to undo: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // editor.redo - Refaire une action annulée
    // ========================================================================
    
    factory.registerCommand("editor.redo",
        [](const json& params) -> json {
            Logger::debug("EditorAPI", "Redo...");
            
            try {
                if (!params.contains("file_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: file_id"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                if (!EditorStateManager::instance().has(fileId)) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"}
                    };
                }
                
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                // Vérifier si redo possible
                if (!state->canRedo()) {
                    return {
                        {"success", false},
                        {"error", "Nothing to redo"}
                    };
                }
                
                // Effectuer redo
                bool success = state->redo();
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Redo operation failed"}
                    };
                }
                
                Logger::info("EditorAPI", "Redo performed");
                
                return {
                    {"success", true},
                    {"message", "Redo performed successfully"},
                    {"data", {
                        {"jsonmidi", state->getDataCopy()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to redo: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to redo: " + std::string(e.what())}
                };
            }
        }
    );
    
    Logger::info("EditorHandlers", "✅ Editor commands registered (7 commands)");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER editor.cpp v3.1.0-corrections
// ============================================================================
