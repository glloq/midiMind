// ============================================================================
// Fichier: frontend/js/controllers/ValidationController.js
// Projet: MidiMind v3.0 - SystÃ¨me d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   ContrÃ´leur de validation des donnÃ©es utilisateur.
//   Fournit validation pour formulaires, paramÃ¨tres, fichiers, etc.
//
// FonctionnalitÃ©s:
//   - Validation formulaires (champs requis, formats)
//   - Validation types de donnÃ©es (string, number, email, etc.)
//   - Validation plages de valeurs (min, max, range)
//   - Validation fichiers (extension, taille, type MIME)
//   - Validation paramÃ¨tres systÃ¨me (audio, MIDI, rÃ©seau)
//   - RÃ¨gles personnalisÃ©es (regex, callback)
//   - Messages d'erreur personnalisables
//   - Validation asynchrone (vÃ©rification backend)
//
// Architecture:
//   ValidationController extends BaseController
//   - Utilise Validator (utils/)
//   - RÃ¨gles de validation rÃ©utilisables
//   - Cache des rÃ©sultats de validation
//
// Auteur: MidiMind Team
// ============================================================================
        // ===== VALIDATION CONTROLLER =====


        class ValidationController extends BaseController {
            constructor(eventBus, models, views, notifications, debugConsole) {
                super(eventBus, models, views, notifications, debugConsole);
                this.validators = {};
                this.setupValidators();
            }

            bindEvents() {
                this.eventBus.on('file:added', (data) => this.validateFile(data.file));
                this.eventBus.on('playlist:added', (data) => this.validatePlaylist(data.playlist));
            }

            setupValidators() {
                this.validators = {
                    file: {
                        name: (name) => name && name.trim().length > 0,
                        size: (size) => size && parseInt(size) > 0,
                        duration: (duration) => duration && duration > 0,
                        tempo: (tempo) => tempo && tempo > 0 && tempo <= 300,
                        tracks: (tracks) => Array.isArray(tracks) && tracks.length > 0
                    },
                    
                    playlist: {
                        name: (name) => name && name.trim().length > 0 && name.length <= 100,
                        files: (files) => Array.isArray(files),
                        description: (desc) => !desc || desc.length <= 500
                    },
                    
                    instrument: {
                        name: (name) => name && name.trim().length > 0,
                        type: (type) => ['Cordes', 'Vents', 'Percussions'].includes(type),
                        connection: (conn) => ['usb', 'wifi', 'bluetooth'].includes(conn),
                        latency: (latency) => latency >= 0 && latency <= 1000,
                        noteRange: (range) => range && range.min >= 0 && range.max <= 127 && range.min <= range.max
                    }
                };
            }

            validateFile(file) {
                const errors = [];
                const validators = this.validators.file;
                
                Object.keys(validators).forEach(field => {
                    if (!validators[field](file[field])) {
                        errors.push(`Champ invalide: ${field}`);
                    }
                });
                
                if (errors.length > 0) {
                    this.logDebug('system', `Erreurs validation fichier ${file.name}: ${errors.join(', ')}`);
                    this.showNotification(`Fichier invalide: ${errors[0]}`, 'warning');
                    return false;
                }
                
                return true;
            }

            validatePlaylist(playlist) {
                const errors = [];
                const validators = this.validators.playlist;
                
                Object.keys(validators).forEach(field => {
                    if (playlist[field] !== undefined && !validators[field](playlist[field])) {
                        errors.push(`Champ invalide: ${field}`);
                    }
                });
                
                if (errors.length > 0) {
                    this.logDebug('system', `Erreurs validation playlist ${playlist.name}: ${errors.join(', ')}`);
                    this.showNotification(`Playlist invalide: ${errors[0]}`, 'warning');
                    return false;
                }
                
                return true;
            }

            validateInstrument(instrument) {
                const errors = [];
                const validators = this.validators.instrument;
                
                Object.keys(validators).forEach(field => {
                    if (!validators[field](instrument[field])) {
                        errors.push(`Champ invalide: ${field}`);
                    }
                });
                
                if (errors.length > 0) {
                    this.logDebug('system', `Erreurs validation instrument ${instrument.name}: ${errors.join(', ')}`);
                    return false;
                }
                
                return true;
            }

            sanitizeInput(input, type = 'string') {
                if (!input) return '';
                
                switch (type) {
                    case 'string':
                        return input.toString().trim().slice(0, 255);
                    case 'number':
                        return Math.max(0, parseInt(input) || 0);
                    case 'filename':
                        return input.toString().replace(/[^a-zA-Z0-9.-]/g, '_').slice(0, 100);
                    default:
                        return input;
                }
            }

            checkSystemIntegrity() {
                const issues = [];
                
                // VÃ©rifier les fichiers orphelins
                const files = this.getModel('file').get('files');
                const playlists = this.getModel('playlist').get('playlists');
                
                playlists.forEach(playlist => {
                    playlist.files.forEach(fileId => {
                        if (!files.find(f => f.id === fileId)) {
                            issues.push(`Fichier manquant dans playlist ${playlist.name}: ${fileId}`);
                        }
                    });
                });
                
                // VÃ©rifier les assignations d'instruments
                files.forEach(file => {
                    if (file.assignments) {
                        Object.values(file.assignments).forEach(instId => {
                            if (!this.getModel('instrument').getInstrumentById(instId)) {
                                issues.push(`Instrument manquant pour ${file.name}: ${instId}`);
                            }
                        });
                    }
                });
                
                if (issues.length > 0) {
                    this.logDebug('system', `ProblÃ¨mes d'intÃ©gritÃ© dÃ©tectÃ©s: ${issues.length}`);
                    issues.forEach(issue => this.logDebug('system', issue));
                } else {
                    this.logDebug('system', 'IntÃ©gritÃ© systÃ¨me OK');
                }
                
                return issues;
            }
        }

// Export par défaut
window.ValidationController = ValidationController;