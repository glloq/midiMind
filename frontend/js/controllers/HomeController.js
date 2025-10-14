// ============================================================================
// Fichier: frontend/js/controllers/HomeController.js
// Version: v3.0.1 - CORRECTED (syntax errors fixed, all features integrated)
// Date: 2025-10-10
// Projet: midiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// CORRECTIONS v3.0.1:
// ✓ Fixed missing closing braces
// ✓ Integrated playlist patch methods into main class
// ✓ Fixed line 613 syntax error (orphan closing brace removed)
// ✓ Proper class structure with all methods
// ✓ All playlist management features included
// ============================================================================

/**
 * HomeController - Contrôleur de la page d'accueil
 * Gestion du player, routing et playlists
 */
class HomeController {
    constructor(eventBus) {
        this.eventBus = eventBus;
        this.view = null;
        this.playbackController = null;
        this.routingModel = null;
        this.fileModel = null;
        this.playlistModel = null;
        
        this.currentFile = null;
        this.playbackState = 'stopped'; // stopped, playing, paused
        this.playbackTimer = null;
        this.currentTime = 0;
        this.isPlaying = false;
    }

    /**
     * Initialise le contrôleur
     */
    async initialize() {
        console.log('HomeController: Initializing...');
        
        // Récupérer les dépendances
        this.playbackController = window.playbackController;
        this.routingModel = window.routingModel;
        this.fileModel = window.fileModel;
        this.playlistModel = window.playlistModel;

        // Créer la vue
        const container = document.getElementById('home');
        this.view = new HomeView(container);
        this.view.init();

        // Charger les données initiales
        await this.loadInitialData();

        // Attacher les événements
        this.attachEvents();
        
        // Initialiser les listeners de playlist
        this.initPlaylistEventListeners();
        
        console.log('✓ HomeController initialized');
    }

    /**
     * Charge les données initiales
     */
    async loadInitialData() {
        try {
            // Charger les fichiers
            const files = await this.fileModel.loadAll();
            this.view.updateFileList(files);

            // Charger les instruments
            const instruments = await instrumentModel.loadAll();

            // Charger les playlists
            await this.playlistModel.loadAll();

            // Charger les presets de routing
            await this.routingModel.loadAllPresets();

        } catch (error) {
            console.error('Error loading initial data:', error);
            this.showError('Failed to load initial data');
        }
    }

    /**
     * Attache les événements
     */
    attachEvents() {
		
		this.eventBus.on('file:list:updated', (data) => {
			console.log('✅ HomeController: file:list:updated received', data);
			this.view.updateFileList(data.files);
			this.view.updateStats({
			totalFiles: data.count,
			totalDuration: calculateTotalDuration(data.files),
			totalSize: calculateTotalSize(data.files)
			 });
		});
        // Événements du playback
        this.eventBus.on('playback:started', () => {
            this.playbackState = 'playing';
            this.isPlaying = true;
            this.view.updatePlaybackState('playing');
            this.startProgressTimer();
        });

        this.eventBus.on('playback:paused', () => {
            this.playbackState = 'paused';
            this.isPlaying = false;
            this.view.updatePlaybackState('paused');
            this.stopProgressTimer();
        });

        this.eventBus.on('playback:stopped', () => {
            this.playbackState = 'stopped';
            this.isPlaying = false;
            this.view.updatePlaybackState('stopped');
            this.stopProgressTimer();
            this.currentTime = 0;
            this.view.updateProgress(0, this.currentFile?.duration || 0);
        });

        this.eventBus.on('playback:ended', () => {
            this.onPlaybackEnded();
        });

        // Événements du routing
        this.eventBus.on('routing:changed', () => {
            this.updateRoutingDisplay();
        });

        // Événements de la playlist
        this.eventBus.on('playlist:changed', (data) => {
            this.loadFile(data.file.fileId);
        });
    }

