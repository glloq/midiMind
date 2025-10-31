// ============================================================================
// Fichier: frontend/js/models/PlaybackModel.js
// Chemin réel: frontend/js/models/PlaybackModel.js
// Version: v3.2.0 - SIGNATURE COHÉRENTE
// Date: 2025-10-31
// ============================================================================

class PlaybackModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super(eventBus, backend, logger, {}, {
            persistKey: 'playbackmodel',
            eventPrefix: 'playback',
            autoPersist: false
        });
        
        this.data.state = 'stopped';
        this.data.currentFile = null;
        this.data.currentTime = 0;
        this.data.duration = 0;
        this.data.tempo = 1.0;
        this.data.loop = false;
        
        this.log('debug', 'PlaybackModel', 'Initialized');
    }
    
    async load(filename) {
        if (!this.backend || !this.backend.isConnected()) return false;
        
        try {
            const response = await this.backend.send('playback.load', { filename });
            
            if (response.success) {
                this.data.currentFile = filename;
                this.emit('playback:loaded', { filename });
                return true;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.load', error);
        }
        
        return false;
    }
    
    async play() {
        if (!this.backend || !this.backend.isConnected()) return false;
        
        try {
            const response = await this.backend.send('playback.play', {});
            
            if (response.success) {
                this.data.state = 'playing';
                this.emit('playback:play');
                return true;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.play', error);
        }
        
        return false;
    }
    
    async pause() {
        if (!this.backend || !this.backend.isConnected()) return false;
        
        try {
            const response = await this.backend.send('playback.pause', {});
            
            if (response.success) {
                this.data.state = 'paused';
                this.emit('playback:pause');
                return true;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.pause', error);
        }
        
        return false;
    }
    
    async stop() {
        if (!this.backend || !this.backend.isConnected()) return false;
        
        try {
            const response = await this.backend.send('playback.stop', {});
            
            if (response.success) {
                this.data.state = 'stopped';
                this.data.currentTime = 0;
                this.emit('playback:stop');
                return true;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.stop', error);
        }
        
        return false;
    }
    
    async seek(position) {
        if (!this.backend || !this.backend.isConnected()) return false;
        
        try {
            const response = await this.backend.send('playback.seek', { position });
            
            if (response.success) {
                this.data.currentTime = position;
                this.emit('playback:seek', { position });
                return true;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.seek', error);
        }
        
        return false;
    }
    
    async getStatus() {
        if (!this.backend || !this.backend.isConnected()) return null;
        
        try {
            const response = await this.backend.send('playback.getStatus', {});
            
            if (response.success) {
                this.data.state = response.data.state || 'stopped';
                this.data.currentTime = response.data.current_time || 0;
                this.data.duration = response.data.duration || 0;
                this.emit('playback:status', response.data);
                return response.data;
            }
        } catch (error) {
            this.log('error', 'PlaybackModel.getStatus', error);
        }
        
        return null;
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PlaybackModel;
}

if (typeof window !== 'undefined') {
    window.PlaybackModel = PlaybackModel;
}