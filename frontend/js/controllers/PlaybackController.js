// ============================================================================
// Fichier: frontend/js/controllers/PlaybackController.js
// Version: 3.0.0-refonte
// Date: 2025-10-09
// ============================================================================
// Description:
//   Contrôleur de lecture MIDI - MIGRÉ vers protocole v3.0
//   Gestion play/pause/stop avec événements temps réel.
// ============================================================================

class PlaybackController extends BaseController {
    constructor(model, view, eventBus, backendService, logger) {
        super('PlaybackController', model, view, eventBus, logger);
        
        this.backendService = backendService;
        
        // État local
        this.state = {
            playing: false,
            position: 0,
            duration: 0,
            tempo: 120,
            loop: false,
            volume: 100
        };
        
        this.positionUpdateTimer = null;
        
        this.setupEventListeners();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    setupEventListeners() {
        // Événements UI
        this.eventBus.on('playback:play', () => this.play());
        this.eventBus.on('playback:pause', () => this.pause());
        this.eventBus.on('playback:stop', () => this.stop());
        this.eventBus.on('playback:seek', (data) => this.seek(data.position));
        this.eventBus.on('playback:setTempo', (data) => this.setTempo(data.tempo));
        this.eventBus.on('playback:setLoop', (data) => this.setLoop(data.enabled));
        this.eventBus.on('playback:setVolume', (data) => this.setVolume(data.volume));
        
        // Événements backend (NOUVEAU PROTOCOLE)
        this.eventBus.on('playback:state', (data) => this.handleStateUpdate(data));
        this.eventBus.on('playback:position', (data) => this.handlePositionUpdate(data));
        
		
		// CORRECTION: Auto-advance à la fin de lecture
this.eventBus.on('playback:finished', async () => {
    this.logger.info(this.name, 'Playback finished, checking auto-advance...');
    
    if (!this.playlistModel.get('autoAdvance')) {
        this.logger.debug(this.name, 'Auto-advance disabled');
        return;
    }
    
    // Vérifier si on est en mode queue
    if (this.playlistModel.get('isPlayingQueue') && 
        this.playlistModel.get('queue').length > 0) {
        const nextFile = this.playlistModel.nextInQueue();
        if (nextFile) {
            await this.loadAndPlay(nextFile);
            return;
        }
    }
    
    // Sinon, fichier suivant de la playlist
    const nextFile = this.playlistModel.next();
    if (nextFile) {
        await this.loadAndPlay(nextFile);
    } else {
        this.logger.info(this.name, 'End of playlist reached');
        this.eventBus.emit('playlist:ended');
    }
});


		
		
        // Charger l'état initial
        this.refreshState();
    }
	
	
    // Méthode helper pour charger et jouer
async loadAndPlay(file) {
    try {
        this.logger.info(this.name, `Auto-loading: ${file.name}`);
        
        // Charger via FileController
        await this.eventBus.emit('file:load', { fileId: file.fileId });
        
        // Petit délai pour laisser le backend charger
        await new Promise(resolve => setTimeout(resolve, 100));
        
        // Démarrer lecture
        await this.eventBus.emit('playback:play');
        
        this.logger.info(this.name, 'Auto-play started');
    } catch (error) {
        this.logger.error(this.name, 'Failed to auto-play:', error);
    }
}

next() {
    const nextFile = this.playlistModel.next();
    if (nextFile) {
        this.view.updateCurrentFile(nextFile);
        this.eventBus.emit('playlist:changed', { 
            currentIndex: this.playlistModel.get('currentIndex'),
            file: nextFile
        });
    }
    return nextFile; // CORRECTION: Retourner le fichier
}


    // ========================================================================
    // ACTIONS DE LECTURE
    // ========================================================================
    
    async play() {
		if (!this.backendService || !this.backendService.isConnected()) {
			this.logger.error('PlaybackController', '❌ Backend not connected');
			this.eventBus.emit('notification:error', {
			  title: 'Backend Error',
			  message: 'Cannot start playback: backend disconnected'
			});
			return;
		}
  
        this.logger.info(this.name, 'Play requested');
        
        try {
            const result = await this.backendService.sendCommand('playback.play');
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to play');
            }
            
            this.logger.info(this.name, 'Playing');
            
            // Mettre à jour l'état local
            this.state.playing = true;
            
            // Mettre à jour la vue
            this.view.updatePlaybackState('playing');
            
            // Démarrer le timer de position
            this.startPositionUpdates();
            
            // Notifier
            this.eventBus.emit('playback:started');
            
        } catch (error) {
            this.logger.error(this.name, 'Failed to play:', error);
            this.showError('Failed to start playback: ' + error.message);
        }
    }
    