    /**
     * Initialise l'écoute des événements playlist
     */
    initPlaylistEventListeners() {
        if (!this.eventBus) return;
        
        console.log('HomeController: Initializing playlist event listeners...');
        
        // Écouter chargement de playlist
        this.eventBus.on('playlist:loaded', (data) => {
            console.log('Playlist loaded event:', data.playlist?.name);
            
            // Mettre à jour l'UI si nécessaire
            if (this.view && typeof this.view.updatePlaylistInfo === 'function') {
                this.view.updatePlaylistInfo(data.playlist);
            }
        });
        
        // Écouter changement de fichier dans playlist
        this.eventBus.on('playlist:next', (data) => {
            console.log('Playlist next:', data.file?.name);
            
            // Auto-charger le fichier suivant si en lecture
            if (this.isPlaying && data.file) {
                const fileId = typeof data.file === 'string' ? data.file : data.file.id;
                this.loadFile(fileId);
            }
        });
        
        // Écouter fin de lecture pour auto-advance
        this.eventBus.on('playback:finished', () => {
            console.log('Playback finished - checking auto-advance...');
            // Le PlaylistModel gère déjà l'auto-advance
        });
    }

    // ========================================================================
    // GESTION DES FICHIERS
    // ========================================================================

    /**
     * Charge un fichier MIDI
     */
    async loadFile(fileId) {
        try {
            const file = await this.fileModel.get(fileId);

            if (!file) {
                throw new Error('File not found');
            }

            // Convertir en MidiJSON si nécessaire
            if (!file.midiJson) {
                const converter = new MidiJsonConverter();
                file.midiJson = await converter.midiToJson(file.data);
                
                // Sauvegarder la version JSON
                await this.fileModel.update(fileId, { midiJson: file.midiJson });
            }

            this.currentFile = file;
            
            // Mettre à jour la vue
            this.view.updateCurrentFile(file);

            // Configurer le routing model avec le fichier courant
            this.routingModel.setCurrentFile(file);

            // Mettre à jour les canaux
            const channels = file.midiJson.channels || [];
            const instruments = instrumentModel.getAll();
            
            this.view.updateRoutingGrid(channels, instruments);
            this.view.updateChannelToggles(channels);

            // Charger dans le playback controller
            await this.playbackController.loadMidiJson(file.midiJson);

            this.eventBus.emit('file:loaded', { file });

        } catch (error) {
            console.error('Error loading file:', error);
            this.showError(`Failed to load file: ${error.message}`);
        }
    }

    /**
     * Rafraîchit la liste des fichiers
     */
    async refreshFiles() {
        try {
            const files = await this.fileModel.loadAll();
            this.view.updateFileList(files);
            this.showSuccess('Files refreshed');
        } catch (error) {
            console.error('Error refreshing files:', error);
            this.showError('Failed to refresh files');
        }
    }

    /**
     * Upload un fichier
     */
    uploadFile() {
        // Créer un input file temporaire
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = '.mid,.midi';
        
        input.onchange = async (e) => {
            const file = e.target.files[0];
            
            if (!file) return;

            try {
                const arrayBuffer = await file.arrayBuffer();
                
                // Convertir en MidiJSON
                const converter = new MidiJsonConverter();
                const midiJson = await converter.midiToJson(arrayBuffer);

                // Sauvegarder
                const savedFile = await this.fileModel.create({
                    name: file.name,
                    data: arrayBuffer,
                    midiJson: midiJson,
                    size: file.size,
                    duration: midiJson.metadata.duration
                });

                this.showSuccess(`File "${file.name}" uploaded`);

                // Rafraîchir la liste
                await this.refreshFiles();

                // Charger le fichier
                await this.loadFile(savedFile.id);

            } catch (error) {
                console.error('Error uploading file:', error);
                this.showError(`Upload failed: ${error.message}`);
            }
        };

        input.click();
    }

    /**
     * Sélectionne le premier fichier
     */
    async selectFirstFile() {
        const files = await this.fileModel.getAll();
        
        if (files.length > 0) {
            await this.loadFile(files[0].id);
        }
    }

    // ========================================================================
    // CONTRÔLES DE LECTURE
    // ========================================================================

    /**
     * Lance la lecture
     */
    async play() {
        if (!this.currentFile) {
            this.showError('No file loaded');
            return;
        }

        try {
            if (this.playbackState === 'paused') {
                await this.playbackController.resume();
            } else {
                await this.playbackController.play();
            }
        } catch (error) {
            console.error('Error playing:', error);
            this.showError(`Playback error: ${error.message}`);
        }
    }

