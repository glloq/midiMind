// ============================================================================
// Fichier: frontend/js/models/FileModel.js
// Version: v3.0.1 - CORRIGÉ (Événement file:list:updated ajouté)
// Date: 2025-10-14
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Modèle de gestion des fichiers MIDI.
//   Centralise toutes les opérations sur les fichiers et gère le cache.
//
// CORRECTIONS v3.0.1:
//   ✅ Ajout événement 'file:list:updated' après loadAll() (ligne ~175)
//   ✅ Ajout événement 'file:list:updated' après create() (ligne ~405)
//   ✅ Ajout événement 'file:list:updated' après delete() (ligne ~540)
//   ✅ AUCUNE fonctionnalité supprimée - Version complète
//
// Responsabilités:
//   - Liste des fichiers MIDI disponibles
//   - Métadonnées des fichiers (titre, durée, etc.)
//   - Upload de nouveaux fichiers
//   - Cache local des informations
//   - Conversion MIDI ↔ JsonMidi
//   - Synchronisation avec backend
//
// Architecture:
//   FileModel
//   ├── BackendService (communication)
//   ├── MidiJsonConverter (conversion)
//   ├── Cache (optimisation)
//   └── EventBus (notifications)
//
// Design Patterns:
//   - Model (MVC)
//   - Cache (performance)
//   - Observer (EventBus)
//   - Singleton (cache partagé)
//
// Auteur: MidiMind Team
// ============================================================================

