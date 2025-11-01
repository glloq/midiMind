// ============================================================================
// Fichier: frontend/js/models/PlaylistModel.js
// Chemin réel: frontend/js/models/PlaylistModel.js
// Version: v3.3.0 - SIGNATURE CORRIGÉE (5 PARAMÈTRES)
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.3.0:
// ✅ CRITIQUE: Ajout paramètres initialData et options manquants
// ✅ Signature cohérente: (eventBus, backend, logger, initialData = {}, options = {})
// ✅ Merge intelligente des options par défaut
// ✅ Ajout méthodes de gestion de playlist améliorées
// ============================================================================

class PlaylistModel extends BaseModel {
    constructor(eventBus, backend, logger, initialData = {}, options = {}) {
        // ✅ NOUVEAU: Appel super() avec les 5 paramètres
        super(eventBus, backend, logger, initialData, {
            persistKey: 'playlistmodel',
            eventPrefix: 'playlist',
            autoPersist: true,
            ...options
        });
        
        // Initialisation des données de playlist avec valeurs par défaut
        this.data.playlists = this.data.playlists || [];
        this.data.currentPlaylist = this.data.currentPlaylist || null;
        this.data.currentIndex = this.data.currentIndex || 0;
        
        this.log('debug', 'PlaylistModel', 'Initialized v3.3.0');
    }
    
    /**
     * Crée une nouvelle playlist
     * @param {string} name - Nom de la playlist
     * @returns {Object} Playlist créée
     */
    createPlaylist(name) {
        const playlist = {
            id: Date.now().toString(),
            name,
            files: [],
            created: Date.now(),
            modified: Date.now(),
            description: '',
            isActive: false
        };
        
        this.data.playlists.push(playlist);
        this.emit('playlist:created', { playlist });
        
        this.log('info', 'PlaylistModel', `Playlist created: ${name}`);
        return playlist;
    }
    
    /**
     * Supprime une playlist
     * @param {string} id - ID de la playlist
     * @returns {boolean} Succès
     */
    deletePlaylist(id) {
        const index = this.data.playlists.findIndex(p => p.id === id);
        
        if (index !== -1) {
            const playlist = this.data.playlists.splice(index, 1)[0];
            
            // Si c'était la playlist courante, la réinitialiser
            if (this.data.currentPlaylist?.id === id) {
                this.data.currentPlaylist = null;
                this.data.currentIndex = 0;
            }
            
            this.emit('playlist:deleted', { playlist });
            this.log('info', 'PlaylistModel', `Playlist deleted: ${playlist.name}`);
            return true;
        }
        
        this.log('warn', 'PlaylistModel', `Playlist not found: ${id}`);
        return false;
    }
    
    /**
     * Renomme une playlist
     * @param {string} id - ID de la playlist
     * @param {string} newName - Nouveau nom
     * @returns {boolean} Succès
     */
    renamePlaylist(id, newName) {
        const playlist = this.data.playlists.find(p => p.id === id);
        
        if (playlist) {
            playlist.name = newName;
            playlist.modified = Date.now();
            this.emit('playlist:updated', { playlist });
            return true;
        }
        
        return false;
    }
    
    /**
     * Ajoute un fichier à une playlist
     * @param {string} playlistId - ID de la playlist
     * @param {Object} file - Fichier à ajouter
     * @returns {boolean} Succès
     */
    addFile(playlistId, file) {
        const playlist = this.data.playlists.find(p => p.id === playlistId);
        
        if (playlist) {
            // Vérifier que le fichier n'est pas déjà dans la playlist
            const exists = playlist.files.some(f => f.id === file.id || f.path === file.path);
            if (!exists) {
                playlist.files.push(file);
                playlist.modified = Date.now();
                this.emit('playlist:updated', { playlist });
                this.log('info', 'PlaylistModel', `File added to playlist: ${file.name || file.path}`);
                return true;
            } else {
                this.log('warn', 'PlaylistModel', 'File already in playlist');
                return false;
            }
        }
        
        this.log('warn', 'PlaylistModel', `Playlist not found: ${playlistId}`);
        return false;
    }
    
    /**
     * Retire un fichier d'une playlist
     * @param {string} playlistId - ID de la playlist
     * @param {number} fileIndex - Index du fichier
     * @returns {boolean} Succès
     */
    removeFile(playlistId, fileIndex) {
        const playlist = this.data.playlists.find(p => p.id === playlistId);
        
        if (playlist && fileIndex >= 0 && fileIndex < playlist.files.length) {
            const removed = playlist.files.splice(fileIndex, 1)[0];
            playlist.modified = Date.now();
            this.emit('playlist:updated', { playlist });
            this.log('info', 'PlaylistModel', `File removed from playlist: ${removed.name || removed.path}`);
            return true;
        }
        
        return false;
    }
    