    /**
     * Met en pause
     */
    async pause() {
        try {
            await this.playbackController.pause();
        } catch (error) {
            console.error('Error pausing:', error);
        }
    }

    /**
     * Arrête la lecture
     */
    async stop() {
        try {
            await this.playbackController.stop();
        } catch (error) {
            console.error('Error stopping:', error);
        }
    }

    /**
     * Fichier suivant
     */
    async next() {
        const nextFile = this.playlistModel.next();
        
        if (nextFile) {
            await this.loadFile(nextFile.fileId);
            
            if (this.playbackState === 'playing') {
                await this.play();
            }
        }
    }

    /**
     * Fichier précédent
     */
    async previous() {
        const prevFile = this.playlistModel.previous();
        
        if (prevFile) {
            await this.loadFile(prevFile.fileId);
            
            if (this.playbackState === 'playing') {
                await this.play();
            }
        }
    }

    /**
     * Recherche dans la timeline
     */
    async seek(percent) {
        if (!this.currentFile) return;

        const time = this.currentFile.duration * percent;
        
        try {
            await this.playbackController.seek(time);
            this.currentTime = time;
            this.view.updateProgress(time, this.currentFile.duration);
        } catch (error) {
            console.error('Error seeking:', error);
        }
    }

    /**
     * Définit le tempo
     */
    async setTempo(percent) {
        try {
            await this.playbackController.setTempo(percent);
        } catch (error) {
            console.error('Error setting tempo:', error);
        }
    }

    /**
     * Gère la fin de lecture
     */
    async onPlaybackEnded() {
        this.stopProgressTimer();
        this.playbackState = 'stopped';
        this.isPlaying = false;
        this.view.updatePlaybackState('stopped');

        // Passer au suivant si playlist active
        if (this.playlistModel.currentPlaylist) {
            const next = this.playlistModel.next();
            
            if (next) {
                await this.loadFile(next.fileId);
                await this.play();
            }
        }
    }

    // ========================================================================
    // GESTION DU ROUTING
    // ========================================================================

    /**
     * Assigne un instrument à un canal
     */
    async assignInstrument(channel, instrumentId) {
        try {
            if (!instrumentId) {
                this.routingModel.removeRouting(channel);
            } else {
                await this.routingModel.assignInstrument(channel, instrumentId);
            }

            // Mettre à jour l'affichage
            this.updateRoutingDisplay();

            // Appliquer au playback controller
            const routing = this.routingModel.getRouting(channel);
            if (routing) {
                await this.playbackController.updateRouting(channel, routing);
            }

        } catch (error) {
            console.error('Error assigning instrument:', error);
            this.showError(`Failed to assign instrument: ${error.message}`);
        }
    }

    /**
     * Auto-routing intelligent
     */
    async autoRoute() {
        if (!this.currentFile) {
            this.showError('No file loaded');
            return;
        }

        try {
            const assignments = await this.routingModel.autoRoute();

            // Appliquer tous les routings
            for (const assignment of assignments) {
                await this.playbackController.updateRouting(
                    assignment.channel,
                    this.routingModel.getRouting(assignment.channel)
                );
            }

            this.updateRoutingDisplay();

            this.showSuccess(`Auto-routed ${assignments.length} channels`);

        } catch (error) {
            console.error('Error auto-routing:', error);
            this.showError(`Auto-routing failed: ${error.message}`);
        }
    }

    /**
     * Efface tous les routings
     */
    clearRouting() {
        this.routingModel.clearAll();
        this.updateRoutingDisplay();
    }

    /**
     * Sauvegarde un preset de routing
     */
    async saveRoutingPreset() {
        const name = prompt('Enter preset name:');
        
        if (!name) return;

        try {
            const preset = await this.routingModel.savePreset(name);
            this.showSuccess(`Preset "${name}" saved`);

            // Recharger les presets
            await this.loadRoutingPresets();

        } catch (error) {
            console.error('Error saving preset:', error);
            this.showError(`Failed to save preset: ${error.message}`);
        }
    }

