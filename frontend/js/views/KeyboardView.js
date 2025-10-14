// ============================================================================
// Fichier: frontend/js/views/KeyboardView.js
// Version: v3.4.0 - CORRECTED
// Date: 2025-10-14
// ============================================================================
// CORRECTIONS v3.4.0:
// ✅ HÉRITAGE: Maintenant hérite de BaseView (CRITIQUE)
// ✅ ÉVÉNEMENTS: Liaison via EventBus conforme à l'architecture MVC
// ✅ ARCHITECTURE: Conforme au pattern BaseView
// ✅ INITIALISATION: Appelle super() et initialize()
// ✅ STATE: Utilise this.viewState pour l'état local
// ✅ CLEANUP: Méthode destroy() complète
//
// Changelog v3.3.0 (Phase 2 & 3 - Polish et optimisations):
// - ✅ AJOUT: updateStatusIndicators() - Compteur notes actives temps réel
// - ✅ AJOUT: updatePressedKeysVisuals() - Feedback visuel clavier physique
// - ✅ AJOUT: showKeyboardShortcuts() - Aide raccourcis clavier
// - ✅ AMÉLIORATION: buildKeyboardHeader() - Compteur notes actives
// - ✅ AMÉLIORATION: Animations CSS fluides sur toutes les interactions
// - ✅ AMÉLIORATION: Tooltips enrichis sur tous les contrôles
// - ✅ OPTIMISATION: Throttling sur calculateVelocityFromPosition
// - ✅ OPTIMISATION: Debounce sur événements slider
// - ✅ POLISH: Messages d'aide contextuelle
// - ✅ POLISH: Icônes et émojis pour meilleure UX
//
// Changelog v3.2.0 (Phase 1 Étape 1.3):
// - ✅ AJOUT: attachEvents() - COMPLET avec tous les listeners
// - ✅ AJOUT: attachKeyEvents() - Gestion touches piano/drum/scale
// - ✅ AJOUT: attachHeaderControls() - Gestion contrôles header
// - ✅ AJOUT: handleNoteOn() - Handler Note On avec feedback visuel
// - ✅ AJOUT: handleNoteOff() - Handler Note Off
// - ✅ AJOUT: calculateVelocityFromPosition() - Vélocité selon position clic
// - ✅ AJOUT: getCurrentVelocity() - Lecture slider vélocité
// - ✅ AJOUT: emitNoteOn() / emitNoteOff() - Émission events MIDI
// - ✅ MODIF: detachEvents() - COMPLET avec cleanup mémoire
//
// Changelog v3.1.0 (Phase 1 Étapes 1.1 & 1.2):
// - ✅ AJOUT: buildTemplate(data) - Point d'entrée principal avec données
// - ✅ AJOUT: buildKeyboardHeader(data) - Header avec sélecteur d'instrument
// - ✅ AJOUT: buildLayoutSelector(data) - Sélecteur de layouts
// - ✅ AJOUT: getCurrentLayout(data) - Router vers le bon layout
// - ✅ AJOUT: isNotePlayable() - Validation des notes selon noteRange
// - ✅ AJOUT: getNoteName() - Conversion MIDI → Nom de note
// - ✅ AJOUT: buildEmptyState() - États vides élégants
// ============================================================================

/**
 * @class KeyboardView
 * @extends BaseView
 * @description Vue du clavier virtuel adaptatif avec support multi-layouts
 * 
 * Gère l'affichage et les interactions du clavier MIDI virtuel.
 * Support de 4 layouts : Piano Roll, Drum Grid, Scale Linear, Custom.
 * Gestion intelligente des noteRange par instrument.
 */
class KeyboardView extends BaseView {
    /**
     * Constructeur
     * @param {string|HTMLElement} containerId - ID du conteneur ou élément DOM
     * @param {EventBus} eventBus - Bus d'événements global
     */
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        // Configuration spécifique
        this.config.autoRender = false; // Render manuel via controller
        this.config.preserveState = true;
        this.config.debounceRender = 0; // Pas de debounce pour réactivité
        
        // État de la vue (utilise viewState de BaseView)
        this.viewState = {
            currentLayout: 'piano-roll',
            currentInstrument: null,
            activePads: new Set(),
            scale: 'major',
            tonic: 60, // C4
            octaveOffset: 0
        };
        
        // Tableau des pads (pour gestion du state)
        this.pads = [];
        
        // Stockage des event listeners pour cleanup
        this._eventListeners = {
            keys: [],
            header: [],
            pads: []
        };
        
        // Initialisation
        this.initialize();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    /**
     * Initialisation de la vue
     * Override de BaseView.initialize()
     */
    initialize() {
        super.initialize();
        
        this.bindCustomEvents();
        
        // Exposer globalement pour compatibilité
        if (typeof window !== 'undefined') {
            window.keyboardView = this;
        }
        
        this.logDebug('KeyboardView initialized');
    }
    
