// ============================================================================
// Fichier: /home/pi/midiMind/backend/src/api/commands/playback.cpp
// Version: 3.0.5
// Date: 2025-10-16
// ============================================================================
// Description:
//   Handlers pour les commandes de lecture MIDI
//
// CORRECTIONS v3.0.5:
//   ✅ Correction appels registerCommand (2 paramètres)
//
// Commandes implémentées:
//   - playback.play      : Démarrer la lecture
//   - playback.pause     : Mettre en pause
//   - playback.stop      : Arrêter la lecture
//   - playback.seek      : Se déplacer dans le fichier
//   - playback.setTempo  : Définir le tempo
//   - playback.setLoop   : Configuration boucle
//
// Auteur: midiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../midi/player/MidiPlayer.h"
#include "../../core/Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// FONCTION: registerPlaybackCommands()
// ============================================================================

void registerPlaybackCommands(
    CommandFactory& factory,
    std::shared_ptr<MidiPlayer> player
) {
    if (!player) {
        Logger::error("PlaybackCommands", 
            "Cannot register commands: MidiPlayer is null");
        return;
    }
    
    Logger::info("PlaybackHandlers", "Registering playback commands...");

    // ========================================================================
    // playback.play - Démarrer la lecture
    // ========================================================================
    
    factory.registerCommand("playback.play",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Starting playback...");
            
            try {
                player->play();
                
                Logger::info("PlaybackAPI", "✓ Playback started");
                
                return {
                    {"success", true},
                    {"data", {
                        {"state", "playing"}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", 
                    "Failed to start playback: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "PLAY_FAILED"}
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
                
                Logger::info("PlaybackAPI", "✓ Playback paused");
                
                return {
                    {"success", true},
                    {"data", {
                        {"state", "paused"}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", 
                    "Failed to pause: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "PAUSE_FAILED"}
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
                
                Logger::info("PlaybackAPI", "✓ Playback stopped");
                
                return {
                    {"success", true},
                    {"data", {
                        {"state", "stopped"}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", 
                    "Failed to stop: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "STOP_FAILED"}
                };
            }
        }
    );

    // ========================================================================
    // playback.seek - Se déplacer dans le fichier
    // ========================================================================
    
    factory.registerCommand("playback.seek",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Seeking...");
            
            try {
                if (!params.contains("position")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: position"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                uint32_t position = params["position"];
                
                player->seek(position);
                
                Logger::info("PlaybackAPI", "✓ Seeked to: " + std::to_string(position));
                
                return {
                    {"success", true},
                    {"data", {
                        {"position", position}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", 
                    "Failed to seek: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "SEEK_FAILED"}
                };
            }
        }
    );

    // ========================================================================
    // playback.setTempo - Définir le tempo
    // ========================================================================
    
    factory.registerCommand("playback.setTempo",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Setting tempo...");
            
            try {
                if (!params.contains("tempo")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: tempo"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                double tempo = params["tempo"];
                
                if (tempo <= 0 || tempo > 300) {
                    return {
                        {"success", false},
                        {"error", "Tempo must be between 1 and 300 BPM"},
                        {"error_code", "INVALID_TEMPO"}
                    };
                }
                
                player->setTempo(tempo);
                
                Logger::info("PlaybackAPI", "✓ Tempo set to: " + std::to_string(tempo));
                
                return {
                    {"success", true},
                    {"data", {
                        {"tempo", tempo}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", 
                    "Failed to set tempo: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "TEMPO_FAILED"}
                };
            }
        }
    );

    // ========================================================================
    // playback.setLoop - Configuration boucle
    // ========================================================================
    
    factory.registerCommand("playback.setLoop",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Configuring loop...");
            
            try {
                bool enabled = params.value("enabled", false);
                uint32_t start = params.value("start", 0);
                uint32_t end = params.value("end", 0);
                
                player->setLoop(enabled, start, end);
                
                Logger::info("PlaybackAPI", 
                    "✓ Loop configured: " + std::string(enabled ? "ON" : "OFF"));
                
                return {
                    {"success", true},
                    {"data", {
                        {"loop_enabled", enabled},
                        {"loop_start", start},
                        {"loop_end", end}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", 
                    "Failed to set loop: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "LOOP_FAILED"}
                };
            }
        }
    );

    // ========================================================================
    // playback.getState - État de la lecture
    // ========================================================================
    
    factory.registerCommand("playback.getState",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Getting playback state...");
            
            try {
                auto state = player->getState();
                
                return {
                    {"success", true},
                    {"data", state}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", 
                    "Failed to get state: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "STATE_FAILED"}
                };
            }
        }
    );
    
    Logger::info("PlaybackHandlers", "✓ Playback commands registered");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER playback.cpp v3.0.5
// ============================================================================
