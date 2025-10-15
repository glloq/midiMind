// ============================================================================
// Fichier: backend/src/api/commands/playback.cpp
// Version: 3.0.1-corrections
// Date: 2025-10-15
// ============================================================================
// Description:
//   Handlers pour les commandes de lecture MIDI
//   VERSION LAMBDA DIRECTE (json -> json)
//
// CORRECTIONS v3.0.1:
//   ✅ Ajout error_code pour toutes les erreurs
//   ✅ Format de retour harmonisé avec enveloppe "data"
//   ✅ Validation des paramètres renforcée
//   ✅ Logging amélioré
//
// Commandes implémentées (11 commandes):
//   - playback.load        : Charger un fichier MIDI
//   - playback.play        : Démarrer la lecture
//   - playback.pause       : Mettre en pause
//   - playback.stop        : Arrêter la lecture
//   - playback.seek        : Changer la position
//   - playback.status      : Obtenir l'état actuel
//   - playback.getMetadata : Obtenir les métadonnées
//   - playback.setLoop     : Activer/désactiver le loop
//   - playback.setTempo    : Changer le tempo
//   - playback.setVolume   : Changer le volume
//   - playback.getVolume   : Obtenir le volume
//
// Auteur: midiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../midi/MidiPlayer.h"
#include "../../core/Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// FONCTION: registerPlaybackCommands()
// Enregistre toutes les commandes de lecture (11 commandes)
// ============================================================================

