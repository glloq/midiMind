// ============================================================================
// Fichier: frontend/js/views/LoggerView.js
// Version: v3.1.0
// Date: 2025-10-28
// ============================================================================
// Description:
//   Vue pour la configuration des niveaux de log
//   Permet à l'utilisateur de sélectionner le niveau de détail des logs
//
// Responsabilités:
//   - Afficher les niveaux de log disponibles
//   - Afficher le niveau actuel
//   - Interface de sélection
//   - Descriptions des niveaux
//
// Auteur: MidiMind Team
// ============================================================================

class LoggerView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        // État de la vue
        this.state.currentLevel = 'INFO';
        this.state.levels = [
            {
                name: 'DEBUG',
                description: 'Tous les messages de débogage (très verbeux)',
                icon: '🔍',
                color: '#9E9E9E'
            },
            {
                name: 'INFO',
                description: 'Informations générales sur le fonctionnement',
                icon: 'ℹ️',
                color: '#2196F3'
            },
            {
                name: 'WARNING',
                description: 'Avertissements de problèmes potentiels',
                icon: '⚠️',
                color: '#FF9800'
            },
            {
                name: 'ERROR',
                description: 'Erreurs qui nécessitent attention',
                icon: '❌',
                color: '#F44336'
            },
            {
                name: 'CRITICAL',
                description: 'Erreurs critiques du système uniquement',
                icon: '🔥',
                color: '#D32F2F'
            }
        ];
        
        this.logger = window.logger || console;
        this.logger.info('LoggerView', '✓ Vue initialisée');
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    initialize() {
        super.initialize();
        this.render();
        this.attachEventListeners();
    }
    
    // ========================================================================
    // RENDU
    // ========================================================================
    
    render() {
        if (!this.container) {
            this.logger.error('LoggerView', 'Container not found');
            return;
        }
        
        this.container.innerHTML = `
            <div class="logger-content api-page-content">
                ${this.renderCurrentLevel()}
                ${this.renderLevelSelector()}
                ${this.renderApplyButton()}
            </div>
        `;
        
        this.state.isRendered = true;
    }
    
    renderCurrentLevel() {
        const level = this.state.levels.find(l => l.name === this.state.currentLevel);
        
        return `
            <div class="api-section">
                <div class="api-section-title">
                    📊 Niveau Actuel
                </div>
                <div class="logger-current-level">
                    <span class="level-icon" style="font-size: 1.5rem">${level.icon}</span>
                    <div>
                        <div class="level-name" style="font-weight: 600; font-size: 1.2rem; color: ${level.color}">
                            ${level.name}
                        </div>
                        <div class="level-description" style="color: var(--text-secondary, #666); font-size: 0.9rem">
                            ${level.description}
                        </div>
                    </div>
                </div>
            </div>
        `;
    }
    
    renderLevelSelector() {
        return `
            <div class="api-section">
                <div class="api-section-title">
                    🎚️ Sélectionner un Niveau
                </div>
                <div class="logger-levels">
                    ${this.state.levels.map(level => this.renderLevelOption(level)).join('')}
                </div>
            </div>
        `;
    }
    
    renderLevelOption(level) {
        const isActive = level.name === this.state.currentLevel;
        
        return `
            <div class="logger-level-option ${isActive ? 'active' : ''}" 
                 data-level="${level.name}"
                 style="border-color: ${isActive ? level.color : 'var(--border-color, #e0e0e0)'}">
                <div>
                    <div class="logger-level-header" style="display: flex; align-items: center; gap: 0.5rem; margin-bottom: 0.25rem">
                        <span style="font-size: 1.25rem">${level.icon}</span>
                        <span class="logger-level-name" style="color: ${level.color}">${level.name}</span>
                        ${isActive ? '<span style="color: ' + level.color + '; font-size: 0.875rem">✓ Actif</span>' : ''}
                    </div>
                    <div class="logger-level-description">
                        ${level.description}
                    </div>
                </div>
            </div>
        `;
    }
    
    renderApplyButton() {
        return `
            <div class="api-section" style="text-align: center">
                <button id="apply-logger-level" class="api-button">
                    💾 Appliquer le Niveau
                </button>
            </div>
        `;
    }
    
    // ========================================================================
    // ÉVÉNEMENTS
    // ========================================================================
    
    attachEventListeners() {
        if (!this.container) return;
        
        // Sélection d'un niveau
        const levelOptions = this.container.querySelectorAll('.logger-level-option');
        levelOptions.forEach(option => {
            option.addEventListener('click', () => {
                const level = option.dataset.level;
                this.selectLevel(level);
            });
        });
        
        // Bouton appliquer
        const applyButton = this.container.querySelector('#apply-logger-level');
        if (applyButton) {
            applyButton.addEventListener('click', () => {
                this.applyLevel();
            });
        }
    }
    
    selectLevel(levelName) {
        this.state.currentLevel = levelName;
        this.render();
        this.attachEventListeners();
        
        this.logger.info('LoggerView', `Niveau sélectionné: ${levelName}`);
    }
    
    applyLevel() {
        this.eventBus.emit('logger:set-level-request', {
            level: this.state.currentLevel
        });
        
        this.logger.info('LoggerView', `Demande d'application du niveau: ${this.state.currentLevel}`);
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES
    // ========================================================================
    
    /**
     * Mettre à jour le niveau actuel
     */
    updateCurrentLevel(level) {
        if (this.state.levels.find(l => l.name === level)) {
            this.state.currentLevel = level;
            this.render();
            this.attachEventListeners();
            
            this.logger.info('LoggerView', `Niveau mis à jour: ${level}`);
        }
    }
    
    /**
     * Afficher une notification de succès
     */
    showSuccess(message) {
        // Notification temporaire
        const notification = document.createElement('div');
        notification.className = 'logger-notification success';
        notification.textContent = message;
        notification.style.cssText = `
            position: fixed;
            top: 80px;
            right: 20px;
            background: #4CAF50;
            color: white;
            padding: 1rem 1.5rem;
            border-radius: 6px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.2);
            z-index: 10000;
            animation: slideIn 0.3s ease;
        `;
        
        document.body.appendChild(notification);
        
        setTimeout(() => {
            notification.remove();
        }, 3000);
    }
    
    /**
     * Afficher une notification d'erreur
     */
    showError(message) {
        const notification = document.createElement('div');
        notification.className = 'logger-notification error';
        notification.textContent = message;
        notification.style.cssText = `
            position: fixed;
            top: 80px;
            right: 20px;
            background: #F44336;
            color: white;
            padding: 1rem 1.5rem;
            border-radius: 6px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.2);
            z-index: 10000;
            animation: slideIn 0.3s ease;
        `;
        
        document.body.appendChild(notification);
        
        setTimeout(() => {
            notification.remove();
        }, 3000);
    }
    
    // ========================================================================
    // CYCLE DE VIE
    // ========================================================================
    
    show() {
        if (this.container) {
            this.container.style.display = 'block';
            this.state.isVisible = true;
            
            // Demander le niveau actuel au contrôleur
            this.eventBus.emit('logger:get-level-request');
            
            this.logger.info('LoggerView', 'Vue affichée');
        }
    }
    
    hide() {
        if (this.container) {
            this.container.style.display = 'none';
            this.state.isVisible = false;
            
            this.logger.info('LoggerView', 'Vue masquée');
        }
    }
    
    destroy() {
        // Nettoyer les événements
        this.state.isDestroyed = true;
        this.logger.info('LoggerView', 'Vue détruite');
    }
}

// Rendre la classe disponible globalement
if (typeof window !== 'undefined') {
    window.LoggerView = LoggerView;
}