class FileModel {
    constructor(eventBus, backendService, logger) {
        this.eventBus = eventBus;
        this.backend = backendService;
        this.logger = logger;
        
        // Convertisseur MIDI ↔ JsonMidi
        this.converter = new MidiJsonConverter();
        
        // Cache fichiers
        this.files = new Map();              // fileId -> file object
        this.midiJsonCache = new Map();      // fileId -> JsonMidi data
        this.metadataCache = new Map();      // fileId -> metadata
        
        // État
        this.state = {
            isLoading: false,
            lastScanTimestamp: 0,
            totalFiles: 0,
            totalSize: 0,
            currentDirectory: '.',
            sortBy: 'name',                  // name, date, size, duration
            sortOrder: 'asc'                 // asc, desc
        };
        
        // Configuration cache
        this.cacheConfig = {
            maxSize: 50,                     // Max fichiers en cache
            maxMidiJsonSize: 20,             // Max JsonMidi en mémoire
            expirationTime: 300000,          // 5 minutes
            enabled: true
        };
        
        // Index de recherche
        this.searchIndex = new Map();        // terme -> [fileIds]
        
        // Statistiques
        this.stats = {
            filesLoaded: 0,
            cacheHits: 0,
            cacheMisses: 0,
            uploadsCount: 0,
            deletesCount: 0,
            scansPerformed: 0
        };
        
        this.logger.info('FileModel', '✓ Model initialized v3.0.1');
        
        this.initialize();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    initialize() {
        // Attacher événements
        this.attachEvents();
        
        // Charger liste initiale
        this.loadAll().catch(error => {
            this.logger.warn('FileModel', 'Initial load failed:', error);
        });
    }
    
    attachEvents() {
        // Backend connecté
        this.eventBus.on('backend:connected', () => {
            this.logger.info('FileModel', 'Backend connected, refreshing files...');
            this.loadAll();
        });
        
        // App shutdown
        this.eventBus.on('app:shutdown', () => {
            this.clearCache();
        });
    }
    
    // ========================================================================
    // LISTE FICHIERS
    // ========================================================================
    
    async loadAll(options = {}) {
        const {
            directory = this.state.currentDirectory,
            recursive = true,
            refresh = false
        } = options;
        
        this.logger.info('FileModel', `Loading files from: ${directory}`);
        
        try {
            this.state.isLoading = true;
            
            // Vider cache si refresh demandé
            if (refresh) {
                this.files.clear();
            }
            
            // Récupérer liste depuis backend
            const response = await this.backend.sendCommand('files.list', {
                directory: directory,
                recursive: recursive
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Failed to list files');
            }
            
            // Traiter fichiers
            const files = response.files || [];
            
            this.files.clear();
            
            files.forEach(file => {
                this.files.set(file.id, this.normalizeFile(file));
            });
            
            // Mettre à jour état
            this.state.totalFiles = files.length;
            this.state.totalSize = files.reduce((sum, f) => sum + (f.size || 0), 0);
            this.state.lastScanTimestamp = Date.now();
            
            // Stats
            this.stats.filesLoaded = files.length;
            this.stats.scansPerformed++;
            
            // Construire index recherche
            this.buildSearchIndex();
            
            this.logger.info('FileModel', `✓ Loaded ${files.length} files`);
            
            const filesList = Array.from(this.files.values());
            
            // Événement files:loaded (original)
            this.eventBus.emit('files:loaded', {
                count: files.length,
                files: filesList
            });
            
            // ✅ NOUVEAU v3.0.1 - Événement file:list:updated
            this.eventBus.emit('file:list:updated', {
                files: filesList,
                count: files.length,
                directory: directory,
                timestamp: Date.now()
            });
            
            return filesList;
            
        } catch (error) {
            this.logger.error('FileModel', 'Failed to load files:', error);
            
            this.eventBus.emit('files:error', {
                type: 'load',
                error: error.message
            });
            
            throw error;
            
        } finally {
            this.state.isLoading = false;
        }
    }
    
    async scan(directory = '.') {
        this.logger.info('FileModel', `Scanning directory: ${directory}`);
        
        try {
            const response = await this.backend.sendCommand('files.scan', {
                directory: directory
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Failed to scan');
            }
            
            // Recharger liste
            await this.loadAll({ directory, refresh: true });
            
            this.logger.info('FileModel', '✓ Scan completed');
            
            return true;
            
        } catch (error) {
            this.logger.error('FileModel', 'Scan failed:', error);
            throw error;
        }
    }
    
    // ========================================================================
    // OBTENIR FICHIER
    // ========================================================================
    
    async get(fileId, options = {}) {
        const {
            loadMidiJson = false,
            useCache = true
        } = options;
        
        // Vérifier cache d'abord
        if (useCache && this.files.has(fileId)) {
            this.stats.cacheHits++;
            const file = this.files.get(fileId);
            
            // Charger JsonMidi si demandé et pas en cache
            if (loadMidiJson && !this.midiJsonCache.has(fileId)) {
                await this.loadMidiJson(fileId);
            }
            
            return {
                ...file,
                midiJson: this.midiJsonCache.get(fileId) || null
            };
        }
        
        this.stats.cacheMisses++;
        
        this.logger.info('FileModel', `Loading file: ${fileId}`);
        
        try {
            const response = await this.backend.sendCommand('files.info', {
                file_id: fileId
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Failed to get file info');
            }
            
            const file = this.normalizeFile(response.file);
            
            // Mettre en cache
            this.files.set(fileId, file);
            
            // Charger JsonMidi si demandé
            if (loadMidiJson) {
                await this.loadMidiJson(fileId);
                file.midiJson = this.midiJsonCache.get(fileId);
            }
            
            return file;
            
        } catch (error) {
            this.logger.error('FileModel', 'Failed to get file:', error);
            throw error;
        }
    }
    
    async loadMidiJson(fileId) {
        // Vérifier cache
        if (this.midiJsonCache.has(fileId)) {
            this.logger.debug('FileModel', `MidiJson cache hit: ${fileId}`);
            return this.midiJsonCache.get(fileId);
        }
        
        this.logger.info('FileModel', `Loading MidiJson: ${fileId}`);
        
        try {
            const response = await this.backend.sendCommand('files.load', {
                file_id: fileId
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Failed to load MidiJson');
            }
            
            const jsonMidi = response.jsonmidi;
            
            // Mettre en cache
            this.addToMidiJsonCache(fileId, jsonMidi);
            
            return jsonMidi;
            
        } catch (error) {
            this.logger.error('FileModel', 'Failed to load MidiJson:', error);
            throw error;
        }
    }
    
    getFromCache(fileId) {
        return this.files.get(fileId) || null;
    }
    
    getMidiJsonFromCache(fileId) {
        return this.midiJsonCache.get(fileId) || null;
    }
    
    // ========================================================================
    // CRÉER / UPLOAD
    // ========================================================================
    
    async create(fileData) {
        this.logger.info('FileModel', `Creating file: ${fileData.name}`);
        
        try {
            // Convertir données MIDI en JsonMidi si nécessaire
            let jsonMidi = fileData.midiJson;
            
            if (!jsonMidi && fileData.data) {
                this.logger.debug('FileModel', 'Converting MIDI to JsonMidi...');
                jsonMidi = await this.converter.midiToJson(fileData.data);
            }
            
            if (!jsonMidi) {
                throw new Error('No MIDI data provided');
            }
            
            // Créer fichier backend
            const response = await this.backend.sendCommand('files.create', {
                name: fileData.name,
                jsonmidi: jsonMidi,
                size: fileData.size || 0
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Failed to create file');
            }
            
            const file = this.normalizeFile(response.file);
            
            // Ajouter au cache
            this.files.set(file.id, file);
            this.addToMidiJsonCache(file.id, jsonMidi);
            
            // Mettre à jour index recherche
            this.addToSearchIndex(file);
            
            // Stats
            this.stats.uploadsCount++;
            this.state.totalFiles++;
            
            this.logger.info('FileModel', `✓ File created: ${file.id}`);
            
            // Événement file:created (original)
            this.eventBus.emit('file:created', {
                file: file
            });
            
            // ✅ NOUVEAU v3.0.1 - Événement file:list:updated
            this.eventBus.emit('file:list:updated', {
                files: Array.from(this.files.values()),
                count: this.files.size,
                directory: this.state.currentDirectory,
                timestamp: Date.now()
            });
            
            return file;
            
        } catch (error) {
            this.logger.error('FileModel', 'Failed to create file:', error);
            
            this.eventBus.emit('files:error', {
                type: 'create',
                error: error.message
            });
            
            throw error;
        }
    }
    
    async upload(file) {
        this.logger.info('FileModel', `Uploading file: ${file.name}`);
        
        try {
            // Lire fichier
            const arrayBuffer = await file.arrayBuffer();
            
            // Convertir en JsonMidi
            const jsonMidi = await this.converter.midiToJson(arrayBuffer);
            
            // Créer
            return await this.create({
                name: file.name,
                midiJson: jsonMidi,
                size: file.size
            });
            
        } catch (error) {
            this.logger.error('FileModel', 'Upload failed:', error);
            throw error;
        }
    }
    
    // ========================================================================
    // METTRE À JOUR
    // ========================================================================
    
    async update(fileId, updates) {
        this.logger.info('FileModel', `Updating file: ${fileId}`);
        
        try {
            const response = await this.backend.sendCommand('files.update', {
                file_id: fileId,
                updates: updates
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Failed to update file');
            }
            
            const updatedFile = this.normalizeFile(response.file);
            
            // Mettre à jour cache
            this.files.set(fileId, updatedFile);
            
            // Invalider JsonMidi cache si modifié
            if (updates.midiJson) {
                this.midiJsonCache.delete(fileId);
            }
            
            // Mettre à jour index
            this.updateSearchIndex(updatedFile);
            
            this.logger.info('FileModel', `✓ File updated: ${fileId}`);
            
            this.eventBus.emit('file:updated', {
                fileId: fileId,
                file: updatedFile,
                changes: updates
            });
            
            return updatedFile;
            
        } catch (error) {
            this.logger.error('FileModel', 'Failed to update file:', error);
            throw error;
        }
    }
    
    async rename(fileId, newName) {
        return await this.update(fileId, { name: newName });
    }
    
    async updateMetadata(fileId, metadata) {
        return await this.update(fileId, { metadata: metadata });
    }
    
    // ========================================================================
    // SUPPRIMER
    // ========================================================================
    
    async delete(fileId) {
        this.logger.info('FileModel', `Deleting file: ${fileId}`);
        
        try {
            const response = await this.backend.sendCommand('files.delete', {
                file_id: fileId
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Failed to delete file');
            }
            
            // Retirer du cache
            const file = this.files.get(fileId);
            this.files.delete(fileId);
            this.midiJsonCache.delete(fileId);
            this.metadataCache.delete(fileId);
            
            // Retirer de l'index
            this.removeFromSearchIndex(fileId);
            
            // Stats
            this.stats.deletesCount++;
            this.state.totalFiles--;
            
            this.logger.info('FileModel', `✓ File deleted: ${fileId}`);
            
            // Événement file:deleted (original)
            this.eventBus.emit('file:deleted', {
                fileId: fileId,
                file: file
            });
            
            // ✅ NOUVEAU v3.0.1 - Événement file:list:updated
            this.eventBus.emit('file:list:updated', {
                files: Array.from(this.files.values()),
                count: this.files.size,
                directory: this.state.currentDirectory,
                timestamp: Date.now()
            });
            
            return true;
            
        } catch (error) {
            this.logger.error('FileModel', 'Failed to delete file:', error);
            throw error;
        }
    }
    
    async deleteMultiple(fileIds) {
        const results = {
            success: [],
            failed: []
        };
        
        for (const fileId of fileIds) {
            try {
                await this.delete(fileId);
                results.success.push(fileId);
            } catch (error) {
                results.failed.push({ fileId, error: error.message });
            }
        }
        
        this.eventBus.emit('files:deleted', {
            count: results.success.length,
            failed: results.failed.length
        });
        
        return results;
    }
    
    // ========================================================================
    // EXPORT
    // ========================================================================
    
    async exportToMidi(fileId, outputPath) {
        this.logger.info('FileModel', `Exporting to MIDI: ${fileId} → ${outputPath}`);
        
        try {
            // Charger JsonMidi
            const jsonMidi = await this.loadMidiJson(fileId);
            
            // Exporter
            const response = await this.backend.sendCommand('files.export', {
                jsonmidi: jsonMidi,
                format: 'midi',
                output_path: outputPath
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Failed to export');
            }
            
            this.logger.info('FileModel', '✓ Exported to MIDI');
            
            return response.output_path;
            
        } catch (error) {
            this.logger.error('FileModel', 'Export failed:', error);
            throw error;
        }
    }
    
    async downloadAsMidi(fileId) {
        // Charger JsonMidi
        const jsonMidi = await this.loadMidiJson(fileId);
        
        // Convertir en MIDI binaire
        const midiData = this.converter.jsonToMidi(jsonMidi);
        
        // Créer blob et télécharger
        const blob = new Blob([midiData], { type: 'audio/midi' });
        const url = URL.createObjectURL(blob);
        
        const file = this.files.get(fileId);
        const filename = file?.name || 'export.mid';
        
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        
        this.logger.info('FileModel', `✓ Downloaded: ${filename}`);
    }
    
    // ========================================================================
    // RECHERCHE
    // ========================================================================
    
    search(query, options = {}) {
        const {
            fields = ['name', 'metadata.title', 'metadata.author'],
            caseSensitive = false,
            limit = 50
        } = options;
        
        const searchTerm = caseSensitive ? query : query.toLowerCase();
        const results = [];
        
        for (const file of this.files.values()) {
            let matches = false;
            
            for (const field of fields) {
                const value = this.getNestedValue(file, field);
                
                if (value) {
                    const searchValue = caseSensitive ? 
                        String(value) : String(value).toLowerCase();
                    
                    if (searchValue.includes(searchTerm)) {
                        matches = true;
                        break;
                    }
                }
            }
            
            if (matches) {
                results.push(file);
                
                if (results.length >= limit) {
                    break;
                }
            }
        }
        
        this.logger.debug('FileModel', 
            `Search "${query}": ${results.length} results`);
        
        return results;
    }
    
    filter(predicate) {
        return Array.from(this.files.values()).filter(predicate);
    }
    
    findByName(name) {
        return Array.from(this.files.values()).find(f => f.name === name);
    }
    
    findByPath(path) {
        return Array.from(this.files.values()).find(f => f.path === path);
    }
    
    // ========================================================================
    // TRI
    // ========================================================================
    
    sort(sortBy = 'name', sortOrder = 'asc') {
        this.state.sortBy = sortBy;
        this.state.sortOrder = sortOrder;
        
        const files = Array.from(this.files.values());
        
        files.sort((a, b) => {
            let aVal, bVal;
            
            switch (sortBy) {
                case 'name':
                    aVal = a.name || '';
                    bVal = b.name || '';
                    break;
                    
                case 'date':
                    aVal = a.modifiedAt || a.createdAt || 0;
                    bVal = b.modifiedAt || b.createdAt || 0;
                    break;
                    
                case 'size':
                    aVal = a.size || 0;
                    bVal = b.size || 0;
                    break;
                    
                case 'duration':
                    aVal = a.metadata?.duration || 0;
                    bVal = b.metadata?.duration || 0;
                    break;
                    
                default:
                    return 0;
            }
            
            if (aVal < bVal) return sortOrder === 'asc' ? -1 : 1;
            if (aVal > bVal) return sortOrder === 'asc' ? 1 : -1;
            return 0;
        });
        
        return files;
    }
    
    getAll(sorted = true) {
        if (sorted) {
            return this.sort(this.state.sortBy, this.state.sortOrder);
        }
        return Array.from(this.files.values());
    }
    
    // ========================================================================
    // CACHE MANAGEMENT
    // ========================================================================
    
    addToMidiJsonCache(fileId, jsonMidi) {
        // Vérifier taille max
        if (this.midiJsonCache.size >= this.cacheConfig.maxMidiJsonSize) {
            // Supprimer le plus ancien
            const firstKey = this.midiJsonCache.keys().next().value;
            this.midiJsonCache.delete(firstKey);
        }
        
        this.midiJsonCache.set(fileId, jsonMidi);
        
        this.logger.debug('FileModel', 
            `MidiJson cached: ${fileId} (${this.midiJsonCache.size}/${this.cacheConfig.maxMidiJsonSize})`);
    }
    
    clearCache() {
        this.files.clear();
        this.midiJsonCache.clear();
        this.metadataCache.clear();
        
        this.logger.info('FileModel', 'Cache cleared');
    }
    
    invalidateCache(fileId) {
        if (fileId) {
            this.files.delete(fileId);
            this.midiJsonCache.delete(fileId);
            this.metadataCache.delete(fileId);
        } else {
            this.clearCache();
        }
    }
    
    getCacheStats() {
        return {
            filesCount: this.files.size,
            midiJsonCount: this.midiJsonCache.size,
            maxSize: this.cacheConfig.maxSize,
            maxMidiJsonSize: this.cacheConfig.maxMidiJsonSize,
            hits: this.stats.cacheHits,
            misses: this.stats.cacheMisses,
            hitRate: this.stats.cacheHits + this.stats.cacheMisses > 0
                ? ((this.stats.cacheHits / (this.stats.cacheHits + this.stats.cacheMisses)) * 100).toFixed(2) + '%'
                : '0%'
        };
    }
    
    // ========================================================================
    // INDEX RECHERCHE
    // ========================================================================
    
    buildSearchIndex() {
        this.searchIndex.clear();
        
        for (const file of this.files.values()) {
            this.addToSearchIndex(file);
        }
        
        this.logger.debug('FileModel', 
            `Search index built: ${this.searchIndex.size} terms`);
    }
    
    addToSearchIndex(file) {
        const terms = new Set();
        
        // Extraire termes du nom
        if (file.name) {
            this.extractTerms(file.name, terms);
        }
        
        // Extraire termes des métadonnées
        if (file.metadata) {
            if (file.metadata.title) {
                this.extractTerms(file.metadata.title, terms);
            }
            if (file.metadata.author) {
                this.extractTerms(file.metadata.author, terms);
            }
        }
        
        // Ajouter à l'index
        for (const term of terms) {
            if (!this.searchIndex.has(term)) {
                this.searchIndex.set(term, new Set());
            }
            this.searchIndex.get(term).add(file.id);
        }
    }
    
    updateSearchIndex(file) {
        this.removeFromSearchIndex(file.id);
        this.addToSearchIndex(file);
    }
    
    removeFromSearchIndex(fileId) {
        for (const [term, fileIds] of this.searchIndex.entries()) {
            fileIds.delete(fileId);
            
            // Supprimer terme si plus de fichiers
            if (fileIds.size === 0) {
                this.searchIndex.delete(term);
            }
        }
    }
    
    extractTerms(text, terms) {
        // Normaliser et découper
        const normalized = text.toLowerCase()
            .replace(/[^\w\s]/g, ' ')
            .trim();
        
        const words = normalized.split(/\s+/);
        
        words.forEach(word => {
            if (word.length >= 2) {  // Min 2 caractères
                terms.add(word);
            }
        });
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    normalizeFile(file) {
        return {
            id: file.id || file.file_id,
            name: file.name || 'Untitled',
            path: file.path || file.filepath || null,
            size: file.size || 0,
            duration: file.duration || file.metadata?.duration || 0,
            metadata: file.metadata || {},
            createdAt: file.createdAt || file.created_at || Date.now(),
            modifiedAt: file.modifiedAt || file.modified_at || Date.now()
        };
    }
    
    getNestedValue(obj, path) {
        return path.split('.').reduce((acc, part) => acc?.[part], obj);
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    getCount() {
        return this.files.size;
    }
    
    getTotalSize() {
        return this.state.totalSize;
    }
    
    getState() {
        return {
            ...this.state,
            cacheStats: this.getCacheStats()
        };
    }
    
    getStats() {
        return {
            ...this.stats,
            cacheStats: this.getCacheStats()
        };
    }
    
    isLoading() {
        return this.state.isLoading;
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = FileModel;
}

if (typeof window !== 'undefined') {
    window.FileModel = FileModel;
}