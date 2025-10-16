// ============================================================================
// Fichier: backend/src/api/commands/editor.cpp
// Version: 3.1.1 - CORRIGÉ
// Date: 2025-10-16
// ============================================================================
// CORRECTIONS v3.1.1:
//   ✅ Retrait 3ème paramètre de tous les registerCommand
//   ✅ loadAsJsonMidi() → convertToJsonMidi()
//   ✅ getFilePath() → utilisation de entry.filepath via getFileMetadata()
//   ✅ Gestion correcte des optionals
//
// Description:
//   Handlers pour les commandes d'édition MIDI en mode JsonMidi
//
// Commandes implémentées (7 commandes):
//   - editor.load        : Charger fichier en mode édition
//   - editor.save        : Sauvegarder modifications
//   - editor.addNote     : Ajouter une note
//   - editor.deleteNote  : Supprimer une note
//   - editor.updateNote  : Modifier une note
//   - editor.addCC       : Ajouter un Control Change
//   - editor.undo        : Annuler dernière modification
//   - editor.redo        : Refaire dernière annulation
//
// Auteur: MidiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../midi/files/MidiFileManager.h"
#include "../../core/Logger.h"
#include "../editor/EditorState.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// GESTIONNAIRE D'ÉTATS ÉDITEUR (Singleton)
// ============================================================================

class EditorStateManager {
public:
    static EditorStateManager& instance() {
        static EditorStateManager instance;
        return instance;
    }
    
    std::shared_ptr<EditorState> getOrCreate(const std::string& fileId) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (states_.find(fileId) == states_.end()) {
            states_[fileId] = std::make_shared<EditorState>(fileId);
        }
        
