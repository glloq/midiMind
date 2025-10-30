// ============================================================================
// Fichier: frontend/js/models/PlaylistModel.js
// Version: v3.1.1 - FIXED LOGGER PROTECTION
// Date: 2025-10-30
// ============================================================================
// CORRECTIONS v3.1.1:
// ✅ CRITIQUE: Protection contre logger undefined
// ✅ Utilise logger || window.logger || console comme fallback
// ✅ Vérification avant chaque appel logger.info/error
// ============================================================================

class PlaylistModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super({}, {
            persistKey: 'playlistmodel',
            eventPrefix: 'playlist',
            autoPersist: true
        });
        
        // ✅ PROTECTION: Fallback sur window.logger ou console
        this.eventBus = eventBus || window.eventBus || window.eventBus;
        this.backend = backend || window.backendService || window.app?.services?.backend;
        this.logger = logger || window.logger || console;
        
        // Validation des dépendances critiques
        if (!this.eventBus) {
            console.error('[PlaylistModel] EventBus not available!');
        }
        if (!this.backend) {
            console.warn('[PlaylistModel] BackendService not available');
        }
        
        // Données du modèle
        this.data = {
            playlists: [],
            currentPlaylist: null,
            currentIndex: 0,
            
            // Queue de lecture
            queue: [],
            queueIndex: 0,
            
            // Modes de lecture
            shuffle: false,
            repeat: 'none', // none, one, all
            autoAdvance: true
        };
        
        // ✅ Vérification avant utilisation
        if (this.logger && typeof this.logger.info === 'function') {
            this.logger.info('PlaylistModel', '✓ Model initialized (v3.1.1)');
        }
    }
    
    // ========================================================================
    // GESTION PLAYLISTS
    // ========================================================================
    
    async createPlaylist(name, files = []) {
        const playlists = this.get('playlists');
        
        const playlist = {
            id: `playlist_${Date.now()}`,
            name: name,
            files: files,
            createdAt: Date.now(),
            modifiedAt: Date.now()
        };
        
        playlists.push(playlist);
        this.set('playlists', playlists);
        
        if (this.logger && typeof this.logger.info === 'function') {
            this.logger.info('PlaylistModel', `Playlist created: ${name}`);
        }
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:created', { playlist });
        }
        
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
        
        // Charger la playlist dans la queue
        this.set('queue', [...playlist.files]);
        this.set('queueIndex', 0);
        
        if (this.logger && typeof this.logger.info === 'function') {
            this.logger.info('PlaylistModel', `Playlist loaded: ${playlist.name}`);
        }
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:loaded', { playlist });
        }
        
        return playlist;
    }
    
    async deletePlaylist(playlistId) {
        const playlists = this.get('playlists').filter(p => p.id !== playlistId);
        
        this.set('playlists', playlists);
        
        if (this.get('currentPlaylist')?.id === playlistId) {
            this.set('currentPlaylist', null);
            this.set('queue', []);
        }
        
        if (this.logger && typeof this.logger.info === 'function') {
            this.logger.info('PlaylistModel', `Playlist deleted: ${playlistId}`);
        }
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:deleted', { playlistId });
        }
        
        return true;
    }
    
    updatePlaylist(playlistId, updates) {
        const playlists = this.get('playlists');
        const index = playlists.findIndex(p => p.id === playlistId);
        
        if (index === -1) {
            throw new Error(`Playlist not found: ${playlistId}`);
        }
        
        playlists[index] = {
            ...playlists[index],
            ...updates,
            modifiedAt: Date.now()
        };
        
        this.set('playlists', playlists);
        
        // Mettre à jour la playlist courante si c'est celle-ci
        if (this.get('currentPlaylist')?.id === playlistId) {
            this.set('currentPlaylist', playlists[index]);
        }
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:updated', { playlist: playlists[index] });
        }
        
        return playlists[index];
    }
    
    // ========================================================================
    // GESTION DES FICHIERS DANS PLAYLIST
    // ========================================================================
    
    addFile(playlistId, fileId) {
        const playlists = this.get('playlists');
        const playlist = playlists.find(p => p.id === playlistId);
        
        if (!playlist) {
            throw new Error(`Playlist not found: ${playlistId}`);
        }
        
        if (!playlist.files.includes(fileId)) {
            playlist.files.push(fileId);
            playlist.modifiedAt = Date.now();
            
            this.set('playlists', playlists);
            
            // Mettre à jour la queue si c'est la playlist courante
            if (this.get('currentPlaylist')?.id === playlistId) {
                const queue = this.get('queue');
                queue.push(fileId);
                this.set('queue', queue);
            }
            
            if (this.eventBus) {
                this.eventBus.emit('playlist:file-added', { playlistId, fileId });
            }
        }
        
        return playlist;
    }
    
    removeFile(playlistId, fileId) {
        const playlists = this.get('playlists');
        const playlist = playlists.find(p => p.id === playlistId);
        
        if (!playlist) {
            throw new Error(`Playlist not found: ${playlistId}`);
        }
        
        const index = playlist.files.indexOf(fileId);
        if (index !== -1) {
            playlist.files.splice(index, 1);
            playlist.modifiedAt = Date.now();
            
            this.set('playlists', playlists);
            
            // Mettre à jour la queue si c'est la playlist courante
            if (this.get('currentPlaylist')?.id === playlistId) {
                const queue = this.get('queue');
                const queueIndex = queue.indexOf(fileId);
                if (queueIndex !== -1) {
                    queue.splice(queueIndex, 1);
                    this.set('queue', queue);
                }
            }
            
            if (this.eventBus) {
                this.eventBus.emit('playlist:file-removed', { playlistId, fileId });
            }
        }
        
        return playlist;
    }
    
    // ========================================================================
    // GESTION DE LA QUEUE
    // ========================================================================
    
    addToQueue(fileId) {
        const queue = this.get('queue');
        queue.push(fileId);
        this.set('queue', queue);
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:queue-updated', { queue });
        }
        
        return queue;
    }
    
    addMultipleToQueue(fileIds) {
        const queue = this.get('queue');
        queue.push(...fileIds);
        this.set('queue', queue);
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:queue-updated', { queue });
        }
        
        return queue;
    }
    
    removeFromQueue(index) {
        const queue = this.get('queue');
        
        if (index >= 0 && index < queue.length) {
            const removed = queue.splice(index, 1)[0];
            this.set('queue', queue);
            
            // Ajuster l'index si nécessaire
            const queueIndex = this.get('queueIndex');
            if (index < queueIndex) {
                this.set('queueIndex', queueIndex - 1);
            } else if (index === queueIndex) {
                // Si on supprime le fichier en cours, passer au suivant
                if (queueIndex >= queue.length) {
                    this.set('queueIndex', 0);
                }
            }
            
            if (this.eventBus) {
                this.eventBus.emit('playlist:queue-updated', { queue });
            }
            
            return removed;
        }
        
        return null;
    }
    
    clearQueue() {
        this.set('queue', []);
        this.set('queueIndex', 0);
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:queue-cleared');
        }
        
        return true;
    }
    
    reorderQueue(fromIndex, toIndex) {
        const queue = this.get('queue');
        
        if (fromIndex < 0 || fromIndex >= queue.length || toIndex < 0 || toIndex >= queue.length) {
            return queue;
        }
        
        const item = queue.splice(fromIndex, 1)[0];
        queue.splice(toIndex, 0, item);
        
        this.set('queue', queue);
        
        // Ajuster l'index de queue si nécessaire
        const queueIndex = this.get('queueIndex');
        if (fromIndex === queueIndex) {
            this.set('queueIndex', toIndex);
        } else if (fromIndex < queueIndex && toIndex >= queueIndex) {
            this.set('queueIndex', queueIndex - 1);
        } else if (fromIndex > queueIndex && toIndex <= queueIndex) {
            this.set('queueIndex', queueIndex + 1);
        }
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:queue-reordered', { queue });
        }
        
        return queue;
    }
    
    playQueue() {
        const queue = this.get('queue');
        
        if (queue.length === 0) {
            return null;
        }
        
        this.set('queueIndex', 0);
        
        const file = queue[0];
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:queue-play', { file, index: 0 });
        }
        
        return file;
    }
    
    getQueueStats() {
        const queue = this.get('queue');
        const queueIndex = this.get('queueIndex');
        
        return {
            total: queue.length,
            current: queueIndex,
            remaining: Math.max(0, queue.length - queueIndex - 1)
        };
    }
    
    // ========================================================================
    // NAVIGATION
    // ========================================================================
    
    getCurrentFile() {
        const queue = this.get('queue');
        const index = this.get('queueIndex');
        
        if (!queue || index < 0 || index >= queue.length) {
            return null;
        }
        
        return queue[index];
    }
    
    next() {
        const queue = this.get('queue');
        const repeat = this.get('repeat');
        
        if (!queue || queue.length === 0) {
            return null;
        }
        
        let newIndex = this.get('queueIndex') + 1;
        
        // Gérer le mode repeat
        if (repeat === 'one') {
            // Rester sur le même fichier
            newIndex = this.get('queueIndex');
        } else if (newIndex >= queue.length) {
            if (repeat === 'all') {
                // Boucler au début
                newIndex = 0;
            } else {
                // Arrêter à la fin
                return null;
            }
        }
        
        this.set('queueIndex', newIndex);
        
        const file = queue[newIndex];
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:next', { 
                file, 
                index: newIndex
            });
        }
        
        return file;
    }
    
    previous() {
        const queue = this.get('queue');
        
        if (!queue || queue.length === 0) {
            return null;
        }
        
        let newIndex = this.get('queueIndex') - 1;
        
        if (newIndex < 0) {
            newIndex = queue.length - 1; // Boucler à la fin
        }
        
        this.set('queueIndex', newIndex);
        
        const file = queue[newIndex];
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:previous', { 
                file, 
                index: newIndex
            });
        }
        
        return file;
    }
    
    jumpTo(index) {
        const queue = this.get('queue');
        
        if (!queue || index < 0 || index >= queue.length) {
            return null;
        }
        
        this.set('queueIndex', index);
        
        const file = queue[index];
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:jump', { file, index });
        }
        
        return file;
    }
    
    // ========================================================================
    // MODES DE LECTURE
    // ========================================================================
    
    setShuffle(enabled) {
        this.set('shuffle', enabled);
        
        if (enabled) {
            // Mélanger la queue
            this.shuffleQueue();
        }
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:shuffle-changed', { enabled });
        }
        
        return enabled;
    }
    
    setRepeat(mode) {
        // mode: 'none', 'one', 'all'
        this.set('repeat', mode);
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:repeat-changed', { mode });
        }
        
        return mode;
    }
    
    setAutoAdvance(enabled) {
        this.set('autoAdvance', enabled);
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:autoadvance-changed', { enabled });
        }
        
        return enabled;
    }
    
    shuffleQueue() {
        const queue = this.get('queue');
        const currentIndex = this.get('queueIndex');
        const currentFile = queue[currentIndex];
        
        // Mélanger la queue en gardant le fichier courant en place
        const before = queue.slice(0, currentIndex);
        const after = queue.slice(currentIndex + 1);
        
        // Mélanger les parties avant et après
        const shuffled = [
            ...this.shuffle(before),
            currentFile,
            ...this.shuffle(after)
        ];
        
        this.set('queue', shuffled);
        this.set('queueIndex', before.length);
        
        return shuffled;
    }
    
    shuffle(array) {
        const arr = [...array];
        for (let i = arr.length - 1; i > 0; i--) {
            const j = Math.floor(Math.random() * (i + 1));
            [arr[i], arr[j]] = [arr[j], arr[i]];
        }
        return arr;
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
    
    getPlaylistById(playlistId) {
        return this.getPlaylist(playlistId);
    }
    
    getCurrentPlaylist() {
        return this.get('currentPlaylist');
    }
    
    getCurrentIndex() {
        return this.get('queueIndex');
    }
    
    get(key) {
        return this.data[key];
    }
    
    set(key, value) {
        this.data[key] = value;
        if (this.eventBus) {
            this.eventBus.emit(`${this.config.eventPrefix}:${key}-changed`, value);
        }
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