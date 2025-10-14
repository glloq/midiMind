// ===== KEYBOARD CONTROLLER - Contrôleur du clavier virtuel MIDI =====
// ====================================================================
// Gère toute la logique du clavier virtuel :
// - Interface avec KeyboardView pour l'affichage
// - Gestion des événements clavier physique et souris
// - Envoi des messages MIDI aux instruments via Backend C++
// - Contrôle de vélocité et navigation octaves
// - Mode haut-parleurs pour écoute directe
// - Synchronisation avec les instruments connectés
// ====================================================================

class KeyboardController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Référence au backend C++
        this.backend = null;
        
        // État du clavier
        this.selectedInstrument = null;
        this.velocity = 64;
        this.keyboardRange = { start: 48, end: 84 }; // C3 à C6 par défaut
        this.speakerMode = false;
        this.pressedKeys = new Set();
        this.sustainPedal = false;
        
        // Configuration des touches physiques (AZERTY)
        this.physicalKeyMap = {
            // Touches blanches (rangée du bas)
            'q': 0, 's': 1, 'd': 2, 'f': 3, 'g': 4, 'h': 5, 'j': 6, 'k': 7, 'l': 8, 'm': 9,
            // Touches noires (rangée du haut)
            'z': 0.5, 'e': 1.5, 't': 3.5, 'y': 4.5, 'u': 5.5, 'o': 7.5, 'p': 8.5
        };
        
        // État des touches physiques pressées
        this.physicalKeysPressed = new Set();
        
        this.init();
    }

    /**
     * Initialisation du contrôleur
     */
    init() {
        // Récupérer le backend depuis PlaybackController (avec délai pour l'initialisation)
        setTimeout(() => {
            if (this.app && this.app.playbackController && this.app.playbackController.backend) {
                this.backend = this.app.playbackController.backend;
                this.logDebug('keyboard', '✅ Backend C++ connecté au clavier');
            } else {
                this.logDebug('warning', '⚠️ Backend non disponible - mode dégradé');
            }
        }, 500);
        
        this.setupKeyboardListeners();
        this.bindEvents();
        
        this.logDebug('system', '🎹 KeyboardController initialisé');
    }


    /**
     * Configuration des listeners clavier physique
     */
    setupKeyboardListeners() {
        document.addEventListener('keydown', (event) => {
            if (this.isKeyboardPageActive()) {
                this.handlePhysicalKeyDown(event);
            }
        });
        
        document.addEventListener('keyup', (event) => {
            if (this.isKeyboardPageActive()) {
                this.handlePhysicalKeyUp(event);
            }
        });
        
        // Relâcher toutes les notes si on quitte la page
        window.addEventListener('blur', () => {
            this.releaseAllKeys();
        });
    }

    /**
     * Vérifie si on est sur la page clavier
     */
    isKeyboardPageActive() {
        return document.getElementById('keyboard-page') && 
               this.app.navigationController.getCurrentPage() === 'keyboard';
    }

    // ===== SÉLECTION D'INSTRUMENT =====

    /**
     * Sélectionne un instrument pour le clavier
     */
    selectInstrument(instrumentId) {
        this.selectedInstrument = instrumentId;
        
        if (instrumentId) {
            const instrumentModel = this.getModel('instrument');
            const instrument = instrumentModel.getInstrumentById(instrumentId);
            if (instrument) {
                this.logDebug('keyboard', `Instrument sélectionné: ${instrument.name}`);
                this.showNotification(`Clavier: ${instrument.name}`, 'info');
            }
        }
        
        this.refreshKeyboardView();
    }

    // ===== CONTRÔLES DE VÉLOCITÉ =====

    /**
     * Définit la vélocité (force de frappe)
     */
    setVelocity(velocity) {
        this.velocity = Math.max(1, Math.min(127, velocity));
        this.logDebug('keyboard', `Vélocité: ${this.velocity}/127`);
        this.refreshKeyboardView();
    }

    /**
     * Ajuste la vélocité en fonction de la position de clic
     */
    adjustVelocity(event) {
        const rect = event.currentTarget.getBoundingClientRect();
        const x = event.clientX - rect.left;
        const percentage = x / rect.width;
        const velocity = Math.round(percentage * 127);
        this.setVelocity(velocity);
    }

    // ===== NAVIGATION OCTAVES =====

    /**
     * Change la plage de notes affichée
     */
    setKeyboardRange(startNote) {
        const range = 36; // 3 octaves
        this.keyboardRange = {
            start: Math.max(0, Math.min(108 - range, startNote)),
            end: Math.max(range, Math.min(127, startNote + range))
        };
        
        this.logDebug('keyboard', `Plage: ${this.getMidiNoteName(this.keyboardRange.start)} - ${this.getMidiNoteName(this.keyboardRange.end)}`);
        this.refreshKeyboardView();
    }

    /**
     * Décale les octaves
     */
    shiftOctave(direction) {
        const shift = direction * 12; // 1 octave = 12 demi-tons
        const newStart = this.keyboardRange.start + shift;
        
        if (newStart >= 0 && newStart + 36 <= 127) {
            this.setKeyboardRange(newStart);
            this.showNotification(`Octave ${direction > 0 ? '+' : ''}${direction}`, 'info');
        }
    }

    // ===== MODE HAUT-PARLEURS =====

    /**
     * Active/désactive le mode haut-parleurs
     */
    toggleSpeakerMode() {
        this.speakerMode = !this.speakerMode;
        this.logDebug('keyboard', `Mode haut-parleurs: ${this.speakerMode ? 'ON' : 'OFF'}`);
        this.showNotification(`Haut-parleurs ${this.speakerMode ? 'activés' : 'désactivés'}`, 'info');
        this.refreshKeyboardView();
    }

    // ===== CLAVIER PHYSIQUE =====

    /**
     * Gère les touches du clavier physique (keydown)
     */
    handlePhysicalKeyDown(event) {
        const key = event.key.toLowerCase();
        
        // Touches spéciales
        if (this.handleSpecialKeys(event)) {
            return;
        }
        
        // Touches de notes
        if (this.physicalKeyMap.hasOwnProperty(key) && !this.physicalKeysPressed.has(key)) {
            event.preventDefault();
            this.physicalKeysPressed.add(key);
            
            const relativeNote = this.physicalKeyMap[key];
            const midiNote = this.keyboardRange.start + Math.floor(relativeNote) + 
                            (relativeNote % 1) * 12; // Ajout pour touches noires
            
            if (midiNote <= this.keyboardRange.end) {
                const noteName = this.getMidiNoteName(midiNote);
                this.playNote(noteName, midiNote);
            }
        }
    }

    /**
     * Gère les touches du clavier physique (keyup)
     */
    handlePhysicalKeyUp(event) {
        const key = event.key.toLowerCase();
        
        // Shift = Sustain pedal
        if (event.code === 'ShiftLeft' || event.code === 'ShiftRight') {
            this.sustainPedal = false;
            this.logDebug('keyboard', 'Pédale sustain OFF');
            
            // Relâcher toutes les notes
            this.pressedKeys.forEach(midiNote => {
                const noteName = this.getMidiNoteName(midiNote);
                this.stopNote(noteName, midiNote);
            });
            return;
        }
        
        if (this.physicalKeyMap.hasOwnProperty(key) && this.physicalKeysPressed.has(key)) {
            event.preventDefault();
            this.physicalKeysPressed.delete(key);
            
            const relativeNote = this.physicalKeyMap[key];
            const midiNote = this.keyboardRange.start + Math.floor(relativeNote) + 
                            (relativeNote % 1) * 12;
            
            if (midiNote <= this.keyboardRange.end) {
                const noteName = this.getMidiNoteName(midiNote);
                this.stopNote(noteName, midiNote);
            }
        }
    }

    /**
     * Gère les touches spéciales (raccourcis)
     */
    handleSpecialKeys(event) {
        switch (event.code) {
            case 'ArrowLeft':
                event.preventDefault();
                this.shiftOctave(-1);
                return true;
                
            case 'ArrowRight':
                event.preventDefault();
                this.shiftOctave(1);
                return true;
                
            case 'ArrowUp':
                event.preventDefault();
                this.setVelocity(this.velocity + 10);
                return true;
                
            case 'ArrowDown':
                event.preventDefault();
                this.setVelocity(this.velocity - 10);
                return true;
                
            case 'Space':
                event.preventDefault();
                this.toggleSpeakerMode();
                return true;
                
            case 'ShiftLeft':
            case 'ShiftRight':
                this.sustainPedal = true;
                this.logDebug('keyboard', 'Pédale sustain ON');
                return true;
                
            default:
                return false;
        }
    }

    /**
     * Relâche toutes les touches pressées
     */
    releaseAllKeys() {
        this.pressedKeys.forEach(midiNote => {
            const noteName = this.getMidiNoteName(midiNote);
            this.stopNote(noteName, midiNote);
        });
        
        this.pressedKeys.clear();
        this.physicalKeysPressed.clear();
        this.sustainPedal = false;
        
        this.logDebug('keyboard', 'Toutes les notes relâchées');
    }

    // ===== INTERFACE =====

    /**
     * Construit les données pour la vue du clavier
     */
    buildKeyboardData() {
        const instrumentModel = this.getModel('instrument');
        const connectedInstruments = instrumentModel.getConnectedInstruments();
        
        return {
            velocity: this.velocity,
            selectedInstrument: this.selectedInstrument,
            selectedInstrumentDetails: this.selectedInstrument ? 
                instrumentModel.getInstrumentById(this.selectedInstrument) : null,
            connectedInstruments: connectedInstruments,
            keyboardView: this.keyboardRange,
            speakerMode: this.speakerMode,
            pressedKeys: Array.from(this.pressedKeys)
        };
    }
