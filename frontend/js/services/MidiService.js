// ============================================================================
// Fichier: frontend/scripts/services/MidiService.js
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Service centralisé pour toutes les opérations MIDI.
//   Parse les fichiers, gère les métadonnées, convertit les formats.
//
// Auteur: midiMind Team
// Date: 2025-10-04
// Version: 3.0.0
// ============================================================================

class MidiService {
    constructor(eventBus, logger) {
        this.eventBus = eventBus;
        this.logger = logger;
        
        // Cache des métadonnées parsées
        this.metadataCache = new Map();
        
        // Configuration
        this.config = {
            maxCacheSize: 100,
            cacheExpiration: 3600000, // 1 heure
            supportedFormats: ['.mid', '.midi', '.MID', '.MIDI'],
            maxFileSize: 10 * 1024 * 1024 // 10MB
        };
        
        // Parser MIDI (utilise MidiParser existant si disponible)
        this.parser = typeof MidiParser !== 'undefined' ? new MidiParser() : null;
        
        // Statistiques
        this.stats = {
            filesParsed: 0,
            cacheHits: 0,
            cacheMisses: 0,
            parseErrors: 0
        };
        
        this.initialize();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    initialize() {
        this.logger.info('MidiService', 'Initializing MIDI service...');
        
        // Nettoyer le cache périodiquement
        setInterval(() => this.cleanCache(), 60000); // Toutes les minutes
    }
    
    // ========================================================================
    // PARSING DE FICHIERS MIDI
    // ========================================================================
    
    /**
     * Parser un fichier MIDI et extraire les métadonnées
     * @param {ArrayBuffer|Uint8Array} data - Données du fichier MIDI
     * @param {string} fileName - Nom du fichier
     * @returns {Object} Métadonnées du fichier
     */
    async parseFile(data, fileName = 'unknown.mid') {
        try {
            this.logger.debug('MidiService', `Parsing file: ${fileName}`);
            
            // Vérifier le cache
            const cacheKey = this.generateCacheKey(data);
            if (this.metadataCache.has(cacheKey)) {
                const cached = this.metadataCache.get(cacheKey);
                if (Date.now() - cached.timestamp < this.config.cacheExpiration) {
                    this.stats.cacheHits++;
                    return cached.metadata;
                }
            }
            
            this.stats.cacheMisses++;
            
            // Parser le fichier
            const metadata = await this._parseFileInternal(data, fileName);
            
            // Mettre en cache
            this.metadataCache.set(cacheKey, {
                metadata,
                timestamp: Date.now()
            });
            
            // Limiter la taille du cache
            if (this.metadataCache.size > this.config.maxCacheSize) {
                const firstKey = this.metadataCache.keys().next().value;
                this.metadataCache.delete(firstKey);
            }
            
            this.stats.filesParsed++;
            
            return metadata;
            
        } catch (error) {
            this.logger.error('MidiService', `Failed to parse file ${fileName}:`, error);
            this.stats.parseErrors++;
            throw error;
        }
    }
    
    /**
     * Parser interne du fichier MIDI
     */
    async _parseFileInternal(data, fileName) {
        // Si MidiParser est disponible, l'utiliser
        if (this.parser) {
            return this.parser.parse(data);
        }
        
        // Sinon, parser manuellement (version simplifiée)
        const view = new DataView(data.buffer || data);
        const metadata = {
            fileName: fileName,
            fileSize: data.byteLength,
            format: 0,
            trackCount: 0,
            division: 480,
            duration: 0,
            tempo: 120,
            timeSignature: '4/4',
            tracks: [],
            instruments: [],
            noteCount: 0,
            minNote: 127,
            maxNote: 0,
            hasLyrics: false,
            hasMarkers: false
        };
        
        // Vérifier l'en-tête MThd
        const headerChunk = this.readChunk(view, 0);
        if (headerChunk.type !== 'MThd') {
            throw new Error('Invalid MIDI file: missing MThd header');
        }
        
        // Lire les données de l'en-tête
        metadata.format = view.getUint16(14, false);
        metadata.trackCount = view.getUint16(16, false);
        metadata.division = view.getUint16(18, false);
        
        // Parser chaque piste
        let offset = 14 + headerChunk.length;
        for (let i = 0; i < metadata.trackCount; i++) {
            const trackData = this.parseTrack(view, offset);
            metadata.tracks.push(trackData);
            offset += trackData.size;
            
            // Collecter les statistiques
            metadata.noteCount += trackData.noteCount;
            metadata.minNote = Math.min(metadata.minNote, trackData.minNote);
            metadata.maxNote = Math.max(metadata.maxNote, trackData.maxNote);
            
            if (trackData.name) {
                metadata.instruments.push(trackData.name);
            }
            
            if (trackData.hasLyrics) metadata.hasLyrics = true;
            if (trackData.hasMarkers) metadata.hasMarkers = true;
        }
        
        // Calculer la durée totale
        metadata.duration = this.calculateDuration(metadata);
        
        return metadata;
    }
    
    /**
     * Lire un chunk MIDI
     */
    readChunk(view, offset) {
        const type = String.fromCharCode(
            view.getUint8(offset),
            view.getUint8(offset + 1),
            view.getUint8(offset + 2),
            view.getUint8(offset + 3)
        );
        const length = view.getUint32(offset + 4, false);
        
        return { type, length };
    }
    
    /**
     * Parser une piste MIDI (simplifié)
     */
    parseTrack(view, offset) {
        const chunk = this.readChunk(view, offset);
        if (chunk.type !== 'MTrk') {
            throw new Error('Invalid track chunk');
        }
        
        const trackData = {
            size: 8 + chunk.length,
            name: null,
            noteCount: 0,
            minNote: 127,
            maxNote: 0,
            hasLyrics: false,
            hasMarkers: false,
            events: []
        };
        
        // Parser simplifié - compter les événements principaux
        let position = offset + 8;
        const endPosition = position + chunk.length;
        let runningStatus = 0;
        
        while (position < endPosition) {
            // Lire le delta time (variable length)
            let deltaTime = 0;
            let byte;
            do {
                byte = view.getUint8(position++);
                deltaTime = (deltaTime << 7) | (byte & 0x7F);
            } while (byte & 0x80);
            
            // Lire le statut
            byte = view.getUint8(position++);
            let status = byte;
            
            if (byte < 0x80) {
                // Running status
                status = runningStatus;
                position--;
            } else {
                runningStatus = status;
            }
            
            // Traiter l'événement
            const eventType = status & 0xF0;
            
            switch (eventType) {
                case 0x90: // Note On
                case 0x80: // Note Off
                    const note = view.getUint8(position++);
                    const velocity = view.getUint8(position++);
                    
                    if (eventType === 0x90 && velocity > 0) {
                        trackData.noteCount++;
                        trackData.minNote = Math.min(trackData.minNote, note);
                        trackData.maxNote = Math.max(trackData.maxNote, note);
                    }
                    break;
                    
                case 0xA0: // Poly Aftertouch
                case 0xB0: // Control Change
                case 0xE0: // Pitch Bend
                    position += 2;
                    break;
                    
                case 0xC0: // Program Change
                case 0xD0: // Channel Aftertouch
                    position += 1;
                    break;
                    
                case 0xF0: // System/Meta
                    if (status === 0xFF) {
                        // Meta event
                        const metaType = view.getUint8(position++);
                        const length = this.readVariableLength(view, position);
                        position += length.bytesRead;
                        
                        // Extraire les informations importantes
                        if (metaType === 0x03) { // Track name
                            trackData.name = this.readString(view, position, length.value);
                        } else if (metaType === 0x05) { // Lyrics
                            trackData.hasLyrics = true;
                        } else if (metaType === 0x06) { // Marker
                            trackData.hasMarkers = true;
                        }
                        
                        position += length.value;
                    } else if (status === 0xF0 || status === 0xF7) {
                        // SysEx
                        const length = this.readVariableLength(view, position);
                        position += length.bytesRead + length.value;
                    }
                    break;
            }
        }
        
        return trackData;
    }
    
    /**
     * Lire une longueur variable (MIDI variable-length quantity)
     */
    readVariableLength(view, offset) {
        let value = 0;
        let bytesRead = 0;
        let byte;
        
        do {
            byte = view.getUint8(offset + bytesRead);
            value = (value << 7) | (byte & 0x7F);
            bytesRead++;
        } while (byte & 0x80);
        
        return { value, bytesRead };
    }
    
    /**
     * Lire une chaîne de caractères
     */
    readString(view, offset, length) {
        let str = '';
        for (let i = 0; i < length; i++) {
            str += String.fromCharCode(view.getUint8(offset + i));
        }
        return str;
    }
    
    // ========================================================================
    // CALCULS ET CONVERSIONS
    // ========================================================================
    
    /**
     * Calculer la durée totale du fichier
     */
    calculateDuration(metadata) {
        // Calcul simplifié basé sur le tempo par défaut
        // Pour une implémentation complète, il faudrait parser tous les événements tempo
        const ticksPerQuarter = metadata.division;
        const microsecondsPerQuarter = 500000; // Tempo par défaut (120 BPM)
        
        // Trouver le dernier événement
        let maxTicks = 0;
        for (const track of metadata.tracks) {
            // Dans une implémentation complète, on accumulerait les delta times
            maxTicks = Math.max(maxTicks, 10000); // Valeur par défaut
        }
        
        // Convertir en millisecondes
        const quarters = maxTicks / ticksPerQuarter;
        const microseconds = quarters * microsecondsPerQuarter;
        const milliseconds = microseconds / 1000;
        
        return Math.round(milliseconds);
    }
    
    /**
     * Extraire le BPM du fichier
     */
    extractBPM(metadata) {
        // BPM = 60,000,000 / microsecondsPerQuarter
        // Par défaut: 120 BPM
        return metadata.tempo || 120;
    }
    
    /**
     * Obtenir le nom de la note à partir du numéro MIDI
     */
    getNoteName(noteNumber) {
        if (typeof MidiConstants !== 'undefined') {
            return MidiConstants.getNoteName(noteNumber);
        }
        
        const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const octave = Math.floor(noteNumber / 12) - 1;
        const note = noteNames[noteNumber % 12];
        return `${note}${octave}`;
    }
    
    /**
     * Convertir la durée en format lisible
     */
    formatDuration(milliseconds) {
        const totalSeconds = Math.floor(milliseconds / 1000);
        const minutes = Math.floor(totalSeconds / 60);
        const seconds = totalSeconds % 60;
        return `${minutes}:${seconds.toString().padStart(2, '0')}`;
    }
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    /**
     * Vérifier si un fichier est un MIDI valide
     */
    isValidMidiFile(data) {
        if (!data || data.byteLength < 14) {
            return false;
        }
        
        const view = new DataView(data.buffer || data);
        
        // Vérifier l'en-tête MThd
        const header = String.fromCharCode(
            view.getUint8(0),
            view.getUint8(1),
            view.getUint8(2),
            view.getUint8(3)
        );
        
        return header === 'MThd';
    }
    
    /**
     * Vérifier si une extension est supportée
     */
    isSupportedFormat(fileName) {
        const extension = fileName.substring(fileName.lastIndexOf('.')).toLowerCase();
        return this.config.supportedFormats.includes(extension);
    }
    
    // ========================================================================
    // CACHE
    // ========================================================================
    
    /**
     * Générer une clé de cache unique pour les données
     */
    generateCacheKey(data) {
        // Simple hash basé sur la taille et les premiers octets
        const view = new DataView(data.buffer || data);
        let hash = data.byteLength;
        
        for (let i = 0; i < Math.min(100, data.byteLength); i++) {
            hash = ((hash << 5) - hash) + view.getUint8(i);
            hash = hash & hash; // Convertir en 32-bit integer
        }
        
        return hash.toString(16);
    }
    
    /**
     * Nettoyer le cache expiré
     */
    cleanCache() {
        const now = Date.now();
        const expiredKeys = [];
        
        for (const [key, value] of this.metadataCache) {
            if (now - value.timestamp > this.config.cacheExpiration) {
                expiredKeys.push(key);
            }
        }
        
        for (const key of expiredKeys) {
            this.metadataCache.delete(key);
        }
        
        if (expiredKeys.length > 0) {
            this.logger.debug('MidiService', `Cleaned ${expiredKeys.length} expired cache entries`);
        }
    }
    
    /**
     * Vider tout le cache
     */
    clearCache() {
        this.metadataCache.clear();
        this.logger.info('MidiService', 'Cache cleared');
    }
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    getStats() {
        return {
            ...this.stats,
            cacheSize: this.metadataCache.size,
            cacheHitRate: this.stats.cacheHits / (this.stats.cacheHits + this.stats.cacheMisses) || 0
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = MidiService;
}

if (typeof window !== 'undefined') {
    window.MidiService = MidiService;
}