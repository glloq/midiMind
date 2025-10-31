// ============================================================================
// Fichier: frontend/js/models/PlaylistModel.js
// Chemin réel: frontend/js/models/PlaylistModel.js
// Version: v3.2.0 - SIGNATURE COHÉRENTE
// Date: 2025-10-31
// ============================================================================

class PlaylistModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super(eventBus, backend, logger, {}, {
            persistKey: 'playlistmodel',
            eventPrefix: 'playlist',
            autoPersist: true
        });
        
        this.data.playlists = [];
        this.data.currentPlaylist = null;
        this.data.currentIndex = 0;
        
        this.log('debug', 'PlaylistModel', 'Initialized');
    }
    
    createPlaylist(name) {
        const playlist = {
            id: Date.now().toString(),
            name,
            files: [],
            created: Date.now(),
            modified: Date.now()
        };
        
        this.data.playlists.push(playlist);
        this.emit('playlist:created', { playlist });
        
        return playlist;
    }
    
    deletePlaylist(id) {
        const index = this.data.playlists.findIndex(p => p.id === id);
        
        if (index !== -1) {
            const playlist = this.data.playlists.splice(index, 1)[0];
            this.emit('playlist:deleted', { playlist });
            return true;
        }
        
        return false;
    }
    
    addFile(playlistId, file) {
        const playlist = this.data.playlists.find(p => p.id === playlistId);
        
        if (playlist) {
            playlist.files.push(file);
            playlist.modified = Date.now();
            this.emit('playlist:updated', { playlist });
            return true;
        }
        
        return false;
    }
    
    removeFile(playlistId, fileIndex) {
        const playlist = this.data.playlists.find(p => p.id === playlistId);
        
        if (playlist && fileIndex >= 0 && fileIndex < playlist.files.length) {
            playlist.files.splice(fileIndex, 1);
            playlist.modified = Date.now();
            this.emit('playlist:updated', { playlist });
            return true;
        }
        
        return false;
    }
    
    getPlaylists() {
        return this.data.playlists;
    }
    
    getPlaylist(id) {
        return this.data.playlists.find(p => p.id === id);
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PlaylistModel;
}

if (typeof window !== 'undefined') {
    window.PlaylistModel = PlaylistModel;
}