/**
 * Attache tous les événements du controller
 * ✅ VERSION COMPLÈTE - Phase 1 Étape 1.4
 */
bindEvents() {
    console.log('KeyboardController.bindEvents() - Configuration des événements...');
    
    // ========================================================================
    // ÉVÉNEMENTS DE LA VUE (KeyboardView) - NOUVEAUX
    // ========================================================================
    
    // 1. NOTES JOUÉES DEPUIS L'INTERFACE GRAPHIQUE
    this.eventBus.on('keyboard:note', (event) => {
        const { type, note, velocity } = event;
        const noteName = this.getMidiNoteName(note);
        
        if (type === 'noteOn') {
            this.playNote(noteName, note, velocity);
        } else if (type === 'noteOff') {
            this.stopNote(noteName, note);
        }
    });
    
    // 2. CHANGEMENT DE VÉLOCITÉ VIA LE SLIDER
    this.eventBus.on('keyboard:velocity-changed', ({ velocity }) => {
        this.setVelocity(velocity);
    });
    
    // 3. TOGGLE MODE HAUT-PARLEURS
    this.eventBus.on('keyboard:toggle-speaker', () => {
        this.toggleSpeakerMode();
    });
    
    // 4. SÉLECTION D'INSTRUMENT VIA LE DROPDOWN
    this.eventBus.on('keyboard:instrument-selected', ({ instrumentId }) => {
        this.selectInstrument(instrumentId);
    });
    
    // ========================================================================
    // ÉVÉNEMENTS DU SYSTÈME
    // ========================================================================
    
    // 5. DÉCONNEXION DU BACKEND
    this.eventBus.on('backend:disconnected', () => {
        this.handleBackendDisconnection();
    });
    
    // 6. DÉCONNEXION D'UN INSTRUMENT
    this.eventBus.on('instrument:disconnected', ({ id }) => {
        if (id === this.selectedInstrument) {
            this.showNotification(`⚠️ Instrument ${id} déconnecté`, 'warning');
            this.selectedInstrument = null;
            this.refreshKeyboardView();
        }
    });
    
    // 7. RECONNEXION DU BACKEND
    this.eventBus.on('backend:connected', () => {
        this.showNotification('✅ Backend MIDI reconnecté', 'success');
        this.refreshKeyboardView();
    });
    
    // 8. CHANGEMENT DE PAGE (cleanup si on quitte la page keyboard)
    this.eventBus.on('page:changed', ({ page }) => {
        if (page !== 'keyboard') {
            // Relâcher toutes les notes si on quitte la page
            this.releaseAllKeys();
        } else {
            // Rafraîchir la vue si on arrive sur la page
            this.refreshKeyboardView();
        }
    });
    
    console.log('✅ KeyboardController - Tous les événements configurés');
}

