// ============================================================================
// Fichier: frontend/js/views/HomeView.js
// Version: v3.8.0 - INTERFACE REFACTORISÉE COHÉRENTE
// Date: 2025-10-29
// Projet: MidiMind v3.1
// ============================================================================
// NOUVELLES FONCTIONNALITÉS v3.8.0:
// ✅ Layout simplifié: sélection + visualizer
// ✅ Barre d'instruments avec routing 
// ✅ Bouton mute/unmute global
// ✅ Tabs fichiers/playlists
// ✅ Intégration visualizer live
// ============================================================================

class HomeView {
    constructor(container, eventBus) {
        // Container
        if (typeof container === 'string') {
            this.container = document.getElementById(container) || document.querySelector(container);
        } else if (container instanceof HTMLElement) {
            this.container = container;
        } else {
            this.container = null;
        }
        
        if (!this.container) {
            console.error('[HomeView] Container not found:', container);
        }
        
        this.eventBus = eventBus;
        this.logger = window.logger || console;
        
        // État
        this.state = {
            mode: 'files', // 'files' ou 'playlists'
            currentFile: null,
            currentPlaylist: null,
            files: [],
            playlists: [],
            instruments: [],
            allMuted: false
        };
        
        // Visualizer
        this.visualizerCanvas = null;
        this.visualizerContext = null;
        this.visualizerAnimationId = null;
        
        // Éléments DOM
        this.elements = {};
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    init() {
        if (!this.container) {
            this.logger.error('[HomeView] Cannot initialize: container not found');
            return;
        }
        
        this.render();
        this.cacheElements();
        this.attachEvents();
        this.initVisualizer();
        
        // Charger les données initiales
        this.loadFiles();
        this.loadPlaylists();
        this.loadInstruments();
        
        this.logger.info('[HomeView] Initialized');
    }

    render() {
        if (!this.container) return;
        
        this.container.innerHTML = `
            <div class="home-layout">
                
                <!-- Sélection fichiers/playlists -->
                <div class="home-selector">
                    <div class="selector-tabs">
                        <button class="selector-tab active" data-mode="files">Fichiers</button>
                        <button class="selector-tab" data-mode="playlists">Playlists</button>
                    </div>
                    
                    <div class="selector-content">
                        <!-- Liste des fichiers -->
                        <div class="files-list" id="homeFilesList">
                            ${this.renderEmptyState('files')}
                        </div>
                        
                        <!-- Liste des playlists -->
                        <div class="playlists-list" id="homePlaylistsList" style="display: none;">
                            ${this.renderEmptyState('playlists')}
                        </div>
                    </div>
                </div>
                
                <!-- Visualizer avec barre instruments -->
                <div class="home-visualizer">
                    <!-- Barre instruments -->
                    <div class="instruments-bar">
                        <div class="instruments-title">
                            <span class="icon">🎸</span>
                            <span class="label">Instruments actifs</span>
                        </div>
                        <div class="instruments-list" id="homeActiveInstruments">
                            <!-- Généré dynamiquement -->
                        </div>
                        <div class="instruments-actions">
                            <button class="btn-mute-all" id="homeBtnMuteAll" title="Mute/Unmute tous les canaux">
                                <span class="mute-icon">🔇</span>
                                <span class="mute-label">Mute All</span>
                            </button>
                        </div>
                    </div>
                    
                    <!-- Canvas visualizer -->
                    <div class="visualizer-container">
                        <canvas id="homeVisualizerCanvas" class="visualizer-canvas"></canvas>
                        <div class="visualizer-info" id="homeVisualizerInfo">
                            <span class="note-count">0 notes</span>
                            <span class="active-channels">0 canaux actifs</span>
                        </div>
                    </div>
                </div>
            </div>
        `;
    }

    cacheElements() {
        this.elements = {
            // Tabs
            tabFiles: this.container.querySelector('[data-mode="files"]'),
            tabPlaylists: this.container.querySelector('[data-mode="playlists"]'),
            
            // Listes
            filesList: document.getElementById('homeFilesList'),
            playlistsList: document.getElementById('homePlaylistsList'),
            
            // Instruments
            instrumentsList: document.getElementById('homeActiveInstruments'),
            btnMuteAll: document.getElementById('homeBtnMuteAll'),
            
            // Visualizer
            visualizerCanvas: document.getElementById('homeVisualizerCanvas'),
            visualizerInfo: document.getElementById('homeVisualizerInfo')
        };
    }

    attachEvents() {
        // Tabs
        if (this.elements.tabFiles) {
            this.elements.tabFiles.addEventListener('click', () => this.switchMode('files'));
        }
        if (this.elements.tabPlaylists) {
            this.elements.tabPlaylists.addEventListener('click', () => this.switchMode('playlists'));
        }
        
        // Mute global
        if (this.elements.btnMuteAll) {
            this.elements.btnMuteAll.addEventListener('click', () => this.toggleMuteAll());
        }
        
        // Délégation d'événements pour les listes
        if (this.elements.filesList) {
            this.elements.filesList.addEventListener('click', (e) => this.handleFileClick(e));
        }
        if (this.elements.playlistsList) {
            this.elements.playlistsList.addEventListener('click', (e) => this.handlePlaylistClick(e));
        }
        
        // EventBus
        this.setupEventBusListeners();
    }

    setupEventBusListeners() {
        if (!this.eventBus) return;
        
        // Fichiers
        this.eventBus.on('files:loaded', (data) => {
            this.state.files = data.files || [];
            this.renderFilesList();
        });
        
        this.eventBus.on('file:selected', (data) => {
            this.state.currentFile = data.file;
            this.updateFileSelection();
        });
        
        // Playlists
        this.eventBus.on('playlists:loaded', (data) => {
            this.state.playlists = data.playlists || [];
            this.renderPlaylistsList();
        });
        
        this.eventBus.on('playlist:selected', (data) => {
            this.state.currentPlaylist = data.playlist;
            this.updatePlaylistSelection();
        });
        
        // Instruments
        this.eventBus.on('instruments:updated', (data) => {
            this.state.instruments = data.instruments || [];
            this.renderInstrumentsList();
        });
        
        // MIDI
        this.eventBus.on('midi:note_on', (data) => {
            this.visualizeNote(data);
        });
        
        this.eventBus.on('midi:note_off', (data) => {
            this.clearNote(data);
        });
    }

    // ========================================================================
    // NAVIGATION TABS
    // ========================================================================

    switchMode(mode) {
        this.state.mode = mode;
        
        // Update tabs
        const tabs = this.container.querySelectorAll('.selector-tab');
        tabs.forEach(tab => {
            if (tab.dataset.mode === mode) {
                tab.classList.add('active');
            } else {
                tab.classList.remove('active');
            }
        });
        
        // Update lists visibility
        if (this.elements.filesList) {
            this.elements.filesList.style.display = mode === 'files' ? 'flex' : 'none';
        }
        if (this.elements.playlistsList) {
            this.elements.playlistsList.style.display = mode === 'playlists' ? 'flex' : 'none';
        }
    }

    // ========================================================================
    // RENDU DES LISTES
    // ========================================================================

    renderFilesList() {
        if (!this.elements.filesList) return;
        
        if (!this.state.files || this.state.files.length === 0) {
            this.elements.filesList.innerHTML = this.renderEmptyState('files');
            return;
        }
        
        this.elements.filesList.innerHTML = this.state.files
            .map(file => this.renderFileItem(file))
            .join('');
    }

    renderFileItem(file) {
        const isActive = this.state.currentFile && this.state.currentFile.id === file.id;
        const duration = this.formatDuration(file.duration || 0);
        const tracks = file.tracks || 0;
        
        return `
            <div class="file-item ${isActive ? 'active' : ''}" data-file-id="${file.id}">
                <div class="file-item-header">
                    <span class="file-item-icon">🎵</span>
                    <span class="file-item-name">${file.name || 'Sans nom'}</span>
                </div>
                <div class="file-item-info">
                    <span class="file-duration">⏱️ ${duration}</span>
                    <span class="file-tracks">🎹 ${tracks} pistes</span>
                </div>
            </div>
        `;
    }

    renderPlaylistsList() {
        if (!this.elements.playlistsList) return;
        
        if (!this.state.playlists || this.state.playlists.length === 0) {
            this.elements.playlistsList.innerHTML = this.renderEmptyState('playlists');
            return;
        }
        
        this.elements.playlistsList.innerHTML = this.state.playlists
            .map(playlist => this.renderPlaylistItem(playlist))
            .join('');
    }

    renderPlaylistItem(playlist) {
        const isActive = this.state.currentPlaylist && this.state.currentPlaylist.id === playlist.id;
        const fileCount = playlist.files ? playlist.files.length : 0;
        
        return `
            <div class="playlist-item ${isActive ? 'active' : ''}" data-playlist-id="${playlist.id}">
                <div class="playlist-item-header">
                    <span class="playlist-item-icon">📋</span>
                    <span class="playlist-item-name">${playlist.name || 'Sans nom'}</span>
                    <span class="playlist-item-count">${fileCount}</span>
                </div>
            </div>
        `;
    }

    renderInstrumentsList() {
        if (!this.elements.instrumentsList) return;
        
        if (!this.state.instruments || this.state.instruments.length === 0) {
            this.elements.instrumentsList.innerHTML = `
                <div class="empty-state" style="padding: 8px;">
                    <span style="font-size: 12px; color: var(--text-muted);">Aucun instrument connecté</span>
                </div>
            `;
            return;
        }
        
        this.elements.instrumentsList.innerHTML = this.state.instruments
            .map(instrument => this.renderInstrumentChip(instrument))
            .join('');
    }

    renderInstrumentChip(instrument) {
        const icon = this.getInstrumentIcon(instrument.type);
        const isActive = instrument.connected;
        
        return `
            <div class="instrument-chip ${isActive ? 'active' : ''}" data-instrument-id="${instrument.id}">
                <span class="instrument-chip-icon">${icon}</span>
                <span class="instrument-chip-name">${instrument.name}</span>
                <span class="instrument-chip-status"></span>
            </div>
        `;
    }

    renderEmptyState(type) {
        if (type === 'files') {
            return `
                <div class="empty-state">
                    <div class="empty-state-icon">🎵</div>
                    <div class="empty-state-text">Aucun fichier MIDI</div>
                    <div class="empty-state-hint">Importez des fichiers depuis la page Fichiers</div>
                </div>
            `;
        } else {
            return `
                <div class="empty-state">
                    <div class="empty-state-icon">📋</div>
                    <div class="empty-state-text">Aucune playlist</div>
                    <div class="empty-state-hint">Créez une playlist depuis la page Fichiers</div>
                </div>
            `;
        }
    }

    // ========================================================================
    // GESTION DES CLICS
    // ========================================================================

    handleFileClick(e) {
        const fileItem = e.target.closest('.file-item');
        if (!fileItem) return;
        
        const fileId = fileItem.dataset.fileId;
        const file = this.state.files.find(f => f.id === fileId);
        
        if (file) {
            this.selectFile(file);
        }
    }

    handlePlaylistClick(e) {
        const playlistItem = e.target.closest('.playlist-item');
        if (!playlistItem) return;
        
        const playlistId = playlistItem.dataset.playlistId;
        const playlist = this.state.playlists.find(p => p.id === playlistId);
        
        if (playlist) {
            this.selectPlaylist(playlist);
        }
    }

    // ========================================================================
    // SÉLECTION
    // ========================================================================

    selectFile(file) {
        this.state.currentFile = file;
        this.state.currentPlaylist = null;
        this.updateFileSelection();
        
        if (this.eventBus) {
            this.eventBus.emit('home:file_selected', { file });
        }
    }

    selectPlaylist(playlist) {
        this.state.currentPlaylist = playlist;
        this.state.currentFile = null;
        this.updatePlaylistSelection();
        
        if (this.eventBus) {
            this.eventBus.emit('home:playlist_selected', { playlist });
        }
    }

    updateFileSelection() {
        const items = this.elements.filesList?.querySelectorAll('.file-item');
        if (!items) return;
        
        items.forEach(item => {
            const isActive = this.state.currentFile && 
                           item.dataset.fileId === this.state.currentFile.id;
            item.classList.toggle('active', isActive);
        });
    }

    updatePlaylistSelection() {
        const items = this.elements.playlistsList?.querySelectorAll('.playlist-item');
        if (!items) return;
        
        items.forEach(item => {
            const isActive = this.state.currentPlaylist && 
                           item.dataset.playlistId === this.state.currentPlaylist.id;
            item.classList.toggle('active', isActive);
        });
    }

    // ========================================================================
    // MUTE GLOBAL
    // ========================================================================

    toggleMuteAll() {
        this.state.allMuted = !this.state.allMuted;
        
        // Update button appearance
        if (this.elements.btnMuteAll) {
            this.elements.btnMuteAll.classList.toggle('muted', this.state.allMuted);
            const icon = this.elements.btnMuteAll.querySelector('.mute-icon');
            const label = this.elements.btnMuteAll.querySelector('.mute-label');
            if (icon) icon.textContent = this.state.allMuted ? '🔊' : '🔇';
            if (label) label.textContent = this.state.allMuted ? 'Unmute All' : 'Mute All';
        }
        
        // Émettre événement
        if (this.eventBus) {
            this.eventBus.emit('home:mute_all_toggled', { muted: this.state.allMuted });
        }
    }

    // ========================================================================
    // VISUALIZER
    // ========================================================================

    initVisualizer() {
        this.visualizerCanvas = this.elements.visualizerCanvas;
        if (!this.visualizerCanvas) return;
        
        this.visualizerContext = this.visualizerCanvas.getContext('2d');
        
        // Resize canvas
        this.resizeVisualizer();
        window.addEventListener('resize', () => this.resizeVisualizer());
        
        // Start animation loop
        this.startVisualizerLoop();
    }

    resizeVisualizer() {
        if (!this.visualizerCanvas) return;
        
        const container = this.visualizerCanvas.parentElement;
        if (!container) return;
        
        this.visualizerCanvas.width = container.clientWidth;
        this.visualizerCanvas.height = container.clientHeight;
    }

    startVisualizerLoop() {
        const animate = () => {
            this.renderVisualizer();
            this.visualizerAnimationId = requestAnimationFrame(animate);
        };
        animate();
    }

    renderVisualizer() {
        if (!this.visualizerContext || !this.visualizerCanvas) return;
        
        const ctx = this.visualizerContext;
        const width = this.visualizerCanvas.width;
        const height = this.visualizerCanvas.height;
        
        // Clear with gradient
        const gradient = ctx.createLinearGradient(0, 0, width, height);
        gradient.addColorStop(0, 'rgba(26, 26, 46, 0.8)');
        gradient.addColorStop(1, 'rgba(22, 33, 62, 0.8)');
        ctx.fillStyle = gradient;
        ctx.fillRect(0, 0, width, height);
        
        // TODO: Draw active notes
        // This will be implemented when MIDI events are properly connected
    }

    visualizeNote(data) {
        // TODO: Add visual feedback for note on events
        // This will be implemented when MIDI events are properly connected
    }

    clearNote(data) {
        // TODO: Clear visual feedback for note off events
        // This will be implemented when MIDI events are properly connected
    }

    // ========================================================================
    // CHARGEMENT DES DONNÉES
    // ========================================================================

    loadFiles() {
        if (this.eventBus) {
            this.eventBus.emit('home:request_files');
        }
    }

    loadPlaylists() {
        if (this.eventBus) {
            this.eventBus.emit('home:request_playlists');
        }
    }

    loadInstruments() {
        if (this.eventBus) {
            this.eventBus.emit('home:request_instruments');
        }
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    formatDuration(seconds) {
        const mins = Math.floor(seconds / 60);
        const secs = Math.floor(seconds % 60);
        return `${mins}:${secs.toString().padStart(2, '0')}`;
    }

    getInstrumentIcon(type) {
        const icons = {
            usb: '🔌',
            bluetooth: '📡',
            network: '🌐',
            virtual: '💻'
        };
        return icons[type] || '🎸';
    }

    // ========================================================================
    // CLEANUP
    // ========================================================================

    destroy() {
        // Stop visualizer animation
        if (this.visualizerAnimationId) {
            cancelAnimationFrame(this.visualizerAnimationId);
            this.visualizerAnimationId = null;
        }
        
        // Remove event listeners
        if (this.eventBus) {
            this.eventBus.off('files:loaded');
            this.eventBus.off('file:selected');
            this.eventBus.off('playlists:loaded');
            this.eventBus.off('playlist:selected');
            this.eventBus.off('instruments:updated');
            this.eventBus.off('midi:note_on');
            this.eventBus.off('midi:note_off');
        }
        
        this.logger.info('[HomeView] Destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = HomeView;
}

if (typeof window !== 'undefined') {
    window.HomeView = HomeView;
}