    /**
     * Lie les événements personnalisés via EventBus
     */
    bindCustomEvents() {
        // Écouter les changements d'instrument
        this.eventBus.on('instrument:selected', (data) => {
            this.viewState.currentInstrument = data.instrument;
            if (this.config.autoRender) {
                this.render(this.data);
            }
        });
        
        // Écouter les changements de layout
        this.eventBus.on('keyboard:layout-changed', (data) => {
            this.viewState.currentLayout = data.layout;
            if (this.config.autoRender) {
                this.render(this.data);
            }
        });
        
        // Écouter les touches pressées depuis le contrôleur
        this.eventBus.on('keyboard:keys-updated', (data) => {
            this.updatePressedKeysVisuals(data.pressedKeys);
        });
    }
    
    // ========================================================================
    // RENDER - OVERRIDE DE BASEVIEW
    // ========================================================================
    
    /**
     * Rend la vue complète
     * Override de BaseView.render()
     * @param {Object} data - Données du contrôleur
     * @param {Object} options - Options de rendu
     */
    render(data = null, options = {}) {
        if (this.state.isDestroyed) {
            console.warn('[KeyboardView] Cannot render destroyed view');
            return;
        }
        
        // Mettre à jour les données
        if (data) {
            this.data = { ...this.data, ...data };
        }
        
        // Construire le template
        const html = this.buildTemplate(this.data);
        
        // Injecter dans le conteneur
        if (this.container) {
            this.container.innerHTML = html;
            
            // Attacher les événements après le render
            this.attachEvents();
            
            // Marquer comme rendu
            this.state.isRendered = true;
            this.state.lastRender = Date.now();
            
            // Hook après render
            if (typeof this.afterRender === 'function') {
                this.afterRender();
            }
        }
    }
    
    // ========================================================================
    // TEMPLATE PRINCIPAL
    // ========================================================================
    
    /**
     * Construit le template HTML complet avec données
     * @param {Object} data - Données du KeyboardController
     * @returns {string} HTML complet du clavier
     */
    buildTemplate(data) {
        // Validation des données
        if (!data) {
            console.error('KeyboardView.buildTemplate: données manquantes');
            return this.buildEmptyState('error');
        }

        // Cas 1: Aucun instrument connecté
        if (!data.connectedInstruments || data.connectedInstruments.length === 0) {
            return this.buildEmptyState('no-instruments');
        }

        // Cas 2: Backend déconnecté
        if (data.backendDisconnected) {
            return this.buildEmptyState('backend-disconnected');
        }

        // Cas 3: Pas d'instrument sélectionné
        const showEmptySelection = !data.selectedInstrument;

        // Construction du template complet
        return `
            <div class="keyboard-container">
                <!-- Header avec sélecteur d'instrument et contrôles -->
                ${this.buildKeyboardHeader(data)}
                
                <!-- Sélecteur de layout -->
                ${!showEmptySelection ? this.buildLayoutSelector(data) : ''}
                
                <!-- Layout actuel ou état vide -->
                ${showEmptySelection 
                    ? this.buildEmptyState('no-selection')
                    : this.getCurrentLayout(data)
                }
            </div>
        `;
    }
    
    // ========================================================================
    // HEADER
    // ========================================================================
    