    /**
     * Déplace un fichier dans une playlist
     * @param {string} playlistId - ID de la playlist
     * @param {number} fromIndex - Index source
     * @param {number} toIndex - Index destination
     * @returns {boolean} Succès
     */
    moveFile(playlistId, fromIndex, toIndex) {
        const playlist = this.data.playlists.find(p => p.id === playlistId);
        
        if (playlist && 
            fromIndex >= 0 && fromIndex < playlist.files.length &&
            toIndex >= 0 && toIndex < playlist.files.length) {
            
            const file = playlist.files.splice(fromIndex, 1)[0];
            playlist.files.splice(toIndex, 0, file);
            playlist.modified = Date.now();
            this.emit('playlist:updated', { playlist });
            return true;
        }
        
        return false;
    }
    
    /**
     * Récupère toutes les playlists
     * @returns {Array}
     */
    getPlaylists() {
        return this.data.playlists;
    }
    
    /**
     * Récupère une playlist par son ID
     * @param {string} id - ID de la playlist
     * @returns {Object|null}
     */
    getPlaylist(id) {
        return this.data.playlists.find(p => p.id === id) || null;
    }
    
    /**
     * Définit la playlist courante
     * @param {string} playlistId - ID de la playlist
     * @returns {boolean} Succès
     */
    setCurrentPlaylist(playlistId) {
        const playlist = this.getPlaylist(playlistId);
        
        if (playlist) {
            // Désactiver l'ancienne playlist active
            if (this.data.currentPlaylist) {
                this.data.currentPlaylist.isActive = false;
            }
            
            this.data.currentPlaylist = playlist;
            this.data.currentIndex = 0;
            playlist.isActive = true;
            
            this.emit('playlist:current:changed', { playlist });
            this.log('info', 'PlaylistModel', `Current playlist: ${playlist.name}`);
            return true;
        }
        
        return false;
    }
    
    /**
     * Récupère la playlist courante
     * @returns {Object|null}
     */
    getCurrentPlaylist() {
        return this.data.currentPlaylist;
    }
    
    /**
     * Définit l'index courant dans la playlist
     * @param {number} index
     */
    setCurrentIndex(index) {
        if (this.data.currentPlaylist && 
            index >= 0 && 
            index < this.data.currentPlaylist.files.length) {
            
            this.data.currentIndex = index;
            this.emit('playlist:index:changed', { 
                index, 
                file: this.data.currentPlaylist.files[index] 
            });
        }
    }
    
    /**
     * Récupère l'index courant
     * @returns {number}
     */
    getCurrentIndex() {
        return this.data.currentIndex;
    }
    
    /**
     * Récupère le fichier courant dans la playlist
     * @returns {Object|null}
     */
    getCurrentFile() {
        if (this.data.currentPlaylist && 
            this.data.currentIndex >= 0 &&
            this.data.currentIndex < this.data.currentPlaylist.files.length) {
            
            return this.data.currentPlaylist.files[this.data.currentIndex];
        }
        
        return null;
    }
    
    /**
     * Passe au fichier suivant dans la playlist
     * @returns {Object|null} Fichier suivant ou null si fin
     */
    nextFile() {
        if (this.data.currentPlaylist && 
            this.data.currentIndex < this.data.currentPlaylist.files.length - 1) {
            
            this.data.currentIndex++;
            const file = this.data.currentPlaylist.files[this.data.currentIndex];
            this.emit('playlist:index:changed', { 
                index: this.data.currentIndex, 
                file 
            });
            return file;
        }
        
        return null;
    }
    
    /**
     * Passe au fichier précédent dans la playlist
     * @returns {Object|null} Fichier précédent ou null si début
     */
    previousFile() {
        if (this.data.currentPlaylist && this.data.currentIndex > 0) {
            this.data.currentIndex--;
            const file = this.data.currentPlaylist.files[this.data.currentIndex];
            this.emit('playlist:index:changed', { 
                index: this.data.currentIndex, 
                file 
            });
            return file;
        }
        
        return null;
    }
    
    /**
     * Vérifie s'il y a un fichier suivant
     * @returns {boolean}
     */
    hasNext() {
        return this.data.currentPlaylist && 
               this.data.currentIndex < this.data.currentPlaylist.files.length - 1;
    }
    
    /**
     * Vérifie s'il y a un fichier précédent
     * @returns {boolean}
     */
    hasPrevious() {
        return this.data.currentPlaylist && this.data.currentIndex > 0;
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PlaylistModel;
}

if (typeof window !== 'undefined') {
    window.PlaylistModel = PlaylistModel;
}