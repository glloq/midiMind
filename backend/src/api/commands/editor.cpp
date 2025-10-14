// ============================================================================
// Fichier: backend/src/api/commands/editor.cpp
// Version: 3.0.1 - NOUVEAU FICHIER
// Date: 2025-10-12
// ============================================================================
// Description:
//   Handlers pour les commandes d'édition MIDI.
//   Édition de fichiers MIDI au format JsonMidi.
//
// Commandes:
//   - editor.load      : Charger un fichier en JsonMidi pour édition
//   - editor.save      : Sauvegarder les modifications
//   - editor.addNote   : Ajouter une note MIDI
//   - editor.deleteNote : Supprimer des notes
//   - editor.modifyNote : Modifier une note existante
//   - editor.addCC     : Ajouter un Control Change
//   - editor.undo      : Annuler la dernière action
//   - editor.redo      : Refaire une action annulée
//
// Auteur: MidiMind Team
// ============================================================================

#include "../core/commands/CommandFactory.h"
#include "../midi/MidiFileManager.h"
#include "../core/Logger.h"
#include <nlohmann/json.hpp>
#include <stack>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// GESTION DE L'HISTORIQUE D'ÉDITION (pour undo/redo)
// ============================================================================

/**
 * @struct EditorState
 * @brief État d'édition pour un fichier
 */
struct EditorState {
    std::string fileId;
    json currentData;
    std::stack<json> undoStack;
    std::stack<json> redoStack;
    
    void pushUndo(const json& state) {
        undoStack.push(state);
        // Clear redo stack when new action
        while (!redoStack.empty()) redoStack.pop();
    }
};

// Map globale des états d'édition (en mémoire)
// TODO: Passer à un système plus robuste avec sessions
static std::unordered_map<std::string, EditorState> editorStates;
static std::mutex editorMutex;

// ============================================================================
// FONCTION: registerEditorCommands()
// Enregistre toutes les commandes d'édition (8 commandes)
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
                
                // Charger le fichier en JsonMidi
                auto jsonMidi = fileManager->loadAsJsonMidi(fileId);
                
                // Initialiser l'état d'édition
                std::lock_guard<std::mutex> lock(editorMutex);
                EditorState& state = editorStates[fileId];
                state.fileId = fileId;
                state.currentData = jsonMidi;
                // Clear undo/redo stacks
                while (!state.undoStack.empty()) state.undoStack.pop();
                while (!state.redoStack.empty()) state.redoStack.pop();
                
                Logger::info("EditorAPI", "File loaded for editing: " + fileId);
                
                return {
                    {"success", true},
                    {"message", "File loaded for editing"},
                    {"data", {
                        {"file_id", fileId},
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
        [fileManager](const json& params) -> json {
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
                
                // Récupérer l'état d'édition
                std::lock_guard<std::mutex> lock(editorMutex);
                auto it = editorStates.find(fileId);
                if (it == editorStates.end()) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor: " + fileId}
                    };
                }
                
                const json& jsonMidi = it->second.currentData;
                
                // Sauvegarder via FileManager
                bool success = fileManager->saveFromJsonMidi(fileId, jsonMidi);
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to save file"}
                    };
                }
                
                Logger::info("EditorAPI", "File saved: " + fileId);
                
                return {
                    {"success", true},
                    {"message", "File saved successfully"},
                    {"file_id", fileId}
                };
                
            } catch (const std::exception& e) {
                Logger::error("EditorAPI", 
                    "Failed to save file: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to save file: " + std::string(e.what())}
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
                // Validation des paramètres
                if (!params.contains("file_id") || !params.contains("track") ||
                    !params.contains("tick") || !params.contains("note") ||
                    !params.contains("velocity") || !params.contains("duration")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameters"}
                    };
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
                
                // Récupérer l'état d'édition
                std::lock_guard<std::mutex> lock(editorMutex);
                auto it = editorStates.find(fileId);
                if (it == editorStates.end()) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"}
                    };
                }
                
                EditorState& state = it->second;
                
                // Sauvegarder l'état actuel pour undo
                state.pushUndo(state.currentData);
                
                // Ajouter la note dans le JsonMidi
                json& tracks = state.currentData["tracks"];
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
                
                // TODO: Trier les événements par tick
                
                Logger::info("EditorAPI", 
                    "Note added: " + std::to_string(note) + " at tick " + std::to_string(tick));
                
                return {
                    {"success", true},
                    {"message", "Note added successfully"},
                    {"data", {
                        {"track", track},
                        {"tick", tick},
                        {"note", note}
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
                std::lock_guard<std::mutex> lock(editorMutex);
                auto it = editorStates.find(fileId);
                if (it == editorStates.end()) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"}
                    };
                }
                
                EditorState& state = it->second;
                state.pushUndo(state.currentData);
                
                json& tracks = state.currentData["tracks"];
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
                if (!params.contains("file_id") || !params.contains("track") ||
                    !params.contains("tick") || !params.contains("controller") ||
                    !params.contains("value")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameters"}
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
                        {"error", "Invalid controller or value (0-127)"}
                    };
                }
                
                // Récupérer l'état
                std::lock_guard<std::mutex> lock(editorMutex);
                auto it = editorStates.find(fileId);
                if (it == editorStates.end()) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"}
                    };
                }
                
                EditorState& state = it->second;
                state.pushUndo(state.currentData);
                
                json& tracks = state.currentData["tracks"];
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
                
                std::lock_guard<std::mutex> lock(editorMutex);
                auto it = editorStates.find(fileId);
                if (it == editorStates.end()) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"}
                    };
                }
                
                EditorState& state = it->second;
                
                if (state.undoStack.empty()) {
                    return {
                        {"success", false},
                        {"error", "Nothing to undo"}
                    };
                }
                
                // Push current to redo
                state.redoStack.push(state.currentData);
                
                // Pop from undo
                state.currentData = state.undoStack.top();
                state.undoStack.pop();
                
                Logger::info("EditorAPI", "Undo performed");
                
                return {
                    {"success", true},
                    {"message", "Undo performed successfully"},
                    {"data", {
                        {"jsonmidi", state.currentData}
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
                
                std::lock_guard<std::mutex> lock(editorMutex);
                auto it = editorStates.find(fileId);
                if (it == editorStates.end()) {
                    return {
                        {"success", false},
                        {"error", "File not loaded in editor"}
                    };
                }
                
                EditorState& state = it->second;
                
                if (state.redoStack.empty()) {
                    return {
                        {"success", false},
                        {"error", "Nothing to redo"}
                    };
                }
                
                // Push current to undo
                state.undoStack.push(state.currentData);
                
                // Pop from redo
                state.currentData = state.redoStack.top();
                state.redoStack.pop();
                
                Logger::info("EditorAPI", "Redo performed");
                
                return {
                    {"success", true},
                    {"message", "Redo performed successfully"},
                    {"data", {
                        {"jsonmidi", state.currentData}
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
    
    Logger::info("EditorHandlers", "✓ Registered 7 editor commands");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER editor.cpp
// ============================================================================
