// ============================================================================
// Fichier: frontend/scripts/views/components/DeviceCard.js
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Composant carte pour afficher un instrument/device MIDI.
//   Affiche l'état, les contrôles et les statistiques.
//
// Auteur: midiMind Team
// Date: 2025-10-04
// Version: 3.0.0
// ============================================================================

class DeviceCard {
    constructor(container, device, config = {}) {
        this.container = typeof container === 'string' ? 
            document.getElementById(container) : container;
        
        // Données du device
        this.device = device;
        
        // Configuration
        this.config = {
            showStats: true,
            showControls: true,
            showChannels: true,
            interactive: true,
            animated: true,
            compact: false,
            onSelect: null,
            onTest: null,
            onConfigure: null,
            ...config
        };
        
        // État
        this.state = {
            selected: false,
            expanded: false,
            testing: false
        };
        
        // ID unique
        this.id = `device-card-${device.id || Date.now()}`;
        
        // Timer pour animation d'activité
        this.activityTimer = null;
        
        this.init();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    init() {
        if (!this.container) {
            console.error('DeviceCard: Container not found');
            return;
        }
        
        this.render();
        this.attachEvents();
        
        // Animation d'entrée si activée
        if (this.config.animated) {
            this.animateIn();
        }
    }
    
    // ========================================================================
    // RENDU
    // ========================================================================
    
    render() {
        const html = this.config.compact ? 
            this.buildCompactTemplate() : 
            this.buildFullTemplate();
        
        this.container.innerHTML = html;
        this.applyStyles();
    }
    
    /**
     * Template complet de la carte
     */
    buildFullTemplate() {
        const statusClass = this.getStatusClass();
        const statusIcon = this.getStatusIcon();
        const typeIcon = this.getTypeIcon();
        
        return `
            <div class="device-card ${statusClass} ${this.state.selected ? 'selected' : ''}" 
                 id="${this.id}"
                 data-device-id="${this.device.id}">
                
                <!-- En-tête -->
                <div class="device-header">
                    <div class="device-icon">
                        ${typeIcon}
                    </div>
                    <div class="device-info">
                        <h3 class="device-name">${this.device.name || 'Unknown Device'}</h3>
                        <div class="device-meta">
                            <span class="device-type">${this.device.type || 'MIDI'}</span>
                            <span class="device-status">
                                ${statusIcon} ${this.device.status || 'unknown'}
                            </span>
                        </div>
                    </div>
                    ${this.config.interactive ? `
                        <button class="device-expand-btn" aria-label="Expand">
                            ${this.state.expanded ? '▼' : '▶'}
                        </button>
                    ` : ''}
                </div>
                
                <!-- Indicateur d'activité -->
                <div class="device-activity">
                    <div class="activity-bar">
                        <div class="activity-fill" style="width: 0%"></div>
                    </div>
                    <span class="activity-label">Activité MIDI</span>
                </div>
                
                <!-- Statistiques -->
                ${this.config.showStats ? this.buildStats() : ''}
                
                <!-- Canaux assignés -->
                ${this.config.showChannels ? this.buildChannels() : ''}
                
                <!-- Contrôles -->
                ${this.config.showControls ? this.buildControls() : ''}
                
                <!-- Détails expansés -->
                <div class="device-details" style="display: ${this.state.expanded ? 'block' : 'none'}">
                    ${this.buildDetails()}
                </div>
            </div>
        `;
    }
    
    /**
     * Template compact de la carte
     */
    buildCompactTemplate() {
        const statusClass = this.getStatusClass();
        const typeIcon = this.getTypeIcon();
        
        return `
            <div class="device-card-compact ${statusClass} ${this.state.selected ? 'selected' : ''}" 
                 id="${this.id}"
                 data-device-id="${this.device.id}">
                
                <div class="device-icon-compact">${typeIcon}</div>
                <div class="device-name-compact">${this.device.name}</div>
                <div class="device-status-compact">
                    <span class="status-indicator ${statusClass}"></span>
                </div>
                
                ${this.config.showControls ? `
                    <div class="device-controls-compact">
                        <button class="btn-test-compact" title="Test">♪</button>
                    </div>
                ` : ''}
            </div>
        `;
    }
    
    /**
     * Construire les statistiques
     */
    buildStats() {
        const stats = this.device.stats || {};
        
        return `
            <div class="device-stats">
                <div class="stat-item">
                    <span class="stat-label">Messages:</span>
                    <span class="stat-value">${stats.messagesCount || 0}</span>
                </div>
                <div class="stat-item">
                    <span class="stat-label">Notes:</span>
                    <span class="stat-value">${stats.notesCount || 0}</span>
                </div>
                <div class="stat-item">
                    <span class="stat-label">Latence:</span>
                    <span class="stat-value">${stats.latency || 0}ms</span>
                </div>
            </div>
        `;
    }
    
    /**
     * Construire la liste des canaux
     */
    buildChannels() {
        const channels = this.device.channels || [];
        
        if (channels.length === 0) {
            return `
                <div class="device-channels">
                    <span class="no-channels">Aucun canal assigné</span>
                </div>
            `;
        }
        
        return `
            <div class="device-channels">
                <span class="channels-label">Canaux:</span>
                <div class="channels-list">
                    ${channels.map(ch => `
                        <span class="channel-badge" style="background: ${this.getChannelColor(ch)}">
                            ${ch}
                        </span>
                    `).join('')}
                </div>
            </div>
        `;
    }
    
    /**
     * Construire les contrôles
     */
    buildControls() {
        return `
            <div class="device-controls">
                <button class="btn btn-primary btn-test" 
                        ${this.device.status !== 'connected' ? 'disabled' : ''}>
                    ♪ Test
                </button>
                <button class="btn btn-secondary btn-config">
                    ⚙️ Configurer
                </button>
                ${this.device.status === 'disconnected' ? `
                    <button class="btn btn-success btn-connect">
                        🔌 Connecter
                    </button>
                ` : ''}
            </div>
        `;
    }
    
    /**
     * Construire les détails expansés
     */
    buildDetails() {
        const info = this.device.info || {};
        
        return `
            <div class="device-details-content">
                <h4>Informations détaillées</h4>
                
                <dl class="details-list">
                    <dt>ID:</dt>
                    <dd>${this.device.id}</dd>
                    
                    <dt>Type:</dt>
                    <dd>${this.device.type}</dd>
                    
                    <dt>Port:</dt>
                    <dd>${info.port || 'N/A'}</dd>
                    
                    <dt>Fabricant:</dt>
                    <dd>${info.manufacturer || 'Unknown'}</dd>
                    
                    <dt>Modèle:</dt>
                    <dd>${info.model || 'Unknown'}</dd>
                    
                    <dt>Version:</dt>
                    <dd>${info.version || 'N/A'}</dd>
                    
                    <dt>Capacités:</dt>
                    <dd>${this.formatCapabilities(info.capabilities)}</dd>
                </dl>
                
                ${info.lastError ? `
                    <div class="error-info">
                        <strong>Dernière erreur:</strong>
                        <p>${info.lastError}</p>
                    </div>
                ` : ''}
            </div>
        `;
    }
    
    /**
     * Appliquer les styles CSS
     */
    applyStyles() {
        if (document.getElementById('device-card-styles')) return;
        
        const style = document.createElement('style');
        style.id = 'device-card-styles';
        style.textContent = `
            .device-card {
                background: linear-gradient(145deg, #2a2a3e, #1a1a2e);
                border-radius: 12px;
                padding: 16px;
                margin-bottom: 12px;
                box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
                transition: all 0.3s ease;
                opacity: 0;
                transform: translateY(20px);
            }
            
            .device-card.show {
                opacity: 1;
                transform: translateY(0);
            }
            
            .device-card:hover {
                box-shadow: 0 6px 12px rgba(0, 0, 0, 0.2);
                transform: translateY(-2px);
            }
            
            .device-card.selected {
                border: 2px solid #667eea;
                box-shadow: 0 0 20px rgba(102, 126, 234, 0.3);
            }
            
            .device-card.connected {
                border-left: 4px solid #4ade80;
            }
            
            .device-card.disconnected {
                border-left: 4px solid #ef4444;
                opacity: 0.7;
            }
            
            .device-card.connecting {
                border-left: 4px solid #fbbf24;
            }
            
            .device-header {
                display: flex;
                align-items: center;
                gap: 12px;
                margin-bottom: 12px;
            }
            
            .device-icon {
                width: 48px;
                height: 48px;
                display: flex;
                align-items: center;
                justify-content: center;
                background: rgba(255, 255, 255, 0.05);
                border-radius: 12px;
                font-size: 24px;
            }
            
            .device-info {
                flex: 1;
            }
            
            .device-name {
                margin: 0;
                font-size: 18px;
                font-weight: 600;
                color: #fff;
            }
            
            .device-meta {
                display: flex;
                gap: 12px;
                margin-top: 4px;
                font-size: 12px;
                color: rgba(255, 255, 255, 0.6);
            }
            
            .device-type {
                padding: 2px 8px;
                background: rgba(255, 255, 255, 0.1);
                border-radius: 4px;
            }
            
            .device-expand-btn {
                background: none;
                border: none;
                color: rgba(255, 255, 255, 0.6);
                cursor: pointer;
                font-size: 16px;
                padding: 8px;
                transition: transform 0.2s ease;
            }
            
            .device-expand-btn:hover {
                color: #fff;
            }
            
            /* Activité */
            .device-activity {
                margin: 12px 0;
            }
            
            .activity-bar {
                height: 4px;
                background: rgba(255, 255, 255, 0.1);
                border-radius: 2px;
                overflow: hidden;
            }
            
            .activity-fill {
                height: 100%;
                background: linear-gradient(90deg, #667eea, #764ba2);
                transition: width 0.3s ease;
            }
            
            .activity-label {
                font-size: 11px;
                color: rgba(255, 255, 255, 0.5);
                margin-top: 4px;
                display: block;
            }
            
            /* Stats */
            .device-stats {
                display: grid;
                grid-template-columns: repeat(3, 1fr);
                gap: 12px;
                margin: 12px 0;
                padding: 12px;
                background: rgba(255, 255, 255, 0.03);
                border-radius: 8px;
            }
            
            .stat-item {
                text-align: center;
            }
            
            .stat-label {
                display: block;
                font-size: 11px;
                color: rgba(255, 255, 255, 0.5);
                margin-bottom: 4px;
            }
            
            .stat-value {
                font-size: 16px;
                font-weight: 600;
                color: #fff;
            }
            
            /* Canaux */
            .device-channels {
                margin: 12px 0;
            }
            
            .channels-label {
                font-size: 12px;
                color: rgba(255, 255, 255, 0.6);
                margin-right: 8px;
            }
            
            .channels-list {
                display: inline-flex;
                gap: 4px;
                flex-wrap: wrap;
            }
            
            .channel-badge {
                padding: 2px 8px;
                border-radius: 12px;
                font-size: 11px;
                font-weight: 600;
                color: #fff;
            }
            
            .no-channels {
                font-size: 12px;
                color: rgba(255, 255, 255, 0.4);
                font-style: italic;
            }
            
            /* Contrôles */
            .device-controls {
                display: flex;
                gap: 8px;
                margin-top: 12px;
            }
            
            .device-controls .btn {
                flex: 1;
                padding: 8px 12px;
                border-radius: 6px;
                border: none;
                font-size: 12px;
                font-weight: 500;
                cursor: pointer;
                transition: all 0.2s ease;
            }
            
            .btn-test {
                background: linear-gradient(135deg, #667eea, #764ba2);
                color: white;
            }
            
            .btn-config {
                background: rgba(255, 255, 255, 0.1);
                color: rgba(255, 255, 255, 0.8);
            }
            
            .btn-connect {
                background: linear-gradient(135deg, #4ade80, #22c55e);
                color: white;
            }
            
            .btn:hover:not(:disabled) {
                transform: translateY(-1px);
                box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2);
            }
            
            .btn:disabled {
                opacity: 0.5;
                cursor: not-allowed;
            }
            
            /* Détails */
            .device-details {
                margin-top: 16px;
                padding-top: 16px;
                border-top: 1px solid rgba(255, 255, 255, 0.1);
            }
            
            .device-details-content h4 {
                margin: 0 0 12px 0;
                font-size: 14px;
                color: rgba(255, 255, 255, 0.8);
            }
            
            .details-list {
                display: grid;
                grid-template-columns: auto 1fr;
                gap: 8px 16px;
                font-size: 12px;
            }
            
            .details-list dt {
                color: rgba(255, 255, 255, 0.5);
                font-weight: 500;
            }
            
            .details-list dd {
                margin: 0;
                color: rgba(255, 255, 255, 0.8);
            }
            
            .error-info {
                margin-top: 12px;
                padding: 8px;
                background: rgba(239, 68, 68, 0.1);
                border: 1px solid rgba(239, 68, 68, 0.3);
                border-radius: 6px;
                font-size: 12px;
                color: #ef4444;
            }
            
            /* Mode compact */
            .device-card-compact {
                display: flex;
                align-items: center;
                gap: 12px;
                padding: 12px;
                background: rgba(255, 255, 255, 0.03);
                border-radius: 8px;
                transition: all 0.2s ease;
            }
            
            .device-card-compact:hover {
                background: rgba(255, 255, 255, 0.05);
            }
            
            .device-card-compact.selected {
                background: rgba(102, 126, 234, 0.1);
                border: 1px solid #667eea;
            }
            
            .device-icon-compact {
                font-size: 20px;
            }
            
            .device-name-compact {
                flex: 1;
                font-size: 14px;
                font-weight: 500;
            }
            
            .status-indicator {
                width: 8px;
                height: 8px;
                border-radius: 50%;
                background: #6b7280;
            }
            
            .status-indicator.connected {
                background: #4ade80;
                animation: pulse 2s infinite;
            }
            
            .status-indicator.disconnected {
                background: #ef4444;
            }
            
            .btn-test-compact {
                padding: 4px 8px;
                background: rgba(255, 255, 255, 0.1);
                border: none;
                border-radius: 4px;
                color: rgba(255, 255, 255, 0.6);
                cursor: pointer;
                font-size: 14px;
            }
            
            .btn-test-compact:hover {
                background: rgba(255, 255, 255, 0.2);
                color: #fff;
            }
            
            @keyframes pulse {
                0% { opacity: 1; }
                50% { opacity: 0.5; }
                100% { opacity: 1; }
            }
        `;
        
        document.head.appendChild(style);
    }
    
    // ========================================================================
    // ÉVÉNEMENTS
    // ========================================================================
    
    attachEvents() {
        const card = document.getElementById(this.id);
        if (!card) return;
        
        // Click sur la carte
        if (this.config.interactive) {
            card.addEventListener('click', (e) => {
                if (!e.target.closest('button')) {
                    this.toggleSelection();
                }
            });
        }
        
        // Bouton expand
        const expandBtn = card.querySelector('.device-expand-btn');
        if (expandBtn) {
            expandBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                this.toggleExpanded();
            });
        }
        
