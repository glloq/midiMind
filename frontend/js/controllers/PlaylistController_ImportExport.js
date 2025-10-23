// ============================================================================
// Fichier: frontend/js/controllers/PlaylistController_ImportExport.js
// Projet: MidiMind v3.0 - SystÃ¨me d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0 - 2025-10-09
// ============================================================================
// Description:
//   Module d'import/export de playlists
//   GÃ¨re les formats: M3U, PLS, XSPF, JSON
//
// FonctionnalitÃ©s:
//   - Export playlists (M3U, PLS, XSPF, JSON)
//   - Import playlists depuis fichiers
//   - Validation et parsing
//   - RÃ©solution des chemins de fichiers
// ============================================================================

/**
 * Extension du PlaylistController avec fonctionnalitÃ©s Import/Export
 * @module PlaylistController_ImportExport
 */

const PlaylistImportExport = {

    // ========================================================================
    // CONSTANTES
    // ========================================================================

    SUPPORTED_FORMATS: ['m3u', 'm3u8', 'pls', 'xspf', 'json'],
    
    MAX_FILE_SIZE: 10 * 1024 * 1024, // 10MB
    
    // ========================================================================
    // EXPORT
    // ========================================================================

    /**
     * Exporte une playlist dans le format spÃ©cifiÃ©
     * @param {Object} playlist - Playlist Ã  exporter
     * @param {string} format - Format: 'm3u', 'pls', 'xspf', 'json'
     * @returns {string} Contenu du fichier exportÃ©
     */
    async exportPlaylist(playlist, format = 'm3u') {
        this.logDebug('playlist', `Exporting playlist as ${format.toUpperCase()}`);
        
        if (!playlist || !playlist.files || playlist.files.length === 0) {
            throw new Error('Playlist is empty or invalid');
        }
        
        // RÃ©soudre les fichiers complets
        const files = await this.resolvePlaylistFiles(playlist.files);
        
        if (files.length === 0) {
            throw new Error('No valid files found in playlist');
        }
        
        // Exporter selon le format
        switch (format.toLowerCase()) {
            case 'm3u':
            case 'm3u8':
                return this.exportToM3U(playlist, files);
            
            case 'pls':
                return this.exportToPLS(playlist, files);
            
            case 'xspf':
                return this.exportToXSPF(playlist, files);
            
            case 'json':
                return this.exportToJSON(playlist, files);
            
            default:
                throw new Error(`Unsupported export format: ${format}`);
        }
    },

    /**
     * Exporte au format M3U
     * @private
     */
    exportToM3U(playlist, files) {
        let m3u = '#EXTM3U\n';
        m3u += `#PLAYLIST:${this.escapeM3U(playlist.name)}\n`;
        
        if (playlist.description) {
            m3u += `#EXTENC:${this.escapeM3U(playlist.description)}\n`;
        }
        
        for (const file of files) {
            // #EXTINF:duration,Artist - Title
            const duration = Math.floor((file.duration || 0) / 1000);
            const artist = file.metadata?.composer || 'Unknown';
            const title = file.metadata?.title || file.name;
            
            m3u += `#EXTINF:${duration},${artist} - ${title}\n`;
            m3u += `${this.makeRelativePath(file.path)}\n`;
        }
        
        return m3u;
    },

    /**
     * Exporte au format PLS
     * @private
     */
    exportToPLS(playlist, files) {
        let pls = '[playlist]\n';
        pls += `PlaylistName=${this.escapePLS(playlist.name)}\n`;
        pls += `NumberOfEntries=${files.length}\n\n`;
        
        files.forEach((file, index) => {
            const num = index + 1;
            const duration = Math.floor((file.duration || 0) / 1000);
            const title = file.metadata?.title || file.name;
            
            pls += `File${num}=${this.makeRelativePath(file.path)}\n`;
            pls += `Title${num}=${this.escapePLS(title)}\n`;
            pls += `Length${num}=${duration}\n\n`;
        });
        
        pls += 'Version=2\n';
        
        return pls;
    },

    /**
     * Exporte au format XSPF (XML Shareable Playlist Format)
     * @private
     */
    exportToXSPF(playlist, files) {
        let xspf = '<?xml version="1.0" encoding="UTF-8"?>\n';
        xspf += '<playlist version="1" xmlns="http://xspf.org/ns/0/">\n';
        xspf += `  <title>${this.escapeXml(playlist.name)}</title>\n`;
        
        if (playlist.description) {
            xspf += `  <annotation>${this.escapeXml(playlist.description)}</annotation>\n`;
        }
        
        xspf += `  <creator>MidiMind v3.0</creator>\n`;
        xspf += `  <date>${new Date().toISOString()}</date>\n`;
        
        xspf += '  <trackList>\n';
        
        for (const file of files) {
            xspf += '    <track>\n';
            xspf += `      <location>file://${this.escapeXml(file.path)}</location>\n`;
            
            if (file.metadata?.title) {
                xspf += `      <title>${this.escapeXml(file.metadata.title)}</title>\n`;
            }
            
            if (file.metadata?.composer) {
                xspf += `      <creator>${this.escapeXml(file.metadata.composer)}</creator>\n`;
            }
            
            if (file.duration) {
                xspf += `      <duration>${file.duration}</duration>\n`;
            }
            
            xspf += '    </track>\n';
        }
        
        xspf += '  </trackList>\n';
        xspf += '</playlist>\n';
        
        return xspf;
    },

    /**
     * Exporte au format JSON (format natif MidiMind)
     * @private
     */
    exportToJSON(playlist, files) {
        const exportData = {
            version: '1.0',
            format: 'MidiMind Playlist',
            exported_at: new Date().toISOString(),
            playlist: {
                name: playlist.name,
                description: playlist.description || '',
                created_at: playlist.createdAt,
                files: files.map(file => ({
                    path: this.makeRelativePath(file.path),
                    id: file.id,
                    name: file.name,
                    duration: file.duration,
                    size: file.size,
                    metadata: file.metadata || {}
                })),
                metadata: {
                    total_files: files.length,
                    total_duration: this.calculateTotalDuration(files),
                    total_size: this.calculateTotalSize(files)
                }
            }
        };
        
        return JSON.stringify(exportData, null, 2);
    },

    /**
     * TÃ©lÃ©charge une playlist exportÃ©e
     * @param {Object} playlist - Playlist Ã  exporter
     * @param {string} format - Format d'export
     */
    async downloadPlaylist(playlist, format = 'm3u') {
        try {
            const content = await this.exportPlaylist(playlist, format);
            
            const filename = this.sanitizeFilename(playlist.name) + '.' + format;
            
            const blob = new Blob([content], { 
                type: this.getMimeType(format) 
            });
            
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = filename;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
            
            this.logInfo('playlist', `âœ“ Playlist exported: ${filename}`);
            
        } catch (error) {
            this.logError('playlist', 'Export failed:', error);
            throw error;
        }
    },

    // ========================================================================
    // IMPORT
    // ========================================================================

    /**
     * Importe une playlist depuis un fichier
     * @param {File} file - Fichier Ã  importer
     * @returns {Object} Playlist importÃ©e
     */
    async importPlaylist(file) {
        this.logInfo('playlist', `Importing playlist: ${file.name}`);
        
        // VÃ©rifier la taille
        if (file.size > this.MAX_FILE_SIZE) {
            throw new Error('File too large (max 10MB)');
        }
        
        // DÃ©tecter le format
        const format = this.detectFormat(file.name);
        
        if (!this.SUPPORTED_FORMATS.includes(format)) {
            throw new Error(`Unsupported format: ${format}`);
        }
        
        // Lire le contenu
        const content = await this.readFileContent(file);
        
        // Parser selon le format
        let playlistData;
        
        switch (format) {
            case 'm3u':
            case 'm3u8':
                playlistData = this.parseM3U(content);
                break;
            
            case 'pls':
                playlistData = this.parsePLS(content);
                break;
            
            case 'xspf':
                playlistData = this.parseXSPF(content);
                break;
            
            case 'json':
                playlistData = this.parseJSON(content);
                break;
            
            default:
                throw new Error(`Cannot parse format: ${format}`);
        }
        
        // RÃ©soudre les IDs de fichiers
        playlistData.fileIds = await this.resolveFileIds(playlistData.files);
        
        this.logInfo('playlist', `âœ“ Imported ${playlistData.fileIds.length} files`);
        
        return playlistData;
    },

    /**
     * Parse format M3U
     * @private
     */
    parseM3U(content) {
        const lines = content.split('\n').map(line => line.trim());
        
        let name = 'Imported Playlist';
        let description = '';
        const files = [];
        
        let currentFile = {};
        
        for (const line of lines) {
            if (!line || line.startsWith('#')) {
                // Directives M3U
                if (line.startsWith('#PLAYLIST:')) {
                    name = line.substring(10).trim();
                }
                else if (line.startsWith('#EXTENC:')) {
                    description = line.substring(8).trim();
                }
                else if (line.startsWith('#EXTINF:')) {
                    // #EXTINF:duration,Artist - Title
                    const match = line.match(/#EXTINF:(-?\d+),(.+)/);
                    if (match) {
                        currentFile.duration = parseInt(match[1]) * 1000; // En ms
                        currentFile.title = match[2].trim();
                    }
                }
                continue;
            }
            
            // Ligne de chemin de fichier
            currentFile.path = line;
            files.push({ ...currentFile });
            currentFile = {};
        }
        
        return { name, description, files };
    },

    /**
     * Parse format PLS
     * @private
     */
    parsePLS(content) {
        const lines = content.split('\n').map(line => line.trim());
        
        let name = 'Imported Playlist';
        const filesMap = {};
        
        for (const line of lines) {
            if (!line || line.startsWith('[')) continue;
            
            if (line.startsWith('PlaylistName=')) {
                name = line.substring(13);
                continue;
            }
            
            const match = line.match(/^(File|Title|Length)(\d+)=(.+)$/);
            if (!match) continue;
            
            const [, field, num, value] = match;
            
            if (!filesMap[num]) {
                filesMap[num] = {};
            }
            
            if (field === 'File') {
                filesMap[num].path = value;
            }
            else if (field === 'Title') {
                filesMap[num].title = value;
            }
            else if (field === 'Length') {
                filesMap[num].duration = parseInt(value) * 1000; // En ms
            }
        }
        
        const files = Object.values(filesMap).filter(f => f.path);
        
        return { name, description: '', files };
    },

    /**
     * Parse format XSPF
     * @private
     */
    parseXSPF(content) {
        const parser = new DOMParser();
        const doc = parser.parseFromString(content, 'text/xml');
        
        // VÃ©rifier erreurs de parsing
        if (doc.querySelector('parsererror')) {
            throw new Error('Invalid XSPF XML');
        }
        
        const playlist = doc.querySelector('playlist');
        if (!playlist) {
            throw new Error('Missing <playlist> element');
        }
        
        const name = this.getXmlText(playlist, 'title') || 'Imported Playlist';
        const description = this.getXmlText(playlist, 'annotation') || '';
        
        const files = [];
        const tracks = playlist.querySelectorAll('track');
        
        for (const track of tracks) {
            const location = this.getXmlText(track, 'location');
            if (!location) continue;
            
            // Retirer le prÃ©fixe file://
            const path = location.replace(/^file:\/\//, '');
            
            files.push({
                path: path,
                title: this.getXmlText(track, 'title'),
                creator: this.getXmlText(track, 'creator'),
                duration: parseInt(this.getXmlText(track, 'duration')) || 0
            });
        }
        
        return { name, description, files };
    },

    /**
     * Parse format JSON
     * @private
     */
    parseJSON(content) {
        const data = JSON.parse(content);
        
        // VÃ©rifier la structure
        if (!data.playlist || !data.playlist.files) {
            throw new Error('Invalid JSON playlist structure');
        }
        
        const p = data.playlist;
        
        return {
            name: p.name || 'Imported Playlist',
            description: p.description || '',
            files: p.files.map(file => ({
                path: file.path,
                id: file.id,
                title: file.metadata?.title,
                duration: file.duration || 0
            }))
        };
    },

    // ========================================================================
    // HELPERS
    // ========================================================================

    /**
     * Lit le contenu d'un fichier
     * @private
     */
    readFileContent(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            
            reader.onload = (e) => resolve(e.target.result);
            reader.onerror = (e) => reject(new Error('Failed to read file'));
            
            reader.readAsText(file);
        });
    },

    /**
     * DÃ©tecte le format depuis le nom de fichier
     * @private
     */
    detectFormat(filename) {
        const ext = filename.split('.').pop().toLowerCase();
        return ext;
    },

    /**
     * RÃ©cupÃ¨re le type MIME pour un format
     * @private
     */
    getMimeType(format) {
        const mimeTypes = {
            'm3u': 'audio/x-mpegurl',
            'm3u8': 'audio/x-mpegurl',
            'pls': 'audio/x-scpls',
            'xspf': 'application/xspf+xml',
            'json': 'application/json'
        };
        
        return mimeTypes[format] || 'text/plain';
    },

    /**
     * RÃ©sout les fichiers complets depuis les IDs
     * @private
     */
    async resolvePlaylistFiles(fileIds) {
        if (!this.fileModel) {
            return [];
        }
        
        const files = [];
        
        for (const fileId of fileIds) {
            const file = this.fileModel.get ? 
                await this.fileModel.get(fileId) : 
                this.fileModel.get(fileId);
            
            if (file) {
                files.push(file);
            }
        }
        
        return files;
    },

    /**
     * RÃ©sout les IDs de fichiers depuis paths
     * @private
     */
    async resolveFileIds(files) {
        if (!this.fileModel) {
            return [];
        }
        
        const fileIds = [];
        
        for (const fileData of files) {
            const path = fileData.path || fileData.id;
            
            // Chercher fichier par path
            const allFiles = this.fileModel.getAll ? this.fileModel.getAll() : [];
            const matchingFile = allFiles.find(f => 
                f.path === path || 
                f.relativePath === path ||
                f.id === path
            );
            
            if (matchingFile) {
                fileIds.push(matchingFile.id);
            } else {
                this.logDebug('playlist', `âš ï¸ File not found: ${path}`);
            }
        }
        
        return fileIds;
    },

    /**
     * Calcule durÃ©e totale
     * @private
     */
    calculateTotalDuration(files) {
        return files.reduce((sum, file) => sum + (file.duration || 0), 0);
    },

    /**
     * Calcule taille totale
     * @private
     */
    calculateTotalSize(files) {
        return files.reduce((sum, file) => sum + (file.size || 0), 0);
    },

    /**
     * Nettoie nom de fichier
     * @private
     */
    sanitizeFilename(name) {
        return name
            .replace(/[^a-z0-9_\-\.]/gi, '_')
            .replace(/_{2,}/g, '_')
            .substring(0, 100);
    },

    /**
     * Rend chemin relatif
     * @private
     */
    makeRelativePath(path) {
        // Retirer prÃ©fixe absolu commun si prÃ©sent
        const basePath = '/midi/';
        
        if (path.startsWith(basePath)) {
            return path.substring(basePath.length);
        }
        
        return path;
    },

    /**
     * Ã‰chappe M3U
     * @private
     */
    escapeM3U(str) {
        return str.replace(/\n/g, ' ').replace(/\r/g, '');
    },

    /**
     * Ã‰chappe PLS
     * @private
     */
    escapePLS(str) {
        return str.replace(/\n/g, ' ').replace(/\r/g, '');
    },

    /**
     * Ã‰chappe XML
     * @private
     */
    escapeXml(str) {
        return str
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&apos;');
    },

    /**
     * DÃ©sÃ©chappe XML
     * @private
     */
    unescapeXml(str) {
        return str
            .replace(/&amp;/g, '&')
            .replace(/&lt;/g, '<')
            .replace(/&gt;/g, '>')
            .replace(/&quot;/g, '"')
            .replace(/&apos;/g, "'");
    },

    /**
     * RÃ©cupÃ¨re texte d'un Ã©lÃ©ment XML
     * @private
     */
    getXmlText(parent, tagName) {
        const element = parent.querySelector(tagName);
        return element ? element.textContent.trim() : '';
    }
};

// ============================================================================
// EXPORT
// ============================================================================

// Export pour utilisation comme module
if (typeof module !== 'undefined' && module.exports) {
    module.exports = PlaylistImportExport;
}
window.PlaylistController_ImportExport = PlaylistImportExport;
// ============================================================================
// FIN DU MODULE PlaylistController_ImportExport.js
// ============================================================================