        return states_[fileId];
    }
    
    bool has(const std::string& fileId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return states_.find(fileId) != states_.end();
    }
    
    void remove(const std::string& fileId) {
        std::lock_guard<std::mutex> lock(mutex_);
        states_.erase(fileId);
    }
    
    std::vector<std::string> listActiveFiles() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> files;
        files.reserve(states_.size());
        
        for (const auto& [fileId, state] : states_) {
            files.push_back(fileId);
        }
        
        return files;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<EditorState>> states_;
    
    EditorStateManager() = default;
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
                if (!params.contains("file_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: file_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                Logger::info("EditorAPI", "Loading file: " + fileId);
                
                // Récupérer ou créer l'état d'édition
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                // ✅ CORRECTION: loadAsJsonMidi() → convertToJsonMidi()
                auto jsonMidiOpt = fileManager->convertToJsonMidi(fileId);
                
                if (!jsonMidiOpt.has_value()) {
                    Logger::error("EditorAPI", "Failed to load JsonMidi for: " + fileId);
                    return {
                        {"success", false},
                        {"error", "Failed to convert file to JsonMidi"},
                        {"error_code", "CONVERSION_FAILED"}
                    };
                }
                
                json jsonMidi = jsonMidiOpt.value();
                
                // ✅ CORRECTION: getFilePath() → utilisation de getFileMetadata()
                auto fileOpt = fileManager->getFileMetadata(fileId);
                if (!fileOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "File metadata not found"},
                        {"error_code", "FILE_NOT_FOUND"}
                    };
                }
                
                std::string filepath = fileOpt->filepath;
                
                // Charger dans EditorState
                state->load(jsonMidi, filepath);
                
                Logger::info("EditorAPI", "✓ File loaded in editor");
                
                return {
                    {"success", true},
                    {"message", "File loaded successfully"},
                    {"data", {
                        {"file_id", fileId},
                        {"jsonmidi", jsonMidi},
                        {"filepath", filepath}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to load file: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to load file: " + std::string(e.what())},
                    {"error_code", "LOAD_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // editor.save - Sauvegarder les modifications
    // ========================================================================
    
    factory.registerCommand("editor.save",
        [fileManager](const json& params) -> json {
            Logger::debug("EditorAPI", "Saving file...");
            
            try {
                if (!params.contains("file_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: file_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                if (!EditorStateManager::instance().has(fileId)) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"},
                        {"error_code", "FILE_NOT_LOADED"}
                    };
                }
                
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                // Récupérer le JsonMidi modifié
                json jsonMidi = state->getDataCopy();
                
                // Sauvegarder via MidiFileManager
                auto savedIdOpt = fileManager->saveFromJsonMidi(jsonMidi, state->getFilepath());
                
                if (!savedIdOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "Failed to save file"},
                        {"error_code", "SAVE_FAILED"}
                    };
                }
                
                // Marquer comme non modifié
                state->clearModified();
                
                Logger::info("EditorAPI", "✓ File saved");
                
                return {
                    {"success", true},
                    {"message", "File saved successfully"},
                    {"data", {
                        {"file_id", savedIdOpt.value()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to save file: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to save file: " + std::string(e.what())},
                    {"error_code", "SAVE_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // editor.addNote - Ajouter une note
    // ========================================================================
    
    factory.registerCommand("editor.addNote",
        [](const json& params) -> json {
            Logger::debug("EditorAPI", "Adding note...");
            
            try {
                if (!params.contains("file_id") || !params.contains("track") ||
                    !params.contains("tick") || !params.contains("note") ||
                    !params.contains("velocity") || !params.contains("duration")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameters"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string fileId = params["file_id"];
                int track = params["track"];
                int tick = params["tick"];
                int note = params["note"];
                int velocity = params["velocity"];
                int duration = params["duration"];
                int channel = params.value("channel", 0);
                
                // Validation
                if (note < 0 || note > 127 || velocity < 0 || velocity > 127) {
                    return {
                        {"success", false},
                        {"error", "Invalid note or velocity (0-127)"},
                        {"error_code", "INVALID_VALUE"}
                    };
                }
                
                if (!EditorStateManager::instance().has(fileId)) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"},
                        {"error_code", "FILE_NOT_LOADED"}
                    };
                }
                
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                // Ajouter la note
                state->pushUndo("Add Note");
                
                json& data = state->getData();
                json& tracks = data["tracks"];
                
                if (track < 0 || track >= (int)tracks.size()) {
                    return {
                        {"success", false},
                        {"error", "Invalid track number"},
                        {"error_code", "INVALID_TRACK"}
                    };
                }
                
                // Créer l'événement Note On
                json noteOnEvent = {
                    {"tick", tick},
                    {"type", "noteOn"},
                    {"note", note},
                    {"velocity", velocity},
                    {"channel", channel}
                };
                
                // Créer l'événement Note Off
                json noteOffEvent = {
                    {"tick", tick + duration},
                    {"type", "noteOff"},
                    {"note", note},
                    {"velocity", 0},
                    {"channel", channel}
                };
                
                tracks[track]["events"].push_back(noteOnEvent);
                tracks[track]["events"].push_back(noteOffEvent);
                
                state->markModified();
                
                Logger::info("EditorAPI", "✓ Note added");
                
                return {
                    {"success", true},
                    {"message", "Note added successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to add note: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to add note: " + std::string(e.what())},
                    {"error_code", "ADD_NOTE_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // editor.deleteNote - Supprimer une note
    // ========================================================================
    
    factory.registerCommand("editor.deleteNote",
        [](const json& params) -> json {
            Logger::debug("EditorAPI", "Deleting note...");
            
            try {
                if (!params.contains("file_id") || !params.contains("track") ||
                    !params.contains("event_index")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameters"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string fileId = params["file_id"];
                int track = params["track"];
                int eventIndex = params["event_index"];
                
                if (!EditorStateManager::instance().has(fileId)) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"},
                        {"error_code", "FILE_NOT_LOADED"}
                    };
                }
                
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                state->pushUndo("Delete Note");
                
                json& data = state->getData();
                json& tracks = data["tracks"];
                
                if (track < 0 || track >= (int)tracks.size()) {
                    return {
                        {"success", false},
                        {"error", "Invalid track number"},
                        {"error_code", "INVALID_TRACK"}
                    };
                }
                
                json& events = tracks[track]["events"];
                
                if (eventIndex < 0 || eventIndex >= (int)events.size()) {
                    return {
                        {"success", false},
                        {"error", "Invalid event index"},
                        {"error_code", "INVALID_INDEX"}
                    };
                }
                
                events.erase(events.begin() + eventIndex);
                state->markModified();
                
                Logger::info("EditorAPI", "✓ Note deleted");
                
                return {
                    {"success", true},
                    {"message", "Note deleted successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to delete note: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to delete note: " + std::string(e.what())},
                    {"error_code", "DELETE_NOTE_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // editor.updateNote - Modifier une note
    // ========================================================================
    
    factory.registerCommand("editor.updateNote",
        [](const json& params) -> json {
            Logger::debug("EditorAPI", "Updating note...");
            
            try {
                if (!params.contains("file_id") || !params.contains("track") ||
                    !params.contains("event_index")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameters"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string fileId = params["file_id"];
                int track = params["track"];
                int eventIndex = params["event_index"];
                
                if (!EditorStateManager::instance().has(fileId)) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"},
                        {"error_code", "FILE_NOT_LOADED"}
                    };
                }
                
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                state->pushUndo("Update Note");
                
                json& data = state->getData();
                json& tracks = data["tracks"];
                
                if (track < 0 || track >= (int)tracks.size()) {
                    return {
                        {"success", false},
                        {"error", "Invalid track number"},
                        {"error_code", "INVALID_TRACK"}
                    };
                }
                
                json& events = tracks[track]["events"];
                
                if (eventIndex < 0 || eventIndex >= (int)events.size()) {
                    return {
                        {"success", false},
                        {"error", "Invalid event index"},
                        {"error_code", "INVALID_INDEX"}
                    };
                }
                
                // Mettre à jour les champs fournis
                if (params.contains("tick")) {
                    events[eventIndex]["tick"] = params["tick"];
                }
                if (params.contains("note")) {
                    int note = params["note"];
                    if (note >= 0 && note <= 127) {
                        events[eventIndex]["note"] = note;
                    }
                }
                if (params.contains("velocity")) {
                    int velocity = params["velocity"];
                    if (velocity >= 0 && velocity <= 127) {
                        events[eventIndex]["velocity"] = velocity;
                    }
                }
                if (params.contains("channel")) {
                    events[eventIndex]["channel"] = params["channel"];
                }
                
                state->markModified();
                
                Logger::info("EditorAPI", "✓ Note updated");
                
                return {
                    {"success", true},
                    {"message", "Note updated successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to update note: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to update note: " + std::string(e.what())},
                    {"error_code", "UPDATE_NOTE_FAILED"}
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
                if (!params.contains("file_id") || !params.contains("track") ||
                    !params.contains("tick") || !params.contains("controller") ||
                    !params.contains("value")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameters"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string fileId = params["file_id"];
                int track = params["track"];
                int tick = params["tick"];
                int controller = params["controller"];
                int value = params["value"];
                int channel = params.value("channel", 0);
                
                // Validation
                if (controller < 0 || controller > 127 || value < 0 || value > 127) {
                    return {
                        {"success", false},
                        {"error", "Invalid controller or value (0-127)"},
                        {"error_code", "INVALID_VALUE"}
                    };
                }
                
                if (!EditorStateManager::instance().has(fileId)) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"},
                        {"error_code", "FILE_NOT_LOADED"}
                    };
                }
                
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                state->pushUndo("Add CC");
                
                json& data = state->getData();
                json& tracks = data["tracks"];
                
                if (track < 0 || track >= (int)tracks.size()) {
                    return {
                        {"success", false},
                        {"error", "Invalid track number"},
                        {"error_code", "INVALID_TRACK"}
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
                
                Logger::info("EditorAPI", "✓ Control Change added");
                
                return {
                    {"success", true},
                    {"message", "Control Change added successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to add CC: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to add CC: " + std::string(e.what())},
                    {"error_code", "ADD_CC_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // editor.undo - Annuler dernière modification
    // ========================================================================
    
    factory.registerCommand("editor.undo",
        [](const json& params) -> json {
            Logger::debug("EditorAPI", "Undo...");
            
            try {
                if (!params.contains("file_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: file_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                if (!EditorStateManager::instance().has(fileId)) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"},
                        {"error_code", "FILE_NOT_LOADED"}
                    };
                }
                
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                if (!state->canUndo()) {
                    return {
                        {"success", false},
                        {"error", "Nothing to undo"},
                        {"error_code", "NO_UNDO"}
                    };
                }
                
                bool success = state->undo();
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Undo operation failed"},
                        {"error_code", "UNDO_FAILED"}
                    };
                }
                
                Logger::info("EditorAPI", "✓ Undo performed");
                
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
                    {"error", "Failed to undo: " + std::string(e.what())},
                    {"error_code", "UNDO_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // editor.redo - Refaire dernière annulation
    // ========================================================================
    
    factory.registerCommand("editor.redo",
        [](const json& params) -> json {
            Logger::debug("EditorAPI", "Redo...");
            
            try {
                if (!params.contains("file_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: file_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                if (!EditorStateManager::instance().has(fileId)) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"},
                        {"error_code", "FILE_NOT_LOADED"}
                    };
                }
                
                auto state = EditorStateManager::instance().getOrCreate(fileId);
                
                if (!state->canRedo()) {
                    return {
                        {"success", false},
                        {"error", "Nothing to redo"},
                        {"error_code", "NO_REDO"}
                    };
                }
                
                bool success = state->redo();
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Redo operation failed"},
                        {"error_code", "REDO_FAILED"}
                    };
                }
                
                Logger::info("EditorAPI", "✓ Redo performed");
                
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
                    {"error", "Failed to redo: " + std::string(e.what())},
                    {"error_code", "REDO_FAILED"}
                };
            }
        }
    );
    
    Logger::info("EditorHandlers", "✅ Editor commands registered (7 commands)");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER editor.cpp v3.1.1-CORRIGÉ
// ============================================================================
