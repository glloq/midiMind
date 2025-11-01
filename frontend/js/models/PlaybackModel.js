// ============================================================================
// Fichier: frontend/js/models/PlaybackModel.js
// Chemin rÃ©el: frontend/js/models/PlaybackModel.js
// Version: v3.3.0 - API FORMAT SIMPLIFIÃ‰
// Date: 2025-11-01
// ============================================================================
// MODIFICATIONS v3.3.0:
// âœ… Utilisation sendCommand() au lieu de send()
// âœ… Format API simplifiÃ© (id, command, params)
// âœ… Signature constructeur cohÃ©rente avec BaseModel
// âœ… Gestion amÃ©liorÃ©e des erreurs
// ============================================================================

class PlaybackModel extends BaseModel {
    constructor(eventBus, backend, logger, initialData = {}, options = {}) {
        super(eventBus, backend, logger, initialData, {
            persistKey: 'playbackmodel',
            eventPrefix: 'playback',
            autoPersist: false,
            ...options
        });
        
        // Ã‰tat initial de la lecture
        this.data.state = initialData.state || 'stopped'; // 'stopped', 'playing', 'paused'
        this.data.currentFile = initialData.currentFile || null;
        this.data.currentTime = initialData.currentTime || 0;
        this.data.duration = initialData.duration || 0;
        this.data.tempo = initialData.tempo || 1.0;
        this.data.loop = initialData.loop || false;
        
        this.log('debug', 'PlaybackModel', 'Initialized v3.3.0');
    }
    
    /**
     * Charge un fichier MIDI pour la lecture
     * @param {string} filename - Nom du fichier Ã  charger
     */
    async load(filename) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'PlaybackModel.load', 'Backend not connected');
            return false;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('load_file', { 
                filename 
            });
            
            if (data) {
                this.data.currentFile = filename;
                this.data.duration = data.duration || 0;
                this.emit('playback:loaded', { filename, duration: data.duration });
                return true;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.load', error.message);
        }
        
        return false;
    }
    
    /**
     * DÃ©marre la lecture
     */
    async play() {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'PlaybackModel.play', 'Backend not connected');
            return false;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('play');
            
            if (data) {
                this.data.state = 'playing';
                this.emit('playback:play');
                this.emit('state:changed', { state: 'playing' });
                return true;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.play', error.message);
        }
        
        return false;
    }
    
    /**
     * Met la lecture en pause
     */
    async pause() {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'PlaybackModel.pause', 'Backend not connected');
            return false;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('pause');
            
            if (data) {
                this.data.state = 'paused';
                this.emit('playback:pause');
                this.emit('state:changed', { state: 'paused' });
                return true;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.pause', error.message);
        }
        
        return false;
    }
    
    /**
     * ArrÃªte la lecture
     */
    async stop() {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'PlaybackModel.stop', 'Backend not connected');
            return false;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('stop');
            
            if (data) {
                this.data.state = 'stopped';
                this.data.currentTime = 0;
                this.emit('playback:stop');
                this.emit('state:changed', { state: 'stopped' });
                return true;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.stop', error.message);
        }
        
        return false;
    }
    
    /**
     * Se dÃ©place Ã  une position spÃ©cifique dans la lecture
     * @param {number} position - Position en secondes
     */
    async seek(position) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'PlaybackModel.seek', 'Backend not connected');
            return false;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('seek', { 
                position 
            });
            
            if (data) {
                this.data.currentTime = position;
                this.emit('playback:seek', { position });
                return true;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.seek', error.message);
        }
        
        return false;
    }
    
    /**
     * RÃ©cupÃ¨re l'Ã©tat actuel de la lecture
     */
    async getStatus() {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'PlaybackModel.getStatus', 'Backend not connected');
            return null;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('get_status');
            
            if (data) {
                // Mettre Ã  jour l'Ã©tat local
                this.data.state = data.state || 'stopped';
                this.data.currentTime = data.current_time || 0;
                this.data.duration = data.duration || 0;
                this.data.currentFile = data.current_file || null;
                
                this.emit('playback:status', data);
                return data;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.getStatus', error.message);
        }
        
        return null;
    }
    
    /**
     * Active/dÃ©sactive la boucle
     * @param {boolean} enabled - True pour activer la boucle
     */
    async setLoop(enabled) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'PlaybackModel.setLoop', 'Backend not connected');
            return false;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('set_loop', { 
                enabled 
            });
            
            if (data) {
                this.data.loop = enabled;
                this.emit('playback:loop', { enabled });
                return true;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.setLoop', error.message);
        }
        
        return false;
    }
    
    /**
     * DÃ©finit le tempo de lecture
     * @param {number} tempo - Facteur de tempo (1.0 = normal, 0.5 = moitiÃ©, 2.0 = double)
     */
    async setTempo(tempo) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'PlaybackModel.setTempo', 'Backend not connected');
            return false;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('set_tempo', { 
                tempo 
            });
            
            if (data) {
                this.data.tempo = tempo;
                this.emit('playback:tempo', { tempo });
                return true;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.setTempo', error.message);
        }
        
        return false;
    }
    
    // ========================================================================
    // MÃ‰THODES LOCALES (GETTERS)
    // ========================================================================
    
    /**
     * Retourne l'Ã©tat actuel de la lecture
     */
    getState() {
        return this.data.state;
    }
    
    /**
     * VÃ©rifie si la lecture est en cours
     */
    isPlaying() {
        return this.data.state === 'playing';
    }
    
    /**
     * VÃ©rifie si la lecture est en pause
     */
    isPaused() {
        return this.data.state === 'paused';
    }
    
    /**
     * VÃ©rifie si la lecture est arrÃªtÃ©e
     */
    isStopped() {
        return this.data.state === 'stopped';
    }
    
    /**
     * Retourne le fichier actuellement chargÃ©
     */
    getCurrentFile() {
        return this.data.currentFile;
    }
    
    /**
     * Retourne la position actuelle
     */
    getCurrentTime() {
        return this.data.currentTime;
    }
    
    /**
     * Retourne la durÃ©e totale
     */
    getDuration() {
        return this.data.duration;
    }
    
    /**
     * Retourne le tempo actuel
     */
    getTempo() {
        return this.data.tempo;
    }
    
    /**
     * VÃ©rifie si la boucle est activÃ©e
     */
    isLooping() {
        return this.data.loop;
    }
    
    /**
     * Met Ã  jour la position actuelle (depuis Ã©vÃ©nements backend)
     * @param {number} time - Nouvelle position
     */
    updateCurrentTime(time) {
        this.data.currentTime = time;
        this.emit('time:updated', { time });
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PlaybackModel;
}

if (typeof window !== 'undefined') {
    window.PlaybackModel = PlaybackModel;
}