        // Bouton test
        const testBtn = card.querySelector('.btn-test, .btn-test-compact');
        if (testBtn) {
            testBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                this.testDevice();
            });
        }
        
        // Bouton config
        const configBtn = card.querySelector('.btn-config');
        if (configBtn) {
            configBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                this.configureDevice();
            });
        }
        
        // Bouton connect
        const connectBtn = card.querySelector('.btn-connect');
        if (connectBtn) {
            connectBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                this.connectDevice();
            });
        }
    }
    
    // ========================================================================
    // ACTIONS
    // ========================================================================
    
    /**
     * Basculer la sélection
     */
    toggleSelection() {
        this.state.selected = !this.state.selected;
        
        const card = document.getElementById(this.id);
        if (card) {
            card.classList.toggle('selected', this.state.selected);
        }
        
        if (this.config.onSelect) {
            this.config.onSelect(this.device, this.state.selected);
        }
    }
    
    /**
     * Basculer l'expansion
     */
    toggleExpanded() {
        this.state.expanded = !this.state.expanded;
        
        const card = document.getElementById(this.id);
        const details = card?.querySelector('.device-details');
        const expandBtn = card?.querySelector('.device-expand-btn');
        
        if (details) {
            details.style.display = this.state.expanded ? 'block' : 'none';
        }
        
        if (expandBtn) {
            expandBtn.textContent = this.state.expanded ? '▼' : '▶';
        }
    }
    
    /**
     * Tester le device
     */
    async testDevice() {
        if (this.state.testing) return;
        
        this.state.testing = true;
        
        const card = document.getElementById(this.id);
        const testBtn = card?.querySelector('.btn-test, .btn-test-compact');
        
        if (testBtn) {
            testBtn.disabled = true;
            testBtn.textContent = 'Testing...';
        }
        
        // Animation de test
        this.animateActivity(100, 2000);
        
        if (this.config.onTest) {
            await this.config.onTest(this.device);
        }
        
        // Simulation d'envoi de note de test
        setTimeout(() => {
            this.state.testing = false;
            
            if (testBtn) {
                testBtn.disabled = false;
                testBtn.textContent = '♪ Test';
            }
        }, 2000);
    }
    
    /**
     * Configurer le device
     */
    configureDevice() {
        if (this.config.onConfigure) {
            this.config.onConfigure(this.device);
        }
    }
    
    /**
     * Connecter le device
     */
    connectDevice() {
        // Simulation de connexion
        this.updateStatus('connecting');
        
        setTimeout(() => {
            this.updateStatus('connected');
        }, 2000);
    }
    
    // ========================================================================
    // MISES À JOUR
    // ========================================================================
    
    /**
     * Mettre à jour les données du device
     */
    updateDevice(device) {
        this.device = device;
        this.render();
    }
    
    /**
     * Mettre à jour le statut
     */
    updateStatus(status) {
        this.device.status = status;
        
        const card = document.getElementById(this.id);
        if (card) {
            // Retirer toutes les classes de statut
            card.classList.remove('connected', 'disconnected', 'connecting');
            // Ajouter la nouvelle
            card.classList.add(this.getStatusClass());
        }
        
        // Mettre à jour l'indicateur si compact
        const indicator = card?.querySelector('.status-indicator');
        if (indicator) {
            indicator.className = `status-indicator ${this.getStatusClass()}`;
        }
    }
    
    /**
     * Mettre à jour l'activité
     */
    updateActivity(level) {
        const card = document.getElementById(this.id);
        const fill = card?.querySelector('.activity-fill');
        
        if (fill) {
            fill.style.width = `${Math.min(100, level)}%`;
        }
    }
    
    /**
     * Animer l'activité
     */
    animateActivity(level, duration = 500) {
        this.updateActivity(level);
        
        // Retour à zéro après la durée
        clearTimeout(this.activityTimer);
        this.activityTimer = setTimeout(() => {
            this.updateActivity(0);
        }, duration);
    }
    
    /**
     * Mettre à jour les statistiques
     */
    updateStats(stats) {
        this.device.stats = stats;
        
        const card = document.getElementById(this.id);
        
        if (stats.messagesCount !== undefined) {
            const element = card?.querySelector('.stat-value:nth-child(1)');
            if (element) element.textContent = stats.messagesCount;
        }
        
        if (stats.notesCount !== undefined) {
            const element = card?.querySelector('.stat-value:nth-child(2)');
            if (element) element.textContent = stats.notesCount;
        }
        
        if (stats.latency !== undefined) {
            const element = card?.querySelector('.stat-value:nth-child(3)');
            if (element) element.textContent = `${stats.latency}ms`;
        }
    }
    
    // ========================================================================
    // ANIMATIONS
    // ========================================================================
    
    /**
     * Animation d'entrée
     */
    animateIn() {
        const card = document.getElementById(this.id);
        if (!card) return;
        
        // Forcer un reflow
        card.offsetHeight;
        
        requestAnimationFrame(() => {
            card.classList.add('show');
        });
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Obtenir la classe de statut
     */
    getStatusClass() {
        switch (this.device.status) {
            case 'connected': return 'connected';
            case 'disconnected': return 'disconnected';
            case 'connecting': return 'connecting';
            default: return '';
        }
    }
    
    /**
     * Obtenir l'icône de statut
     */
    getStatusIcon() {
        switch (this.device.status) {
            case 'connected': return '🟢';
            case 'disconnected': return '🔴';
            case 'connecting': return '🟡';
            default: return '⚪';
        }
    }
    
    /**
     * Obtenir l'icône de type
     */
    getTypeIcon() {
        switch (this.device.type) {
            case 'USB': return '🔌';
            case 'WiFi': return '📶';
            case 'Bluetooth': return '🔵';
            case 'Virtual': return '💻';
            case 'Keyboard': return '🎹';
            case 'Drum': return '🥁';
            case 'Synth': return '🎛️';
            default: return '🎵';
        }
    }
    
    /**
     * Obtenir la couleur d'un canal
     */
    getChannelColor(channel) {
        const colors = [
            '#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A',
            '#98D8C8', '#F7DC6F', '#BB8FCE', '#85C1E2',
            '#F8B739', '#52C7B8', '#FF8C94', '#A8E6CF',
            '#FFD93D', '#BCB3E5', '#FAB1A0', '#81C784'
        ];
        
        return colors[(channel - 1) % colors.length];
    }
    
    /**
     * Formater les capacités
     */
    formatCapabilities(capabilities) {
        if (!capabilities) return 'Standard MIDI';
        
        if (Array.isArray(capabilities)) {
            return capabilities.join(', ');
        }
        
        return capabilities;
    }
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    /**
     * Détruire la carte
     */
    destroy() {
        clearTimeout(this.activityTimer);
        
        if (this.container) {
            this.container.innerHTML = '';
        }
    }
}