// ============================================================================
// Fichier: backend/src/api/commands/loops.cpp
// Version: 1.0.0
// Date: 2025-10-10
// ============================================================================
// Description:
//   Commandes WebSocket pour la gestion des loops.
//   Enregistre toutes les commandes loops.* dans la CommandFactory.
//
// Commandes:
//   - loops.save      : Sauvegarder un loop
//   - loops.load      : Charger un loop
//   - loops.list      : Lister les loops
//   - loops.delete    : Supprimer un loop
//   - loops.search    : Rechercher des loops
//   - loops.count     : Compter les loops
//
// Usage dans CommandProcessor:
//   registerLoopCommands(factory_);
// ============================================================================

#pragma once

#include "../../core/commands/CommandFactory.h"
#include "../../loop/LoopManager.h"
#include "../../core/Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @brief Enregistre toutes les commandes loop dans la factory
 * @param factory Factory où enregistrer les commandes
 */
void registerLoopCommands(CommandFactory& factory) {
    
    // ========================================================================
    // loops.save - Sauvegarder un loop (création ou mise à jour)
    // ========================================================================
    factory.registerCommand("loops.save",
        [](const json& params) -> json {
            Logger::debug("LoopAPI", "Saving loop...");
            
            try {
                // Vérifier que les params contiennent les données du loop
                if (!params.contains("loop") || !params["loop"].is_object()) {
                    return {
                        {"success", false},
                        {"error", "Missing or invalid 'loop' parameter"}
                    };
                }
                
                json loopData = params["loop"];
                
                // Sauvegarder via LoopManager
                auto& loopMgr = LoopManager::instance();
                json savedLoop = loopMgr.saveLoop(loopData);
                
                Logger::info("LoopAPI", 
                    "✓ Loop saved: " + savedLoop.value("name", "Unknown"));
                
                return {
                    {"success", true},
                    {"message", "Loop saved successfully"},
                    {"data", savedLoop}
                };
                
            } catch (const MidiMindException& e) {
                Logger::error("LoopAPI", 
                    "Failed to save loop: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", e.getCodeName()}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoopAPI", 
                    "Unexpected error: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Internal error: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // loops.load - Charger un loop par ID
    // ========================================================================
    factory.registerCommand("loops.load",
        [](const json& params) -> json {
            Logger::debug("LoopAPI", "Loading loop...");
            
            try {
                // Vérifier le paramètre loopId
                if (!params.contains("loopId") || 
                    !params["loopId"].is_string()) {
                    return {
                        {"success", false},
                        {"error", "Missing or invalid 'loopId' parameter"}
                    };
                }
                
                std::string loopId = params["loopId"];
                
                // Charger via LoopManager
                auto& loopMgr = LoopManager::instance();
                auto loopOpt = loopMgr.loadLoop(loopId);
                
                if (!loopOpt) {
                    return {
                        {"success", false},
                        {"error", "Loop not found"},
                        {"error_code", "LOOP_NOT_FOUND"}
                    };
                }
                
                Logger::info("LoopAPI", "✓ Loop loaded: " + loopId);
                
                return {
                    {"success", true},
                    {"message", "Loop loaded successfully"},
                    {"data", *loopOpt}
                };
                
            } catch (const MidiMindException& e) {
                Logger::error("LoopAPI", 
                    "Failed to load loop: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", e.getCodeName()}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoopAPI", 
                    "Unexpected error: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Internal error: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // loops.list - Lister les loops avec pagination
    // ========================================================================
    factory.registerCommand("loops.list",
        [](const json& params) -> json {
            Logger::debug("LoopAPI", "Listing loops...");
            
            try {
                // Paramètres optionnels
                int limit = params.value("limit", 50);
                int offset = params.value("offset", 0);
                std::string sortBy = params.value("sortBy", "lastModified");
                std::string sortOrder = params.value("sortOrder", "desc");
                
                // Lister via LoopManager
                auto& loopMgr = LoopManager::instance();
                json loops = loopMgr.listLoops(limit, offset, sortBy, sortOrder);
                int totalCount = loopMgr.getTotalCount();
                
                Logger::info("LoopAPI", 
                    "✓ Listed " + std::to_string(loops.size()) + 
                    " loops (total: " + std::to_string(totalCount) + ")");
                
                return {
                    {"success", true},
                    {"data", {
                        {"loops", loops},
                        {"count", loops.size()},
                        {"total", totalCount},
                        {"limit", limit},
                        {"offset", offset}
                    }}
                };
                
            } catch (const MidiMindException& e) {
                Logger::error("LoopAPI", 
                    "Failed to list loops: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", e.getCodeName()}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoopAPI", 
                    "Unexpected error: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Internal error: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // loops.delete - Supprimer un loop
    // ========================================================================
    factory.registerCommand("loops.delete",
        [](const json& params) -> json {
            Logger::debug("LoopAPI", "Deleting loop...");
            
            try {
                // Vérifier le paramètre loopId
                if (!params.contains("loopId") || 
                    !params["loopId"].is_string()) {
                    return {
                        {"success", false},
                        {"error", "Missing or invalid 'loopId' parameter"}
                    };
                }
                
                std::string loopId = params["loopId"];
                
                // Supprimer via LoopManager
                auto& loopMgr = LoopManager::instance();
                bool deleted = loopMgr.deleteLoop(loopId);
                
                if (!deleted) {
                    return {
                        {"success", false},
                        {"error", "Loop not found"},
                        {"error_code", "LOOP_NOT_FOUND"}
                    };
                }
                
                Logger::info("LoopAPI", "✓ Loop deleted: " + loopId);
                
                return {
                    {"success", true},
                    {"message", "Loop deleted successfully"}
                };
                
            } catch (const MidiMindException& e) {
                Logger::error("LoopAPI", 
                    "Failed to delete loop: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", e.getCodeName()}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoopAPI", 
                    "Unexpected error: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Internal error: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // loops.search - Rechercher des loops par nom
    // ========================================================================
    factory.registerCommand("loops.search",
        [](const json& params) -> json {
            Logger::debug("LoopAPI", "Searching loops...");
            
            try {
                // Vérifier le paramètre query
                if (!params.contains("query") || 
                    !params["query"].is_string()) {
                    return {
                        {"success", false},
                        {"error", "Missing or invalid 'query' parameter"}
                    };
                }
                
                std::string query = params["query"];
                int limit = params.value("limit", 20);
                
                // Rechercher via LoopManager
                auto& loopMgr = LoopManager::instance();
                json loops = loopMgr.searchLoops(query, limit);
                
                Logger::info("LoopAPI", 
                    "✓ Found " + std::to_string(loops.size()) + 
                    " loops for: " + query);
                
                return {
                    {"success", true},
                    {"data", {
                        {"loops", loops},
                        {"count", loops.size()},
                        {"query", query}
                    }}
                };
                
            } catch (const MidiMindException& e) {
                Logger::error("LoopAPI", 
                    "Failed to search loops: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", e.getCodeName()}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoopAPI", 
                    "Unexpected error: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Internal error: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // loops.count - Obtenir le nombre total de loops
    // ========================================================================
    factory.registerCommand("loops.count",
        [](const json& params) -> json {
            Logger::debug("LoopAPI", "Counting loops...");
            
            try {
                auto& loopMgr = LoopManager::instance();
                int count = loopMgr.getTotalCount();
                
                Logger::debug("LoopAPI", "✓ Total loops: " + std::to_string(count));
                
                return {
                    {"success", true},
                    {"data", {
                        {"count", count}
                    }}
                };
                
            } catch (const MidiMindException& e) {
                Logger::error("LoopAPI", 
                    "Failed to count loops: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", e.getCodeName()}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoopAPI", 
                    "Unexpected error: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Internal error: " + std::string(e.what())}
                };
            }
        }
    );
    
    Logger::info("LoopHandlers", 
                "✓ Loop commands registered (6 commands)");
}






/**
 * Modèle de gestion des boucles MIDI
 */
class LoopModel extends EventEmitter {
    constructor() {
        super();
        this.loops = new Map();
        this.currentLoop = null;
        this.isRecording = false;
        this.isPlaying = false;
        this.recordBuffer = [];
        this.recordStartTime = 0;
        this.recordMode = 'overdub'; // overdub, replace, merge
        this.recordChannel = 0;
        this.recordInstrument = null;
        this.quantizeOnRecord = false;
        this.quantizeResolution = 480; // ms
        
        // Playback
        this.playbackTimer = null;
        this.loopPosition = 0;
        this.loopStartTime = 0;
        this.lastEventTime = 0;
        this.playbackInterval = 10; // ms (100 Hz)
    }

    // ========================================================================
    // CRÉATION DE LOOP
    // ========================================================================

    /**
     * Crée une nouvelle boucle
     */
    createLoop(bars = 4, tempo = 120, timeSignature = "4/4") {
        const [numerator, denominator] = timeSignature.split('/').map(Number);
        const beatDuration = 60000 / tempo; // ms par beat
        const barDuration = beatDuration * numerator;
        const duration = barDuration * bars;

        const loop = {
            id: `loop_${Date.now()}`,
            name: `Loop ${this.loops.size + 1}`,
            duration: duration,
            bars: bars,
            timeSignature: timeSignature,
            tempo: tempo,
            layers: [],
            createdAt: Date.now(),
            lastModified: Date.now()
        };

        this.loops.set(loop.id, loop);
        this.currentLoop = loop;

        this.emit('loop:created', loop);

        return loop;
    }

    // ========================================================================
    // ENREGISTREMENT
    // ========================================================================

    /**
     * Démarre l'enregistrement
     */
    startRecording(channel, instrumentId, mode = 'overdub') {
        if (!this.currentLoop) {
            throw new Error('No loop selected');
        }

        this.isRecording = true;
        this.recordMode = mode;
        this.recordChannel = channel;
        this.recordInstrument = instrumentId;
        this.recordBuffer = [];
        this.recordStartTime = Date.now();

        this.emit('recording:started', { 
            loopId: this.currentLoop.id,
            channel,
            instrumentId,
            mode 
        });
    }

    /**
     * Enregistre un événement MIDI
     */
    recordEvent(event) {
        if (!this.isRecording) return;

        const timestamp = Date.now() - this.recordStartTime;
        const loopTime = timestamp % this.currentLoop.duration;

        const recordedEvent = {
            ...event,
            time: loopTime,
            channel: this.recordChannel,
            id: `event_${Date.now()}_${Math.random()}`
        };

        this.recordBuffer.push(recordedEvent);
    }

    /**
     * Arrête l'enregistrement
     */
    stopRecording() {
        if (!this.isRecording) return;

        this.isRecording = false;

        // Quantifier si activé
        if (this.quantizeOnRecord) {
            this.quantizeBuffer();
        }

        // Créer ou mettre à jour le layer
        const existingLayer = this.currentLoop.layers.find(
            l => l.channel === this.recordChannel
        );

        if (existingLayer) {
            switch (this.recordMode) {
                case 'overdub':
                    existingLayer.events.push(...this.recordBuffer);
                    existingLayer.events.sort((a, b) => a.time - b.time);
                    break;
                    
                case 'replace':
                    existingLayer.events = [...this.recordBuffer];
                    break;
                    
                case 'merge':
                    this.mergeBufferToLayer(existingLayer);
                    break;
            }

        } else {
            // Nouveau layer
            this.currentLoop.layers.push({
                id: `layer_${Date.now()}`,
                channel: this.recordChannel,
                instrument: this.recordInstrument,
                events: [...this.recordBuffer],
                volume: 100,
                muted: false,
                solo: false
            });
        }

        const eventCount = this.recordBuffer.length;
        this.recordBuffer = [];
        this.currentLoop.lastModified = Date.now();

        this.emit('recording:stopped', {
            loopId: this.currentLoop.id,
            eventCount: eventCount
        });
    }

    /**
     * Quantifie le buffer d'enregistrement
     */
    quantizeBuffer() {
        this.recordBuffer.forEach(event => {
            event.time = Math.round(event.time / this.quantizeResolution) * this.quantizeResolution;
        });

        // Retrier
        this.recordBuffer.sort((a, b) => a.time - b.time);
    }

    /**
     * Fusionne le buffer avec un layer existant
     */
    mergeBufferToLayer(layer) {
        // Ajouter les nouveaux événements
        layer.events.push(...this.recordBuffer);
        
        // Retrier par temps
        layer.events.sort((a, b) => a.time - b.time);

        // Supprimer les doublons exacts
        const unique = [];
        layer.events.forEach(event => {
            const duplicate = unique.find(e => 
                e.time === event.time && 
                e.note === event.note && 
                e.type === event.type
            );
            
            if (!duplicate) {
                unique.push(event);
            }
        });

        layer.events = unique;
    }

    // ========================================================================
    // PLAYBACK ENGINE (NOUVEAU v2.0.0)
    // ========================================================================

    /**
     * Lance la lecture de la boucle
     */
    playLoop(loopId) {
        const loop = loopId ? this.loops.get(loopId) : this.currentLoop;
        
        if (!loop) {
            throw new Error('Loop not found');
        }

        if (this.isPlaying) {
            console.warn('Loop already playing');
            return;
        }

        this.currentLoop = loop;
        this.isPlaying = true;
        this.loopPosition = 0;
        this.lastEventTime = 0;
        this.loopStartTime = Date.now();

        this.startPlaybackTimer();

        this.emit('loop:playing', { loopId: loop.id });
    }

    /**
     * Met en pause la lecture
     */
    pauseLoop() {
        if (!this.isPlaying) return;

        this.isPlaying = false;
        this.stopPlaybackTimer();

        this.emit('loop:paused', { 
            loopId: this.currentLoop?.id,
            position: this.loopPosition 
        });
    }

    /**
     * Arrête la lecture
     */
    stopLoop() {
        if (!this.isPlaying && this.loopPosition === 0) return;

        this.isPlaying = false;
        this.loopPosition = 0;
        this.lastEventTime = 0;
        this.stopPlaybackTimer();

        // Envoyer note-off pour toutes les notes actives
        this.sendAllNotesOff();

        this.emit('loop:stopped', { loopId: this.currentLoop?.id });
    }

    /**
     * Démarre le timer de playback
     */
    startPlaybackTimer() {
        if (this.playbackTimer) {
            clearInterval(this.playbackTimer);
        }

        this.playbackTimer = setInterval(() => {
            this.updatePlayback();
        }, this.playbackInterval);
    }

    /**
     * Arrête le timer de playback
     */
    stopPlaybackTimer() {
        if (this.playbackTimer) {
            clearInterval(this.playbackTimer);
            this.playbackTimer = null;
        }
    }

    /**
     * Met à jour le playback (appelé toutes les 10ms)
     */
    updatePlayback() {
        if (!this.isPlaying || !this.currentLoop) return;

        // Calculer la position actuelle
        const elapsed = Date.now() - this.loopStartTime;
        this.loopPosition = elapsed % this.currentLoop.duration;

        // Détecter le cycle (quand on revient au début)
        if (this.loopPosition < this.lastEventTime) {
            this.emit('loop:cycle');
        }

        // Jouer les événements entre lastEventTime et loopPosition
        this.playEventsBetween(this.lastEventTime, this.loopPosition);

        // Émettre la position
        this.emit('loop:position', { 
            position: this.loopPosition,
            duration: this.currentLoop.duration
        });

        this.lastEventTime = this.loopPosition;
    }

    /**
     * Joue les événements MIDI entre deux timestamps
     */
    playEventsBetween(startTime, endTime) {
        if (!this.currentLoop) return;

        // Gérer le wrap-around (fin → début de loop)
        const wrappedAround = endTime < startTime;

        // Obtenir les événements actifs (respect mute/solo)
        const activeEvents = this.getActiveEvents();

        activeEvents.forEach(event => {
            let shouldPlay = false;

            if (wrappedAround) {
                // On a bouclé : jouer les événements de startTime à duration
                // ET les événements de 0 à endTime
                shouldPlay = (event.time >= startTime && event.time <= this.currentLoop.duration) ||
                            (event.time >= 0 && event.time <= endTime);
            } else {
                // Normal : jouer les événements entre startTime et endTime
                shouldPlay = event.time >= startTime && event.time <= endTime;
            }

            if (shouldPlay) {
                this.sendMidiEvent(event);
            }
        });
    }

    /**
     * Obtient les événements actifs (respect mute/solo/volume)
     */
    getActiveEvents() {
        if (!this.currentLoop) return [];

        const events = [];
        const hasSolo = this.currentLoop.layers.some(l => l.solo);

        this.currentLoop.layers.forEach(layer => {
            // Skip si muted
            if (layer.muted) return;

            // Si un layer est en solo, ignorer les autres
            if (hasSolo && !layer.solo) return;

            // Ajouter les événements avec volume du layer
            layer.events.forEach(event => {
                events.push({
                    ...event,
                    channel: layer.channel,
                    velocity: event.type === 'noteOn' ? 
                        Math.round((event.velocity * layer.volume) / 100) : 
                        event.velocity
                });
            });
        });

        return events;
    }

    /**
     * Envoie un événement MIDI au backend
     */
    sendMidiEvent(event) {
        if (!window.wsManager || !window.wsManager.connected) {
            console.warn('WebSocket not connected, cannot send MIDI');
            return;
        }

        // Émettre localement pour visualisation
        this.emit('loop:event', event);

        // Envoyer au backend via WebSocket
        window.wsManager.send({
            command: 'midi.send',
            params: {
                type: event.type,
                channel: event.channel,
                note: event.note,
                velocity: event.velocity,
                timestamp: Date.now()
            }
        }).catch(error => {
            console.error('Failed to send MIDI event:', error);
        });
    }

    /**
     * Envoie note-off pour toutes les notes
     */
    sendAllNotesOff() {
        if (!this.currentLoop) return;

        // Pour chaque canal utilisé, envoyer All Notes Off (CC 123)
        const channels = new Set(this.currentLoop.layers.map(l => l.channel));
        
        channels.forEach(channel => {
            this.sendMidiEvent({
                type: 'controlChange',
                channel: channel,
                controller: 123, // All Notes Off
                value: 0
            });
        });
    }

    // ========================================================================
    // GESTION DES LAYERS
    // ========================================================================

    /**
     * Mute/unmute un layer
     */
    muteLayer(layerId, muted = null) {
        if (!this.currentLoop) return;

        const layer = this.currentLoop.layers.find(l => l.id === layerId);
        if (!layer) return;

        layer.muted = muted !== null ? muted : !layer.muted;

        this.emit('layer:muted', { layerId, muted: layer.muted });
    }

    /**
     * Solo un layer
     */
    soloLayer(layerId, solo = null) {
        if (!this.currentLoop) return;

        const layer = this.currentLoop.layers.find(l => l.id === layerId);
        if (!layer) return;

        layer.solo = solo !== null ? solo : !layer.solo;

        this.emit('layer:solo', { layerId, solo: layer.solo });
    }

    /**
     * Définit le volume d'un layer
     */
    setLayerVolume(layerId, volume) {
        if (!this.currentLoop) return;

        const layer = this.currentLoop.layers.find(l => l.id === layerId);
        if (!layer) return;

        layer.volume = Math.max(0, Math.min(127, volume));

        this.emit('layer:volume', { layerId, volume: layer.volume });
    }

    /**
     * Efface un layer
     */
    clearLayer(layerId) {
        if (!this.currentLoop) return;

        const index = this.currentLoop.layers.findIndex(l => l.id === layerId);
        if (index === -1) return;

        this.currentLoop.layers.splice(index, 1);
        this.currentLoop.lastModified = Date.now();

        this.emit('layer:cleared', { layerId });
    }

    /**
     * Efface toute la boucle
     */
    clearLoop() {
        if (!this.currentLoop) return;

        this.currentLoop.layers = [];
        this.currentLoop.lastModified = Date.now();

        this.emit('loop:cleared', { loopId: this.currentLoop.id });
    }

    // ========================================================================
    // QUANTIFICATION
    // ========================================================================

    /**
     * Définit la quantification automatique
     */
    setQuantize(enabled, resolution = 480) {
        this.quantizeOnRecord = enabled;
        this.quantizeResolution = resolution;

        this.emit('quantize:changed', { enabled, resolution });
    }

    // ========================================================================
    // EXPORT
    // ========================================================================

    /**
     * Exporte la boucle en MidiJSON
     */
    exportToMidiJson() {
        if (!this.currentLoop) return null;

        const timeline = [];

        // Collecter tous les événements des layers non-muted
        this.currentLoop.layers.forEach(layer => {
            if (!layer.muted) {
                timeline.push(...layer.events.map(e => ({
                    ...e,
                    channel: layer.channel
                })));
            }
        });

        // Trier par temps
        timeline.sort((a, b) => a.time - b.time);

        // Calculer les canaux utilisés
        const channelsUsed = [...new Set(timeline.map(e => e.channel))];
        const channels = channelsUsed.map(ch => ({
            number: ch,
            noteCount: timeline.filter(e => e.channel === ch && e.type === 'noteOn').length
        }));

        return {
            format: "midijson-v1.0",
            version: "1.0",
            metadata: {
                tempo: this.currentLoop.tempo,
                timeSignature: this.currentLoop.timeSignature,
                duration: this.currentLoop.duration,
                source: 'loop-recorder',
                bars: this.currentLoop.bars
            },
            timeline: timeline,
            channels: channels
        };
    }

    // ========================================================================
    // PERSISTENCE (Mis à jour pour WebSocket v2.0.0)
    // ========================================================================

    /**
     * Sauvegarde la boucle
     */
    async saveLoop(loopId) {
        const loop = loopId ? 
            this.loops.get(loopId) : this.currentLoop;
        
        if (!loop) {
            throw new Error('Loop not found');
        }

        try {
            const response = await window.wsManager.send({
                command: 'loops.save',
                params: {
                    loop: loop
                }
            });

            if (!response.success) {
                throw new Error(response.error || 'Failed to save loop');
            }

            const saved = response.data;
            this.loops.set(saved.id, saved);

            this.emit('loop:saved', saved);

            return saved;
        } catch (error) {
            console.error('Error saving loop:', error);
            throw error;
        }
    }

    /**
     * Charge une boucle
     */
    async loadLoop(loopId) {
        try {
            const response = await window.wsManager.send({
                command: 'loops.load',
                params: {
                    loopId: loopId
                }
            });

            if (!response.success) {
                throw new Error(response.error || 'Failed to load loop');
            }

            const loop = response.data;
            this.loops.set(loop.id, loop);
            this.currentLoop = loop;

            this.emit('loop:loaded', loop);

            return loop;
        } catch (error) {
            console.error('Error loading loop:', error);
            throw error;
        }
    }

    /**
     * Liste tous les loops
     */
    async listLoops(limit = 50, offset = 0) {
        try {
            const response = await window.wsManager.send({
                command: 'loops.list',
                params: {
                    limit: limit,
                    offset: offset,
                    sortBy: 'lastModified',
                    sortOrder: 'desc'
                }
            });

            if (!response.success) {
                throw new Error(response.error || 'Failed to list loops');
            }

            return response.data;
        } catch (error) {
            console.error('Error listing loops:', error);
            throw error;
        }
    }

    /**
     * Supprime un loop
     */
    async deleteLoop(loopId) {
        try {
            const response = await window.wsManager.send({
                command: 'loops.delete',
                params: {
                    loopId: loopId
                }
            });

            if (!response.success) {
                throw new Error(response.error || 'Failed to delete loop');
            }

            this.loops.delete(loopId);
            
            if (this.currentLoop && this.currentLoop.id === loopId) {
                this.currentLoop = null;
            }

            this.emit('loop:deleted', { loopId });

            return true;
        } catch (error) {
            console.error('Error deleting loop:', error);
            throw error;
        }
    }

    // ========================================================================
    // GETTERS
    // ========================================================================

    /**
     * Obtient la boucle courante
     */
    getCurrentLoop() {
        return this.currentLoop;
    }

    /**
     * Obtient l'état du loop
     */
    getState() {
        return {
            isRecording: this.isRecording,
            isPlaying: this.isPlaying,
            loopPosition: this.loopPosition,
            currentLoop: this.currentLoop,
            recordMode: this.recordMode,
            quantizeOnRecord: this.quantizeOnRecord
        };
    }

    // ========================================================================
    // CLEANUP
    // ========================================================================

    /**
     * Nettoie les ressources
     */
    destroy() {
        this.stopLoop();
        this.stopPlaybackTimer();
        this.loops.clear();
        this.currentLoop = null;
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = LoopModel;
}











} // namespace midiMind

// ============================================================================
// FIN DU FICHIER loops.cpp
// ============================================================================
