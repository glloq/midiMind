// ============================================================================
// Fichier: frontend/js/views/FileView.js
// Version: v3.10.0 - DOCUMENTATION INTÉGRÉE
// Date: 2025-10-31
// Projet: MidiMind v3.1.0
// ============================================================================
// AMÉLIORATIONS v3.10.0:
// ✅ Section d'aide intégrée avec documentation complète
// ✅ Explications sur l'upload et la chaîne MIDI
// ✅ Description des modes de routing 1→1 et N→N
// ✅ Guide visuel de l'éditeur piano roll
// ============================================================================

class FileView {
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
            console.error('[FileView] Container not found:', container);
        }
        
        this.eventBus = eventBus;
        this.logger = window.logger || console;
        
        // État
        this.state = {
            files: [],
            playlists: [],
            selectedFile: null,
            selectedPlaylist: null,
            showHelp: false
        };
        
        // Éléments DOM
        this.elements = {};
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    init() {
        if (!this.container) {
            this.logger.error('[FileView] Cannot initialize: container not found');
            return;
        }
        
        this.render();
        this.cacheElements();
        this.attachEvents();
        this.loadData();
        
        this.logger.info('[FileView] Initialized v3.10.0');
    }

    render() {
        if (!this.container) return;
        
        this.container.innerHTML = `
            <div class="page-header">
                <h1>📁 Gestion des Fichiers MIDI</h1>
                <button class="btn-help" id="btnToggleHelp" title="Afficher l'aide">
                    ❓ Aide
                </button>
            </div>
            
            <!-- Section d'aide (cachée par défaut) -->
            <div class="help-section" id="helpSection" style="display: none;">
                ${this.renderHelpContent()}
            </div>
            
            <div class="files-layout">
                <!-- Section Fichiers -->
                <div class="files-section">
                    <div class="section-header">
                        <h2>📄 Fichiers MIDI JSON</h2>
                        <div class="section-actions">
                            <button class="btn-action btn-primary" id="btnUploadFile" title="Uploader un fichier MIDI">
                                ⬆️ Uploader
                            </button>
                            <button class="btn-action" id="btnRefreshFiles" title="Actualiser la liste">
                                🔄 Actualiser
                            </button>
                        </div>
                    </div>
                    
                    <div class="files-grid" id="filesGrid">
                        ${this.renderEmptyFiles()}
                    </div>
                </div>
                
                <!-- Section Playlists -->
                <div class="playlists-section">
                    <div class="section-header">
                        <h2>📋 Playlists</h2>
                        <button class="btn-action btn-create" id="btnCreatePlaylist">
                            ➕ Nouvelle Playlist
                        </button>
                    </div>
                    
                    <div class="playlists-grid" id="playlistsGrid">
                        ${this.renderEmptyPlaylists()}
                    </div>
                </div>
            </div>
        `;
    }

    renderHelpContent() {
        return `
            <div class="help-content">
                <button class="help-close" id="btnCloseHelp">✖</button>
                
                <h2>🎯 Guide d'utilisation</h2>
                
                <!-- Upload de fichiers -->
                <section class="help-card">
                    <h3>📤 Upload et Traitement MIDI</h3>
                    <div class="help-text">
                        <p><strong>Comment uploader un fichier :</strong></p>
                        <ol>
                            <li>Cliquez sur le bouton <strong>"⬆️ Uploader"</strong></li>
                            <li>Sélectionnez un fichier <code>.mid</code> ou <code>.midi</code></li>
                            <li>Le fichier sera automatiquement converti en MidiJSON</li>
                            <li>Il apparaîtra dans la liste après traitement</li>
                        </ol>
                        
                        <p><strong>Chaîne de traitement :</strong></p>
                        <div class="code-block">
Fichier MIDI (.mid)
    ↓
[Upload] → FileController.uploadFile()
    ↓
[Conversion] → MidiJsonConverter.midiToJson()
    ↓
[Backend] → Stockage + Validation
    ↓
[Affichage] → Liste des fichiers
                        </div>
                        
                        <p><strong>Format MidiJSON :</strong></p>
                        <ul>
                            <li><strong>Timeline</strong> : Tous les événements MIDI en ordre chronologique</li>
                            <li><strong>Tracks</strong> : Organisation par pistes musicales</li>
                            <li><strong>Metadata</strong> : Titre, durée, nombre de notes, BPM</li>
                        </ul>
                    </div>
                </section>

                <!-- Éditeur Piano Roll -->
                <section class="help-card">
                    <h3>🎹 Éditeur Piano Roll</h3>
                    <div class="help-text">
                        <p><strong>Cliquez sur "✏️ Éditer"</strong> pour ouvrir l'éditeur graphique.</p>
                        
                        <p><strong>Affichage des notes :</strong></p>
                        <ul>
                            <li><strong>Notes rectangulaires</strong> : Chaque rectangle = une note MIDI</li>
                            <li><strong>Axe horizontal</strong> : Temps (mesures et temps)</li>
                            <li><strong>Axe vertical</strong> : Hauteur de note (clavier piano)</li>
                            <li><strong>Couleur</strong> : Indique le canal MIDI ou la vélocité</li>
                            <li><strong>Largeur</strong> : Durée de la note</li>
                        </ul>
                        
                        <p><strong>Navigation :</strong></p>
                        <ul>
                            <li><strong>Zoom</strong> : Molette souris ou Z/X</li>
                            <li><strong>Défilement</strong> : Clic droit + glisser ou barres de scroll</li>
                            <li><strong>Playhead</strong> : Ligne verticale indiquant la position de lecture</li>
                        </ul>
                        
                        <p><strong>Outils d'édition :</strong></p>
                        <ul>
                            <li><strong>Select (1)</strong> : Sélection multiple (rectangle ou Shift+clic)</li>
                            <li><strong>Pencil (2)</strong> : Ajouter des notes (clic sur la grille)</li>
                            <li><strong>Eraser (3)</strong> : Supprimer des notes (clic sur note)</li>
                            <li><strong>Resize</strong> : Modifier la durée (poignées à droite)</li>
                            <li><strong>Move</strong> : Déplacer les notes (drag & drop)</li>
                        </ul>
                        
                        <p><strong>Raccourcis clavier :</strong></p>
                        <div class="shortcuts-grid">
                            <div class="shortcut"><kbd>Espace</kbd> = Play/Pause</div>
                            <div class="shortcut"><kbd>Ctrl+S</kbd> = Sauvegarder</div>
                            <div class="shortcut"><kbd>Ctrl+Z</kbd> = Undo</div>
                            <div class="shortcut"><kbd>Ctrl+Y</kbd> = Redo</div>
                            <div class="shortcut"><kbd>Delete</kbd> = Supprimer sélection</div>
                            <div class="shortcut"><kbd>Ctrl+A</kbd> = Tout sélectionner</div>
                            <div class="shortcut"><kbd>Ctrl+C/V</kbd> = Copier/Coller</div>
                            <div class="shortcut"><kbd>Z/X</kbd> = Zoom H</div>
                        </div>
                    </div>
                </section>

                <!-- Routing MIDI -->
                <section class="help-card">
                    <h3>🔀 Système de Routing MIDI</h3>
                    <div class="help-text">
                        <p><strong>Cliquez sur "🔀 Routes"</strong> pour configurer le routage.</p>
                        
                        <h4>Mode Simple (1→1) - Recommandé</h4>
                        <div class="routing-mode">
                            <p><strong>Un canal → Un device</strong></p>
                            <ul>
                                <li>Configuration directe et simple</li>
                                <li>Faible latence optimale</li>
                                <li>Économe en ressources (idéal Raspberry Pi)</li>
                                <li>Facile à débugger</li>
                            </ul>
                            <div class="code-block">
Exemple :
Canal 0 (Piano)  → Device A
Canal 1 (Drums)  → Device B
Canal 2 (Bass)   → Device C
Canal 9 (Percus) → Device D
                            </div>
                            <p><strong>Cas d'usage :</strong></p>
                            <ul>
                                <li>Performance live</li>
                                <li>Setup simple multi-instruments</li>
                                <li>Contraintes matérielles (RAM/CPU)</li>
                            </ul>
                        </div>
                        
                        <h4>Mode Complexe (N→N) - Avancé</h4>
                        <div class="routing-mode">
                            <p><strong>Plusieurs canaux → Plusieurs devices</strong></p>
                            <ul>
                                <li>Routage flexible avec transformations</li>
                                <li>Layering (plusieurs devices pour un canal)</li>
                                <li>Splitting (un canal vers plusieurs)</li>
                                <li>Effets temps réel (transpose, velocity)</li>
                            </ul>
                            <div class="code-block">
Exemple :
Canal 0 → [Device A, Device B]  (split/layer)
Canal 1 → Device C (transpose +12)
[Ch 0, Ch 1] → Device D (merge)
                            </div>
                            <p><strong>Transformations disponibles :</strong></p>
                            <ul>
                                <li><strong>Transpose</strong> : Décalage d'octaves (±12 semitons)</li>
                                <li><strong>Velocity Scale</strong> : Ajustement dynamique (0.1-2.0)</li>
                                <li><strong>Channel Mapping</strong> : Réassignation de canal</li>
                                <li><strong>Note Filter</strong> : Filtrage de gammes</li>
                                <li><strong>CC Transform</strong> : Modification contrôleurs</li>
                            </ul>
                            <p><strong>Cas d'usage :</strong></p>
                            <ul>
                                <li>Production studio</li>
                                <li>Arrangements complexes</li>
                                <li>Orchestration multi-timbrale</li>
                            </ul>
                        </div>
                        
                        <p><strong>⚠️ Note importante :</strong></p>
                        <p>En mode Phase 1 (optimisations performance), seul le mode <strong>1→1</strong> est actif pour économiser la RAM (150MB au lieu de 300MB).</p>
                    </div>
                </section>

                <!-- Format de fichier -->
                <section class="help-card">
                    <h3>📋 Format MidiJSON</h3>
                    <div class="help-text">
                        <p>Les fichiers MIDI sont convertis en <strong>MidiJSON</strong>, un format optimisé pour l'édition :</p>
                        <div class="code-block">
{
  "format": 1,
  "ppq": 480,          // Pulses Per Quarter
  "bpm": 120,          // Tempo
  
  "tracks": [
    {
      "name": "Piano",
      "channel": 0,
      "events": [...]
    }
  ],
  
  "timeline": [       // Tous les événements triés par temps
    {
      "id": "evt_001",
      "type": "noteOn",
      "time": 0,       // millisecondes
      "channel": 0,
      "note": 60,      // C4 (Do central)
      "velocity": 100,
      "duration": 480  // millisecondes
    }
  ],
  
  "metadata": {
    "title": "Ma Composition",
    "duration": 120000,  // ms
    "totalNotes": 150
  }
}
                        </div>
                        
                        <p><strong>Avantages :</strong></p>
                        <ul>
                            <li>✅ Plus rapide à parser que MIDI binaire</li>
                            <li>✅ Facilite l'édition en temps réel</li>
                            <li>✅ Timeline pré-calculée pour le rendering</li>
                            <li>✅ Stockage SQLite + fichiers JSON</li>
                        </ul>
                    </div>
                </section>

                <!-- Performance -->
                <section class="help-card">
                    <h3>⚡ Optimisations Performance</h3>
                    <div class="help-text">
                        <p>MidiMind v3.1 inclut des optimisations pour Raspberry Pi 4 :</p>
                        
                        <p><strong>Phase 1 (Configuration) - Actuelle :</strong></p>
                        <ul>
                            <li>Routing <strong>1→1 uniquement</strong> (économise CPU)</li>
                            <li>Max 5000 notes affichées (culling viewport)</li>
                            <li>Rendering simplifié (pas d'ombres/gradients)</li>
                            <li>Target RAM : <strong>150 MB</strong> (réduit de 50%)</li>
                        </ul>
                        
                        <p><strong>Phase 2 (Simplifications) - Optionnel :</strong></p>
                        <ul>
                            <li>Désactiver recording (économise 50MB)</li>
                            <li>Désactiver loops complexes</li>
                            <li>Batching des rendus par canal</li>
                            <li>Event throttling (limiter événements)</li>
                        </ul>
                        
                        <p><strong>Conseils :</strong></p>
                        <ul>
                            <li>Privilégiez les fichiers < 500 KB</li>
                            <li>Limitez à 16 pistes maximum</li>
                            <li>Utilisez le mode 1→1 pour la performance</li>
                            <li>Fermez les onglets navigateur inutilisés</li>
                        </ul>
                    </div>
                </section>

                <!-- Dépannage -->
                <section class="help-card">
                    <h3>🔧 Dépannage</h3>
                    <div class="help-text">
                        <p><strong>❌ Le WebSocket ne se connecte pas :</strong></p>
                        <ul>
                            <li>Vérifier que le backend est démarré : <code>sudo systemctl status midimind-backend</code></li>
                            <li>Vérifier le port 8080 : <code>sudo netstat -tulpn | grep 8080</code></li>
                            <li>Voir les logs : <code>journalctl -u midimind-backend -f</code></li>
                        </ul>
                        
                        <p><strong>❌ L'upload échoue :</strong></p>
                        <ul>
                            <li>Vérifier la taille du fichier (max 10 MB)</li>
                            <li>Vérifier l'extension (.mid ou .midi uniquement)</li>
                            <li>Ouvrir la console browser (F12) pour voir les erreurs</li>
                            <li>Vérifier les permissions : <code>ls -la /data/midi/</code></li>
                        </ul>
                        
                        <p><strong>❌ Les notes ne s'affichent pas :</strong></p>
                        <ul>
                            <li>Vérifier que le fichier est bien chargé (console F12)</li>
                            <li>Vérifier <code>EditorModel.data.timeline</code> dans la console</li>
                            <li>Ajuster le zoom/scroll pour voir les notes</li>
                            <li>Forcer un re-render : <code>editor.visualizer.render()</code></li>
                        </ul>
                        
                        <p><strong>❌ Problème de RAM/CPU :</strong></p>
                        <ul>
                            <li>Activer les optimisations Phase 2 dans AppConfig.js</li>
                            <li>Réduire le nombre de notes max affichées</li>
                            <li>Désactiver les effets visuels complexes</li>
                            <li>Redémarrer le service backend</li>
                        </ul>
                    </div>
                </section>

                <!-- Commandes Backend -->
                <section class="help-card">
                    <h3>🔌 Commandes Backend WebSocket</h3>
                    <div class="help-text">
                        <p>Le backend expose <strong>66 commandes</strong> via WebSocket (port 8080) :</p>
                        
                        <p><strong>Fichiers (8 commandes) :</strong></p>
                        <ul>
                            <li><code>files.list</code> - Liste des fichiers</li>
                            <li><code>files.load</code> - Charger un fichier</li>
                            <li><code>files.upload</code> - Upload nouveau fichier</li>
                            <li><code>files.save</code> - Sauvegarder modifications</li>
                            <li><code>files.delete</code>, <code>files.rename</code>, <code>files.export</code>, <code>files.read</code></li>
                        </ul>
                        
                        <p><strong>Playback (10 commandes) :</strong></p>
                        <ul>
                            <li><code>playback.play</code>, <code>playback.pause</code>, <code>playback.stop</code></li>
                            <li><code>playback.seek</code>, <code>playback.set_tempo</code>, <code>playback.set_loop</code></li>
                            <li><code>playback.mute_channel</code>, <code>playback.solo_channel</code></li>
                        </ul>
                        
                        <p><strong>Routing (12 commandes) :</strong></p>
                        <ul>
                            <li><code>routing.get_matrix</code> - Récupérer la matrice</li>
                            <li><code>routing.assign</code> - Assigner canal → device</li>
                            <li><code>routing.unassign</code>, <code>routing.clear</code></li>
                            <li><code>routing.save_preset</code>, <code>routing.load_preset</code></li>
                            <li><code>routing.test_route</code> - Tester une route</li>
                        </ul>
                        
                        <p><strong>Editor (8 commandes) :</strong></p>
                        <ul>
                            <li><code>editor.add_note</code>, <code>editor.delete_note</code></li>
                            <li><code>editor.move_note</code>, <code>editor.resize_note</code></li>
                            <li><code>editor.transpose</code>, <code>editor.quantize</code></li>
                            <li><code>editor.set_velocity</code>, <code>editor.undo</code>, <code>editor.redo</code></li>
                        </ul>
                    </div>
                </section>

                <!-- Raccourcis -->
                <section class="help-card">
                    <h3>⌨️ Raccourcis Clavier Globaux</h3>
                    <div class="help-text">
                        <div class="shortcuts-grid">
                            <div class="shortcut"><kbd>F1</kbd> = Afficher cette aide</div>
                            <div class="shortcut"><kbd>Ctrl+H</kbd> = Page Home</div>
                            <div class="shortcut"><kbd>Ctrl+E</kbd> = Page Editor</div>
                            <div class="shortcut"><kbd>Ctrl+R</kbd> = Page Routing</div>
                            <div class="shortcut"><kbd>Ctrl+F</kbd> = Page Files</div>
                            <div class="shortcut"><kbd>Espace</kbd> = Play/Pause (global)</div>
                            <div class="shortcut"><kbd>Esc</kbd> = Fermer modal/aide</div>
                            <div class="shortcut"><kbd>F12</kbd> = Console développeur</div>
                        </div>
                    </div>
                </section>
            </div>
        `;
    }

    cacheElements() {
        this.elements = {
            filesGrid: document.getElementById('filesGrid'),
            playlistsGrid: document.getElementById('playlistsGrid'),
            btnUploadFile: document.getElementById('btnUploadFile'),
            btnRefreshFiles: document.getElementById('btnRefreshFiles'),
            btnCreatePlaylist: document.getElementById('btnCreatePlaylist'),
            btnToggleHelp: document.getElementById('btnToggleHelp'),
            helpSection: document.getElementById('helpSection'),
            btnCloseHelp: document.getElementById('btnCloseHelp')
        };
    }

    attachEvents() {
        // Boutons
        if (this.elements.btnUploadFile) {
            this.elements.btnUploadFile.addEventListener('click', () => this.uploadFile());
        }
        if (this.elements.btnRefreshFiles) {
            this.elements.btnRefreshFiles.addEventListener('click', () => this.refreshFiles());
        }
        if (this.elements.btnCreatePlaylist) {
            this.elements.btnCreatePlaylist.addEventListener('click', () => this.createPlaylist());
        }
        
        // Aide
        if (this.elements.btnToggleHelp) {
            this.elements.btnToggleHelp.addEventListener('click', () => this.toggleHelp());
        }
        if (this.elements.btnCloseHelp) {
            this.elements.btnCloseHelp.addEventListener('click', () => this.toggleHelp());
        }
        
        // Délégation d'événements
        if (this.elements.filesGrid) {
            this.elements.filesGrid.addEventListener('click', (e) => this.handleFileAction(e));
        }
        if (this.elements.playlistsGrid) {
            this.elements.playlistsGrid.addEventListener('click', (e) => this.handlePlaylistAction(e));
        }
        
        // EventBus
        this.setupEventBusListeners();
        
        // Raccourci F1 pour l'aide
        document.addEventListener('keydown', (e) => {
            if (e.key === 'F1') {
                e.preventDefault();
                this.toggleHelp();
            }
        });
    }

    setupEventBusListeners() {
        if (!this.eventBus) return;
        
        this.eventBus.on('files:loaded', (data) => {
            this.state.files = data.files || [];
            this.renderFilesGrid();
        });
        
        this.eventBus.on('playlists:loaded', (data) => {
            this.state.playlists = data.playlists || [];
            this.renderPlaylistsGrid();
        });
        
        this.eventBus.on('file:updated', () => {
            this.refreshFiles();
        });
        
        this.eventBus.on('playlist:updated', () => {
            this.refreshPlaylists();
        });
    }

    // ========================================================================
    // AIDE
    // ========================================================================

    toggleHelp() {
        this.state.showHelp = !this.state.showHelp;
        
        if (this.elements.helpSection) {
            this.elements.helpSection.style.display = this.state.showHelp ? 'block' : 'none';
        }
        
        if (this.elements.btnToggleHelp) {
            this.elements.btnToggleHelp.textContent = this.state.showHelp ? '✖ Fermer l\'aide' : '❓ Aide';
        }
    }

    // ========================================================================
    // RENDU DES FICHIERS
    // ========================================================================

    renderFilesGrid() {
        if (!this.elements.filesGrid) return;
        
        if (!this.state.files || this.state.files.length === 0) {
            this.elements.filesGrid.innerHTML = this.renderEmptyFiles();
            return;
        }
        
        this.elements.filesGrid.innerHTML = this.state.files
            .map(file => this.renderFileCard(file))
            .join('');
    }

    renderFileCard(file) {
        const duration = this.formatDuration(file.duration || 0);
        const tracks = file.tracks || 0;
        const notes = file.noteCount || 0;
        
        return `
            <div class="file-card" data-file-id="${file.id}">
                <div class="file-card-header">
                    <div class="file-card-icon">🎵</div>
                    <div class="file-card-info">
                        <div class="file-card-name">${file.name || 'Sans nom'}</div>
                        <div class="file-card-meta">
                            <span>⏱️ ${duration}</span>
                            <span>🎹 ${tracks} pistes</span>
                            <span>🎼 ${notes} notes</span>
                        </div>
                    </div>
                </div>
                
                <div class="file-card-actions">
                    <button class="file-card-btn btn-edit" data-action="edit" data-file-id="${file.id}" title="Ouvrir dans l'éditeur">
                        <span>✏️</span>
                        <span>Éditer</span>
                    </button>
                    <button class="file-card-btn btn-routes" data-action="routes" data-file-id="${file.id}" title="Configurer le routing MIDI">
                        <span>🔀</span>
                        <span>Routes</span>
                    </button>
                </div>
            </div>
        `;
    }

    renderEmptyFiles() {
        return `
            <div class="empty-state">
                <div class="empty-state-icon">🎵</div>
                <div class="empty-state-text">Aucun fichier MIDI</div>
                <div class="empty-state-hint">Cliquez sur "⬆️ Uploader" pour ajouter des fichiers</div>
            </div>
        `;
    }

    // ========================================================================
    // ACTIONS FICHIERS
    // ========================================================================

    handleFileAction(e) {
        const button = e.target.closest('[data-action]');
        if (!button) return;
        
        const action = button.dataset.action;
        const fileId = button.dataset.fileId;
        const file = this.state.files.find(f => f.id === fileId);
        
        if (!file) return;
        
        switch (action) {
            case 'edit':
                this.editFile(file);
                break;
            case 'routes':
                this.editRoutes(file);
                break;
        }
    }

    uploadFile() {
        this.logger.info('[FileView] Upload file requested');
        
        if (this.eventBus) {
            this.eventBus.emit('file:upload_requested');
        }
        
        // Créer un input file pour sélectionner le fichier
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = '.mid,.midi';
        input.onchange = (e) => {
            const file = e.target.files[0];
            if (file) {
                this.handleFileUpload(file);
            }
        };
        input.click();
    }

    handleFileUpload(file) {
        this.logger.info('[FileView] Uploading file:', file.name);
        
        const reader = new FileReader();
        reader.onload = (e) => {
            try {
                const arrayBuffer = e.target.result;
                
                if (this.eventBus) {
                    this.eventBus.emit('file:upload', {
                        file: file,
                        filename: file.name,
                        data: arrayBuffer,
                        size: file.size
                    });
                }
                
                this.logger.info('[FileView] File uploaded successfully');
            } catch (error) {
                this.logger.error('[FileView] Error uploading file:', error);
            }
        };
        
        reader.readAsArrayBuffer(file);
    }

    editFile(file) {
        this.logger.info('[FileView] Edit file:', file.name);
        
        if (this.eventBus) {
            this.eventBus.emit('file:edit_requested', { file });
            this.eventBus.emit('navigation:page_request', {
                page: 'editor',
                data: { fileId: file.id, file: file }
            });
        }
    }

    editRoutes(file) {
        this.logger.info('[FileView] Edit routes for file:', file.name);
        
        // Modal pour choisir le mode de routing
        if (this.eventBus) {
            this.eventBus.emit('file:routes_requested', { file });
            this.showRoutingModeModal(file);
        }
    }

    showRoutingModeModal(file) {
        // Créer un modal pour choisir entre 1→1 et N→N
        const modalContent = `
            <div class="routing-mode-modal">
                <h2>Mode de routing pour ${file.name}</h2>
                <p>Choisissez le mode de routing MIDI :</p>
                <div class="routing-mode-buttons">
                    <button class="btn-routing-mode" data-mode="simple">
                        <span class="mode-icon">→</span>
                        <span class="mode-title">Simple (1→1)</span>
                        <span class="mode-desc">Un canal d'entrée vers un device de sortie</span>
                        <span class="mode-hint">✓ Recommandé pour performances live</span>
                    </button>
                    <button class="btn-routing-mode" data-mode="complex">
                        <span class="mode-icon">⚡</span>
                        <span class="mode-title">Complexe (N→N)</span>
                        <span class="mode-desc">Plusieurs canaux avec routage avancé</span>
                        <span class="mode-hint">⚠️ Requiert plus de ressources</span>
                    </button>
                </div>
            </div>
        `;
        
        if (this.eventBus) {
            this.eventBus.emit('modal:show', {
                content: modalContent,
                onAction: (mode) => {
                    this.eventBus.emit('navigation:page_request', {
                        page: 'routing',
                        data: { file, mode }
                    });
                }
            });
        }
    }

    refreshFiles() {
        this.logger.info('[FileView] Refreshing files...');
        
        if (this.eventBus) {
            this.eventBus.emit('files:refresh_requested');
        }
    }

    // ========================================================================
    // RENDU DES PLAYLISTS
    // ========================================================================

    renderPlaylistsGrid() {
        if (!this.elements.playlistsGrid) return;
        
        if (!this.state.playlists || this.state.playlists.length === 0) {
            this.elements.playlistsGrid.innerHTML = this.renderEmptyPlaylists();
            return;
        }
        
        this.elements.playlistsGrid.innerHTML = this.state.playlists
            .map(playlist => this.renderPlaylistCard(playlist))
            .join('');
    }

    renderPlaylistCard(playlist) {
        const filesCount = playlist.files ? playlist.files.length : 0;
        const duration = this.calculatePlaylistDuration(playlist);
        
        return `
            <div class="playlist-card" data-playlist-id="${playlist.id}">
                <div class="playlist-card-header">
                    <div class="playlist-card-icon">📋</div>
                    <div class="playlist-card-info">
                        <div class="playlist-card-name">${playlist.name || 'Sans nom'}</div>
                        <div class="playlist-card-meta">
                            <span>📄 ${filesCount} fichiers</span>
                            <span>⏱️ ${duration}</span>
                        </div>
                    </div>
                </div>
                
                <div class="playlist-card-actions">
                    <button class="playlist-card-btn btn-play" data-action="play-playlist" data-playlist-id="${playlist.id}">
                        <span>▶️</span>
                        <span>Lire</span>
                    </button>
                    <button class="playlist-card-btn btn-edit" data-action="edit-playlist" data-playlist-id="${playlist.id}">
                        <span>✏️</span>
                        <span>Éditer</span>
                    </button>
                </div>
            </div>
        `;
    }

    renderEmptyPlaylists() {
        return `
            <div class="empty-state">
                <div class="empty-state-icon">📋</div>
                <div class="empty-state-text">Aucune playlist</div>
                <div class="empty-state-hint">Les playlists vous permettent d'organiser vos fichiers</div>
            </div>
        `;
    }

    // ========================================================================
    // ACTIONS PLAYLISTS
    // ========================================================================

    handlePlaylistAction(e) {
        const button = e.target.closest('[data-action]');
        if (!button) return;
        
        const action = button.dataset.action;
        const playlistId = button.dataset.playlistId;
        const playlist = this.state.playlists.find(p => p.id === playlistId);
        
        if (!playlist && action !== 'create-playlist') return;
        
        switch (action) {
            case 'edit-playlist':
                this.editPlaylist(playlist);
                break;
            case 'play-playlist':
                this.playPlaylist(playlist);
                break;
            case 'delete-playlist':
                this.deletePlaylist(playlist);
                break;
        }
    }

    createPlaylist() {
        this.logger.info('[FileView] Create new playlist');
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:create_requested');
            this.showPlaylistEditorModal(null);
        }
    }

    editPlaylist(playlist) {
        this.logger.info('[FileView] Edit playlist:', playlist.name);
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:edit_requested', { playlist });
            this.showPlaylistEditorModal(playlist);
        }
    }

    playPlaylist(playlist) {
        this.logger.info('[FileView] Play playlist:', playlist.name);
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:play_requested', { playlist });
            this.eventBus.emit('navigation:page_request', { page: 'home' });
        }
    }

    deletePlaylist(playlist) {
        this.logger.info('[FileView] Delete playlist:', playlist.name);
        
        // Confirmation
        if (confirm(`Supprimer la playlist "${playlist.name}" ?`)) {
            if (this.eventBus) {
                this.eventBus.emit('playlist:delete_requested', { playlist });
            }
        }
    }

    showPlaylistEditorModal(playlist) {
        // Modal pour créer/éditer une playlist
        const isNew = !playlist;
        const title = isNew ? 'Nouvelle Playlist' : `Éditer ${playlist.name}`;
        
        const modalContent = `
            <div class="playlist-editor-modal">
                <h2>${title}</h2>
                <div class="playlist-editor-form">
                    <div class="form-group">
                        <label>Nom de la playlist</label>
                        <input type="text" id="playlistName" value="${playlist ? playlist.name : ''}" />
                    </div>
                    
                    <div class="form-group">
                        <label>Fichiers</label>
                        <div class="playlist-files-selector" id="playlistFilesSelector">
                            <!-- Généré dynamiquement -->
                        </div>
                    </div>
                    
                    <div class="form-actions">
                        <button class="btn-action" id="btnSavePlaylist">💾 Enregistrer</button>
                        <button class="btn-action btn-cancel" id="btnCancelPlaylist">❌ Annuler</button>
                    </div>
                </div>
            </div>
        `;
        
        if (this.eventBus) {
            this.eventBus.emit('modal:show', { content: modalContent });
        }
    }

    refreshPlaylists() {
        this.logger.info('[FileView] Refreshing playlists...');
        
        if (this.eventBus) {
            this.eventBus.emit('playlists:refresh_requested');
        }
    }

    // ========================================================================
    // CHARGEMENT DES DONNÉES
    // ========================================================================

    loadData() {
        if (this.eventBus) {
            this.eventBus.emit('files:load_requested');
            this.eventBus.emit('playlists:load_requested');
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

    calculatePlaylistDuration(playlist) {
        if (!playlist.files || playlist.files.length === 0) {
            return '0:00';
        }
        
        const totalSeconds = playlist.files.reduce((sum, fileId) => {
            const file = this.state.files.find(f => f.id === fileId);
            return sum + (file ? file.duration || 0 : 0);
        }, 0);
        
        return this.formatDuration(totalSeconds);
    }

    // ========================================================================
    // CLEANUP
    // ========================================================================

    destroy() {
        if (this.eventBus) {
            this.eventBus.off('files:loaded');
            this.eventBus.off('playlists:loaded');
            this.eventBus.off('file:updated');
            this.eventBus.off('playlist:updated');
        }
        
        this.logger.info('[FileView] Destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = FileView;
}

if (typeof window !== 'undefined') {
    window.FileView = FileView;
}

// ============================================================================
// FIN DU FICHIER FileView.js v3.10.0
// ============================================================================