    /**
     * Charge un preset de routing
     */
    async loadRoutingPreset(presetId) {
        try {
            await this.routingModel.loadPreset(presetId);

            // Appliquer tous les routings
            const routings = this.routingModel.getAllRoutings();
            
            for (const routing of routings) {
                await this.playbackController.updateRouting(
                    routing.channel,
                    routing
                );
            }

            this.updateRoutingDisplay();
            this.showSuccess('Preset loaded');

        } catch (error) {
            console.error('Error loading preset:', error);
            this.showError(`Failed to load preset: ${error.message}`);
        }
    }

    /**
     * Charge les presets de routing
     */
    async loadRoutingPresets() {
        const presets = this.routingModel.presets;
        const select = document.getElementById('routingPresetSelect');

        if (!select) return;

        select.innerHTML = '<option value="">-- Select preset --</option>';

        Array.from(presets.values()).forEach(preset => {
            const option = document.createElement('option');
            option.value = preset.id;
            option.textContent = preset.name;
            select.appendChild(option);
        });
    }

    /**
     * Met à jour l'affichage du routing
     */
    updateRoutingDisplay() {
        if (!this.currentFile) return;

        const channels = this.currentFile.midiJson.channels || [];
        const instruments = instrumentModel.getAll();

        this.view.updateRoutingGrid(channels, instruments);

        // Mettre à jour les statistiques
        const stats = this.routingModel.getGlobalCompatibility();
        this.view.updateRoutingStats(stats);
    }

    // ========================================================================
    // GESTION DES PLAYLISTS
    // ========================================================================

    /**
     * Gère la playlist - Ouvre l'éditeur de playlist
     */
    managePlaylist() {
        console.log('HomeController: Opening playlist manager...');
        
        // Vérifier que ModalController est disponible
        if (!window.app?.modalController) {
            console.error('ModalController not available');
            this.showError('Modal system not initialized');
            return;
        }
        
        // Vérifier que PlaylistController est disponible
        if (!window.app?.playlistController) {
            console.error('PlaylistController not available');
            this.showError('Playlist system not initialized');
            return;
        }
        
        // Déterminer le mode: édition ou création
        const playlistController = window.app.playlistController;
        const currentPlaylist = playlistController.state?.currentPlaylist;
        
        if (currentPlaylist) {
            // Mode édition - playlist courante existe
            this.editCurrentPlaylist();
        } else {
            // Mode création - nouvelle playlist
            this.createNewPlaylist();
        }
    }

    /**
     * Ouvre l'éditeur pour créer une nouvelle playlist
     */
    createNewPlaylist() {
        console.log('HomeController: Creating new playlist...');
        
        const modalController = window.app.modalController;
        
        if (!modalController) {
            this.showError('Modal system not available');
            return;
        }
        
        // Ouvrir l'éditeur en mode création (sans playlistId)
        modalController.openPlaylistEditor(null);
    }

    /**
     * Édite la playlist courante
     */
    editCurrentPlaylist() {
        console.log('HomeController: Editing current playlist...');
        
        const modalController = window.app.modalController;
        const playlistController = window.app.playlistController;
        
        if (!modalController || !playlistController) {
            this.showError('Controllers not available');
            return;
        }
        
        const currentPlaylist = playlistController.state?.currentPlaylist;
        
        if (!currentPlaylist) {
            this.showError('No playlist selected');
            return;
        }
        
        // Ouvrir l'éditeur avec l'ID de la playlist courante
        modalController.openPlaylistEditor(currentPlaylist.id);
    }

    /**
     * Édite une playlist spécifique par son ID
     * @param {string} playlistId - ID de la playlist
     */
    editPlaylist(playlistId) {
        console.log(`HomeController: Editing playlist ${playlistId}...`);
        
        const modalController = window.app.modalController;
        
        if (!modalController) {
            this.showError('Modal system not available');
            return;
        }
        
        if (!playlistId) {
            this.showError('Invalid playlist ID');
            return;
        }
        
        modalController.openPlaylistEditor(playlistId);
    }

