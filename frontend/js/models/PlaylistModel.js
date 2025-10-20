// ============================================================================
// Fichier: frontend/js/models/PlaylistModel.js
// Version: v3.0.7 - ULTRA MINIMAL (Juste les bases)
// Date: 2025-10-19
// ============================================================================
// VERSION LA PLUS SIMPLE POSSIBLE
// - Créer/Charger/Supprimer playlist
// - Navigation (next/previous)
// - C'EST TOUT !
// ============================================================================

class PlaylistModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        // ✅ FIX: Correct super() call
        super({}, {
            persistKey: 'playlistmodel',
            eventPrefix: 'playlist',
            autoPersist: true
        });
        
        // ✅ FIX: Assign immediately
        this.eventBus = eventBus;
        this.logger = logger;
        this.backend = backend;
        
        // ✅ FIX: Initialize data directly
        this.data = {
            playlists: [],
            currentPlaylist: null,
            currentIndex: 0
        };
        
        this.logger.info('PlaylistModel', '✓ Model initialized (ultra minimal)');
    }
    
    // ========================================================================
    // GESTION PLAYLISTS - SIMPLE
    // ========================================================================
    
    async createPlaylist(name, files = []) {
        const playlists = this.get('playlists');
        
        const playlist = {
            id: `playlist_${Date.now()}`,
            name: name,
            files: files,
            createdAt: Date.now()
        };
        
        playlists.push(playlist);
        this.set('playlists', playlists);
        
        this.logger.info('PlaylistModel', `Playlist created: ${name}`);
        
        this.eventBus.emit('playlist:created', { playlist });
        
        return playlist;
    }
    
    async loadPlaylist(playlistId) {
        const playlists = this.get('playlists');
        const playlist = playlists.find(p => p.id === playlistId);
        
        if (!playlist) {
            throw new Error(`Playlist not found: ${playlistId}`);
        }
        
        this.set('currentPlaylist', playlist);
        this.set('currentIndex', 0);
        
        this.logger.info('PlaylistModel', `Playlist loaded: ${playlist.name}`);
        
        this.eventBus.emit('playlist:loaded', { playlist });
        
        return playlist;
    }
    
    async deletePlaylist(playlistId) {
        const playlists = this.get('playlists').filter(p => p.id !== playlistId);
        
        this.set('playlists', playlists);
        
        if (this.get('currentPlaylist')?.id === playlistId) {
            this.set('currentPlaylist', null);
        }
        
        this.logger.info('PlaylistModel', `Playlist deleted: ${playlistId}`);
        
        this.eventBus.emit('playlist:deleted', { playlistId });
        
        return true;
    }
    
    // ========================================================================
    // NAVIGATION - SIMPLE
    // ========================================================================
    
    getCurrentFile() {
        const playlist = this.get('currentPlaylist');
        const index = this.get('currentIndex');
        
        if (!playlist || !playlist.files || index < 0 || index >= playlist.files.length) {
            return null;
        }
        
        return playlist.files[index];
    }
    
    next() {
        const playlist = this.get('currentPlaylist');
        
        if (!playlist || !playlist.files || playlist.files.length === 0) {
            return null;
        }
        
        let newIndex = this.get('currentIndex') + 1;
        
        if (newIndex >= playlist.files.length) {
            newIndex = 0; // Boucler au début
        }
        
        this.set('currentIndex', newIndex);
        
        const file = this.getCurrentFile();
        
        this.eventBus.emit('playlist:next', { 
            file, 
            index: newIndex
        });
        
        return file;
    }
    
    previous() {
        const playlist = this.get('currentPlaylist');
        
        if (!playlist || !playlist.files || playlist.files.length === 0) {
            return null;
        }
        
        let newIndex = this.get('currentIndex') - 1;
        
        if (newIndex < 0) {
            newIndex = playlist.files.length - 1; // Boucler à la fin
        }
        
        this.set('currentIndex', newIndex);
        
        const file = this.getCurrentFile();
        
        this.eventBus.emit('playlist:previous', { 
            file, 
            index: newIndex
        });
        
        return file;
    }
    
    jumpTo(index) {
        const playlist = this.get('currentPlaylist');
        
        if (!playlist || index < 0 || index >= playlist.files.length) {
            return null;
        }
        
        this.set('currentIndex', index);
        
        const file = this.getCurrentFile();
        
        this.eventBus.emit('playlist:jump', { file, index });
        
        return file;
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    getAllPlaylists() {
        return this.get('playlists');
    }
    
    getPlaylist(playlistId) {
        return this.get('playlists').find(p => p.id === playlistId) || null;
    }
    
    getCurrentPlaylist() {
        return this.get('currentPlaylist');
    }
    
    getCurrentIndex() {
        return this.get('currentIndex');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PlaylistModel;
}

if (typeof window !== 'undefined') {
    window.PlaylistModel = PlaylistModel;
}