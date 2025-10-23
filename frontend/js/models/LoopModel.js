// ============================================================================
// Fichier: frontend/js/models/LoopModel.js
// Version: v3.0.1 - CORRIGÃ‰ (Harmonisation avec EventBus global)
// Date: 2025-10-14
// Projet: midiMind v3.0 - SystÃ¨me d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   ModÃ¨le de gestion des boucles et enregistrement MIDI
//   
// CORRECTIONS v3.0.1:
//   âœ… HÃ©ritage de BaseModel au lieu de EventEmitter
//   âœ… Utilisation de EventBus global (this.eventBus.emit)
//   âœ… Signature constructeur standardisÃ©e (eventBus, backend, logger)
//   âœ… IntÃ©gration BackendService pour persistence
//   âœ… CohÃ©rence avec les autres modÃ¨les
//
// Auteur: midiMind Team
// ============================================================================


class LoopModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super(eventBus, logger);
        
        // DÃ©pendances
        this.backend = backend;
        this.logger = logger;
        
        // Loops stockÃ©s
        this.loops = new Map();
        this.currentLoop = null;
        
        // Ã‰tat enregistrement
        this.isRecording = false;
        this.recordBuffer = [];
        this.recordStartTime = 0;
        this.recordMode = 'overdub'; // overdub, replace, merge
        this.recordChannel = 0;
        this.recordInstrument = null;
        
        // Quantization
        this.quantizeOnRecord = false;
        this.quantizeResolution = 480; // ms
        
        // Playback
        this.isPlaying = false;
        this.playbackTimer = null;
        this.loopPosition = 0;
        this.loopStartTime = 0;
        this.lastEventTime = 0;
        this.playbackInterval = 10; // ms (100 Hz)
        
        this.logger.info('LoopModel', 'âœ“ Model initialized v3.0.1');
    }

    // ========================================================================
    // CRÃ‰ATION DE LOOP
    // ========================================================================

    /**
     * CrÃ©e une nouvelle boucle
     * @param {number} bars - Nombre de mesures
     * @param {number} tempo - Tempo en BPM
     * @param {string} timeSignature - Signature temporelle (ex: "4/4")
     * @returns {Object} Loop crÃ©Ã©
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

        // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus au lieu de emit
        this.eventBus.emit('loop:created', loop);

        this.logger.info('LoopModel', `Loop created: ${loop.name}`);

        return loop;
    }

    // ========================================================================
    // ENREGISTREMENT
    // ========================================================================

    /**
     * DÃ©marre l'enregistrement
     * @param {number} channel - Canal MIDI
     * @param {string} instrumentId - ID de l'instrument
     * @param {string} mode - Mode d'enregistrement ('overdub', 'replace', 'merge')
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

        // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
        this.eventBus.emit('recording:started', { 
            loopId: this.currentLoop.id,
            channel,
            instrumentId,
            mode 
        });

        this.logger.info('LoopModel', `Recording started: ${mode} mode on channel ${channel}`);
    }

    /**
     * Enregistre un Ã©vÃ©nement MIDI
     * @param {Object} event - Ã‰vÃ©nement MIDI
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
     * ArrÃªte l'enregistrement
     */
    stopRecording() {
        if (!this.isRecording) return;

        this.isRecording = false;

        // Quantifier si activÃ©
        if (this.quantizeOnRecord) {
            this.quantizeBuffer();
        }

        // CrÃ©er ou mettre Ã  jour le layer
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

        // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
        this.eventBus.emit('recording:stopped', {
            loopId: this.currentLoop.id,
            eventCount: eventCount
        });

        this.logger.info('LoopModel', `Recording stopped: ${eventCount} events recorded`);
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
        
        this.logger.debug('LoopModel', `Buffer quantized to ${this.quantizeResolution}ms`);
    }

    /**
     * Fusionne le buffer avec un layer existant
     * @param {Object} layer - Layer Ã  fusionner
     */
    mergeBufferToLayer(layer) {
        // Ajouter les nouveaux Ã©vÃ©nements
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
    // PLAYBACK
    // ========================================================================

    /**
     * Lance la lecture de la boucle
     * @param {string} loopId - ID du loop (optionnel, utilise currentLoop si non fourni)
     */
    playLoop(loopId) {
        const loop = loopId ? this.loops.get(loopId) : this.currentLoop;

        if (!loop) {
            this.logger.warn('LoopModel', 'No loop to play');
            return;
        }

        this.currentLoop = loop;
        this.isPlaying = true;
        this.loopPosition = 0;
        this.loopStartTime = Date.now();
        this.lastEventTime = 0;

        // DÃ©marrer le timer de playback
        this.playbackTimer = setInterval(() => {
            this.updatePlayback();
        }, this.playbackInterval);

        // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
        this.eventBus.emit('loop:playing', {
            loopId: loop.id,
            duration: loop.duration
        });

        this.logger.info('LoopModel', `Playing loop: ${loop.name}`);
    }

    /**
     * Met en pause la lecture
     */
    pauseLoop() {
        if (!this.isPlaying) return;

        this.isPlaying = false;

        if (this.playbackTimer) {
            clearInterval(this.playbackTimer);
            this.playbackTimer = null;
        }

        // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
        this.eventBus.emit('loop:paused', {
            loopId: this.currentLoop.id,
            position: this.loopPosition
        });

        this.logger.info('LoopModel', 'Loop paused');
    }

    /**
     * ArrÃªte la lecture
     */
    stopLoop() {
        this.isPlaying = false;
        this.loopPosition = 0;

        if (this.playbackTimer) {
            clearInterval(this.playbackTimer);
            this.playbackTimer = null;
        }

        // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
        this.eventBus.emit('loop:stopped', {
            loopId: this.currentLoop?.id
        });

        this.logger.info('LoopModel', 'Loop stopped');
    }

    /**
     * Met Ã  jour le playback
     * @private
     */
    updatePlayback() {
        if (!this.isPlaying || !this.currentLoop) return;

        const elapsed = Date.now() - this.loopStartTime;
        this.loopPosition = elapsed % this.currentLoop.duration;

        // VÃ©rifier si on a bouclÃ©
        if (elapsed > this.lastEventTime && this.loopPosition < this.playbackInterval) {
            // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
            this.eventBus.emit('loop:cycle', {
                loopId: this.currentLoop.id
            });
        }

        this.lastEventTime = elapsed;

        // Jouer les Ã©vÃ©nements
        this.currentLoop.layers.forEach(layer => {
            if (layer.muted || (this.hasSoloLayers() && !layer.solo)) {
                return;
            }

            layer.events.forEach(event => {
                const eventTime = event.time;
                const timeDiff = Math.abs(this.loopPosition - eventTime);

                if (timeDiff < this.playbackInterval) {
                    // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
                    this.eventBus.emit('loop:event', {
                        ...event,
                        volume: layer.volume
                    });
                }
            });
        });
    }

    /**
     * VÃ©rifie si des layers sont en solo
     * @private
     */
    hasSoloLayers() {
        if (!this.currentLoop) return false;
        return this.currentLoop.layers.some(l => l.solo);
    }

    // ========================================================================
    // GESTION LAYERS
    // ========================================================================

    /**
     * Mute/unmute un layer
     * @param {string} layerId - ID du layer
     * @param {boolean} muted - Ã‰tat muted (optionnel, toggle si non fourni)
     */
    muteLayer(layerId, muted) {
        if (!this.currentLoop) return;

        const layer = this.currentLoop.layers.find(l => l.id === layerId);
        if (!layer) return;

        layer.muted = muted !== undefined ? muted : !layer.muted;

        // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
        this.eventBus.emit('layer:muted', {
            layerId,
            muted: layer.muted
        });

        this.logger.debug('LoopModel', `Layer ${layerId} ${layer.muted ? 'muted' : 'unmuted'}`);
    }

    /**
     * Solo un layer
     * @param {string} layerId - ID du layer
     * @param {boolean} solo - Ã‰tat solo (optionnel, toggle si non fourni)
     */
    soloLayer(layerId, solo) {
        if (!this.currentLoop) return;

        const layer = this.currentLoop.layers.find(l => l.id === layerId);
        if (!layer) return;

        layer.solo = solo !== undefined ? solo : !layer.solo;

        // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
        this.eventBus.emit('layer:solo', {
            layerId,
            solo: layer.solo
        });

        this.logger.debug('LoopModel', `Layer ${layerId} solo: ${layer.solo}`);
    }

    /**
     * Change le volume d'un layer
     * @param {string} layerId - ID du layer
     * @param {number} volume - Volume (0-127)
     */
    setLayerVolume(layerId, volume) {
        if (!this.currentLoop) return;

        const layer = this.currentLoop.layers.find(l => l.id === layerId);
        if (!layer) return;

        layer.volume = Math.max(0, Math.min(127, volume));

        // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
        this.eventBus.emit('layer:volume-changed', {
            layerId,
            volume: layer.volume
        });

        this.logger.debug('LoopModel', `Layer ${layerId} volume: ${layer.volume}`);
    }

    /**
     * Efface un layer
     * @param {string} layerId - ID du layer
     */
    clearLayer(layerId) {
        if (!this.currentLoop) return;

        const index = this.currentLoop.layers.findIndex(l => l.id === layerId);
        if (index === -1) return;

        this.currentLoop.layers.splice(index, 1);
        this.currentLoop.lastModified = Date.now();

        // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
        this.eventBus.emit('layer:cleared', { layerId });

        this.logger.info('LoopModel', `Layer ${layerId} cleared`);
    }

    /**
     * Efface le loop complet
     */
    clearLoop() {
        if (!this.currentLoop) return;

        this.currentLoop.layers = [];
        this.currentLoop.lastModified = Date.now();

        // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
        this.eventBus.emit('loop:cleared', {
            loopId: this.currentLoop.id
        });

        this.logger.info('LoopModel', 'Loop cleared');
    }

    // ========================================================================
    // PERSISTENCE - âœ… NOUVEAU v3.0.1 (IntÃ©gration BackendService)
    // ========================================================================

    /**
     * Sauvegarde un loop vers le backend
     * @param {string} loopId - ID du loop (optionnel, utilise currentLoop si non fourni)
     */
    async saveLoop(loopId) {
        const loop = loopId ? this.loops.get(loopId) : this.currentLoop;

        if (!loop) {
            throw new Error('No loop to save');
        }

        this.logger.info('LoopModel', `Saving loop: ${loop.name}`);

        try {
            const response = await this.backend.sendCommand('loops.save', {
                loop: loop
            });

            if (!response.success) {
                throw new Error(response.error || 'Failed to save loop');
            }

            // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
            this.eventBus.emit('loop:saved', {
                loop: loop
            });

            this.logger.info('LoopModel', `âœ“ Loop saved: ${loop.name}`);

            return response.data;

        } catch (error) {
            this.logger.error('LoopModel', 'Failed to save loop:', error);
            throw error;
        }
    }

    /**
     * Charge un loop depuis le backend
     * @param {string} loopId - ID du loop
     */
    async loadLoop(loopId) {
        this.logger.info('LoopModel', `Loading loop: ${loopId}`);

        try {
            const response = await this.backend.sendCommand('loops.load', {
                loop_id: loopId
            });

            if (!response.success) {
                throw new Error(response.error || 'Failed to load loop');
            }

            const loop = response.data.loop;

            this.loops.set(loop.id, loop);
            this.currentLoop = loop;

            // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
            this.eventBus.emit('loop:loaded', {
                loop: loop
            });

            this.logger.info('LoopModel', `âœ“ Loop loaded: ${loop.name}`);

            return loop;

        } catch (error) {
            this.logger.error('LoopModel', 'Failed to load loop:', error);
            throw error;
        }
    }

    /**
     * Liste les loops disponibles
     * @param {number} limit - Nombre maximum de rÃ©sultats
     * @param {number} offset - Offset pour pagination
     */
    async listLoops(limit = 50, offset = 0) {
        try {
            const response = await this.backend.sendCommand('loops.list', {
                limit,
                offset
            });

            if (!response.success) {
                throw new Error(response.error || 'Failed to list loops');
            }

            return response.data;

        } catch (error) {
            this.logger.error('LoopModel', 'Failed to list loops:', error);
            throw error;
        }
    }

    /**
     * Supprime un loop
     * @param {string} loopId - ID du loop
     */
    async deleteLoop(loopId) {
        this.logger.info('LoopModel', `Deleting loop: ${loopId}`);

        try {
            const response = await this.backend.sendCommand('loops.delete', {
                loop_id: loopId
            });

            if (!response.success) {
                throw new Error(response.error || 'Failed to delete loop');
            }

            this.loops.delete(loopId);

            if (this.currentLoop?.id === loopId) {
                this.currentLoop = null;
            }

            // âœ… CORRIGÃ‰ v3.0.1 - Utilise eventBus
            this.eventBus.emit('loop:deleted', { loopId });

            this.logger.info('LoopModel', `âœ“ Loop deleted: ${loopId}`);

            return true;

        } catch (error) {
            this.logger.error('LoopModel', 'Failed to delete loop:', error);
            throw error;
        }
    }

    // ========================================================================
    // EXPORT
    // ========================================================================

    /**
     * Exporte le loop en MidiJSON
     * @returns {Object} MidiJSON
     */
    exportToMidiJson() {
        if (!this.currentLoop) {
            throw new Error('No loop to export');
        }

        const loop = this.currentLoop;

        // Convertir les layers en tracks MIDI
        const tracks = loop.layers.map((layer, index) => ({
            index: index,
            channel: layer.channel,
            instrument: layer.instrument,
            events: layer.events.map(e => ({
                type: e.type,
                time: e.time,
                note: e.note,
                velocity: e.velocity,
                duration: e.duration
            }))
        }));

        const midiJson = {
            metadata: {
                name: loop.name,
                tempo: loop.tempo,
                timeSignature: loop.timeSignature,
                duration: loop.duration,
                bars: loop.bars,
                createdAt: loop.createdAt,
                exportedAt: Date.now()
            },
            tracks: tracks
        };

        this.logger.info('LoopModel', `Loop exported to MidiJSON: ${loop.name}`);

        return midiJson;
    }

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * Configure la quantization
     * @param {boolean} enabled - Activer/dÃ©sactiver
     * @param {number} resolution - RÃ©solution en ms
     */
    setQuantize(enabled, resolution = 480) {
        this.quantizeOnRecord = enabled;
        this.quantizeResolution = resolution;

        this.logger.info('LoopModel', 
            `Quantize: ${enabled ? 'ON' : 'OFF'} (${resolution}ms)`);
    }

    // ========================================================================
    // GETTERS
    // ========================================================================

    /**
     * RÃ©cupÃ¨re le loop actuel
     * @returns {Object|null}
     */
    getCurrentLoop() {
        return this.currentLoop;
    }

    /**
     * RÃ©cupÃ¨re l'Ã©tat complet
     * @returns {Object}
     */
    getState() {
        return {
            isRecording: this.isRecording,
            isPlaying: this.isPlaying,
            currentLoop: this.currentLoop ? {
                id: this.currentLoop.id,
                name: this.currentLoop.name,
                duration: this.currentLoop.duration,
                bars: this.currentLoop.bars,
                tempo: this.currentLoop.tempo,
                layerCount: this.currentLoop.layers.length
            } : null,
            loopPosition: this.loopPosition,
            recordMode: this.recordMode,
            quantizeEnabled: this.quantizeOnRecord,
            quantizeResolution: this.quantizeResolution
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = LoopModel;
}

if (typeof window !== 'undefined') {
    window.LoopModel = LoopModel;
}

// Export par défaut
window.LoopModel = LoopModel;