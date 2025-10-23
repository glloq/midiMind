// ============================================================================
// Fichier: SearchController.js (ROOT)
// Version: v1.0
// Date: 2025-10-23
// Projet: midiMind v3.0 - Contrôleur de Recherche
// ============================================================================

class SearchController extends BaseController {
    constructor(eventBus, logger) {
        super(eventBus, logger, null, null);
        
        // Index de recherche
        this.searchIndex = new Map();
        this.fileIndex = new Map();
        this.playlistIndex = new Map();
        
        // Résultats
        this.lastQuery = '';
        this.lastResults = [];
        
        // Configuration
        this.config = {
            minQueryLength: 2,
            maxResults: 50,
            fuzzySearch: true,
            caseSensitive: false
        };
    }
    
    /**
     * Initialisation du contrôleur
     */
    onInitialize() {
        this.logger.info('SearchController', 'Initializing search controller...');
        this.bindEvents();
        this.logger.info('SearchController', 'Search controller initialized');
    }
    
    /**
     * Bind des événements
     */
    bindEvents() {
        // Écouter les changements de fichiers pour réindexer
        this.eventBus.on('file:loaded', (file) => this.indexFile(file));
        this.eventBus.on('file:updated', (file) => this.updateFileIndex(file));
        this.eventBus.on('file:deleted', (fileId) => this.removeFromIndex(fileId));
        
        // Écouter les changements de playlists
        this.eventBus.on('playlist:created', (playlist) => this.indexPlaylist(playlist));
        this.eventBus.on('playlist:updated', (playlist) => this.indexPlaylist(playlist));
        this.eventBus.on('playlist:deleted', (playlistId) => this.removePlaylistFromIndex(playlistId));
    }
    
    /**
     * Recherche principale
     */
    search(query, options = {}) {
        if (!query || query.length < this.config.minQueryLength) {
            return [];
        }
        
        const opts = { ...this.config, ...options };
        const normalizedQuery = opts.caseSensitive ? query : query.toLowerCase();
        
        this.lastQuery = query;
        this.lastResults = [];
        
        // Recherche dans les fichiers
        const fileResults = this.searchFiles(normalizedQuery, opts);
        
        // Recherche dans les playlists
        const playlistResults = this.searchPlaylists(normalizedQuery, opts);
        
        // Combiner et trier les résultats
        this.lastResults = [
            ...fileResults,
            ...playlistResults
        ].slice(0, opts.maxResults);
        
        // Émettre événement
        this.eventBus.emit('search:results', {
            query,
            results: this.lastResults,
            count: this.lastResults.length
        });
        
        return this.lastResults;
    }
    
    /**
     * Recherche dans les fichiers
     */
    searchFiles(query, options) {
        const results = [];
        
        for (const [fileId, file] of this.fileIndex) {
            const score = this.calculateScore(query, file, options);
            if (score > 0) {
                results.push({
                    type: 'file',
                    id: fileId,
                    data: file,
                    score
                });
            }
        }
        
        return results.sort((a, b) => b.score - a.score);
    }
    
    /**
     * Recherche dans les playlists
     */
    searchPlaylists(query, options) {
        const results = [];
        
        for (const [playlistId, playlist] of this.playlistIndex) {
            const score = this.calculateScore(query, playlist, options);
            if (score > 0) {
                results.push({
                    type: 'playlist',
                    id: playlistId,
                    data: playlist,
                    score
                });
            }
        }
        
        return results.sort((a, b) => b.score - a.score);
    }
    
    /**
     * Calcul du score de pertinence
     */
    calculateScore(query, item, options) {
        let score = 0;
        const fields = ['name', 'title', 'filename', 'path', 'description', 'tags'];
        
        for (const field of fields) {
            if (item[field]) {
                const fieldValue = options.caseSensitive ? 
                    item[field] : 
                    item[field].toLowerCase();
                
                // Match exact
                if (fieldValue === query) {
                    score += 100;
                }
                // Commence par
                else if (fieldValue.startsWith(query)) {
                    score += 50;
                }
                // Contient
                else if (fieldValue.includes(query)) {
                    score += 25;
                }
                // Recherche floue si activée
                else if (options.fuzzySearch && this.fuzzyMatch(query, fieldValue)) {
                    score += 10;
                }
            }
        }
        
        return score;
    }
    
