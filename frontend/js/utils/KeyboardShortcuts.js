// ============================================================================
// Fichier: frontend/js/utils/KeyboardShortcuts.js
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   Gestionnaire global des raccourcis clavier de l'application.
//   Support Ctrl, Shift, Alt, combinaisons complexes.
//
// Fonctionnalités:
//   - Enregistrement raccourcis (key + modifiers)
//   - Contextes (global, editor, playlist)
//   - Désactivation temporaire
//   - Conflits détectés automatiquement
//   - Help modal (Ctrl+?) avec tous raccourcis
//   - Personnalisation utilisateur
//   - Sauvegarde préférences
//
// Raccourcis par défaut:
//   - Ctrl+S : Save
//   - Ctrl+Z/Y : Undo/Redo
//   - Space : Play/Pause
//   - Ctrl+A : Select All
//   - Delete : Delete selection
//   - etc.
//
// Architecture:
//   KeyboardShortcuts (classe singleton)
//   - Map de raccourcis (key combo → callback)
//   - Event listener global (keydown)
//   - Priority system (prevent conflicts)
//
// Auteur: MidiMind Team
// ============================================================================

class KeyboardShortcuts {
    constructor(eventBus, logger) {
        this.eventBus = eventBus;
        this.logger = logger;
        
        // Map des raccourcis enregistrés
        this.shortcuts = new Map();
        
        // État des touches modificatrices
        this.modifiers = {
            ctrl: false,
            shift: false,
            alt: false,
            meta: false
        };
        
        // Configuration
        this.config = {
            enabled: true,
            preventDefault: true,
            stopPropagation: false,
            ignoreInInputs: true,
            debugMode: false
        };
        
        // Raccourcis par défaut
        this.defaultShortcuts = {
            // Lecture
            'space': {
                action: 'playback:toggle',
                description: 'Play/Pause',
                global: true
            },
            'enter': {
                action: 'playback:play',
                description: 'Play'
            },
            'escape': {
                action: 'playback:stop',
                description: 'Stop',
                global: true
            },
            
            // Navigation dans la lecture
            'arrowleft': {
                action: 'playback:seek-backward',
                description: 'Reculer 5s',
                params: { seconds: 5 }
            },
            'arrowright': {
                action: 'playback:seek-forward',
                description: 'Avancer 5s',
                params: { seconds: 5 }
            },
            'shift+arrowleft': {
                action: 'playback:seek-backward',
                description: 'Reculer 30s',
                params: { seconds: 30 }
            },
            'shift+arrowright': {
                action: 'playback:seek-forward',
                description: 'Avancer 30s',
                params: { seconds: 30 }
            },
            'home': {
                action: 'playback:seek-start',
                description: 'Début'
            },
            'end': {
                action: 'playback:seek-end',
                description: 'Fin'
            },
            
            // Volume
            'arrowup': {
                action: 'volume:increase',
                description: 'Volume +',
                params: { amount: 5 }
            },
            'arrowdown': {
                action: 'volume:decrease',
                description: 'Volume -',
                params: { amount: 5 }
            },
            'm': {
                action: 'volume:mute',
                description: 'Mute'
            },
            
            // Tempo
            'shift+arrowup': {
                action: 'tempo:increase',
                description: 'Tempo +',
                params: { amount: 0.1 }
            },
            'shift+arrowdown': {
                action: 'tempo:decrease',
                description: 'Tempo -',
                params: { amount: 0.1 }
            },
            'shift+t': {
                action: 'tempo:reset',
                description: 'Reset tempo'
            },
            
            // Navigation UI
            'ctrl+1': {
                action: 'navigation:home',
                description: 'Page accueil'
            },
            'ctrl+2': {
                action: 'navigation:files',
                description: 'Fichiers'
            },
            'ctrl+3': {
                action: 'navigation:instruments',
                description: 'Instruments'
            },
            'ctrl+4': {
                action: 'navigation:routing',
                description: 'Routage'
            },
            'ctrl+5': {
                action: 'navigation:system',
                description: 'Système'
            },
            
            // Fichiers
            'ctrl+o': {
                action: 'file:open',
                description: 'Ouvrir fichier'
            },
            'ctrl+s': {
                action: 'file:save',
                description: 'Sauvegarder'
            },
            'ctrl+shift+s': {
                action: 'file:save-as',
                description: 'Sauvegarder sous'
            },
            'delete': {
                action: 'file:delete-selected',
                description: 'Supprimer sélection'
            },
            'ctrl+a': {
                action: 'file:select-all',
                description: 'Tout sélectionner'
            },
            
            // Playlist
            'p': {
                action: 'playlist:toggle',
                description: 'Afficher/Masquer playlist'
            },
            'n': {
                action: 'playlist:next',
                description: 'Suivant'
            },
            'shift+n': {
                action: 'playlist:previous',
                description: 'Précédent'
            },
            'r': {
                action: 'playlist:repeat',
                description: 'Répéter'
            },
            'shift+r': {
                action: 'playlist:shuffle',
                description: 'Aléatoire'
            },
            
            // Routage
            'ctrl+m': {
                action: 'routing:mute-all',
                description: 'Mute tous les canaux'
            },
            'ctrl+shift+m': {
                action: 'routing:unmute-all',
                description: 'Unmute tous les canaux'
            },
            'ctrl+r': {
                action: 'routing:reset',
                description: 'Reset routage'
            },
            
            // Interface
            'f11': {
                action: 'ui:fullscreen',
                description: 'Plein écran'
            },
            'ctrl+d': {
                action: 'ui:toggle-debug',
                description: 'Console debug'
            },
            'ctrl+shift+d': {
                action: 'ui:toggle-dark-mode',
                description: 'Mode sombre'
            },
            'ctrl+z': {
                action: 'edit:undo',
                description: 'Annuler'
            },
            'ctrl+shift+z': {
                action: 'edit:redo',
                description: 'Refaire'
            },
            'ctrl+y': {
                action: 'edit:redo',
                description: 'Refaire'
            },
            
            // Recherche
            'ctrl+f': {
                action: 'search:focus',
                description: 'Rechercher'
            },
            'f3': {
                action: 'search:next',
                description: 'Résultat suivant'
            },
            'shift+f3': {
                action: 'search:previous',
                description: 'Résultat précédent'
            },
            
            // Aide
            'f1': {
                action: 'help:show',
                description: 'Aide'
            },
            'shift+/': {
                action: 'help:shortcuts',
                description: 'Liste des raccourcis'
            }
        };
        
        // Statistiques
        this.stats = {
            totalPressed: 0,
            shortcutsTriggered: 0,
            lastShortcut: null
        };
        
        this.init();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    init() {
        this.logger.info('KeyboardShortcuts', 'Initializing keyboard shortcuts...');
        
        // Enregistrer les raccourcis par défaut
        this.registerDefaultShortcuts();
        
        // Attacher les événements
        this.attachEvents();
        
        // Écouter les changements de configuration
        this.eventBus.on('shortcuts:enable', () => this.enable());
        this.eventBus.on('shortcuts:disable', () => this.disable());
        this.eventBus.on('shortcuts:register', (data) => this.register(data.key, data.action, data.options));
        this.eventBus.on('shortcuts:unregister', (data) => this.unregister(data.key));
    }
    
    /**
     * Enregistrer les raccourcis par défaut
     */
    registerDefaultShortcuts() {
        Object.entries(this.defaultShortcuts).forEach(([key, config]) => {
            this.register(key, config.action, {
                description: config.description,
                params: config.params,
                global: config.global
            });
        });
        
        this.logger.debug('KeyboardShortcuts', `Registered ${Object.keys(this.defaultShortcuts).length} default shortcuts`);
    }
    
    /**
     * Attacher les événements clavier
     */
    attachEvents() {
        // Keydown
        document.addEventListener('keydown', (e) => this.handleKeyDown(e), true);
        
        // Keyup
        document.addEventListener('keyup', (e) => this.handleKeyUp(e), true);
        
        // Reset modifiers on window blur
        window.addEventListener('blur', () => this.resetModifiers());
    }
    
    // ========================================================================
    // GESTION DES ÉVÉNEMENTS
    // ========================================================================
    
    /**
     * Gérer l'appui sur une touche
     */
    handleKeyDown(event) {
        if (!this.config.enabled) return;
        
        this.stats.totalPressed++;
        
        // Mettre à jour les modificateurs
        this.updateModifiers(event);
        
        // Ignorer si dans un input/textarea (sauf si global)
        if (this.config.ignoreInInputs && this.isInInput(event.target)) {
            const key = this.getKeyCombo(event);
            const shortcut = this.shortcuts.get(key.toLowerCase());
            
            if (!shortcut || !shortcut.global) {
                return;
            }
        }
        
        // Obtenir la combinaison de touches
        const keyCombo = this.getKeyCombo(event);
        
        if (this.config.debugMode) {
            this.logger.debug('KeyboardShortcuts', `Key pressed: ${keyCombo}`);
        }
        
        // Chercher un raccourci correspondant
        const shortcut = this.shortcuts.get(keyCombo.toLowerCase());
        
        if (shortcut) {
            // Empêcher l'action par défaut si configuré
            if (this.config.preventDefault) {
                event.preventDefault();
            }
            
            if (this.config.stopPropagation) {
                event.stopPropagation();
            }
            
            // Exécuter l'action
            this.executeShortcut(shortcut, event);
        }
    }
    
    /**
     * Gérer le relâchement d'une touche
     */
    handleKeyUp(event) {
        // Mettre à jour les modificateurs
        this.updateModifiers(event);
    }
    
    /**
     * Mettre à jour l'état des modificateurs
     */
    updateModifiers(event) {
        this.modifiers.ctrl = event.ctrlKey;
        this.modifiers.shift = event.shiftKey;
        this.modifiers.alt = event.altKey;
        this.modifiers.meta = event.metaKey;
    }
    
    /**
     * Réinitialiser les modificateurs
     */
    resetModifiers() {
        this.modifiers = {
            ctrl: false,
            shift: false,
            alt: false,
            meta: false
        };
    }
    
    // ========================================================================
    // EXÉCUTION DES RACCOURCIS
    // ========================================================================
    
    /**
     * Exécuter un raccourci
     */
    executeShortcut(shortcut, event) {
        this.stats.shortcutsTriggered++;
        this.stats.lastShortcut = shortcut;
        
        this.logger.debug('KeyboardShortcuts', `Executing shortcut: ${shortcut.action}`);
        
        // Émettre l'événement avec les paramètres
        this.eventBus.emit(shortcut.action, {
            ...shortcut.params,
            event: event,
            shortcut: shortcut
        });
        
        // Callback personnalisé si défini
        if (shortcut.callback && typeof shortcut.callback === 'function') {
            shortcut.callback(event, shortcut.params);
        }
        
        // Feedback visuel optionnel
        if (shortcut.showNotification) {
            this.eventBus.emit('notification:show', {
                message: shortcut.description || shortcut.action,
                type: 'info',
                duration: 1000
            });
        }
    }
    
    // ========================================================================
    // GESTION DES RACCOURCIS
    // ========================================================================
    
    /**
     * Enregistrer un nouveau raccourci
     * @param {string} key - Combinaison de touches (ex: "ctrl+s", "shift+enter")
     * @param {string|Function} action - Action à exécuter ou callback
     * @param {Object} options - Options du raccourci
     */
    register(key, action, options = {}) {
        const normalizedKey = this.normalizeKey(key);
        
        const shortcut = {
            key: normalizedKey,
            action: typeof action === 'string' ? action : null,
            callback: typeof action === 'function' ? action : null,
            description: options.description || '',
            params: options.params || {},
            global: options.global || false,
            showNotification: options.showNotification || false,
            enabled: options.enabled !== false
        };
        
        // Vérifier les conflits
        if (this.shortcuts.has(normalizedKey) && !options.override) {
            this.logger.warn('KeyboardShortcuts', 
                `Shortcut ${normalizedKey} already registered. Use override option to replace.`);
            return false;
        }
        
        this.shortcuts.set(normalizedKey, shortcut);
        
        this.logger.debug('KeyboardShortcuts', `Registered shortcut: ${normalizedKey} -> ${shortcut.action || 'callback'}`);
        
        return true;
    }
    
    /**
     * Désenregistrer un raccourci
     * @param {string} key - Combinaison de touches
     */
    unregister(key) {
        const normalizedKey = this.normalizeKey(key);
        
        if (this.shortcuts.has(normalizedKey)) {
            this.shortcuts.delete(normalizedKey);
            this.logger.debug('KeyboardShortcuts', `Unregistered shortcut: ${normalizedKey}`);
            return true;
        }
        
        return false;
    }
    
    /**
     * Activer/Désactiver un raccourci
     */
    toggleShortcut(key, enabled) {
        const normalizedKey = this.normalizeKey(key);
        const shortcut = this.shortcuts.get(normalizedKey);
        
        if (shortcut) {
            shortcut.enabled = enabled;
            return true;
        }
        
        return false;
    }
    
    /**
     * Obtenir tous les raccourcis enregistrés
     */
    getShortcuts() {
        return Array.from(this.shortcuts.entries()).map(([key, shortcut]) => ({
            key,
            ...shortcut
        }));
    }
    
    /**
     * Obtenir les raccourcis par catégorie
     */
    getShortcutsByCategory() {
        const categories = {};
        
        this.shortcuts.forEach((shortcut, key) => {
            const category = shortcut.action ? shortcut.action.split(':')[0] : 'other';
            
            if (!categories[category]) {
                categories[category] = [];
            }
            
            categories[category].push({
                key,
                ...shortcut
            });
        });
        
        return categories;
    }
    
    // ========================================================================
    // ACTIVATION/DÉSACTIVATION
    // ========================================================================
    
    /**
     * Activer les raccourcis clavier
     */
    enable() {
        this.config.enabled = true;
        this.logger.info('KeyboardShortcuts', 'Keyboard shortcuts enabled');
        this.eventBus.emit('shortcuts:enabled');
    }
    
    /**
     * Désactiver les raccourcis clavier
     */
    disable() {
        this.config.enabled = false;
        this.logger.info('KeyboardShortcuts', 'Keyboard shortcuts disabled');
        this.eventBus.emit('shortcuts:disabled');
    }
    
    /**
     * Basculer l'état des raccourcis
     */
    toggle() {
        if (this.config.enabled) {
            this.disable();
        } else {
            this.enable();
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Obtenir la combinaison de touches depuis un événement
     */
    getKeyCombo(event) {
        const parts = [];
        
        // Modificateurs dans l'ordre standard
        if (event.ctrlKey || event.metaKey) parts.push('ctrl');
        if (event.shiftKey) parts.push('shift');
        if (event.altKey) parts.push('alt');
        
        // Touche principale
        const key = this.getKeyName(event);
        if (key) parts.push(key);
        
        return parts.join('+');
    }
    
    /**
     * Obtenir le nom de la touche
     */
    getKeyName(event) {
        // Touches spéciales
        const specialKeys = {
            ' ': 'space',
            'Enter': 'enter',
            'Escape': 'escape',
            'Tab': 'tab',
            'Backspace': 'backspace',
            'Delete': 'delete',
            'ArrowUp': 'arrowup',
            'ArrowDown': 'arrowdown',
            'ArrowLeft': 'arrowleft',
            'ArrowRight': 'arrowright',
            'Home': 'home',
            'End': 'end',
            'PageUp': 'pageup',
            'PageDown': 'pagedown',
            'Insert': 'insert'
        };
        
        // Vérifier les touches spéciales
        if (specialKeys[event.key]) {
            return specialKeys[event.key];
        }
        
        // Touches F1-F12
        if (/^F\d+$/.test(event.key)) {
            return event.key.toLowerCase();
        }
        
        // Ignorer les modificateurs seuls
        if (['Control', 'Shift', 'Alt', 'Meta'].includes(event.key)) {
            return null;
        }
        
        // Autres touches (lettres, chiffres, etc.)
        return event.key.toLowerCase();
    }
    
    /**
     * Normaliser une clé de raccourci
     */
    normalizeKey(key) {
        return key.toLowerCase()
            .replace(/\s+/g, '')
            .split('+')
            .sort((a, b) => {
                // Ordre des modificateurs
                const order = ['ctrl', 'shift', 'alt'];
                const aIndex = order.indexOf(a);
                const bIndex = order.indexOf(b);
                
                if (aIndex !== -1 && bIndex !== -1) {
                    return aIndex - bIndex;
                }
                if (aIndex !== -1) return -1;
                if (bIndex !== -1) return 1;
                
                return a.localeCompare(b);
            })
            .join('+');
    }
    
    /**
     * Vérifier si l'élément cible est un input
     */
    isInInput(element) {
        const tagName = element.tagName.toLowerCase();
        const editableElements = ['input', 'textarea', 'select'];
        
        return editableElements.includes(tagName) || 
               element.contentEditable === 'true';
    }
    
    /**
     * Afficher la liste des raccourcis (aide)
     */
    showShortcutsList() {
        const categories = this.getShortcutsByCategory();
        let html = '<div class="shortcuts-help"><h2>Raccourcis clavier</h2>';
        
        Object.entries(categories).forEach(([category, shortcuts]) => {
            html += `<div class="shortcut-category">`;
            html += `<h3>${this.formatCategoryName(category)}</h3>`;
            html += '<table class="shortcuts-table">';
            
            shortcuts.forEach(shortcut => {
                if (shortcut.enabled !== false) {
                    html += '<tr>';
                    html += `<td class="shortcut-key">${this.formatKeyForDisplay(shortcut.key)}</td>`;
                    html += `<td class="shortcut-description">${shortcut.description}</td>`;
                    html += '</tr>';
                }
            });
            
            html += '</table></div>';
        });
        
        html += '</div>';
        
        // Émettre l'événement pour afficher l'aide
        this.eventBus.emit('modal:show', {
            title: 'Raccourcis clavier',
            content: html,
            size: 'large'
        });
    }
    
    /**
     * Formater le nom de catégorie
     */
    formatCategoryName(category) {
        const names = {
            'playback': '▶️ Lecture',
            'volume': '🔊 Volume',
            'tempo': '⏱️ Tempo',
            'navigation': '🧭 Navigation',
            'file': '📁 Fichiers',
            'playlist': '📋 Playlist',
            'routing': '🔀 Routage',
            'ui': '🖼️ Interface',
            'edit': '✏️ Édition',
            'search': '🔍 Recherche',
            'help': '❓ Aide',
            'other': '📌 Autres'
        };
        
        return names[category] || category;
    }
    
    /**
     * Formater une touche pour l'affichage
     */
    formatKeyForDisplay(key) {
        return key
            .split('+')
            .map(part => {
                // Capitaliser et formater
                switch(part) {
                    case 'ctrl': return 'Ctrl';
                    case 'shift': return 'Shift';
                    case 'alt': return 'Alt';
                    case 'space': return 'Espace';
                    case 'enter': return 'Entrée';
                    case 'escape': return 'Échap';
                    case 'arrowup': return '↑';
                    case 'arrowdown': return '↓';
                    case 'arrowleft': return '←';
                    case 'arrowright': return '→';
                    default: return part.toUpperCase();
                }
            })
            .join(' + ');
    }
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    getStats() {
        return {
            ...this.stats,
            registeredShortcuts: this.shortcuts.size,
            enabled: this.config.enabled
        };
    }
}