void registerPlaybackCommands(CommandFactory& factory,
                              std::shared_ptr<MidiPlayer> player) {
    
    Logger::info("PlaybackHandlers", "Registering playback commands...");
    
    // ========================================================================
    // playback.load - Charger un fichier MIDI
    // ========================================================================
    
    factory.registerCommand("playback.load",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Loading file...");
            
            try {
                // Validation
                if (!params.contains("file_path")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: file_path"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string filePath = params["file_path"];
                
                // Charger le fichier
                bool loaded = player->loadFile(filePath);
                
                if (!loaded) {
                    Logger::error("PlaybackAPI", "Failed to load file: " + filePath);
                    return {
                        {"success", false},
                        {"error", "Failed to load file"},
                        {"error_code", "LOAD_FAILED"},
                        {"data", {
                            {"file_path", filePath}
                        }}
                    };
                }
                
                Logger::info("PlaybackAPI", "✓ File loaded: " + filePath);
                
                // Récupérer métadonnées
                auto metadata = player->getMetadata();
                
                return {
                    {"success", true},
                    {"message", "File loaded successfully"},
                    {"data", {
                        {"file_path", filePath},
                        {"duration_ms", player->getDuration()},
                        {"tempo", metadata["initial_tempo"]},
                        {"time_signature", metadata["time_signature"]},
                        {"track_count", player->getTracks().size()},
                        {"format", metadata["format"]},
                        {"ticks_per_quarter", metadata["ticks_per_quarter"]}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", "Failed to load: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to load: " + std::string(e.what())},
                    {"error_code", "LOAD_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.play - Démarrer la lecture
    // ========================================================================
    
    factory.registerCommand("playback.play",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Starting playback...");
            
            try {
                player->play();
                
                if (player->getState() != "playing") {
                    Logger::error("PlaybackAPI", "Failed to start playback");
                    return {
                        {"success", false},
                        {"error", "Failed to start playback"},
                        {"error_code", "PLAYBACK_FAILED"}
                    };
                }
                
                Logger::info("PlaybackAPI", "✓ Playback started");
                
                return {
                    {"success", true},
                    {"message", "Playback started"},
                    {"data", {
                        {"state", "playing"},
                        {"position_ms", player->getCurrentPosition()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", "Failed to play: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to play: " + std::string(e.what())},
                    {"error_code", "PLAYBACK_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.pause - Mettre en pause
    // ========================================================================
    
    factory.registerCommand("playback.pause",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Pausing playback...");
            
            try {
                player->pause();
                
                if (player->getState() != "paused") {
                    Logger::error("PlaybackAPI", "Failed to pause playback");
                    return {
                        {"success", false},
                        {"error", "Failed to pause playback"},
                        {"error_code", "PAUSE_FAILED"}
                    };
                }
                
                Logger::info("PlaybackAPI", "✓ Playback paused");
                
                return {
                    {"success", true},
                    {"message", "Playback paused"},
                    {"data", {
                        {"state", "paused"},
                        {"position_ms", player->getCurrentPosition()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", "Failed to pause: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to pause: " + std::string(e.what())},
                    {"error_code", "PAUSE_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.stop - Arrêter la lecture
    // ========================================================================
    
    factory.registerCommand("playback.stop",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Stopping playback...");
            
            try {
                player->stop();
                
                if (player->getState() != "stopped") {
                    Logger::error("PlaybackAPI", "Failed to stop playback");
                    return {
                        {"success", false},
                        {"error", "Failed to stop playback"},
                        {"error_code", "STOP_FAILED"}
                    };
                }
                
                Logger::info("PlaybackAPI", "✓ Playback stopped");
                
                return {
                    {"success", true},
                    {"message", "Playback stopped"},
                    {"data", {
                        {"state", "stopped"},
                        {"position_ms", 0}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", "Failed to stop: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to stop: " + std::string(e.what())},
                    {"error_code", "STOP_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.seek - Changer la position
    // ========================================================================
    
    factory.registerCommand("playback.seek",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Seeking...");
            
            try {
                // Validation
                if (!params.contains("position")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: position"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                uint32_t position = params["position"];
                
                // Validation range
                if (position > player->getDuration()) {
                    return {
                        {"success", false},
                        {"error", "Position exceeds file duration"},
                        {"error_code", "INVALID_PARAMETER"},
                        {"data", {
                            {"requested_position", position},
                            {"duration", player->getDuration()}
                        }}
                    };
                }
                
                player->seek(position);
                
                Logger::info("PlaybackAPI", "✓ Seeked to: " + std::to_string(position) + "ms");
                
                return {
                    {"success", true},
                    {"message", "Position changed"},
                    {"data", {
                        {"position_ms", position}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", "Failed to seek: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to seek: " + std::string(e.what())},
                    {"error_code", "SEEK_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.status - Obtenir l'état actuel
    // ========================================================================
    
    factory.registerCommand("playback.status",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Getting status...");
            
            try {
                auto status = player->getStatus();
                
                return {
                    {"success", true},
                    {"data", status}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", "Failed to get status: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get status: " + std::string(e.what())},
                    {"error_code", "STATUS_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.getMetadata - Obtient les métadonnées du fichier
    // ========================================================================
    
    factory.registerCommand("playback.getMetadata",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Getting metadata...");
            
            try {
                auto metadata = player->getMetadata();
                
                return {
                    {"success", true},
                    {"data", metadata}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", "Failed to get metadata: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get metadata: " + std::string(e.what())},
                    {"error_code", "METADATA_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.setLoop - Active/désactive le loop
    // ========================================================================
    
    factory.registerCommand("playback.setLoop",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Setting loop...");
            
            try {
                // Validation
                if (!params.contains("enabled")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: enabled"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                bool enabled = params["enabled"];
                
                player->setLoop(enabled);
                
                Logger::info("PlaybackAPI", 
                    std::string("Loop ") + (enabled ? "enabled" : "disabled"));
                
                return {
                    {"success", true},
                    {"message", std::string("Loop ") + (enabled ? "enabled" : "disabled")},
                    {"data", {
                        {"loop_enabled", enabled}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", "Failed to set loop: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to set loop: " + std::string(e.what())},
                    {"error_code", "SETLOOP_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.setTempo - Change le tempo
    // ========================================================================
    
    factory.registerCommand("playback.setTempo",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Setting tempo...");
            
            try {
                // Validation
                if (!params.contains("tempo")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: tempo"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                double tempo = params["tempo"];
                
                // Validation range
                if (tempo < 20.0 || tempo > 300.0) {
                    return {
                        {"success", false},
                        {"error", "Tempo must be between 20 and 300 BPM"},
                        {"error_code", "INVALID_PARAMETER"},
                        {"data", {
                            {"requested_tempo", tempo},
                            {"valid_range", "20-300 BPM"}
                        }}
                    };
                }
                
                player->setTempo(tempo);
                
                Logger::info("PlaybackAPI", "✓ Tempo set to: " + std::to_string(tempo) + " BPM");
                
                return {
                    {"success", true},
                    {"message", "Tempo changed"},
                    {"data", {
                        {"tempo", tempo}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", "Failed to set tempo: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to set tempo: " + std::string(e.what())},
                    {"error_code", "SETTEMPO_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.setVolume - Change le volume
    // ========================================================================
    
    factory.registerCommand("playback.setVolume",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Setting volume...");
            
            try {
                // Validation
                if (!params.contains("volume")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: volume"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                float volume = params["volume"];
                
                // Validation range
                if (volume < 0.0f || volume > 1.0f) {
                    return {
                        {"success", false},
                        {"error", "Volume must be between 0.0 and 1.0"},
                        {"error_code", "INVALID_PARAMETER"},
                        {"data", {
                            {"requested_volume", volume},
                            {"valid_range", "0.0-1.0"}
                        }}
                    };
                }
                
                player->setVolume(volume);
                
                Logger::info("PlaybackAPI", "✓ Volume set to: " + std::to_string(volume));
                
                return {
                    {"success", true},
                    {"message", "Volume changed"},
                    {"data", {
                        {"volume", volume}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", "Failed to set volume: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to set volume: " + std::string(e.what())},
                    {"error_code", "SETVOLUME_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.getVolume - Obtenir le volume actuel
    // ========================================================================
    
    factory.registerCommand("playback.getVolume",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Getting volume...");
            
            try {
                float volume = player->getVolume();
                
                return {
                    {"success", true},
                    {"data", {
                        {"volume", volume}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", "Failed to get volume: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get volume: " + std::string(e.what())},
                    {"error_code", "GETVOLUME_ERROR"}
                };
            }
        }
    );
    
    Logger::info("PlaybackHandlers", "✅ Playback commands registered (11 commands)");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER playback.cpp v3.0.1-corrections
// ============================================================================
