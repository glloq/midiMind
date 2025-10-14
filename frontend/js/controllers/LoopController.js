// ============================================================================
// Fichier: frontend/js/controllers/LoopController.js
// Version: 3.0.0
// Date: 2025-10-10
// ============================================================================
// Description:
//   Contrôleur du Loop Recorder avec NotificationManager intégré.
//   Remplace tous les alerts/prompts par des notifications toast élégantes.
//
// Changelog v3.0.0:
//   - Intégration NotificationManager
//   - Remplacement alerts → toast notifications
//   - Remplacement prompts → dialogs personnalisés
//   - Amélioration feedback utilisateur
//   - Messages plus descriptifs
// ============================================================================

/**
 * @class LoopController
 * @description Contrôleur du Loop Recorder avec notifications
 */
class LoopController {
    constructor(eventBus) {
        this.eventBus = eventBus;
        this.model = null;
        this.view = null;
        this.metronome = null;
        this.keyboardController = null;
        this.notificationManager = null;
        
        // Métronome
        this.metronomeEnabled = false;
        this.countInBars = 1;
        this.countInCounter = 0;
        this.isCountingIn = false;
        this.countInInterval = null;
        
        // Update timer
        this.updateTimer = null;
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    /**
     * Initialise le contrôleur
     */
    init(model, view, metronome, keyboardController) {
        this.model = model;
        this.view = view;
        this.metronome = metronome;
        this.keyboardController = keyboardController;

        // Initialiser le gestionnaire de notifications
        this.notificationManager = new NotificationManager();

        this.attachModelEvents();
        this.attachKeyboardEvents();
        this.setupKeyboardShortcuts();
        this.startUpdateLoop();
        
        console.log('LoopController: Initialized');
    }

    /**
     * Attache les événements du modèle
     */
    attachModelEvents() {
        // Loop créé
        this.model.on('loop:created', (loop) => {
            this.view.updateLayers(loop.layers);
            this.notificationManager.success(
                `Loop created: ${loop.bars} bars at ${loop.tempo} BPM`
            );
        });

        // Enregistrement
        this.model.on('recording:started', () => {
            this.view.updateButtonStates(this.model.getState());
            this.notificationManager.info('Recording started...', 0);
        });

        this.model.on('recording:stopped', (data) => {
            this.view.updateButtonStates(this.model.getState());
            this.view.updateLayers(this.model.currentLoop?.layers || []);
            this.notificationManager.success(
                `Recording stopped`,
                3000,
                { details: `${data.eventCount} MIDI events captured` }
            );
        });

        // Playback
        this.model.on('loop:playing', () => {
            this.view.updateButtonStates(this.model.getState());
        });

        this.model.on('loop:stopped', () => {
            this.view.updateButtonStates(this.model.getState());
        });

        this.model.on('loop:paused', () => {
            this.view.updateButtonStates(this.model.getState());
        });

        this.model.on('loop:cycle', () => {
            if (this.metronomeEnabled && this.model.isPlaying) {
                this.metronome.playAccent();
            }
        });

        // Layers
        this.model.on('layer:muted', ({ layerId, muted }) => {
            this.view.updateLayers(this.model.currentLoop?.layers || []);
            this.notificationManager.info(
                `Layer ${muted ? 'muted' : 'unmuted'}`,
                2000
            );
        });

        this.model.on('layer:solo', ({ layerId, solo }) => {
            this.view.updateLayers(this.model.currentLoop?.layers || []);
            this.notificationManager.info(
                `Solo ${solo ? 'enabled' : 'disabled'}`,
                2000
            );
        });

        this.model.on('layer:volume', () => {
            this.view.updateLayers(this.model.currentLoop?.layers || []);
        });

        this.model.on('layer:cleared', () => {
            this.view.updateLayers(this.model.currentLoop?.layers || []);
            this.notificationManager.success('Layer deleted');
        });

        this.model.on('loop:cleared', () => {
            this.view.updateLayers([]);
            this.notificationManager.warning('All layers cleared');
        });

        // Persistence
        this.model.on('loop:saved', (loop) => {
            this.notificationManager.success(
                `Loop saved: ${loop.name}`,
                3000,
                { details: `${loop.layers.length} layers, ${loop.bars} bars` }
            );
        });

        this.model.on('loop:loaded', (loop) => {
            this.view.updateLayers(loop.layers);
            this.notificationManager.success(
                `Loop loaded: ${loop.name}`,
                3000,
                { details: `${loop.layers.length} layers, ${loop.bars} bars` }
            );
        });

        this.model.on('loop:deleted', () => {
            this.notificationManager.success('Loop deleted');
        });
    }

    /**
     * Attache les événements du clavier
     */
    attachKeyboardEvents() {
        if (!this.keyboardController) {
            console.warn('LoopController: KeyboardController not available');
            return;
        }

        // Écouter les notes du clavier pour enregistrement
        this.eventBus.on('keyboard:note', (event) => {
            if (this.model.isRecording) {
                this.model.recordEvent(event);
            }
        });
    }

    /**
     * Configure les raccourcis clavier
     */
    setupKeyboardShortcuts() {
        document.addEventListener('keydown', (e) => {
            // Ignorer si on est dans un input
            if (e.target.tagName === 'INPUT' || 
                e.target.tagName === 'TEXTAREA' ||
                e.target.isContentEditable) {
                return;
            }

            switch(e.key.toLowerCase()) {
                case 'r':
                    e.preventDefault();
                    this.toggleRecord();
                    break;
                    
                case ' ':
                    e.preventDefault();
                    this.togglePlay();
                    break;
                    
                case 's':
                    e.preventDefault();
                    this.stop();
                    break;
                    
                case 'c':
                    if (e.ctrlKey || e.metaKey) {
                        return;
                    }
                    e.preventDefault();
                    this.promptClearLoop();
                    break;
                    
                case 'm':
                    e.preventDefault();
                    this.toggleMetronome();
                    break;
            }
        });
        
        console.log('LoopController: Keyboard shortcuts enabled');
    }

    /**
     * Démarre la boucle de mise à jour
     */
    startUpdateLoop() {
        this.updateTimer = setInterval(() => {
            const state = this.model.getState();
            
            if (state.isPlaying && state.currentLoop) {
                this.view.drawTimeline(state.currentLoop, state.loopPosition);
                this.view.updatePosition(state.loopPosition, state.currentLoop.duration);
            }
        }, 50); // 20 FPS
    }

    // ========================================================================
    // CONTRÔLES PRINCIPAUX
    // ========================================================================

    /**
     * Toggle enregistrement
     */
    toggleRecord() {
        if (this.model.isRecording) {
            this.stopRecording();
        } else {
            this.startRecording();
        }
    }

    /**
     * Démarre l'enregistrement
     */
    startRecording() {
        // Créer une boucle si nécessaire
        if (!this.model.currentLoop) {
            const bars = parseInt(document.getElementById('loopBars')?.value || 4);
            const tempo = parseInt(document.getElementById('loopTempo')?.value || 120);
            const timeSignature = document.getElementById('loopTimeSignature')?.value || '4/4';
            
            this.model.createLoop(bars, tempo, timeSignature);
        }

        // Obtenir le canal et l'instrument du clavier
        const channel = this.keyboardController?.currentChannel || 0;
        const instrument = this.keyboardController?.currentInstrument || null;
        const mode = document.getElementById('recordMode')?.value || 'overdub';

        // Count-in si activé
        const countInEnabled = document.getElementById('countInEnabled')?.checked || false;
        if (countInEnabled && this.countInBars > 0 && !this.model.isPlaying) {
            this.startCountIn(() => {
                this.model.startRecording(channel, instrument, mode);
                
                if (!this.model.isPlaying) {
                    this.model.playLoop();
                }
            });
        } else {
            this.model.startRecording(channel, instrument, mode);
            
            if (!this.model.isPlaying) {
                this.model.playLoop();
            }
        }
    }

    /**
     * Arrête l'enregistrement
     */
    stopRecording() {
        this.model.stopRecording();
    }

    /**
     * Toggle lecture
     */
    togglePlay() {
        if (this.model.isPlaying) {
            this.pause();
        } else {
            this.play();
        }
    }

    /**
     * Lance la lecture
     */
    play() {
        if (!this.model.currentLoop) {
            // Créer un loop vide si nécessaire
            const bars = parseInt(document.getElementById('loopBars')?.value || 4);
            const tempo = parseInt(document.getElementById('loopTempo')?.value || 120);
            const timeSignature = document.getElementById('loopTimeSignature')?.value || '4/4';
            
            this.model.createLoop(bars, tempo, timeSignature);
        }

        this.model.playLoop();

        // Démarrer le métronome si activé
        if (this.metronomeEnabled) {
            this.startMetronome();
        }
    }

    /**
     * Pause la lecture
     */
    pause() {
        this.model.pauseLoop();
        this.stopMetronome();
    }

    /**
     * Arrête la lecture
     */
    stop() {
        this.model.stopLoop();
        this.stopMetronome();
        
        if (this.isCountingIn) {
            this.cancelCountIn();
        }
    }

    /**
     * Prompt pour effacer la boucle
     */
    promptClearLoop() {
        if (!this.model.currentLoop || this.model.currentLoop.layers.length === 0) {
            this.notificationManager.warning('No layers to clear');
            return;
        }

        // Utiliser une notification avec action
        this.notificationManager.warning(
            'Clear all layers?',
            0,
            {
                action: {
                    label: 'Clear',
                    handler: 'loopController.clearLoop()'
                }
            }
        );
    }

    /**
     * Efface toute la boucle
     */
    clearLoop() {
        this.stop();
        this.model.clearLoop();
    }

    // ========================================================================
    // COUNT-IN
    // ========================================================================

    /**
     * Démarre le count-in
     */
    startCountIn(callback) {
        this.isCountingIn = true;
        this.countInCounter = this.countInBars;

        const loop = this.model.currentLoop;
        const [numerator] = loop.timeSignature.split('/').map(Number);
        const beatDuration = 60000 / loop.tempo;
        const barDuration = beatDuration * numerator;

        // Afficher le count-in
        this.view.showCountIn(this.countInCounter);

        // Métronome pour le count-in
        if (this.metronomeEnabled) {
            this.metronome.start(loop.tempo, numerator);
        }

        this.countInInterval = setInterval(() => {
            this.countInCounter--;
            
            if (this.countInCounter > 0) {
                this.view.showCountIn(this.countInCounter);
                
                if (this.metronomeEnabled) {
                    this.metronome.playAccent();
                }
            } else {
                // Count-in terminé
                this.view.hideCountIn();
                clearInterval(this.countInInterval);
                this.countInInterval = null;
                this.isCountingIn = false;
                
                // Exécuter le callback
                callback();
            }
        }, barDuration);
    }

    /**
     * Annule le count-in
     */
    cancelCountIn() {
        if (this.countInInterval) {
            clearInterval(this.countInInterval);
            this.countInInterval = null;
        }
        
        this.isCountingIn = false;
        this.countInCounter = 0;
        this.view.hideCountIn();
        this.stopMetronome();
        
        this.notificationManager.info('Count-in cancelled');
    }

    // ========================================================================
    // MÉTRONOME
    // ========================================================================

    /**
     * Toggle métronome
     */
    toggleMetronome() {
        this.setMetronome(!this.metronomeEnabled);
    }

    /**
     * Active/désactive le métronome
     */
    setMetronome(enabled) {
        this.metronomeEnabled = enabled;

        if (enabled && this.model.isPlaying) {
            this.startMetronome();
        } else {
            this.stopMetronome();
        }
        
        // Mettre à jour l'UI
        const checkbox = document.getElementById('metronomeEnabled');
        if (checkbox) {
            checkbox.checked = enabled;
        }
        
        this.notificationManager.info(
            `Metronome ${enabled ? 'enabled' : 'disabled'}`,
            2000
        );
    }

    /**
     * Définit le volume du métronome
     */
    setMetronomeVolume(volume) {
        if (this.metronome) {
            this.metronome.setVolume(volume / 100);
        }
    }

    /**
     * Démarre le métronome
     */
    startMetronome() {
        if (!this.metronome || !this.model.currentLoop) return;

        const loop = this.model.currentLoop;
        const [numerator] = loop.timeSignature.split('/').map(Number);

        this.metronome.start(loop.tempo, numerator);
    }

    /**
     * Arrête le métronome
     */
    stopMetronome() {
        if (this.metronome) {
            this.metronome.stop();
        }
    }

    // ========================================================================
    // PARAMÈTRES DE LOOP
    // ========================================================================

    /**
     * Crée un nouveau loop
     */
    createNewLoop() {
        const bars = parseInt(document.getElementById('loopBars')?.value || 4);
        const tempo = parseInt(document.getElementById('loopTempo')?.value || 120);
        const timeSignature = document.getElementById('loopTimeSignature')?.value || '4/4';
        
        this.stop();
        this.model.createLoop(bars, tempo, timeSignature);
    }

    /**
     * Modifie les paramètres du loop
     */
    updateLoopSettings(settings) {
        if (!this.model.currentLoop) return;

        if (settings.bars !== undefined) {
            this.model.currentLoop.bars = settings.bars;
            const [numerator] = this.model.currentLoop.timeSignature.split('/').map(Number);
            const beatDuration = 60000 / this.model.currentLoop.tempo;
            const barDuration = beatDuration * numerator;
            this.model.currentLoop.duration = barDuration * settings.bars;
        }

        if (settings.tempo !== undefined) {
            this.model.currentLoop.tempo = settings.tempo;
            const [numerator] = this.model.currentLoop.timeSignature.split('/').map(Number);
            const beatDuration = 60000 / settings.tempo;
            const barDuration = beatDuration * numerator;
            this.model.currentLoop.duration = barDuration * this.model.currentLoop.bars;
        }

        if (settings.timeSignature !== undefined) {
            this.model.currentLoop.timeSignature = settings.timeSignature;
            const [numerator] = settings.timeSignature.split('/').map(Number);
            const beatDuration = 60000 / this.model.currentLoop.tempo;
            const barDuration = beatDuration * numerator;
            this.model.currentLoop.duration = barDuration * this.model.currentLoop.bars;
        }
        
        this.notificationManager.success('Loop settings updated');
    }

    /**
     * Définit le mode d'enregistrement
     */
    setRecordMode(mode) {
        this.model.recordMode = mode;
        this.notificationManager.info(`Record mode: ${mode}`, 2000);
    }

    /**
     * Active/désactive la quantification
     */
    setQuantize(enabled, resolution) {
        this.model.setQuantize(enabled, resolution);
        if (enabled) {
            this.notificationManager.info(
                `Quantize enabled: 1/${resolution/60} notes`,
                2000
            );
        }
    }

    // ========================================================================
    // GESTION DES LAYERS
    // ========================================================================

    /**
     * Toggle mute d'un layer
     */
    toggleMute(layerId) {
        this.model.muteLayer(layerId);
    }

    /**
     * Toggle solo d'un layer
     */
    toggleSolo(layerId) {
        this.model.soloLayer(layerId);
    }

    /**
     * Définit le volume d'un layer
     */
    setLayerVolume(layerId, volume) {
        this.model.setLayerVolume(layerId, parseInt(volume));
    }

    /**
     * Efface un layer
     */
    clearLayer(layerId) {
        this.notificationManager.warning(
            'Delete this layer?',
            0,
            {
                action: {
                    label: 'Delete',
                    handler: `loopController.confirmClearLayer('${layerId}')`
                }
            }
        );
    }

    /**
     * Confirme l'effacement du layer
     */
    confirmClearLayer(layerId) {
        this.model.clearLayer(layerId);
        this.notificationManager.dismissAll();
    }

    // ========================================================================
    // PERSISTENCE
    // ========================================================================

    /**
     * Exporte en MIDI
     */
    async exportMidi() {
        const midiJson = this.model.exportToMidiJson();
        
        if (!midiJson) {
            this.notificationManager.error('No loop to export');
            return;
        }

        try {
            const fileName = `${this.model.currentLoop.name.replace(/\s+/g, '_')}.mid`;
            
            if (window.fileModel) {
                const file = await window.fileModel.create({
                    name: fileName,
                    midiJson: midiJson
                });

                await window.fileModel.exportToMidi(file.id);
                
                this.notificationManager.success(
                    'Loop exported successfully',
                    3000,
                    { details: fileName }
                );
            } else {
                // Fallback: télécharger le JSON
                const blob = new Blob([JSON.stringify(midiJson, null, 2)], 
                    { type: 'application/json' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = fileName.replace('.mid', '.json');
                a.click();
                URL.revokeObjectURL(url);
                
                this.notificationManager.warning(
                    'Loop exported as JSON',
                    3000,
                    { details: 'FileModel not available for MIDI export' }
                );
            }

        } catch (error) {
            console.error('Error exporting MIDI:', error);
            this.notificationManager.error(
                'Export failed',
                5000,
                { details: error.message }
            );
        }
    }

    /**
     * Sauvegarde la boucle
     */
    async saveLoop() {
        if (!this.model.currentLoop) {
            this.notificationManager.error('No loop to save');
            return;
        }

        // Utiliser prompt natif pour le nom (peut être amélioré avec modal custom)
        const name = prompt('Loop name:', this.model.currentLoop.name);
        if (!name) return;

        try {
            this.model.currentLoop.name = name;
            await this.model.saveLoop();
            // La notification de succès est gérée par l'événement loop:saved
        } catch (error) {
            console.error('Error saving loop:', error);
            this.notificationManager.error(
                'Failed to save loop',
                5000,
                { details: error.message }
            );
        }
    }

    /**
     * Charge une boucle
     */
    async loadLoop() {
        try {
            const list = await this.model.listLoops(20, 0);
            
            if (!list.loops || list.loops.length === 0) {
                this.notificationManager.info('No saved loops found');
                return;
            }

            // Afficher une liste simple (peut être amélioré avec modal custom)
            const loopNames = list.loops.map((l, i) => 
                `${i+1}. ${l.name} (${l.bars} bars, ${l.layers.length} layers)`
            ).join('\n');
            
            const selection = prompt(
                `Select a loop:\n\n${loopNames}\n\nEnter loop name or number:`
            );
            
            if (!selection) return;

            // Trouver le loop
            const loopIndex = parseInt(selection) - 1;
            let selectedLoop;
            
            if (!isNaN(loopIndex) && loopIndex >= 0 && loopIndex < list.loops.length) {
                selectedLoop = list.loops[loopIndex];
            } else {
                selectedLoop = list.loops.find(l => 
                    l.name.toLowerCase().includes(selection.toLowerCase())
                );
            }

            if (!selectedLoop) {
                this.notificationManager.error('Loop not found');
                return;
            }

            // Charger le loop
            this.stop();
            await this.model.loadLoop(selectedLoop.id);
            // La notification de succès est gérée par l'événement loop:loaded
            
        } catch (error) {
            console.error('Error loading loop:', error);
            this.notificationManager.error(
                'Failed to load loop',
                5000,
                { details: error.message }
            );
        }
    }

    // ========================================================================
    // CLEANUP
    // ========================================================================

    /**
     * Nettoie les ressources
     */
    destroy() {
        if (this.updateTimer) {
            clearInterval(this.updateTimer);
            this.updateTimer = null;
        }
        
        this.cancelCountIn();
        this.stopMetronome();
        
        if (this.notificationManager) {
            this.notificationManager.destroy();
        }
        
        console.log('LoopController: Destroyed');
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = LoopController;
}

// Rendre accessible globalement
if (typeof window !== 'undefined') {
    window.LoopController = LoopController;
}

// ============================================================================
// FIN DU FICHIER LoopController.js v3.0.0
// ============================================================================
