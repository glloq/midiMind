// ============================================================================
// Fichier: backend/src/api/commands/playback.cpp
// Version: 3.1.1 - COMMANDE setVolume AJOUTÉE
// Date: 2025-10-13
// ============================================================================
// CORRECTIONS v3.1.1:
// ✅ Ajout playback.setVolume (commande manquante)
// ✅ Ajout playback.getVolume (pour cohérence)
// ✅ Total: 11 commandes playback (au lieu de 9)
// ============================================================================

#include "../core/commands/CommandFactory.h"
#include "../midi/player/MidiPlayer.h"
#include "../core/Logger.h"
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
    // playback.load - Charge un fichier dans le player
    // ========================================================================
    factory.registerCommand("playback.load",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Loading file...");
            
            try {
                // Validation
                if (!params.contains("file_path")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: file_path"}
                    };
                }
                
                std::string filePath = params["file_path"];
                
                // Charger le fichier
                bool success = player->load(filePath);
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to load file"}
                    };
                }
                
                Logger::info("PlaybackAPI", "File loaded: " + filePath);
                
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
                    {"error", "Failed to load: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.play - Démarre la lecture
    // ========================================================================
    factory.registerCommand("playback.play",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Starting playback...");
            
            try {
                player->play();
                
                if (player->getState() != "playing") {
                    return {
                        {"success", false},
                        {"error", "Failed to start playback"}
                    };
                }
                
                Logger::info("PlaybackAPI", "Playback started");
                
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
                    {"error", "Failed to play: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.pause - Met en pause
    // ========================================================================
    factory.registerCommand("playback.pause",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Pausing playback...");
            
            try {
                bool success = player->pause();
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Cannot pause: not playing"}
                    };
                }
                
                Logger::info("PlaybackAPI", "Playback paused");
                
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
                    {"error", "Failed to pause: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.stop - Arrête la lecture
    // ========================================================================
    factory.registerCommand("playback.stop",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Stopping playback...");
            
            try {
                player->stop();
                
                Logger::info("PlaybackAPI", "Playback stopped");
                
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
                    {"error", "Failed to stop: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.seek - Change la position
    // ========================================================================
    factory.registerCommand("playback.seek",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Seeking...");
            
            try {
                // Validation
                if (!params.contains("position")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: position"}
                    };
                }
                
                uint32_t position = params["position"];
                
                player->seek(position);
                
                Logger::info("PlaybackAPI", "Seeked to: " + std::to_string(position) + "ms");
                
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
                    {"error", "Failed to seek: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.status - Obtient l'état actuel
    // ========================================================================
    factory.registerCommand("playback.status",
        [player](const json& params) -> json {
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
                    {"error", "Failed to get status: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.getMetadata - Obtient les métadonnées du fichier
    // ========================================================================
    factory.registerCommand("playback.getMetadata",
        [player](const json& params) -> json {
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
                    {"error", "Failed to get metadata: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.setLoop - Active/désactive le loop
    // ========================================================================
    factory.registerCommand("playback.setLoop",
        [player](const json& params) -> json {
            try {
                // Validation
                if (!params.contains("enabled")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: enabled"}
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
                    {"error", "Failed to set loop: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // playback.setTempo - Change le tempo
    // ========================================================================
    factory.registerCommand("playback.setTempo",
        [player](const json& params) -> json {
            try {
                // Validation
                if (!params.contains("tempo")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: tempo"}
                    };
                }
                
                double tempo = params["tempo"];
                
                // Validation range
                if (tempo < 20.0 || tempo > 300.0) {
                    return {
                        {"success", false},
                        {"error", "Tempo must be between 20 and 300 BPM"}
                    };
                }
                
                player->setTempo(tempo);
                
                Logger::info("PlaybackAPI", "Tempo set to: " + std::to_string(tempo) + " BPM");
                
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
                    {"error", "Failed to set tempo: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // ✅ NOUVEAU: playback.setVolume - Définit le volume master
    // ========================================================================
    factory.registerCommand("playback.setVolume",
        [player](const json& params) -> json {
            Logger::debug("PlaybackAPI", "Setting volume...");
            
            try {
                // Validation
                if (!params.contains("volume")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: volume"}
                    };
                }
                
                int volume = params["volume"];
                
                // Validation range (0-100%)
                if (volume < 0 || volume > 100) {
                    return {
                        {"success", false},
                        {"error", "Volume must be between 0 and 100"}
                    };
                }
                
                // Convertir en float (0.0 - 1.0)
                float volumeFloat = volume / 100.0f;
                
                // Appeler setMasterVolume sur le player
                player->setMasterVolume(volumeFloat);
                
                Logger::info("PlaybackAPI", "Volume set to: " + std::to_string(volume) + "%");
                
                return {
                    {"success", true},
                    {"message", "Volume changed"},
                    {"data", {
                        {"volume", volume},
                        {"volume_float", volumeFloat}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", "Failed to set volume: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to set volume: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // ✅ NOUVEAU: playback.getVolume - Récupère le volume master
    // ========================================================================
    factory.registerCommand("playback.getVolume",
        [player](const json& params) -> json {
            try {
                float volumeFloat = player->getMasterVolume();
                int volume = static_cast<int>(volumeFloat * 100.0f);
                
                return {
                    {"success", true},
                    {"data", {
                        {"volume", volume},
                        {"volume_float", volumeFloat}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("PlaybackAPI", "Failed to get volume: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get volume: " + std::string(e.what())}
                };
            }
        }
    );
    
    Logger::info("PlaybackHandlers", "✓ 11 playback commands registered");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER playback.cpp
// ============================================================================