    /**
     * Supprime une playlist avec confirmation
     * @param {string} playlistId - ID de la playlist à supprimer
     */
    deletePlaylist(playlistId) {
        console.log(`HomeController: Deleting playlist ${playlistId}...`);
        
        const playlistController = window.app.playlistController;
        
        if (!playlistController) {
            this.showError('PlaylistController not available');
            return;
        }
        
        if (!playlistId) {
            this.showError('Invalid playlist ID');
            return;
        }
        
        // Récupérer les infos de la playlist pour le message de confirmation
        const playlistModel = playlistController.playlistModel;
        const playlist = playlistModel?.getPlaylist?.(playlistId);
        
        if (!playlist) {
            this.showError('Playlist not found');
            return;
        }
        
        // Demander confirmation
        const confirmed = confirm(
            `Voulez-vous vraiment supprimer la playlist "${playlist.name}" ?\n\n` +
            `Cette action est irréversible.`
        );
        
        if (!confirmed) {
            console.log('Delete cancelled by user');
            return;
        }
        
        // Supprimer via le controller
        playlistController.deletePlaylist(playlistId)
            .then(success => {
                if (success) {
                    this.showSuccess(`Playlist "${playlist.name}" supprimée`);
                } else {
                    this.showError('Échec de la suppression');
                }
            })
            .catch(error => {
                console.error('Error deleting playlist:', error);
                this.showError(`Erreur: ${error.message}`);
            });
    }

    /**
     * Charge une playlist et commence la lecture
     * @param {string} playlistId - ID de la playlist
     */
    loadPlaylistIntoPlayer(playlistId) {
        console.log(`HomeController: Loading playlist ${playlistId} into player...`);
        
        const playlistController = window.app.playlistController;
        
        if (!playlistController) {
            this.showError('PlaylistController not available');
            return;
        }
        
        if (!playlistId) {
            this.showError('Invalid playlist ID');
            return;
        }
        
        // Charger la playlist
        playlistController.loadPlaylist(playlistId)
            .then(playlist => {
                if (!playlist) {
                    this.showError('Failed to load playlist');
                    return;
                }
                
                console.log(`Playlist loaded: ${playlist.name}`);
                
                // Si la playlist a des fichiers, charger le premier
                if (playlist.files && playlist.files.length > 0) {
                    const firstFile = playlist.files[0];
                    const firstFileId = typeof firstFile === 'string' ? firstFile : firstFile.id;
                    
                    // Charger le fichier dans le player
                    this.loadFile(firstFileId)
                        .then(() => {
                            console.log('First file loaded, ready to play');
                            this.showSuccess(`Playlist "${playlist.name}" chargée`);
                        })
                        .catch(error => {
                            console.error('Error loading first file:', error);
                            this.showError('Erreur lors du chargement du fichier');
                        });
                } else {
                    this.showInfo('Playlist vide');
                }
            })
            .catch(error => {
                console.error('Error loading playlist:', error);
                this.showError(`Erreur: ${error.message}`);
            });
    }

    /**
     * Ajoute rapidement un fichier à la playlist courante
     * @param {string} fileId - ID du fichier
     */
    quickAddToPlaylist(fileId) {
        console.log(`HomeController: Quick adding file ${fileId} to current playlist...`);
        
        const playlistController = window.app.playlistController;
        
        if (!playlistController) {
            this.showError('PlaylistController not available');
            return;
        }
        
        const currentPlaylist = playlistController.state?.currentPlaylist;
        
        if (!currentPlaylist) {
            // Pas de playlist courante - proposer d'en créer une
            const createNew = confirm(
                'Aucune playlist active.\n\n' +
                'Voulez-vous créer une nouvelle playlist ?'
            );
            
            if (createNew) {
                this.createNewPlaylistWithFile(fileId);
            }
            return;
        }
        
        // Ajouter à la playlist courante
        playlistController.addFileToPlaylist(currentPlaylist.id, fileId)
            .then(success => {
                if (success) {
                    const file = this.fileModel?.getFileById?.(fileId);
                    const fileName = file?.name || file?.filename || 'fichier';
                    this.showSuccess(`"${fileName}" ajouté à "${currentPlaylist.name}"`);
                }
            })
            .catch(error => {
                console.error('Error adding file to playlist:', error);
                this.showError('Erreur lors de l\'ajout');
            });
    }

