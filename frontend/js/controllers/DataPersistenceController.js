// ============================================================================
// Fichier: frontend/js/controllers/DataPersistenceController.js
// Projet: MidiMind v3.0 - SystÃ¨me d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   ContrÃ´leur de persistance des donnÃ©es de l'application.
//   GÃ¨re la sauvegarde/chargement automatique des sessions, presets,
//   playlists, et configurations utilisateur.
//
// FonctionnalitÃ©s:
//   - Sauvegarde automatique pÃ©riodique
//   - Gestion sessions (save/restore/clear)
//   - Presets sauvegardables (routing, filtres, settings)
//   - Export/Import donnÃ©es (JSON)
//   - Synchronisation avec backend
//   - Gestion versions de donnÃ©es
//   - Migration automatique anciennes versions
//   - Backup et restore
//
// Architecture:
//   DataPersistenceController extends BaseController
//   - Utilise StorageService (localStorage/IndexedDB)
//   - Versionning avec migration automatique
//   - Compression optionnelle (JSON)
//
// Auteur: MidiMind Team
// ============================================================================
        // ===== DATA PERSISTENCE CONTROLLER =====


        class DataPersistenceController extends BaseController {
            constructor(eventBus, models, views, notifications, debugConsole) {
                super(eventBus, models, views, notifications, debugConsole);
                this.storageKey = 'midiMind_data';
                this.autoSaveInterval = 30000; // 30 secondes
                
                this.setupAutoSave();
            }

            bindEvents() {
                // Sauvegarder lors des changements importants
                this.eventBus.on('file:added', () => this.saveData());
                this.eventBus.on('instrument:updated', () => this.saveData());
                this.eventBus.on('playlist:added', () => this.saveData());
                this.eventBus.on('playlist:removed', () => this.saveData());
            }

            saveData() {
                try {
                    const data = {
                        version: '2.0.0',
                        timestamp: new Date().toISOString(),
                        state: this.getModel('state').get(),
                        files: this.getModel('file').get(),
                        instruments: this.getModel('instrument').get(),
                        playlists: this.getModel('playlist').get()
                    };
                    
                    const compressedData = this.compressData(data);
                    localStorage.setItem(this.storageKey, compressedData);
                    
                    this.logDebug('system', 'DonnÃ©es sauvegardÃ©es automatiquement');
                } catch (error) {
                    this.logDebug('system', `Erreur sauvegarde: ${error.message}`);
                }
            }

            loadData() {
                try {
                    const savedData = localStorage.getItem(this.storageKey);
                    if (!savedData) return false;
                    
                    const data = this.decompressData(savedData);
                    
                    // Validation de version
                    if (data.version !== '2.0.0') {
                        this.logDebug('system', 'Version incompatible, donnÃ©es ignorÃ©es');
                        return false;
                    }
                    
                    // Restaurer les donnÃ©es
                    this.getModel('state').data = { ...this.getModel('state').data, ...data.state };
                    this.getModel('file').data = data.files;
                    this.getModel('instrument').data = data.instruments;
                    this.getModel('playlist').data = data.playlists;
                    
                    this.logDebug('system', `DonnÃ©es chargÃ©es (${this.formatDate(data.timestamp)})`);
                    this.showNotification('DonnÃ©es prÃ©cÃ©dentes restaurÃ©es', 'success');
                    
                    return true;
                } catch (error) {
                    this.logDebug('system', `Erreur chargement: ${error.message}`);
                    return false;
                }
            }

            exportData() {
                try {
                    const data = {
                        version: '2.0.0',
                        timestamp: new Date().toISOString(),
                        state: this.getModel('state').get(),
                        files: this.getModel('file').get(),
                        instruments: this.getModel('instrument').get(),
                        playlists: this.getModel('playlist').get()
                    };
                    
                    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    a.download = `midi-mind-backup-${new Date().toISOString().split('T')[0]}.json`;
                    a.click();
                    URL.revokeObjectURL(url);
                    
                    this.logDebug('system', 'DonnÃ©es exportÃ©es');
                    this.showNotification('Sauvegarde exportÃ©e avec succÃ¨s', 'success');
                } catch (error) {
                    this.logDebug('system', `Erreur export: ${error.message}`);
                    this.showNotification('Erreur lors de l\'export', 'error');
                }
            }

            importData(file) {
                try {
                    const reader = new FileReader();
                    reader.onload = (e) => {
                        try {
                            const data = JSON.parse(e.target.result);
                            
                            if (data.version !== '2.0.0') {
                                this.showNotification('Version de sauvegarde incompatible', 'error');
                                return;
                            }
                            
                            // Confirmer l'import
                            if (!confirm('Importer cette sauvegarde ? Cela remplacera les donnÃ©es actuelles.')) {
                                return;
                            }
                            
                            // Restaurer les donnÃ©es
                            this.getModel('state').data = { ...this.getModel('state').data, ...data.state };
                            this.getModel('file').data = data.files;
                            this.getModel('instrument').data = data.instruments;
                            this.getModel('playlist').data = data.playlists;
                            
                            // RafraÃ®chir l'interface
                            app.navigationController.refreshPageView(app.navigationController.getCurrentPage());
                            
                            this.logDebug('system', 'DonnÃ©es importÃ©es avec succÃ¨s');
                            this.showNotification('Sauvegarde importÃ©e avec succÃ¨s', 'success');
                        } catch (error) {
                            this.logDebug('system', `Erreur parsing import: ${error.message}`);
                            this.showNotification('Fichier de sauvegarde invalide', 'error');
                        }
                    };
                    reader.readAsText(file);
                } catch (error) {
                    this.logDebug('system', `Erreur import: ${error.message}`);
                    this.showNotification('Erreur lors de l\'import', 'error');
                }
            }

            clearData() {
                if (confirm('Supprimer toutes les donnÃ©es stockÃ©es ? Cette action est irrÃ©versible.')) {
                    localStorage.removeItem(this.storageKey);
                    this.logDebug('system', 'DonnÃ©es supprimÃ©es');
                    this.showNotification('DonnÃ©es supprimÃ©es', 'info');
                }
            }

            setupAutoSave() {
                setInterval(() => {
                    this.saveData();
                }, this.autoSaveInterval);
            }

            compressData(data) {
                // Compression simple - dans un vrai projet, utiliser LZ-string ou similaire
                return JSON.stringify(data);
            }

            decompressData(data) {
                return JSON.parse(data);
            }

            formatDate(isoString) {
                return new Date(isoString).toLocaleString('fr-FR');
            }
        }

// Export par défaut
window.DataPersistenceController = DataPersistenceController;