// ============================================================================
// NOUVELLES MÉTHODES POUR LA GESTION D'ERREURS - Phase 1 Étape 1.4
// ============================================================================

/**
 * Valide la sélection d'instrument avant d'envoyer des notes
 * @returns {boolean} true si l'instrument est valide et connecté
 */
validateInstrumentSelection() {
    if (!this.selectedInstrument) {
        this.showNotification('⚠️ Veuillez sélectionner un instrument', 'warning');
        return false;
    }
    
    const instrumentModel = this.getModel('instrument');
    const instrument = instrumentModel.getInstrumentById(this.selectedInstrument);
    
    if (!instrument || !instrument.connected) {
        this.showNotification('❌ Instrument non connecté ou introuvable', 'error');
        this.selectedInstrument = null;
        this.refreshKeyboardView();
        return false;
    }
    
    return true;
}

/**
 * Gère la déconnexion du backend MIDI
 */
handleBackendDisconnection() {
    this.showNotification('❌ Backend MIDI déconnecté - Clavier désactivé', 'error');
    
    // Relâcher toutes les notes actives
    this.releaseAllKeys();
    
    // Rafraîchir la vue pour afficher l'état "backend déconnecté"
    this.refreshKeyboardView();
    
    this.logDebug('error', 'Backend MIDI déconnecté - Clavier désactivé');
}

