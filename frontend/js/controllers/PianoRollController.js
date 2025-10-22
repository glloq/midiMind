// ============================================================================
// Fichier: frontend/js/controllers/PianoRollController.js
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   Contrôleur de logique métier du piano roll (édition graphique des notes).
//   Gère la sélection, édition, création, suppression de notes MIDI.
//
// Fonctionnalités:
//   - Sélection notes (simple, multiple, rectangle)
//   - Édition notes (déplacer, redimensionner)
//   - Création notes (pencil tool)
//   - Suppression notes (eraser tool)
//   - Copier/Coller notes
//   - Quantization (snap to grid)
//   - Transpose (pitch shift)
//   - Vélocité batch (modifier plusieurs notes)
//   - Undo/Redo complet
//
// Architecture:
//   PianoRollController extends BaseController
//   - Utilise EditorModel pour données
//   - Utilise PianoRollView pour rendu
//   - HistoryManager pour undo/redo
//   - SelectionManager pour sélection
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
        
        // État du contrôleur
        this.channelStates = {};
        this.selectedNotes = new Set();
        this.midiData = null;
        this.isPlaying = false;
        this.currentTime = 0;
        this.playbackPosition = 0;
        
        // Système de visualisation avancé
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
        
        // Métriques de performance
        this.performanceStats = {
            notesRendered: 0,
            renderTime: 0,
            cacheHits: 0,
            cacheMisses: 0
        };
    }

    bindEvents() {
        // Événements de changement d'état
        this.eventBus.on('statemodel:changed', (data) => {
            this.handleStateChange(data);
        });
        
        // Événements MIDI spécifiques
        this.eventBus.on('midi:file_added', (data) => {
            this.onMidiFileAdded(data);
        });
        
        this.eventBus.on('midi:file_loaded', (data) => {
            this.loadMidiFile(data.file, data.midiData);
        });
        
        // Événements de synchronisation
        this.eventBus.on('sync:offsets_updated', (data) => {
            this.updateSyncOffsets(data);
        });
        
        this.eventBus.on('sync:playback_started', () => {
            this.onSyncPlaybackStarted();
        });
        
        this.eventBus.on('sync:playback_stopped', () => {
            this.onSyncPlaybackStopped();
        });
        
        // Événements d'interface
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
     * Gère les changements d'état global
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
     * Charge un fichier MIDI avec ses données parsées
     */
    loadMidiFile(file, midiData) {
        if (!file || !midiData) {
            this.clearMidiData();
            return;
        }

        this.debugConsole.log('pianoroll', `Chargement fichier: ${file.name}`);
        
        // Stocker les données
        this.midiData = midiData;
        
        // Initialiser les états des canaux avec les vraies données
        this.initializeChannelStates();
        
        // Préparer la synchronisation
        this.syncManager.prepareSyncForFile(midiData);
        
        // Initialiser le cache de rendu
        this.initializeRenderCache();
        
        // Rafraîchir l'affichage
        this.refreshDisplay();
        
        // Émettre les événements pour les autres composants
        this.eventBus.emit('pianoroll:file_loaded', {
            file,
            midiData,
            channelStates: this.channelStates
        });
        
        this.debugConsole.log('pianoroll', 
            `Fichier chargé: ${midiData.tracks.length} pistes, ${midiData.allNotes.length} notes, ${midiData.duration.toFixed(1)}s`);
    }

    /**
     * Initialise les états des canaux avec les vraies données MIDI
     */
    initializeChannelStates() {
        this.channelStates = {};
        
        if (!this.midiData) return;
        
        // Analyser tous les canaux utilisés
        const usedChannels = new Set();
        this.midiData.tracks.forEach(track => {
            usedChannels.add(track.channel);
        });
        
        // Créer les états pour chaque canal utilisé
        usedChannels.forEach(channel => {
            const trackData = this.midiData.tracks.find(t => t.channel === channel);
            
            this.channelStates[channel] = {
                channel: channel,
                muted: false,
                solo: false,
                volume: 1.0,
                selected: false,
                visible: true,
                
                // Données du canal
                name: trackData?.name || `Canal ${channel + 1}`,
                instrument: trackData?.instrument || 'Piano',
                noteCount: trackData?.notes.length || 0,
                noteRange: this.calculateNoteRange(trackData?.notes || []),
                
                // Synchronisation
                syncOffset: 0,
                latencyCompensation: 0,
                instrumentId: null, // À assigner lors du routage
                
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
     * Génère une couleur pour un canal
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
        // Trier les canaux par priorité de rendu (notes plus nombreuses en premier)
        this.renderOrder = Object.keys(this.channelStates)
            .map(Number)
            .sort((a, b) => {
                const stateA = this.channelStates[a];
                const stateB = this.channelStates[b];
                
                // Priorité aux canaux avec beaucoup de notes
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
        
        // Pré-calculer les positions de rendu pour les notes fréquentes
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
        
        this.debugConsole.log('pianoroll', `Cache initialisé: ${this.noteCache.size} segments`);
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
     * Pré-traite les notes pour le rendu
     */
    preprocessNotes(notes) {
        return notes.map(note => ({
            ...note,
            // Pré-calculer les coordonnées de rendu
            renderX: this.timeToPixel(note.startTime),
            renderY: this.noteToPixel(note.pitch),
            renderWidth: this.durationToPixel(note.duration),
            renderHeight: 8, // Hauteur standard d'une note
            // Pré-calculer la couleur
            color: this.velocityToColor(note.velocity),
            opacity: this.velocityToOpacity(note.velocity)
        }));
    }

    // ===== CONTRÔLES DES CANAUX =====

    /**
     * Active/désactive le mute d'un canal
     */
    toggleChannelMute(channel) {
        if (this.channelStates[channel]) {
            this.channelStates[channel].muted = !this.channelStates[channel].muted;
            
            // Si on mute, arrêter les notes en cours
            if (this.channelStates[channel].muted && this.isPlaying) {
                this.stopChannelNotes(channel);
            }
            
            this.refreshChannelDisplay(channel);
            
            const state = this.channelStates[channel].muted ? 'muté' : 'audible';
            this.debugConsole.log('pianoroll', `Canal ${channel} ${state}`);
            
            this.eventBus.emit('pianoroll:channel_muted', {
                channel,
                muted: this.channelStates[channel].muted
            });
        }
    }

    /**
     * Active/désactive le solo d'un canal
     */
    toggleChannelSolo(channel) {
        if (!this.channelStates[channel]) return;
        
        const wasSolo = this.channelStates[channel].solo;
        
        // Si on active le solo
        if (!wasSolo) {
            // Désactiver tous les autres solos
            Object.keys(this.channelStates).forEach(ch => {
                this.channelStates[ch].solo = false;
            });
            // Activer ce canal
            this.channelStates[channel].solo = true;
        } else {
            // Désactiver le solo
            this.channelStates[channel].solo = false;
        }
        
        this.refreshDisplay();
        
        this.debugConsole.log('pianoroll', 
            `Canal ${channel} solo ${this.channelStates[channel].solo ? 'activé' : 'désactivé'}`);
        
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
     * Sélectionne un canal pour l'affichage
     */
    selectChannel(channel) {
        // Désélectionner tous les canaux
        Object.keys(this.channelStates).forEach(ch => {
            this.channelStates[ch].selected = false;
        });
        
        // Sélectionner le canal spécifié (-1 = tous)
        if (channel >= 0 && this.channelStates[channel]) {
            this.channelStates[channel].selected = true;
        }
        
        this.visualizer.selectedChannel = channel;
        this.refreshDisplay();
        
        this.debugConsole.log('pianoroll', 
            `Canal sélectionné: ${channel >= 0 ? channel : 'tous'}`);
    }

    // ===== MÉTHODES DE RENDU ET CONVERSION =====

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
        
        // Mettre à jour le cache si nécessaire
        this.invalidateRenderCacheIfNeeded();
    }

    updateVisualizerPosition() {
        // Calculer la position du curseur de lecture
        const cursorX = this.timeToPixel(this.playbackPosition);
        
        // Auto-scroll si nécessaire
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
        const viewportWidth = 800; // Largeur de la vue (à adapter)
        const leftEdge = this.visualizer.scrollPosition;
        const rightEdge = leftEdge + viewportWidth;
        
        return cursorX < leftEdge || cursorX > rightEdge - 100;
    }

    // ===== MÉTHODES UTILITAIRES =====

    clearMidiData() {
        this.midiData = null;
        this.channelStates = {};
        this.renderCache.clear();
        this.noteCache.clear();
        this.selectedNotes.clear();
        
        this.refreshDisplay();
        this.debugConsole.log('pianoroll', 'Données MIDI effacées');
    }

    refreshDisplay() {
        // Invalider le cache de rendu
        this.renderCache.clear();
        
        // Déclencher le rendu
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

    // ===== MÉTHODES DE CONTRÔLE DE LECTURE =====

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
                `Nouveau fichier MIDI analysé: ${data.file.name}`);
        }
    }

    updateSyncOffsets(data) {
        // Mettre à jour les offsets de synchronisation
        Object.keys(this.channelStates).forEach(channel => {
            const instrumentId = this.channelStates[channel].instrumentId;
            if (instrumentId && data.offsets[instrumentId]) {
                this.channelStates[channel].syncOffset = data.offsets[instrumentId];
            }
        });
        
        this.debugConsole.log('pianoroll', 'Offsets de synchronisation mis à jour');
    }

    // ===== MÉTHODES DE ZOOM ET NAVIGATION =====

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


    setChannelVolume(channel, volume) {
        if (this.channelStates[channel]) {
            this.channelStates[channel].volume = Math.max(0, Math.min(1, volume));
            this.logDebug('midi', `Canal ${channel} volume: ${Math.round(volume * 100)}%`);
        }
    }

    setSyncOffset(channel, offsetMs) {
        if (this.channelStates[channel]) {
            this.channelStates[channel].syncOffset = offsetMs;
            this.logDebug('midi', `Canal ${channel} décalage: ${offsetMs}ms`);
        }
    }

    // Gestion de la lecture
    updatePlayback() {
        if (!this.midiData || !this.isPlaying) return;
        
        // Calculer quelles notes doivent être jouées maintenant
        const activeNotes = this.getActiveNotes(this.currentTime);
        
        // Envoyer les messages MIDI avec les délais de synchro
        this.sendMidiMessages(activeNotes);
        
        // Mettre à jour l'affichage du piano
        this.updatePianoDisplay(activeNotes);
        
        // Rafraîchir le rendu si nécessaire
        if (this.shouldRefreshDisplay()) {
            this.refreshDisplay();
        }
    }

    getActiveNotes(currentTime) {
        const activeNotes = [];
        const tolerance = 0.05; // 50ms de tolérance
        
        if (this.midiData) {
            this.midiData.tracks.forEach(track => {
                const channelState = this.channelStates[track.channel];
                if (!channelState || channelState.muted) return;
                
                track.notes.forEach(note => {
                    // Appliquer le décalage de synchronisation
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
        // Simulation d'envoi MIDI - ici vous intégreriez votre système MIDI réel
        activeNotes.forEach(note => {
            const instruments = this.getModel('instrument').get('instruments');
            const assignments = this.getModel('state').get('currentFile')?.assignments || {};
            const assignedInstrumentId = assignments[note.channel];
            
            if (assignedInstrumentId) {
                const instrument = instruments.find(i => i.id === assignedInstrumentId);
                if (instrument && instrument.connected) {
                    // Ici vous enverriez le vrai message MIDI
                    this.logDebug('midi', `→ ${instrument.name}: Note ${note.pitch} V${note.velocity}`);
                }
            }
        });
    }

    updatePianoDisplay(activeNotes) {
        // Réinitialiser toutes les touches
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

    stopAllNotes() {
        // Arrêter toutes les notes en cours
        this.updatePianoDisplay([]);
        this.logDebug('midi', 'Toutes les notes arrêtées');
    }

    // Gestion des sélections de notes
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
        // Ici vous pourriez afficher un menu contextuel pour éditer la note
        console.log('Menu note:', noteId);
    }

    // Méthodes d'affichage
    refreshDisplay() {
        if (!this.midiData || !app.navigationController || app.navigationController.getCurrentPage() !== 'home') {
            return;
        }
        
        const state = this.getModel('state');
        const settings = state.get('settings') || {};
        const viewWindow = settings.noteDisplayWindow || 30;
        
        const canvas = document.getElementById('midiVisualizerCanvas');
        if (canvas) {
            const pianoRollHTML = this.renderer.renderPianoRoll(
                this.midiData,
                this.currentTime,
                viewWindow,
                this.channelStates
            );
            canvas.innerHTML = pianoRollHTML;
        }
    }

    shouldRefreshDisplay() {
        // Rafraîchir quand la position change significativement
        return Math.abs(this.currentTime - (this.lastRefreshTime || 0)) > 0.5;
    }

    // Contrôles de zoom
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

    // Méthodes publiques pour l'intégration
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