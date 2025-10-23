// ============================================================================
// Fichier: frontend/js/controllers/PianoRollController.js
// Projet: MidiMind v3.0 - SystÃƒÂ¨me d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   ContrÃƒÂ´leur de logique mÃƒÂ©tier du piano roll (ÃƒÂ©dition graphique des notes).
//   GÃƒÂ¨re la sÃƒÂ©lection, ÃƒÂ©dition, crÃƒÂ©ation, suppression de notes MIDI.
//
// FonctionnalitÃƒÂ©s:
//   - SÃƒÂ©lection notes (simple, multiple, rectangle)
//   - Ãƒâ€°dition notes (dÃƒÂ©placer, redimensionner)
//   - CrÃƒÂ©ation notes (pencil tool)
//   - Suppression notes (eraser tool)
//   - Copier/Coller notes
//   - Quantization (snap to grid)
//   - Transpose (pitch shift)
//   - VÃƒÂ©locitÃƒÂ© batch (modifier plusieurs notes)
//   - Undo/Redo complet
//
// Architecture:
//   PianoRollController extends BaseController
//   - Utilise EditorModel pour donnÃƒÂ©es
//   - Utilise PianoRollView pour rendu
//   - HistoryManager pour undo/redo
//   - SelectionManager pour sÃƒÂ©lection
//
// Auteur: MidiMind Team
// ============================================================================