// ============================================================================
// MODIFICATION DE playNote() POUR VALIDATION - Phase 1 Étape 1.4
// ============================================================================

/**
 * Joue une note (via interface graphique)
 * @param {string} noteName - Nom de la note (ex: "C4")
 * @param {number} midiNote - Numéro MIDI de la note
 * @param {number} velocity - Vélocité optionnelle (sinon utilise this.velocity)
 */
playNote(noteName, midiNote, velocity = null) {
    // Éviter les doublons
    if (this.pressedKeys.has(midiNote)) {
        return;
    }
    
    // ✅ NOUVEAU: Validation avant envoi
    if (!this.validateInstrumentSelection()) {
        return;
    }
    
    // ✅ NOUVEAU: Vérification backend
    if (!this.backend || !this.backend.isConnected()) {
        this.handleBackendDisconnection();
        return;
    }
    
    // Utiliser la vélocité passée ou celle du controller
    const finalVelocity = velocity !== null ? velocity : this.velocity;
    
    this.pressedKeys.add(midiNote);
    this.updateKeyVisual(midiNote, true);
    
    // Envoi au backend C++
    const instrumentModel = this.getModel('instrument');
    const instrument = instrumentModel.getInstrumentById(this.selectedInstrument);
    const deviceName = instrument?.name || 'Unknown Device';
    
    this.backend.sendMidiMessage(
        deviceName,
        0x90,  // Note On
        midiNote,
        finalVelocity
    );
    
    this.logDebug('keyboard', `🎹 ${noteName} (${midiNote}) → ${deviceName} [v${finalVelocity}]`);
}