    /**
     * Crée une nouvelle playlist avec un fichier initial
     * @param {string} fileId - ID du fichier
     */
    createNewPlaylistWithFile(fileId) {
        console.log(`HomeController: Creating new playlist with file ${fileId}...`);
        
        const playlistController = window.app.playlistController;
        
        if (!playlistController) {
            this.showError('PlaylistController not available');
            return;
        }
        
        // Demander le nom de la playlist
        const playlistName = prompt('Nom de la nouvelle playlist:', 'Ma Playlist');
        
        if (!playlistName || !playlistName.trim()) {
            console.log('Playlist creation cancelled');
            return;
        }
        
        // Créer la playlist avec le fichier
        playlistController.createPlaylist(playlistName.trim(), [fileId])
            .then(playlist => {
                if (playlist) {
                    this.showSuccess(`Playlist "${playlist.name}" créée`);
                    
                    // Charger la nouvelle playlist
                    playlistController.loadPlaylist(playlist.id);
                }
            })
            .catch(error => {
                console.error('Error creating playlist:', error);
                this.showError('Erreur lors de la création');
            });
    }

    // ========================================================================
    // VISUALISATION & CANAUX
    // ========================================================================

    /**
     * Active/désactive un canal dans le visualizer
     */
    toggleChannel(channel, enabled) {
        if (this.view.visualizer) {
            this.view.visualizer.toggleChannel(channel, enabled);
        }
    }

    /**
     * Obtient les notes à venir
     */
    getUpcomingNotes(currentTime) {
        if (!this.currentFile || !this.currentFile.midiJson) {
            return [];
        }

        const previewTime = 2000; // 2 secondes
        const endTime = currentTime + previewTime;

        return this.currentFile.midiJson.timeline
            .filter(event => 
                event.type === 'noteOn' &&
                event.time >= currentTime &&
                event.time <= endTime
            )
            .map(event => ({
                ...event,
                timeOffset: event.time - currentTime
            }))
            .slice(0, 10);
    }

    // ========================================================================
    // PROGRESSION & TIMERS
    // ========================================================================

    /**
     * Démarre le timer de progression
     */
    startProgressTimer() {
        this.stopProgressTimer();

        this.playbackTimer = setInterval(() => {
            this.currentTime += 100; // Incrément de 100ms

            if (this.currentFile) {
                this.view.updateProgress(this.currentTime, this.currentFile.duration);

                // Mettre à jour les notes à venir
                const upcomingNotes = this.getUpcomingNotes(this.currentTime);
                this.view.updateNotePreview(upcomingNotes);
            }

            // Arrêter si on dépasse la durée
            if (this.currentFile && this.currentTime >= this.currentFile.duration) {
                this.onPlaybackEnded();
            }
        }, 100);
    }

    /**
     * Arrête le timer de progression
     */
    stopProgressTimer() {
        if (this.playbackTimer) {
            clearInterval(this.playbackTimer);
            this.playbackTimer = null;
        }
    }

    // ========================================================================
    // NAVIGATION
    // ========================================================================

    /**
     * Ouvre l'éditeur MIDI
     */
    openEditor() {
        if (!this.currentFile) {
            this.showError('No file loaded');
            return;
        }

        // Sauvegarder l'état actuel
        this.pause();

        // Naviguer vers l'éditeur
        window.location.hash = '#editor';
        
        // L'EditorController prendra le relais
        if (window.editorController) {
            window.editorController.loadFile(this.currentFile);
        }
    }

    /**
     * Ouvre les réglages
     */
    openSettings() {
        window.location.hash = '#settings';
    }

    // ========================================================================
    // NOTIFICATIONS
    // ========================================================================

    /**
     * Affiche une erreur
     */
    showError(message) {
        console.error(message);
        // TODO: Implémenter un système de toast/notification
        alert(`Error: ${message}`);
    }

    /**
     * Affiche un message de succès
     */
    showSuccess(message) {
        console.log(message);
        // TODO: Implémenter un système de toast/notification
    }

    /**
     * Affiche une information
     */
    showInfo(message) {
        console.log(message);
        // TODO: Implémenter un système de toast/notification
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = HomeController;
}

if (typeof window !== 'undefined') {
    window.HomeController = HomeController;
}