    async pause() {
        this.logger.info(this.name, 'Pause requested');
        
        try {
            const result = await this.backendService.sendCommand('playback.pause');
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to pause');
            }
            
            this.logger.info(this.name, 'Paused');
            
            // Mettre à jour l'état local
            this.state.playing = false;
            
            // Mettre à jour la vue
            this.view.updatePlaybackState('paused');
            
            // Arrêter le timer de position
            this.stopPositionUpdates();
            
            // Notifier
            this.eventBus.emit('playback:paused');
            
        } catch (error) {
            this.logger.error(this.name, 'Failed to pause:', error);
            this.showError('Failed to pause: ' + error.message);
        }
    }
    
    async stop() {
        this.logger.info(this.name, 'Stop requested');
        
        try {
            const result = await this.backendService.sendCommand('playback.stop');
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to stop');
            }
            
            this.logger.info(this.name, 'Stopped');
            
            // Mettre à jour l'état local
            this.state.playing = false;
            this.state.position = 0;
            
            // Mettre à jour la vue
            this.view.updatePlaybackState('stopped');
            this.view.updatePosition(0, this.state.duration);
            
            // Arrêter le timer de position
            this.stopPositionUpdates();
            
            // Notifier
            this.eventBus.emit('playback:stopped');
            
        } catch (error) {
            this.logger.error(this.name, 'Failed to stop:', error);
            this.showError('Failed to stop: ' + error.message);
        }
    }
    
    async seek(position) {
        this.logger.info(this.name, `Seeking to ${position}s`);
        
        try {
            const result = await this.backendService.sendCommand('playback.seek', {
                position: position
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to seek');
            }
            
            // Mettre à jour l'état local
            this.state.position = position;
            
            // Mettre à jour la vue
            this.view.updatePosition(position, this.state.duration);
            
            this.logger.debug(this.name, `Seeked to ${position}s`);
            
        } catch (error) {
            this.logger.error(this.name, 'Failed to seek:', error);
            this.showError('Failed to seek: ' + error.message);
        }
    }
    
    // ========================================================================
    // PARAMÈTRES DE LECTURE
    // ========================================================================
    
    async setTempo(tempo) {
        this.logger.info(this.name, `Setting tempo to ${tempo} BPM`);
        
        try {
            const result = await this.backendService.sendCommand('playback.setTempo', {
                tempo: tempo
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to set tempo');
            }
            
            // Mettre à jour l'état local
            this.state.tempo = tempo;
            
            // Mettre à jour la vue
            this.view.updateTempo(tempo);
            
            this.logger.debug(this.name, `Tempo set to ${tempo} BPM`);
            
        } catch (error) {
            this.logger.error(this.name, 'Failed to set tempo:', error);
            this.showError('Failed to set tempo: ' + error.message);
        }
    }
    
    async setLoop(enabled) {
        this.logger.info(this.name, `Setting loop: ${enabled}`);
        
        try {
            const result = await this.backendService.sendCommand('playback.setLoop', {
                enabled: enabled
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to set loop');
            }
            
            // Mettre à jour l'état local
            this.state.loop = enabled;
            
            // Mettre à jour la vue
            this.view.updateLoop(enabled);
            
            this.logger.debug(this.name, `Loop ${enabled ? 'enabled' : 'disabled'}`);
            
        } catch (error) {
            this.logger.error(this.name, 'Failed to set loop:', error);
            this.showError('Failed to set loop: ' + error.message);
        }
    }
    
    async setVolume(volume) {
        this.logger.info(this.name, `Setting volume to ${volume}%`);
        
        try {
            const result = await this.backendService.sendCommand('playback.setVolume', {
                volume: volume
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to set volume');
            }
            
            // Mettre à jour l'état local
            this.state.volume = volume;
            
            // Mettre à jour la vue
            this.view.updateVolume(volume);
            
            this.logger.debug(this.name, `Volume set to ${volume}%`);
            
        } catch (error) {
            this.logger.error(this.name, 'Failed to set volume:', error);
            this.showError('Failed to set volume: ' + error.message);
        }
    }
    
    // ========================================================================
    // ÉTAT
    // ========================================================================
    
    async refreshState() {
        try {
            const result = await this.backendService.sendCommand('playback.state');
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to get state');
            }
            
            const stateData = result.data || result;
            
            // Mettre à jour l'état local
            this.state.playing = stateData.playing || false;
            this.state.position = stateData.position || 0;
            this.state.duration = stateData.duration || 0;
            this.state.tempo = stateData.tempo || 120;
            
            // Mettre à jour la vue
            this.view.updatePlaybackState(this.state.playing ? 'playing' : 'stopped');
            this.view.updatePosition(this.state.position, this.state.duration);
            this.view.updateTempo(this.state.tempo);
            
            // Démarrer les mises à jour si en lecture
            if (this.state.playing) {
                this.startPositionUpdates();
            }
            
            this.logger.debug(this.name, 'State refreshed:', this.state);
            
        } catch (error) {
            this.logger.error(this.name, 'Failed to refresh state:', error);
        }
    }
    
    // ========================================================================
    // ÉVÉNEMENTS BACKEND (NOUVEAU)
    // ========================================================================
    
    handleStateUpdate(data) {
        this.logger.debug(this.name, 'State update from backend:', data);
        
        // Mettre à jour l'état local
        if (data.state !== undefined) {
            this.state.playing = (data.state === 'playing');
        }
        if (data.position !== undefined) {
            this.state.position = data.position;
        }
        if (data.duration !== undefined) {
            this.state.duration = data.duration;
        }
        if (data.tempo !== undefined) {
            this.state.tempo = data.tempo;
        }
        if (data.loop !== undefined) {
            this.state.loop = data.loop;
        }
        
        // Mettre à jour la vue
        this.view.updatePlaybackState(data.state || (this.state.playing ? 'playing' : 'stopped'));
        this.view.updatePosition(this.state.position, this.state.duration);
        
        // Gérer le timer de position
        if (this.state.playing && !this.positionUpdateTimer) {
            this.startPositionUpdates();
        } else if (!this.state.playing && this.positionUpdateTimer) {
            this.stopPositionUpdates();
        }
    }
    
    handlePositionUpdate(data) {
        if (data.position !== undefined) {
            this.state.position = data.position;
            this.view.updatePosition(this.state.position, this.state.duration);
        }
    }
    
    // ========================================================================
    // TIMER DE POSITION
    // ========================================================================
    
    startPositionUpdates() {
        if (this.positionUpdateTimer) {
            return;
        }
        
        this.positionUpdateTimer = setInterval(() => {
            // Mettre à jour la position locale (estimation)
            if (this.state.playing) {
                this.state.position += 0.1; // 100ms
                
                // Ne pas dépasser la durée
                if (this.state.position > this.state.duration) {
                    if (this.state.loop) {
                        this.state.position = 0;
                    } else {
                        this.state.position = this.state.duration;
                        this.stopPositionUpdates();
                    }
                }
                
                // Mettre à jour la vue
                this.view.updatePosition(this.state.position, this.state.duration);
            }
        }, 100); // Mise à jour toutes les 100ms
    }
    
    stopPositionUpdates() {
        if (this.positionUpdateTimer) {
            clearInterval(this.positionUpdateTimer);
            this.positionUpdateTimer = null;
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    showError(message) {
        this.eventBus.emit('notification:show', {
            type: 'error',
            message: message,
            duration: 5000
        });
    }
    
    // ========================================================================
    // API PUBLIQUE
    // ========================================================================
    
    getState() {
        return { ...this.state };
    }
    
    isPlaying() {
        return this.state.playing;
    }
    
    getPosition() {
        return this.state.position;
    }
    
    getDuration() {
        return this.state.duration;
    }
    
    // ========================================================================
    // CLEANUP
    // ========================================================================
    
    destroy() {
        this.stopPositionUpdates();
        super.destroy();
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = PlaybackController;
}

// ============================================================================
// FIN DU FICHIER PlaybackController.js
// ============================================================================