/**
 * Arrête une note (via interface graphique)
 * @param {string} noteName - Nom de la note
 * @param {number} midiNote - Numéro MIDI de la note
 */
stopNote(noteName, midiNote) {
    if (!this.pressedKeys.has(midiNote)) {
        return;
    }
    
    // Ne pas relâcher si la pédale de sustain est active
    if (this.sustainPedal) {
        this.logDebug('keyboard', `🎹 Note maintenue (sustain): ${noteName}`);
        return;
    }
    
    this.pressedKeys.delete(midiNote);
    this.updateKeyVisual(midiNote, false);
    
    // Envoi au backend C++
    if (this.backend && this.backend.isConnected()) {
        const instrumentModel = this.getModel('instrument');
        const instrument = instrumentModel.getInstrumentById(this.selectedInstrument);
        const deviceName = instrument?.name || 'Unknown Device';
        
        this.backend.sendMidiMessage(
            deviceName,
            0x80,  // Note Off
            midiNote,
            0
        );
        
        this.logDebug('keyboard', `🎹 OFF ${noteName} (${midiNote})`);
    }
}

// ============================================================================
// AMÉLIORATION DE refreshKeyboardView() - Phase 1 Étape 1.4
// ============================================================================

/**
 * Rafraîchit la vue du clavier
 * ✅ MODIFIÉ pour appeler attachEvents() après render
 */
refreshKeyboardView() {
    if (!this.isKeyboardPageActive()) {
        return;
    }
    
    const keyboardView = this.getView('keyboard');
    if (!keyboardView) {
        console.error('KeyboardController: KeyboardView introuvable');
        return;
    }
    
    // Construire les données
    const data = this.buildKeyboardData();
    
    // Ajouter info backend si disponible
    if (this.backend) {
        data.backendDisconnected = !this.backend.isConnected();
    }
    
    // Générer et injecter le HTML
    const container = document.getElementById('keyboard-page');
    if (container) {
        const html = keyboardView.buildTemplate(data);
        container.innerHTML = html;
        
        // ✅ NOUVEAU: Attacher les événements après le render
        keyboardView.attachEvents();
        
        // ✅ NOUVEAU: Appliquer le feedback visuel des touches pressées
        this.updatePressedKeysVisuals();
    }
}

/**
 * Met à jour les visuels des touches actuellement pressées
 * ✅ NOUVEAU - Phase 1 Étape 1.4
 */
updatePressedKeysVisuals() {
    this.pressedKeys.forEach(midiNote => {
        this.updateKeyVisual(midiNote, true);
    });
}

/**
 * Met à jour l'apparence visuelle d'une touche
 * ✅ AMÉLIORÉ pour gérer les erreurs gracieusement
 */
updateKeyVisual(midiNote, isPressed) {
    const keyElement = document.querySelector(`[data-midi="${midiNote}"]`);
    
    if (!keyElement) {
        // Ne pas logger si touche pas dans le layout actuel (normal)
        return;
    }
    
    if (keyElement.classList.contains('disabled')) {
        this.logDebug('warning', `Touche MIDI ${midiNote} est désactivée`);
        return;
    }
    
    if (isPressed) {
        keyElement.classList.add('active');
        keyElement.style.transform = 'scale(0.98)';
    } else {
        keyElement.classList.remove('active');
        keyElement.style.transform = 'scale(1)';
    }
}
    // ===== UTILITAIRES =====

    /**
     * Convertit un numéro MIDI en nom de note
     */
    getMidiNoteName(midiNote) {
        const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const noteIndex = midiNote % 12;
        const octave = Math.floor(midiNote / 12) - 1;
        return noteNames[noteIndex] + octave;
    }

    /**
     * Nettoie les ressources du contrôleur
     */
    destroy() {
        this.releaseAllKeys();
    }
}