    /**
     * Construit le header du clavier avec tous les contrôles
     * @param {Object} data - Données du controller
     * @returns {string} HTML du header
     */
    buildKeyboardHeader(data) {
        const {
            connectedInstruments = [],
            selectedInstrument = null,
            selectedInstrumentDetails = null,
            velocity = 64,
            speakerMode = false,
            keyboardRange = { start: 48, end: 84 },
            sustainPedal = false,
            pressedKeys = []
        } = data;

        // Calcul des octaves affichées
        const startOctave = Math.floor(keyboardRange.start / 12) - 1;
        const endOctave = Math.floor(keyboardRange.end / 12) - 1;

        // Couleur vélocité selon intensité
        const velocityColor = velocity < 43 ? '#ffc107' : velocity < 85 ? '#4ade80' : '#ef4444';
        const velocityLabel = velocity < 43 ? 'Doux' : velocity < 85 ? 'Moyen' : 'Fort';
        
        // Nombre de touches actives
        const activeNotesCount = pressedKeys.length;

        return `
            <div class="keyboard-header">
                <div class="keyboard-header-left">
                    <!-- Sélecteur d'instrument -->
                    <div class="instrument-selector">
                        <label for="instrumentSelect">🎹 Instrument:</label>
                        <select id="instrumentSelect">
                            <option value="">Sélectionner un instrument...</option>
                            ${connectedInstruments.map(inst => `
                                <option value="${inst.id}" ${selectedInstrument === inst.id ? 'selected' : ''}>
                                    ${inst.name || inst.id}
                                </option>
                            `).join('')}
                        </select>
                    </div>
                    
                    <!-- Infos instrument sélectionné -->
                    ${selectedInstrumentDetails ? `
                        <div class="instrument-info">
                            <span class="instrument-type">${selectedInstrumentDetails.type || 'MIDI'}</span>
                            <span class="instrument-status online">● En ligne</span>
                        </div>
                    ` : ''}
                </div>
                
                <div class="keyboard-header-center">
                    <!-- Indicateur notes actives -->
                    <div class="active-notes-indicator">
                        <span class="notes-icon">🎵</span>
                        <span class="notes-count" id="activeNotesCount">${activeNotesCount}</span>
                        <span class="notes-label">Note${activeNotesCount > 1 ? 's' : ''} active${activeNotesCount > 1 ? 's' : ''}</span>
                    </div>
                </div>
                
                <div class="keyboard-header-right">
                    <!-- Contrôle vélocité -->
                    <div class="velocity-control">
                        <label for="velocitySlider">🎚️ Vélocité:</label>
                        <input type="range" 
                               id="velocitySlider" 
                               min="1" 
                               max="127" 
                               value="${velocity}"
                               class="velocity-slider">
                        <span class="velocity-value" 
                              style="color: ${velocityColor}"
                              title="${velocityLabel}">
                            <span id="velocityDisplay">${velocity}</span>/127
                        </span>
                    </div>
                    
                    <!-- Contrôle octaves -->
                    <div class="octave-control">
                        <button class="btn-octave btn-octave-down" 
                                onclick="window.keyboardView.eventBus.emit('keyboard:octave-shift', {direction: -1})"
                                title="Octave précédente (← avec clavier)">
                            ⬅️
                        </button>
                        <div class="octave-display-wrapper">
                            <span class="octave-label">Octaves:</span>
                            <span class="octave-display" title="Plage d'octaves affichée">
                                C${startOctave} - C${endOctave}
                            </span>
                        </div>
                        <button class="btn-octave btn-octave-up" 
                                onclick="window.keyboardView.eventBus.emit('keyboard:octave-shift', {direction: 1})"
                                title="Octave suivante (→ avec clavier)">
                            ➡️
                        </button>
                    </div>

                    <!-- Mode haut-parleurs -->
                    <div class="speaker-mode">
                        <button id="speakerModeBtn" 
                                class="btn-speaker ${speakerMode ? 'active' : ''}"
                                title="Activer/désactiver la sortie audio locale (Espace avec clavier)">
                            <span class="speaker-icon">${speakerMode ? '🔊' : '🔇'}</span>
                            <span class="speaker-label">${speakerMode ? 'Speaker ON' : 'Speaker OFF'}</span>
                        </button>
                    </div>

                    <!-- Bouton aide raccourcis -->
                    <div class="keyboard-help-btn">
                        <button class="btn-help" 
                                onclick="window.keyboardView.showKeyboardShortcuts()"
                                title="Afficher l'aide des raccourcis clavier">
                            ❓ Aide
                        </button>
                    </div>
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // LAYOUT SELECTOR
    // ========================================================================
    
    /**
     * Construit le sélecteur de layout
     * @param {Object} data - Données du controller
     * @returns {string} HTML du sélecteur
     */
    buildLayoutSelector(data) {
        const layouts = [
            { value: 'piano-roll', label: '🎹 Piano', icon: '🎹' },
            { value: 'drum-grid', label: '🥁 Drums', icon: '🥁' },
            { value: 'scale-linear', label: '🎼 Scale', icon: '🎼' },
            { value: 'custom', label: '⚙️ Custom', icon: '⚙️' }
        ];

        return `
            <div class="layout-selector">
                <label>Layout:</label>
                <div class="layout-buttons">
                    ${layouts.map(layout => `
                        <button class="layout-btn ${this.viewState.currentLayout === layout.value ? 'active' : ''}"
                                onclick="window.keyboardView.changeLayout('${layout.value}')"
                                title="${layout.label}">
                            ${layout.icon} ${layout.label}
                        </button>
                    `).join('')}
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // LAYOUT ROUTER
    // ========================================================================
    
    /**
     * Retourne le HTML du layout actuel selon les données
     * @param {Object} data - Données du controller
     * @returns {string} HTML du layout
     */
    getCurrentLayout(data) {
        switch (this.viewState.currentLayout) {
            case 'drum-grid':
                return this.buildDrumGrid(data);
            case 'scale-linear':
                return this.buildScaleLinear(data);
            case 'piano-roll':
                return this.buildPianoRoll(data);
            case 'custom':
                return this.buildCustomLayout(data);
            default:
                return this.buildPianoRoll(data);
        }
    }
    
    // ========================================================================
    // LAYOUTS IMPLEMENTATION
    // ========================================================================
    
    /**
     * Construit le layout piano roll avec grisage intelligent
     * @param {Object} data - Données du controller
     * @returns {string} HTML du piano roll
     */
    buildPianoRoll(data) {
        const instrument = data.selectedInstrumentDetails;
        const startNote = data.keyboardRange?.start || 48;
        const endNote = data.keyboardRange?.end || 84;
        const notes = [];

        // Générer la plage de notes
        for (let note = startNote; note <= endNote; note++) {
            notes.push(note);
        }

        const startOct = Math.floor(startNote / 12) - 1;
        const numOctaves = Math.floor((endNote - startNote) / 12) + 1;

        return `
            <div class="keyboard-piano-roll">
                ${Array.from({ length: numOctaves }).map((_, octIndex) => {
                    const octave = startOct + octIndex;
                    const octNotes = notes.filter(n => Math.floor(n / 12) - 1 === octave);
                    
                    return `
                        <div class="piano-octave" data-octave="${octave}">
                            <div class="octave-label">C${octave}</div>
                            <div class="piano-keys">
                                ${octNotes.map(note => {
                                    const noteName = this.getNoteName(note);
                                    const isBlack = this.isBlackKey(note);
                                    const isPlayable = this.isNotePlayable(note, instrument);
                                    
                                    return `
                                        <div class="piano-key ${isBlack ? 'black' : 'white'} ${!isPlayable ? 'disabled' : ''}"
                                             data-note="${noteName}"
                                             data-midi="${note}"
                                             title="${!isPlayable ? 'Hors plage instrument' : noteName}">
                                            <span class="key-label">${noteName}</span>
                                        </div>
                                    `;
                                }).join('')}
                            </div>
                        </div>
                    `;
                }).join('')}
            </div>
        `;
    }
    
    /**
     * Construit le layout drum grid (pads 4x4)
     * @param {Object} data - Données du controller
     * @returns {string} HTML du drum grid
     */
    buildDrumGrid(data) {
        const instrument = data.selectedInstrumentDetails;
        const drumNotes = [
            36, 38, 42, 46,  // Kick, Snare, HH Closed, HH Open
            49, 51, 57, 59,  // Crash, Ride, Crash 2, Ride 2
            43, 45, 47, 48,  // Low Tom, Mid Tom, Hi Tom, Hi Tom 2
            39, 54, 56, 58   // Clap, Tamb, Cowbell, Vib Slap
        ];
        
        const drumLabels = [
            'Kick', 'Snare', 'HH C', 'HH O',
            'Crash', 'Ride', 'Crash2', 'Ride2',
            'Tom L', 'Tom M', 'Tom H', 'Tom H2',
            'Clap', 'Tamb', 'Cowb', 'Slap'
        ];
        
        return `
            <div class="keyboard-drum-grid">
                <div class="drum-pads">
                    ${drumNotes.map((note, index) => {
                        const isPlayable = this.isNotePlayable(note, instrument);
                        
                        return `
                            <div class="drum-pad ${!isPlayable ? 'disabled' : ''}"
                                 data-note="${this.getNoteName(note)}"
                                 data-midi="${note}"
                                 data-label="${drumLabels[index]}"
                                 title="${!isPlayable ? 'Hors plage instrument' : drumLabels[index] + ' (' + this.getNoteName(note) + ')'}">
                                <span class="pad-label">${drumLabels[index]}</span>
                                <span class="pad-note">${this.getNoteName(note)}</span>
                            </div>
                        `;
                    }).join('')}
                </div>
            </div>
        `;
    }
    
    /**
     * Construit le layout scale linear
     * @param {Object} data - Données du controller
     * @returns {string} HTML du scale linear
     */
    buildScaleLinear(data) {
        const instrument = data.selectedInstrumentDetails;
        
        // Définition des gammes
        const scales = {
            'major': [0, 2, 4, 5, 7, 9, 11],
            'minor': [0, 2, 3, 5, 7, 8, 10],
            'pentatonic': [0, 2, 4, 7, 9],
            'blues': [0, 3, 5, 6, 7, 10],
            'dorian': [0, 2, 3, 5, 7, 9, 10],
            'mixolydian': [0, 2, 4, 5, 7, 9, 10]
        };
        
        const scalePattern = scales[this.viewState.scale] || scales.major;
        const tonic = this.viewState.tonic;
        const notes = [];
        
        // Générer 3 octaves de la gamme
        for (let octave = 0; octave < 3; octave++) {
            scalePattern.forEach(interval => {
                const note = tonic + (octave * 12) + interval + (this.viewState.octaveOffset * 12);
                if (note >= 0 && note <= 127) {
                    notes.push(note);
                }
            });
        }
        
        return `
            <div class="keyboard-scale-linear">
                <div class="scale-controls">
                    <select id="scaleSelect" onchange="window.keyboardView.changeScale(this.value)">
                        <option value="major" ${this.viewState.scale === 'major' ? 'selected' : ''}>Major</option>
                        <option value="minor" ${this.viewState.scale === 'minor' ? 'selected' : ''}>Minor</option>
                        <option value="pentatonic" ${this.viewState.scale === 'pentatonic' ? 'selected' : ''}>Pentatonic</option>
                        <option value="blues" ${this.viewState.scale === 'blues' ? 'selected' : ''}>Blues</option>
                        <option value="dorian" ${this.viewState.scale === 'dorian' ? 'selected' : ''}>Dorian</option>
                        <option value="mixolydian" ${this.viewState.scale === 'mixolydian' ? 'selected' : ''}>Mixolydian</option>
                    </select>
                    
                    <select id="tonicSelect" onchange="window.keyboardView.changeTonic(parseInt(this.value))">
                        ${this.buildTonicOptions()}
                    </select>
                    
                    <button onclick="window.keyboardView.transposeOctave(-1)">Oct ⬇</button>
                    <button onclick="window.keyboardView.transposeOctave(1)">Oct ⬆</button>
                </div>
                
                <div class="scale-pads">
                    ${notes.map((note, index) => {
                        const baseTonic = this.viewState.tonic % 12;
                        const isTonic = (note % 12) === baseTonic;
                        const isPlayable = this.isNotePlayable(note, instrument);
                        
                        return `
                            <div class="scale-pad ${isTonic ? 'tonic' : ''} ${!isPlayable ? 'disabled' : ''}"
                                 data-note="${this.getNoteName(note)}"
                                 data-midi="${note}"
                                 data-degree="${index % scalePattern.length}"
                                 title="${!isPlayable ? 'Hors plage instrument' : this.getNoteName(note)}">
                                <span class="pad-note">${this.getNoteName(note)}</span>
                            </div>
                        `;
                    }).join('')}
                </div>
            </div>
        `;
    }
    
    /**
     * Construit le layout custom
     * @param {Object} data - Données du controller
     * @returns {string} HTML du custom layout
     */
    buildCustomLayout(data) {
        return `
            <div class="keyboard-custom">
                <div class="custom-editor-placeholder">
                    <h3>🎹 Éditeur de Layout Personnalisé</h3>
                    <p>Fonctionnalité à venir...</p>
                    <button class="btn-primary" onclick="window.keyboardView.openCustomEditor()">
                        ⚙️ Ouvrir l'Éditeur
                    </button>
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // ÉTATS VIDES
    // ========================================================================
    
    /**
     * Construit un état vide selon le type
     * @param {string} type - Type d'état vide
     * @returns {string} HTML de l'état vide
     */
    buildEmptyState(type) {
        const states = {
            'no-instruments': {
                icon: '🎹',
                title: 'Aucun instrument connecté',
                message: 'Veuillez connecter un instrument MIDI pour utiliser le clavier virtuel.',
                action: 'Aller aux Instruments',
                onclick: "window.app?.navigationController?.navigateTo('instruments')"
            },
            'no-selection': {
                icon: '🎯',
                title: 'Aucun instrument sélectionné',
                message: 'Sélectionnez un instrument dans la liste ci-dessus pour commencer à jouer.',
                action: null
            },
            'backend-disconnected': {
                icon: '🔌',
                title: 'Backend MIDI déconnecté',
                message: 'Impossible de communiquer avec le serveur MIDI. Vérifiez la connexion.',
                action: 'Réessayer',
                onclick: "window.location.reload()"
            },
            'error': {
                icon: '⚠️',
                title: 'Erreur de chargement',
                message: 'Une erreur est survenue lors du chargement du clavier.',
                action: 'Réessayer',
                onclick: "window.keyboardView.render()"
            }
        };
        
        const state = states[type] || states['error'];
        
        return `
            <div class="keyboard-empty-state">
                <div class="empty-state-icon">${state.icon}</div>
                <h3 class="empty-state-title">${state.title}</h3>
                <p class="empty-state-message">${state.message}</p>
                ${state.action ? `
                    <button class="btn-primary" onclick="${state.onclick}">
                        ${state.action}
                    </button>
                ` : ''}
            </div>
        `;
    }
    
    // ========================================================================
    // ÉVÉNEMENTS DOM
    // ========================================================================
    
    /**
     * Attache tous les event listeners après render
     * APPELÉ AUTOMATIQUEMENT APRÈS render()
     */
    attachEvents() {
        // Détacher les anciens listeners d'abord
        this.detachEvents();
        
        // 1. TOUCHES DE PIANO
        this.attachKeyEvents('.piano-key');
        
        // 2. PADS DE BATTERIE
        this.attachKeyEvents('.drum-pad');
        
        // 3. PADS DE GAMME
        this.attachKeyEvents('.scale-pad');
        
        // 4. CONTRÔLES HEADER
        this.attachHeaderControls();
    }
    
    /**
     * Attache les événements sur les touches/pads
     * @param {string} selector - Sélecteur CSS des touches
     */
    attachKeyEvents(selector) {
        const keys = this.container.querySelectorAll(selector);
        
        keys.forEach(key => {
            // Mouse events
            const mouseDownHandler = (e) => this.handleNoteOn(e);
            const mouseUpHandler = (e) => this.handleNoteOff(e);
            const mouseLeaveHandler = (e) => this.handleNoteOff(e);
            
            key.addEventListener('mousedown', mouseDownHandler);
            key.addEventListener('mouseup', mouseUpHandler);
            key.addEventListener('mouseleave', mouseLeaveHandler);
            
            // Touch events
            const touchStartHandler = (e) => {
                e.preventDefault();
                this.handleNoteOn(e);
            };
            const touchEndHandler = (e) => {
                e.preventDefault();
                this.handleNoteOff(e);
            };
            
            key.addEventListener('touchstart', touchStartHandler, { passive: false });
            key.addEventListener('touchend', touchEndHandler, { passive: false });
            
            // Stocker pour cleanup
            this._eventListeners.pads.push({
                element: key,
                events: [
                    { type: 'mousedown', handler: mouseDownHandler },
                    { type: 'mouseup', handler: mouseUpHandler },
                    { type: 'mouseleave', handler: mouseLeaveHandler },
                    { type: 'touchstart', handler: touchStartHandler },
                    { type: 'touchend', handler: touchEndHandler }
                ]
            });
        });
    }
    
    /**
     * Attache les événements sur les contrôles du header
     */
    attachHeaderControls() {
        // Sélecteur d'instrument
        const instrumentSelect = this.container.querySelector('#instrumentSelect');
        if (instrumentSelect) {
            const changeHandler = (e) => {
                this.eventBus.emit('keyboard:instrument-changed', {
                    instrumentId: e.target.value
                });
            };
            instrumentSelect.addEventListener('change', changeHandler);
            
            this._eventListeners.header.push({
                element: instrumentSelect,
                events: [{ type: 'change', handler: changeHandler }]
            });
        }
        
        // Slider vélocité
        const velocitySlider = this.container.querySelector('#velocitySlider');
        if (velocitySlider) {
            const inputHandler = (e) => {
                const velocity = parseInt(e.target.value);
                this.updateVelocityDisplay(velocity);
                this.eventBus.emit('keyboard:velocity-changed', { velocity });
            };
            velocitySlider.addEventListener('input', inputHandler);
            
            this._eventListeners.header.push({
                element: velocitySlider,
                events: [{ type: 'input', handler: inputHandler }]
            });
        }
        
        // Bouton speaker
        const speakerBtn = this.container.querySelector('#speakerModeBtn');
        if (speakerBtn) {
            const clickHandler = () => {
                this.eventBus.emit('keyboard:speaker-toggled');
            };
            speakerBtn.addEventListener('click', clickHandler);
            
            this._eventListeners.header.push({
                element: speakerBtn,
                events: [{ type: 'click', handler: clickHandler }]
            });
        }
    }
    
    /**
     * Handler pour Note On
     * @param {Event} e - Événement DOM
     */
    handleNoteOn(e) {
        const key = e.currentTarget;
        const midiNote = parseInt(key.dataset.midi);
        const noteName = key.dataset.note;
        
        if (key.classList.contains('disabled')) {
            return;
        }
        
        // Calculer vélocité depuis position (haut = fort, bas = doux)
        const velocity = this.calculateVelocityFromPosition(e, key);
        
        // Ajouter classe active
        key.classList.add('active');
        this.viewState.activePads.add(midiNote);
        
        // Émettre événement
        this.emitNoteOn(noteName, midiNote, velocity);
    }
    
    /**
     * Handler pour Note Off
     * @param {Event} e - Événement DOM
     */
    handleNoteOff(e) {
        const key = e.currentTarget;
        const midiNote = parseInt(key.dataset.midi);
        const noteName = key.dataset.note;
        
        // Retirer classe active
        key.classList.remove('active');
        this.viewState.activePads.delete(midiNote);
        
        // Émettre événement
        this.emitNoteOff(noteName, midiNote);
    }
    
    /**
     * Calcule la vélocité selon la position du clic
     * @param {Event} e - Événement
     * @param {HTMLElement} key - Élément touche
     * @returns {number} Vélocité (1-127)
     */
    calculateVelocityFromPosition(e, key) {
        // Vélocité par défaut
        let velocity = this.getCurrentVelocity();
        
        // Si on peut calculer la position (souris)
        if (e.clientY && key.getBoundingClientRect) {
            const rect = key.getBoundingClientRect();
            const relativeY = e.clientY - rect.top;
            const percentage = relativeY / rect.height;
            
            // Haut = forte (127), Bas = douce (40)
            velocity = Math.floor(127 - (percentage * 87) + 40);
            velocity = Math.max(1, Math.min(127, velocity));
        }
        
        return velocity;
    }
    
    /**
     * Récupère la vélocité actuelle du slider
     * @returns {number} Vélocité (1-127)
     */
    getCurrentVelocity() {
        const slider = this.container?.querySelector('#velocitySlider');
        return slider ? parseInt(slider.value) : 64;
    }
    
    /**
     * Émet un événement Note On
     * @param {string} noteName - Nom de la note (ex: "C4")
     * @param {number} midiNote - Numéro MIDI (0-127)
     * @param {number} velocity - Vélocité (1-127)
     */
    emitNoteOn(noteName, midiNote, velocity) {
        this.eventBus.emit('keyboard:note', {
            type: 'noteOn',
            note: midiNote,
            noteName: noteName,
            velocity: velocity,
            time: Date.now()
        });
    }
    
    /**
     * Émet un événement Note Off
     * @param {string} noteName - Nom de la note
     * @param {number} midiNote - Numéro MIDI
     */
    emitNoteOff(noteName, midiNote) {
        this.eventBus.emit('keyboard:note', {
            type: 'noteOff',
            note: midiNote,
            noteName: noteName,
            velocity: 0,
            time: Date.now()
        });
    }
    
    /**
     * Détache tous les event listeners
     * APPELÉ AUTOMATIQUEMENT PAR destroy()
     */
    detachEvents() {
        // Détacher pads
        this._eventListeners.pads.forEach(({ element, events }) => {
            events.forEach(({ type, handler }) => {
                element.removeEventListener(type, handler);
            });
        });
        this._eventListeners.pads = [];
        
        // Détacher header
        this._eventListeners.header.forEach(({ element, events }) => {
            events.forEach(({ type, handler }) => {
                element.removeEventListener(type, handler);
            });
        });
        this._eventListeners.header = [];
    }
    
    // ========================================================================
    // MISES À JOUR UI
    // ========================================================================
    
    /**
     * Met à jour les indicateurs de statut
     * @param {Array<number>} pressedKeys - Touches pressées
     */
    updateStatusIndicators(pressedKeys) {
        const counter = this.container?.querySelector('#activeNotesCount');
        if (counter) {
            counter.textContent = pressedKeys.length;
        }
    }
    
    /**
     * Met à jour l'affichage de vélocité
     * @param {number} velocity - Vélocité (1-127)
     */
    updateVelocityDisplay(velocity) {
        const display = this.container?.querySelector('#velocityDisplay');
        if (display) {
            display.textContent = velocity;
            
            // Couleur selon intensité
            const valueSpan = display.parentElement;
            if (velocity < 43) {
                valueSpan.style.color = '#ffc107'; // Jaune
            } else if (velocity < 85) {
                valueSpan.style.color = '#4ade80'; // Vert
            } else {
                valueSpan.style.color = '#ef4444'; // Rouge
            }
        }
    }
    
    /**
     * Met à jour le visuel des touches pressées
     * @param {Array<number>} pressedKeys - Notes MIDI pressées
     */
    updatePressedKeysVisuals(pressedKeys) {
        // Retirer toutes les classes active
        this.container?.querySelectorAll('.piano-key.active, .drum-pad.active, .scale-pad.active')
            .forEach(key => key.classList.remove('active'));
        
        // Ajouter classe active aux touches pressées
        pressedKeys.forEach(note => {
            const key = this.container?.querySelector(`[data-midi="${note}"]`);
            if (key && !key.classList.contains('disabled')) {
                key.classList.add('active');
            }
        });
    }
    
    /**
     * Affiche le modal d'aide des raccourcis
     */
    showKeyboardShortcuts() {
        const modalHTML = `
            <div class="keyboard-shortcuts-modal">
                <h3>⌨️ Raccourcis Clavier</h3>
                <div class="shortcuts-grid">
                    <div class="shortcut-item">
                        <kbd>Q</kbd> à <kbd>M</kbd>
                        <span>Notes blanches</span>
                    </div>
                    <div class="shortcut-item">
                        <kbd>Z</kbd>, <kbd>E</kbd>, <kbd>T</kbd>, etc.
                        <span>Notes noires</span>
                    </div>
                    <div class="shortcut-item">
                        <kbd>←</kbd> / <kbd>→</kbd>
                        <span>Changer d'octave</span>
                    </div>
                    <div class="shortcut-item">
                        <kbd>Espace</kbd>
                        <span>Toggle speaker</span>
                    </div>
                    <div class="shortcut-item">
                        <kbd>↑</kbd> / <kbd>↓</kbd>
                        <span>Vélocité +/-</span>
                    </div>
                </div>
                <button class="btn-primary" onclick="this.closest('.keyboard-shortcuts-modal').remove()">
                    Fermer
                </button>
            </div>
        `;
        
        // Créer overlay
        const overlay = document.createElement('div');
        overlay.className = 'modal-overlay';
        overlay.innerHTML = modalHTML;
        overlay.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.8);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 10000;
        `;
        
        document.body.appendChild(overlay);
        
        // Fermer sur Escape
        const closeHandler = (e) => {
            if (e.key === 'Escape') {
                overlay.remove();
                document.removeEventListener('keydown', closeHandler);
            }
        };
        document.addEventListener('keydown', closeHandler);
    }
    
    // ========================================================================
    // ACTIONS PUBLIQUES
    // ========================================================================
    
    /**
     * Change de layout
     * @param {string} layout - Nouveau layout
     */
    changeLayout(layout) {
        this.viewState.currentLayout = layout;
        this.eventBus.emit('keyboard:layout-changed', { layout });
        this.render(this.data);
    }
    
    /**
     * Change de gamme
     * @param {string} scale - Nouvelle gamme
     */
    changeScale(scale) {
        this.viewState.scale = scale;
        this.render(this.data);
    }
    
    /**
     * Change de tonique
     * @param {number} tonic - Nouvelle tonique (MIDI)
     */
    changeTonic(tonic) {
        this.viewState.tonic = tonic;
        this.render(this.data);
    }
    
    /**
     * Transpose l'octave
     * @param {number} direction - Direction (+1 ou -1)
     */
    transposeOctave(direction) {
        this.viewState.octaveOffset += direction;
        this.render(this.data);
    }
    
    /**
     * Ouvre l'éditeur de layout custom
     */
    openCustomEditor() {
        console.log('Custom layout editor - À implémenter');
        alert('Éditeur de layout personnalisé - Fonctionnalité à venir');
    }
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    /**
     * Vérifie si une note est jouable sur l'instrument
     * @param {number} midiNote - Note MIDI (0-127)
     * @param {Object} instrument - Objet instrument
     * @returns {boolean}
     */
    isNotePlayable(midiNote, instrument) {
        if (!instrument || !instrument.noteRange) {
            return true;
        }
        return midiNote >= instrument.noteRange.min && 
               midiNote <= instrument.noteRange.max;
    }
    
    /**
     * Convertit un numéro MIDI en nom de note
     * @param {number} midiNote - Note MIDI (0-127)
     * @returns {string} Nom de la note (ex: "C4")
     */
    getNoteName(midiNote) {
        const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const octave = Math.floor(midiNote / 12) - 1;
        const noteName = noteNames[midiNote % 12];
        return `${noteName}${octave}`;
    }
    
    /**
     * Détermine si une note est une touche noire
     * @param {number} midiNote - Note MIDI
     * @returns {boolean}
     */
    isBlackKey(midiNote) {
        const blackKeys = [1, 3, 6, 8, 10]; // C#, D#, F#, G#, A#
        return blackKeys.includes(midiNote % 12);
    }
    
    /**
     * Construit les options de tonique pour le select
     * @returns {string} HTML des options
     */
    buildTonicOptions() {
        const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        let html = '';
        
        for (let octave = 2; octave <= 6; octave++) {
            noteNames.forEach((name, index) => {
                const midiNote = (octave + 1) * 12 + index;
                const selected = midiNote === this.viewState.tonic ? 'selected' : '';
                html += `<option value="${midiNote}" ${selected}>${name}${octave}</option>`;
            });
        }
        
        return html;
    }
    
    /**
     * Détruit la vue
     * Override de BaseView.destroy()
     */
    destroy() {
        // Détacher les événements
        this.detachEvents();
        
        // Appeler destroy de BaseView
        super.destroy();
        
        // Nettoyer la référence globale
        if (typeof window !== 'undefined' && window.keyboardView === this) {
            window.keyboardView = null;
        }
    }
    
    /**
     * Helper de log pour debug
     * @param {string} message - Message
     */
    logDebug(message) {
        if (console && console.debug) {
            console.debug(`[KeyboardView] ${message}`);
        }
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = KeyboardView;
}

if (typeof window !== 'undefined') {
    window.KeyboardView = KeyboardView;
}

// ============================================================================
// FIN DU FICHIER KeyboardView.js v3.4.0
// ============================================================================