class PianoRollController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Composants principaux
        this.midiParser = new MidiParser();
        this.syncManager = new MidiSyncManager(eventBus, debugConsole);
        this.renderer = new PianoRollRenderer();
        
        // Ãƒâ€°tat du contrÃƒÂ´leur
        this.channelStates = {};
        this.selectedNotes = new Set();
        this.midiData = null;
        this.isPlaying = false;
        this.currentTime = 0;
        this.playbackPosition = 0;
        
        // SystÃƒÂ¨me de visualisation avancÃƒÂ©
        this.visualizer = {
            zoom: 1.0,
            scrollPosition: 0,
            selectedChannel: -1, // -1 = tous les canaux
            viewMode: 'piano_roll', // piano_roll, notation, timeline
            showVelocity: true,
            showTiming: true
        };
        
        // Cache pour les performances
        this.renderCache = new Map();
        this.noteCache = new Map();
        this.lastRenderTime = 0;
        
        // MÃƒÂ©triques de performance
        this.performanceStats = {
            notesRendered: 0,
            renderTime: 0,
            cacheHits: 0,
            cacheMisses: 0
        };
    }

    bindEvents() {
        // Ãƒâ€°vÃƒÂ©nements de changement d'ÃƒÂ©tat
        this.eventBus.on('statemodel:changed', (data) => {
            this.handleStateChange(data);
        });
        
        // Ãƒâ€°vÃƒÂ©nements MIDI spÃƒÂ©cifiques
        this.eventBus.on('midi:file_added', (data) => {
            this.onMidiFileAdded(data);
        });
        
        this.eventBus.on('midi:file_loaded', (data) => {
            this.loadMidiFile(data.file, data.midiData);
        });
        
        // Ãƒâ€°vÃƒÂ©nements de synchronisation
        this.eventBus.on('sync:offsets_updated', (data) => {
            this.updateSyncOffsets(data);
        });
        
        this.eventBus.on('sync:playback_started', () => {
            this.onSyncPlaybackStarted();
        });
        
        this.eventBus.on('sync:playback_stopped', () => {
            this.onSyncPlaybackStopped();
        });
        
        // Ãƒâ€°vÃƒÂ©nements d'interface
        this.eventBus.on('pianoroll:zoom', (data) => {
            this.setZoom(data.zoom);
        });
        
        this.eventBus.on('pianoroll:scroll', (data) => {
            this.setScrollPosition(data.position);
        });
        
        this.eventBus.on('pianoroll:select_channel', (data) => {
            this.selectChannel(data.channel);
        });
    }

    /**
     * GÃƒÂ¨re les changements d'ÃƒÂ©tat global
     */
    handleStateChange(data) {
        switch (data.key) {
            case 'currentFile':
                if (data.newValue && data.newValue.parsedData) {
                    this.loadMidiFile(data.newValue, data.newValue.parsedData);
                } else {
                    this.clearMidiData();
                }
                break;
                
            case 'isPlaying':
                this.isPlaying = data.newValue;
                if (!this.isPlaying) {
                    this.stopAllNotes();
                }
                break;
                
            case 'progress':
                this.currentTime = data.newValue;
                this.updatePlaybackPosition();
                break;
                
            case 'playbackPosition':
                this.playbackPosition = data.newValue;
                this.updateVisualizerPosition();
                break;
        }
    }

    /**
     * Charge un fichier MIDI avec ses donnÃƒÂ©es parsÃƒÂ©es
     */
    loadMidiFile(file, midiData) {
        if (!file || !midiData) {
            this.clearMidiData();
            return;
        }

        this.debugConsole.log('pianoroll', `Chargement fichier: ${file.name}`);
        
        // Stocker les donnÃƒÂ©es
        this.midiData = midiData;
        
        // Initialiser les ÃƒÂ©tats des canaux avec les vraies donnÃƒÂ©es
        this.initializeChannelStates();
        
        // PrÃƒÂ©parer la synchronisation
        this.syncManager.prepareSyncForFile(midiData);
        
        // Initialiser le cache de rendu
        this.initializeRenderCache();
        
        // RafraÃƒÂ®chir l'affichage
        this.refreshDisplay();
        
        // Ãƒâ€°mettre les ÃƒÂ©vÃƒÂ©nements pour les autres composants
        this.eventBus.emit('pianoroll:file_loaded', {
            file,
            midiData,
            channelStates: this.channelStates
        });
        
        this.debugConsole.log('pianoroll', 
            `Fichier chargÃƒÂ©: ${midiData.tracks.length} pistes, ${midiData.allNotes.length} notes, ${midiData.duration.toFixed(1)}s`);
    }

    /**
     * Initialise les ÃƒÂ©tats des canaux avec les vraies donnÃƒÂ©es MIDI
     */
    initializeChannelStates() {
        this.channelStates = {};
        
        if (!this.midiData) return;
        
        // Analyser tous les canaux utilisÃƒÂ©s
        const usedChannels = new Set();
        this.midiData.tracks.forEach(track => {
            usedChannels.add(track.channel);
        });
        
        // CrÃƒÂ©er les ÃƒÂ©tats pour chaque canal utilisÃƒÂ©
        usedChannels.forEach(channel => {
            const trackData = this.midiData.tracks.find(t => t.channel === channel);
            
            this.channelStates[channel] = {
                channel: channel,
                muted: false,
                solo: false,
                volume: 1.0,
                selected: false,
                visible: true,
                
                // DonnÃƒÂ©es du canal
                name: trackData?.name || `Canal ${channel + 1}`,
                instrument: trackData?.instrument || 'Piano',
                noteCount: trackData?.notes.length || 0,
                noteRange: this.calculateNoteRange(trackData?.notes || []),
                
                // Synchronisation
                syncOffset: 0,
                latencyCompensation: 0,
                instrumentId: null, // Ãƒâ‚¬ assigner lors du routage
                
                // Visualisation
                color: this.getChannelColor(channel),
                opacity: 1.0,
                renderPriority: trackData?.notes.length > 500 ? 'high' : 'normal'
            };
        });
        
        // Optimiser l'ordre de rendu
        this.optimizeRenderOrder();
    }

    /**
     * Calcule la plage de notes pour un canal
     */
    calculateNoteRange(notes) {
        if (notes.length === 0) return { min: 60, max: 60 };
        
        const pitches = notes.map(n => n.pitch);
        return {
            min: Math.min(...pitches),
            max: Math.max(...pitches),
            span: Math.max(...pitches) - Math.min(...pitches)
        };
    }

    /**
     * GÃƒÂ©nÃƒÂ¨re une couleur pour un canal
     */
    getChannelColor(channel) {
        const colors = [
            '#3498db', '#e74c3c', '#2ecc71', '#f39c12', '#9b59b6',
            '#1abc9c', '#e67e22', '#34495e', '#95a5a6', '#d35400',
            '#8e44ad', '#27ae60', '#c0392b', '#f1c40f', '#2c3e50', '#7f8c8d'
        ];
        return colors[channel % colors.length];
    }

    /**
     * Optimise l'ordre de rendu des canaux
     */
    optimizeRenderOrder() {
        // Trier les canaux par prioritÃƒÂ© de rendu (notes plus nombreuses en premier)
        this.renderOrder = Object.keys(this.channelStates)
            .map(Number)
            .sort((a, b) => {
                const stateA = this.channelStates[a];
                const stateB = this.channelStates[b];
                
                // PrioritÃƒÂ© aux canaux avec beaucoup de notes
                if (stateA.renderPriority === 'high' && stateB.renderPriority !== 'high') return -1;
                if (stateB.renderPriority === 'high' && stateA.renderPriority !== 'high') return 1;
                
                // Ensuite par nombre de notes
                return stateB.noteCount - stateA.noteCount;
            });
    }

    /**
     * Initialise le cache de rendu pour les performances
     */
    initializeRenderCache() {
        this.renderCache.clear();
        this.noteCache.clear();
        
        if (!this.midiData) return;
        
        // PrÃƒÂ©-calculer les positions de rendu pour les notes frÃƒÂ©quentes
        this.midiData.tracks.forEach(track => {
            if (track.notes.length > 100) {
                // Cache les notes par segments temporels
                const segments = this.segmentNotes(track.notes, 10); // Segments de 10s
                segments.forEach((segment, index) => {
                    const cacheKey = `track_${track.number}_segment_${index}`;
                    this.noteCache.set(cacheKey, this.preprocessNotes(segment));
                });
            }
        });
        
        this.debugConsole.log('pianoroll', `Cache initialisÃƒÂ©: ${this.noteCache.size} segments`);
    }

    /**
     * Segmente les notes par intervalles temporels
     */
    segmentNotes(notes, segmentDuration) {
        const segments = [];
        let currentSegment = [];
        let segmentStart = 0;
        
        notes.forEach(note => {
            if (note.startTime >= segmentStart + segmentDuration) {
                if (currentSegment.length > 0) {
                    segments.push([...currentSegment]);
                }
                currentSegment = [];
                segmentStart = Math.floor(note.startTime / segmentDuration) * segmentDuration;
            }
            currentSegment.push(note);
        });
        
        if (currentSegment.length > 0) {
            segments.push(currentSegment);
        }
        
        return segments;
    }

    /**
     * PrÃƒÂ©-traite les notes pour le rendu
     */
    preprocessNotes(notes) {
        return notes.map(note => ({
            ...note,
            // PrÃƒÂ©-calculer les coordonnÃƒÂ©es de rendu
            renderX: this.timeToPixel(note.startTime),
            renderY: this.noteToPixel(note.pitch),
            renderWidth: this.durationToPixel(note.duration),
            renderHeight: 8, // Hauteur standard d'une note
            // PrÃƒÂ©-calculer la couleur
            color: this.velocityToColor(note.velocity),
            opacity: this.velocityToOpacity(note.velocity)
        }));
    }

    // ===== CONTRÃƒâ€LES DES CANAUX =====

    /**
     * Active/dÃƒÂ©sactive le mute d'un canal
     */
    toggleChannelMute(channel) {
        if (this.channelStates[channel]) {
            this.channelStates[channel].muted = !this.channelStates[channel].muted;
            
            // Si on mute, arrÃƒÂªter les notes en cours
            if (this.channelStates[channel].muted && this.isPlaying) {
                this.stopChannelNotes(channel);
            }
            
            this.refreshChannelDisplay(channel);
            
            const state = this.channelStates[channel].muted ? 'mutÃƒÂ©' : 'audible';
            this.debugConsole.log('pianoroll', `Canal ${channel} ${state}`);
            
            this.eventBus.emit('pianoroll:channel_muted', {
                channel,
                muted: this.channelStates[channel].muted
            });
        }
    }

    /**
     * Active/dÃƒÂ©sactive le solo d'un canal
     */
    toggleChannelSolo(channel) {
        if (!this.channelStates[channel]) return;
        
        const wasSolo = this.channelStates[channel].solo;
        
        // Si on active le solo
        if (!wasSolo) {
            // DÃƒÂ©sactiver tous les autres solos
            Object.keys(this.channelStates).forEach(ch => {
                this.channelStates[ch].solo = false;
            });
            // Activer ce canal
            this.channelStates[channel].solo = true;
        } else {
            // DÃƒÂ©sactiver le solo
            this.channelStates[channel].solo = false;
        }
        
        this.refreshDisplay();
        
        this.debugConsole.log('pianoroll', 
            `Canal ${channel} solo ${this.channelStates[channel].solo ? 'activÃƒÂ©' : 'dÃƒÂ©sactivÃƒÂ©'}`);
        
        this.eventBus.emit('pianoroll:channel_solo', {
            channel,
            solo: this.channelStates[channel].solo
        });
    }

    /**
     * Modifie le volume d'un canal
     */
    setChannelVolume(channel, volume) {
        if (this.channelStates[channel]) {
            this.channelStates[channel].volume = Math.max(0, Math.min(1, volume));
            
            this.eventBus.emit('pianoroll:channel_volume', {
                channel,
                volume: this.channelStates[channel].volume
            });
        }
    }

    /**
     * SÃƒÂ©lectionne un canal pour l'affichage
     */
    selectChannel(channel) {
        // DÃƒÂ©sÃƒÂ©lectionner tous les canaux
        Object.keys(this.channelStates).forEach(ch => {
            this.channelStates[ch].selected = false;
        });
        
        // SÃƒÂ©lectionner le canal spÃƒÂ©cifiÃƒÂ© (-1 = tous)
        if (channel >= 0 && this.channelStates[channel]) {
            this.channelStates[channel].selected = true;
        }
        
        this.visualizer.selectedChannel = channel;
        this.refreshDisplay();
        
        this.debugConsole.log('pianoroll', 
            `Canal sÃƒÂ©lectionnÃƒÂ©: ${channel >= 0 ? channel : 'tous'}`);
    }

    // ===== MÃƒâ€°THODES DE RENDU ET CONVERSION =====

    timeToPixel(time) {
        return time * 100 * this.visualizer.zoom; // 100 pixels par seconde
    }

    noteToPixel(note) {
        return (127 - note) * 8; // 8 pixels par note
    }

    durationToPixel(duration) {
        return duration * 100 * this.visualizer.zoom;
    }

    velocityToColor(velocity) {
        const intensity = velocity / 127;
        return `hsl(240, 70%, ${30 + intensity * 50}%)`;
    }

    velocityToOpacity(velocity) {
        return 0.6 + (velocity / 127) * 0.4;
    }

    // ===== GESTION DE LA LECTURE =====

    updatePlaybackPosition() {
        this.playbackPosition = this.currentTime;
        this.updateVisualizerPosition();
        
        // Mettre ÃƒÂ  jour le cache si nÃƒÂ©cessaire
        this.invalidateRenderCacheIfNeeded();
    }

    updateVisualizerPosition() {
        // Calculer la position du curseur de lecture
        const cursorX = this.timeToPixel(this.playbackPosition);
        
        // Auto-scroll si nÃƒÂ©cessaire
        if (this.shouldAutoScroll(cursorX)) {
            this.visualizer.scrollPosition = cursorX - 200; // Garde le curseur visible
        }
        
        this.eventBus.emit('pianoroll:position_updated', {
            time: this.playbackPosition,
            cursorX: cursorX,
            scrollPosition: this.visualizer.scrollPosition
        });
    }

    shouldAutoScroll(cursorX) {
        const viewportWidth = 800; // Largeur de la vue (ÃƒÂ  adapter)
        const leftEdge = this.visualizer.scrollPosition;
        const rightEdge = leftEdge + viewportWidth;
        
        return cursorX < leftEdge || cursorX > rightEdge - 100;
    }

    // ===== MÃƒâ€°THODES UTILITAIRES =====

    clearMidiData() {
        this.midiData = null;
        this.channelStates = {};
        this.renderCache.clear();
        this.noteCache.clear();
        this.selectedNotes.clear();
        
        this.refreshDisplay();
        this.debugConsole.log('pianoroll', 'DonnÃƒÂ©es MIDI effacÃƒÂ©es');
    }

    refreshDisplay() {
        // Invalider le cache de rendu
        this.renderCache.clear();
        
        // DÃƒÂ©clencher le rendu
        this.eventBus.emit('pianoroll:refresh_required', {
            channelStates: this.channelStates,
            visualizer: this.visualizer,
            midiData: this.midiData
        });
    }

    refreshChannelDisplay(channel) {
        this.eventBus.emit('pianoroll:channel_updated', {
            channel,
            state: this.channelStates[channel]
        });
    }

    invalidateRenderCacheIfNeeded() {
        const now = performance.now();
        if (now - this.lastRenderTime > 16) { // 60 FPS max
            this.renderCache.clear();
            this.lastRenderTime = now;
        }
    }

    // ===== MÃƒâ€°THODES DE CONTRÃƒâ€LE DE LECTURE =====

    stopAllNotes() {
        this.eventBus.emit('midi:stop_all_notes');
    }

    stopChannelNotes(channel) {
        this.eventBus.emit('midi:stop_channel_notes', { channel });
    }

    onSyncPlaybackStarted() {
        this.isPlaying = true;
        this.eventBus.emit('pianoroll:playback_started');
    }

    onSyncPlaybackStopped() {
        this.isPlaying = false;
        this.stopAllNotes();
        this.eventBus.emit('pianoroll:playback_stopped');
    }

    onMidiFileAdded(data) {
        // Analyser le nouveau fichier pour des optimisations
        if (data.midiData) {
            this.debugConsole.log('pianoroll', 
                `Nouveau fichier MIDI analysÃƒÂ©: ${data.file.name}`);
        }
    }

    updateSyncOffsets(data) {
        // Mettre ÃƒÂ  jour les offsets de synchronisation
        Object.keys(this.channelStates).forEach(channel => {
            const instrumentId = this.channelStates[channel].instrumentId;
            if (instrumentId && data.offsets[instrumentId]) {
                this.channelStates[channel].syncOffset = data.offsets[instrumentId];
            }
        });
        
        this.debugConsole.log('pianoroll', 'Offsets de synchronisation mis ÃƒÂ  jour');
    }

    // ===== MÃƒâ€°THODES DE ZOOM ET NAVIGATION =====

    setZoom(zoom) {
        this.visualizer.zoom = Math.max(0.1, Math.min(5.0, zoom));
        this.invalidateRenderCacheIfNeeded();
        this.refreshDisplay();
    }

    setScrollPosition(position) {
        this.visualizer.scrollPosition = Math.max(0, position);
        this.refreshDisplay();
    }

    // ===== STATISTIQUES DE PERFORMANCE =====

    getPerformanceStats() {
        return {
            ...this.performanceStats,
            cacheSize: this.renderCache.size + this.noteCache.size,
            channelCount: Object.keys(this.channelStates).length,
            totalNotes: this.midiData?.allNotes.length || 0,
            renderOrder: this.renderOrder
        };
    }



    setSyncOffset(channel, offsetMs) {
        if (this.channelStates[channel]) {
            this.channelStates[channel].syncOffset = offsetMs;
            this.logDebug('midi', `Canal ${channel} dÃƒÂ©calage: ${offsetMs}ms`);
        }
    }

    // Gestion de la lecture
    updatePlayback() {
        if (!this.midiData || !this.isPlaying) return;
        
        // Calculer quelles notes doivent ÃƒÂªtre jouÃƒÂ©es maintenant
        const activeNotes = this.getActiveNotes(this.currentTime);
        
        // Envoyer les messages MIDI avec les dÃƒÂ©lais de synchro
        this.sendMidiMessages(activeNotes);
        
        // Mettre ÃƒÂ  jour l'affichage du piano
        this.updatePianoDisplay(activeNotes);
        
        // RafraÃƒÂ®chir le rendu si nÃƒÂ©cessaire
        if (this.shouldRefreshDisplay()) {
            this.refreshDisplay();
        }
    }

    getActiveNotes(currentTime) {
        const activeNotes = [];
        const tolerance = 0.05; // 50ms de tolÃƒÂ©rance
        
        if (this.midiData) {
            this.midiData.tracks.forEach(track => {
                const channelState = this.channelStates[track.channel];
                if (!channelState || channelState.muted) return;
                
                track.notes.forEach(note => {
                    // Appliquer le dÃƒÂ©calage de synchronisation
                    const adjustedStartTime = note.startTime + (channelState.syncOffset / 1000);
                    const adjustedEndTime = note.endTime + (channelState.syncOffset / 1000);
                    
                    if (currentTime >= adjustedStartTime - tolerance && 
                        currentTime <= adjustedEndTime + tolerance) {
                        activeNotes.push({
                            ...note,
                            channel: track.channel,
                            adjustedStartTime,
                            adjustedEndTime,
                            volume: channelState.volume
                        });
                    }
                });
            });
        }
        
        return activeNotes;
    }

    sendMidiMessages(activeNotes) {
        // Simulation d'envoi MIDI - ici vous intÃƒÂ©greriez votre systÃƒÂ¨me MIDI rÃƒÂ©el
        activeNotes.forEach(note => {
            const instruments = this.getModel('instrument').get('instruments');
            const assignments = this.getModel('state').get('currentFile')?.assignments || {};
            const assignedInstrumentId = assignments[note.channel];
            
            if (assignedInstrumentId) {
                const instrument = instruments.find(i => i.id === assignedInstrumentId);
                if (instrument && instrument.connected) {
                    // Ici vous enverriez le vrai message MIDI
                    this.logDebug('midi', `Ã¢â€ â€™ ${instrument.name}: Note ${note.pitch} V${note.velocity}`);
                }
            }
        });
    }

    updatePianoDisplay(activeNotes) {
        // RÃƒÂ©initialiser toutes les touches
        document.querySelectorAll('.piano-key-display').forEach(key => {
            const isBlack = key.classList.contains('black-key');
            key.style.background = isBlack ? '#2c3e50' : '#ecf0f1';
        });
        
        // Activer les touches correspondant aux notes en cours
        activeNotes.forEach(note => {
            const keyElement = document.getElementById(`pianoKey${note.pitch}`);
            if (keyElement) {
                const isBlack = keyElement.classList.contains('black-key');
                keyElement.style.background = isBlack ? '#e74c3c' : '#3498db';
            }
        });
    }


    // Gestion des sÃƒÂ©lections de notes
    selectNote(noteId) {
        if (this.selectedNotes.has(noteId)) {
            this.selectedNotes.delete(noteId);
        } else {
            this.selectedNotes.add(noteId);
        }
        
        this.updateNoteSelection();
    }

    updateNoteSelection() {
        document.querySelectorAll('.midi-note-real').forEach(noteEl => {
            const noteId = noteEl.dataset.noteId;
            if (this.selectedNotes.has(noteId)) {
                noteEl.style.border = '2px solid #ffff00';
                noteEl.style.zIndex = '200';
            } else {
                noteEl.style.border = 'none';
                noteEl.style.zIndex = '10';
            }
        });
    }

    showNoteMenu(event, noteId) {
        event.preventDefault();
        // Ici vous pourriez afficher un menu contextuel pour ÃƒÂ©diter la note
        console.log('Menu note:', noteId);
    }

    // MÃƒÂ©thodes d'affichage

    shouldRefreshDisplay() {
        // RafraÃƒÂ®chir quand la position change significativement
        return Math.abs(this.currentTime - (this.lastRefreshTime || 0)) > 0.5;
    }

    // ContrÃƒÂ´les de zoom
    zoomIn() {
        const currentZoom = this.renderer.getZoom();
        this.renderer.setZoom(currentZoom * 1.2);
        this.refreshDisplay();
        this.logDebug('system', `Zoom: ${this.renderer.getZoom().toFixed(0)} px/s`);
    }

    zoomOut() {
        const currentZoom = this.renderer.getZoom();
        this.renderer.setZoom(currentZoom / 1.2);
        this.refreshDisplay();
        this.logDebug('system', `Zoom: ${this.renderer.getZoom().toFixed(0)} px/s`);
    }

    // MÃƒÂ©thodes publiques pour l'intÃƒÂ©gration
    getMidiData() {
        return this.midiData;
    }

    getChannelStates() {
        return this.channelStates;
    }

    exportChannelSettings() {
        return {
            channelStates: this.channelStates,
            syncOffsets: this.syncOffsets
        };
    }

    importChannelSettings(settings) {
        if (settings.channelStates) {
            this.channelStates = { ...settings.channelStates };
        }
        if (settings.syncOffsets) {
            this.syncOffsets = { ...settings.syncOffsets };
        }
        this.refreshDisplay();
    }
}
// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PianoRollController;
}

if (typeof window !== 'undefined') {
    window.PianoRollController = PianoRollController;
}

window.PianoRollController = PianoRollController;