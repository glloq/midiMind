// ============================================================================
// Fichier: frontend/js/controllers/LoggerController.js
// Version: v1.0.0
// Date: 2025-10-28
// ============================================================================
// Description:
//   Contrôleur pour gérer le niveau de logging du backend
//   - Configuration du niveau de log (DEBUG, INFO, WARNING, ERROR, CRITICAL)
//   - Synchronisation avec le Logger frontend
//   - Interface de sélection du niveau
// ============================================================================

class LoggerController extends BaseController {
    constructor(eventBus, backendService, logger = null, notifications = null, debugConsole = null) {
        super(eventBus, {}, {}, notifications, debugConsole);
        
        this.backendService = backendService;
        this.logger = logger || console;
        
        // État du logger
        this.state.logger = {
            backendLevel: 'INFO',
            frontendLevel: 'INFO',
            availableLevels: ['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL']
        };
        
        // Configuration
        this.config.syncWithFrontend = true;
    }
    
    /**
     * Initialisation personnalisée
     */
    onInitialize() {
        this.logDebug('info', 'LoggerController initializing...');
        
        // Charger le niveau actuel
        this.loadBackendLogLevel();
    }
    
    /**
     * Liaison des événements
     */
    bindEvents() {
        // Événements de l'interface
        this.subscribe('logger:set-level', (data) => this.setLogLevel(data.level));
        this.subscribe('logger:get-level', () => this.loadBackendLogLevel());
        this.subscribe('logger:sync-frontend', (data) => this.syncFrontendLevel(data.level));
        
        // Événements backend
        this.subscribe('backend:connected', () => {
            this.loadBackendLogLevel();
        });
    }
    
    /**
     * Charge le niveau de log actuel du backend
     */
    async loadBackendLogLevel() {
        return this.executeAction('loadBackendLogLevel', async () => {
            try {
                const response = await this.backendService.sendCommand('logger.getLevel', {});
                
                if (response.success) {
                    this.state.logger.backendLevel = response.data.level || 'INFO';
                    
                    this.logDebug('info', `Backend log level: ${this.state.logger.backendLevel}`);
                    
                    this.emitEvent('logger:level:loaded', {
                        level: this.state.logger.backendLevel
                    });
                    
                    // Synchroniser avec le frontend si configuré
                    if (this.config.syncWithFrontend) {
                        this.syncFrontendLevel(this.state.logger.backendLevel);
                    }
                }
                
                return response;
            } catch (error) {
                this.handleError('Erreur lors du chargement du niveau de log', error);
                throw error;
            }
        });
    }
    
    /**
     * Définit le niveau de log du backend
     */
    async setLogLevel(level) {
        return this.executeAction('setLogLevel', async (data) => {
            try {
                // Valider le niveau
                if (!this.state.logger.availableLevels.includes(data.level)) {
                    throw new Error(`Niveau de log invalide: ${data.level}`);
                }
                
                this.logDebug('info', `Setting backend log level to ${data.level}`);
                
                const response = await this.backendService.sendCommand('logger.setLevel', {
                    level: data.level
                });
                
                if (response.success) {
                    this.state.logger.backendLevel = data.level;
                    
                    this.showNotification(
                        `Niveau de log backend défini à ${data.level}`,
                        'success'
                    );
                    
                    this.emitEvent('logger:level:updated', {
                        level: data.level
                    });
                    
                    // Synchroniser avec le frontend si configuré
                    if (this.config.syncWithFrontend) {
                        this.syncFrontendLevel(data.level);
                    }
                }
                
                return response;
            } catch (error) {
                this.handleError(`Erreur lors de la définition du niveau de log à ${level}`, error);
                throw error;
            }
        }, { level });
    }
    
    /**
     * Synchronise le niveau de log avec le Logger frontend
     */
    syncFrontendLevel(level) {
        this.state.logger.frontendLevel = level;
        
        // Si un Logger frontend est disponible, le synchroniser
        if (this.logger && typeof this.logger.setLevel === 'function') {
            this.logger.setLevel(level);
            this.logDebug('info', `Frontend log level synchronized to ${level}`);
        }
        
        // Si window.Logger global existe
        if (typeof window !== 'undefined' && window.Logger && typeof window.Logger.setLevel === 'function') {
            window.Logger.setLevel(level);
            this.logDebug('info', `Global Logger level synchronized to ${level}`);
        }
        
        this.emitEvent('logger:frontend:synced', {
            level
        });
    }
    
    /**
     * Obtient les niveaux de log disponibles
     */
    getAvailableLevels() {
        return [...this.state.logger.availableLevels];
    }
    
    /**
     * Obtient le niveau de log actuel
     */
    getCurrentLevel() {
        return {
            backend: this.state.logger.backendLevel,
            frontend: this.state.logger.frontendLevel
        };
    }
    
    /**
     * Vérifie si un niveau de log est valide
     */
    isValidLevel(level) {
        return this.state.logger.availableLevels.includes(level);
    }
    
    /**
     * Obtient la priorité numérique d'un niveau
     */
    getLevelPriority(level) {
        const priorities = {
            'DEBUG': 0,
            'INFO': 1,
            'WARNING': 2,
            'ERROR': 3,
            'CRITICAL': 4
        };
        
        return priorities[level] || 1;
    }
    
    /**
     * Compare deux niveaux de log
     */
    compareLevels(level1, level2) {
        return this.getLevelPriority(level1) - this.getLevelPriority(level2);
    }
    
    /**
     * Active le mode debug (raccourci)
     */
    async enableDebugMode() {
        return this.setLogLevel('DEBUG');
    }
    
    /**
     * Désactive le mode debug (raccourci)
     */
    async disableDebugMode() {
        return this.setLogLevel('INFO');
    }
    
    /**
     * Afficher une notification
     */
    showNotification(message, type = 'info') {
        if (this.notifications && typeof this.notifications.show === 'function') {
            this.notifications.show(message, type);
        } else {
            this.logDebug(type, message);
        }
    }
    
    /**
     * Obtenir l'état actuel
     */
    getLoggerState() {
        return {
            ...this.state.logger,
            isDebugMode: this.state.logger.backendLevel === 'DEBUG'
        };
    }
    
    /**
     * Crée une interface UI simple pour le changement de niveau
     */
    createLevelSelectorUI(containerId) {
        const container = document.getElementById(containerId);
        if (!container) {
            this.logDebug('error', `Container ${containerId} not found`);
            return;
        }
        
        const html = `
            <div class="logger-level-selector">
                <label for="log-level-select">Niveau de Log Backend:</label>
                <select id="log-level-select" class="form-control">
                    ${this.state.logger.availableLevels.map(level => `
                        <option value="${level}" ${level === this.state.logger.backendLevel ? 'selected' : ''}>
                            ${level}
                        </option>
                    `).join('')}
                </select>
                <button id="apply-log-level" class="btn btn-primary">Appliquer</button>
            </div>
        `;
        
        container.innerHTML = html;
        
        // Attacher les événements
        const select = document.getElementById('log-level-select');
        const button = document.getElementById('apply-log-level');
        
        if (button) {
            button.addEventListener('click', () => {
                const level = select.value;
                this.setLogLevel(level);
            });
        }
        
        // Mettre à jour le sélecteur quand le niveau change
        this.subscribe('logger:level:updated', (data) => {
            if (select) {
                select.value = data.level;
            }
        });
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = LoggerController;
}

if (typeof window !== 'undefined') {
    window.LoggerController = LoggerController;
}