    /**
     * Recherche floue simple
     */
    fuzzyMatch(query, text) {
        let queryIndex = 0;
        for (let i = 0; i < text.length && queryIndex < query.length; i++) {
            if (text[i] === query[queryIndex]) {
                queryIndex++;
            }
        }
        return queryIndex === query.length;
    }
    
    /**
     * Indexation d'un fichier
     */
    indexFile(file) {
        if (!file || !file.id) return;
        
        const indexed = {
            id: file.id,
            name: file.name || '',
            filename: file.filename || '',
            path: file.path || '',
            type: file.type || 'midi',
            size: file.size || 0,
            duration: file.duration || 0,
            tracks: file.tracks || 0,
            bpm: file.bpm || 0,
            tags: Array.isArray(file.tags) ? file.tags.join(' ') : '',
            searchableText: this.buildSearchableText(file)
        };
        
        this.fileIndex.set(file.id, indexed);
        this.logger.debug('SearchController', `Indexed file: ${file.name}`);
    }
    
    /**
     * Indexation d'une playlist
     */
    indexPlaylist(playlist) {
        if (!playlist || !playlist.id) return;
        
        const indexed = {
            id: playlist.id,
            name: playlist.name || '',
            description: playlist.description || '',
            fileCount: playlist.files ? playlist.files.length : 0,
            tags: Array.isArray(playlist.tags) ? playlist.tags.join(' ') : '',
            searchableText: this.buildSearchableText(playlist)
        };
        
        this.playlistIndex.set(playlist.id, indexed);
        this.logger.debug('SearchController', `Indexed playlist: ${playlist.name}`);
    }
    
    /**
     * Construction du texte recherchable
     */
    buildSearchableText(item) {
        const parts = [];
        
        if (item.name) parts.push(item.name);
        if (item.filename) parts.push(item.filename);
        if (item.description) parts.push(item.description);
        if (item.path) parts.push(item.path);
        if (item.tags) parts.push(Array.isArray(item.tags) ? item.tags.join(' ') : item.tags);
        
        return parts.join(' ').toLowerCase();
    }
    
    /**
     * Mise à jour de l'index d'un fichier
     */
    updateFileIndex(file) {
        this.indexFile(file);
    }
    
    /**
     * Suppression d'un fichier de l'index
     */
    removeFromIndex(fileId) {
        this.fileIndex.delete(fileId);
        this.logger.debug('SearchController', `Removed file from index: ${fileId}`);
    }
    
    /**
     * Suppression d'une playlist de l'index
     */
    removePlaylistFromIndex(playlistId) {
        this.playlistIndex.delete(playlistId);
        this.logger.debug('SearchController', `Removed playlist from index: ${playlistId}`);
    }
    
    /**
     * Réindexation complète
     */
    reindexAll(files = [], playlists = []) {
        this.logger.info('SearchController', 'Reindexing all content...');
        
        // Vider les index
        this.fileIndex.clear();
        this.playlistIndex.clear();
        
        // Réindexer
        files.forEach(file => this.indexFile(file));
        playlists.forEach(playlist => this.indexPlaylist(playlist));
        
        this.logger.info('SearchController', `Reindexed ${files.length} files and ${playlists.length} playlists`);
    }
    
    /**
     * Effacer l'index
     */
    clearIndex() {
        this.fileIndex.clear();
        this.playlistIndex.clear();
        this.lastQuery = '';
        this.lastResults = [];
        this.logger.info('SearchController', 'Search index cleared');
    }
    
    /**
     * Obtenir les derniers résultats
     */
    getLastResults() {
        return {
            query: this.lastQuery,
            results: this.lastResults,
            count: this.lastResults.length
        };
    }
    
    /**
     * Statistiques de l'index
     */
    getIndexStats() {
        return {
            totalFiles: this.fileIndex.size,
            totalPlaylists: this.playlistIndex.size,
            totalIndexed: this.fileIndex.size + this.playlistIndex.size,
            lastQuery: this.lastQuery,
            lastResultCount: this.lastResults.length
        };
    }
    
    /**
     * Configuration
     */
    setConfig(config) {
        Object.assign(this.config, config);
        this.logger.debug('SearchController', 'Configuration updated');
    }
    
    getConfig() {
        return { ...this.config };
    }
}

// Exposition globale pour compatibilité
if (typeof window !== 'undefined') {
    window.SearchController = SearchController;
}