/* ======================================================================================
   INSTRUMENT VIEW - Vue pour la gestion des instruments MIDI
   ======================================================================================
   Gère l'affichage des instruments connectés au système
   Filtrage par type de connexion, état des instruments, configuration
   Monitoring en temps réel, métriques de performance
   ✨ PHASE 2.1 - Support SysEx DIY intégré
   ====================================================================================== */

class InstrumentView extends BaseView {
    constructor(eventBus) {
        super('instruments-page', eventBus);
        
        // Configuration spécifique à la vue
        this.config.debounceRender = 100; // Refresh rapide pour les statuts
        this.config.trackChanges = true;
        
        // État local de la vue
        this.localState = {
            selectedInstruments: new Set(),
            expandedInstruments: new Set(),
            monitoringEnabled: true,
            refreshInterval: null,
            connectionAnimations: new Map(),
            lastUpdate: Date.now()
        };
        
        // Configuration d'affichage
        this.displayConfig = {
            showDisconnected: true,
            compactMode: false,
            showMetrics: true,
            showTechnicalDetails: false,
            groupByConnection: false,
            autoRefresh: true,
            refreshRate: 2000 // 2 secondes
        };
        
        // Templates de cartes d'instruments
        this.instrumentCardTemplates = {
            normal: this.buildNormalInstrumentCard.bind(this),
            compact: this.buildCompactInstrumentCard.bind(this),
            detailed: this.buildDetailedInstrumentCard.bind(this)
        };
        
        // Couleurs par type de connexion
        this.connectionColors = {
            usb: '#3498db',
            wifi: '#e74c3c', 
            bluetooth: '#9b59b6',
            unknown: '#95a5a6'
        };
        
        // Initialiser le monitoring
        this.startMonitoring();
        
        // Lifecycle hooks
        this.addLifecycleHook('afterRender', () => this.setupInstrumentInteractions());
        this.addLifecycleHook('beforeDestroy', () => this.stopMonitoring());
    }


// Description:
//   - Structure UI
//   - buildInstrumentStatusBar
//   - buildGlobalActions
//   - buildGroupedInstrumentsList




/**
 * PHASE 2.1 - Construire la barre de statut en bas (COMPLET)
 * @param {Object} data - Données
 * @returns {string} HTML de la barre de statut
 */
buildInstrumentStatusBar(data) {
    const {
        instruments = [],
        detailedStats = {},
        performanceMetrics = {},
        healthStatus = {}
    } = data;
    
    // Calculer les statistiques en temps réel
    const connectedCount = instruments.filter(i => i.connected).length;
    const totalCount = instruments.length;
    const avgLatency = performanceMetrics.averageLatency || 0;
    const systemHealth = healthStatus.overall || 'good';
    
    // Dernière mise à jour
    const lastUpdate = new Date().toLocaleTimeString('fr-FR');
    
    // Indicateur de santé
    const healthIcon = {
        good: '✅',
        warning: '⚠️',
        error: '❌',
        unknown: '❓'
    }[systemHealth] || '❓';
    
    const healthLabel = {
        good: 'Système OK',
        warning: 'Attention',
        error: 'Erreur',
        unknown: 'Inconnu'
    }[systemHealth] || 'Inconnu';
    
    // Classe CSS selon la santé
    const healthClass = `health-${systemHealth}`;
    
    return `
        <div class="instrument-status-bar ${healthClass}">
            
            <!-- Section gauche: Statistiques -->
            <div class="status-section status-left">
                
                <!-- Instruments actifs -->
                <div class="status-item">
                    <span class="status-icon">🎹</span>
                    <span class="status-label">Actifs:</span>
                    <span class="status-value">${connectedCount}/${totalCount}</span>
                </div>
                
                <!-- Latence moyenne -->
                <div class="status-item">
                    <span class="status-icon">⚡</span>
                    <span class="status-label">Latence:</span>
                    <span class="status-value ${avgLatency > 50 ? 'warning' : avgLatency > 100 ? 'error' : 'good'}">
                        ${avgLatency.toFixed(1)}ms
                    </span>
                </div>
                
                <!-- État système -->
                <div class="status-item">
                    <span class="status-icon">${healthIcon}</span>
                    <span class="status-label">Système:</span>
                    <span class="status-value">${healthLabel}</span>
                </div>
                
            </div>
            
            <!-- Section centre: Monitoring temps réel -->
            <div class="status-section status-center">
                
                <!-- Indicateur de monitoring actif -->
                ${this.localState.monitoringEnabled ? `
                    <div class="status-item monitoring-active">
                        <span class="status-icon pulse">🔴</span>
                        <span class="status-label">Monitoring actif</span>
                    </div>
                ` : `
                    <div class="status-item monitoring-inactive">
                        <span class="status-icon">⚫</span>
                        <span class="status-label">Monitoring inactif</span>
                    </div>
                `}
                
            </div>
            
            <!-- Section droite: Dernière mise à jour + actions -->
            <div class="status-section status-right">
                
                <!-- Dernière mise à jour -->
                <div class="status-item">
                    <span class="status-icon">🕐</span>
                    <span class="status-label">MAJ:</span>
                    <span class="status-value">${lastUpdate}</span>
                </div>
                
                <!-- Bouton refresh -->
                <button class="status-btn" 
                        onclick="app.eventBus.emit('instrument:refresh')"
                        title="Rafraîchir">
                    <span>🔄</span>
                </button>
                
                <!-- Toggle monitoring -->
                <button class="status-btn ${this.localState.monitoringEnabled ? 'active' : ''}" 
                        onclick="this.toggleMonitoring()"
                        title="${this.localState.monitoringEnabled ? 'Désactiver' : 'Activer'} le monitoring">
                    <span>${this.localState.monitoringEnabled ? '⏸️' : '▶️'}</span>
                </button>
                
            </div>
            
        </div>
    `;
}

/**
 * PHASE 2.2 - Construire le panneau d'actions globales (COMPLET)
 * @param {Object} data - Données
 * @returns {string} HTML des actions globales
 */
buildGlobalActions(data) {
    const {
        instruments = []
    } = data;
    
    const connectedCount = instruments.filter(i => i.connected).length;
    const hasConnected = connectedCount > 0;
    const hasMultiple = connectedCount > 1;
    
    return `
        <div class="global-actions-panel">
            <h4>⚡ Actions globales</h4>
            
            <div class="actions-list">
                
                <!-- Calibrer tout -->
                <button class="action-btn action-calibrate ${!hasConnected ? 'disabled' : ''}" 
                        onclick="${hasConnected ? 'this.calibrateAll()' : 'void(0)'}"
                        ${!hasConnected ? 'disabled' : ''}
                        title="${hasConnected ? 'Calibrer la latence de tous les instruments connectés' : 'Aucun instrument connecté'}">
                    <span class="action-icon">🔬</span>
                    <div class="action-content">
                        <div class="action-label">Calibrer tout</div>
                        <div class="action-description">
                            ${hasConnected ? `${connectedCount} instrument${connectedCount > 1 ? 's' : ''}` : 'Non disponible'}
                        </div>
                    </div>
                </button>
                
                <!-- Tester tout -->
                <button class="action-btn action-test ${!hasConnected ? 'disabled' : ''}" 
                        onclick="${hasConnected ? 'this.testAll()' : 'void(0)'}"
                        ${!hasConnected ? 'disabled' : ''}
                        title="${hasConnected ? 'Envoyer une note test à tous les instruments' : 'Aucun instrument connecté'}">
                    <span class="action-icon">🎵</span>
                    <div class="action-content">
                        <div class="action-label">Tester tout</div>
                        <div class="action-description">Note test C4</div>
                    </div>
                </button>
                
                <!-- Déconnecter tout -->
                <button class="action-btn action-disconnect ${!hasConnected ? 'disabled' : ''}" 
                        onclick="${hasConnected ? 'this.disconnectAll()' : 'void(0)'}"
                        ${!hasConnected ? 'disabled' : ''}
                        title="${hasConnected ? 'Déconnecter tous les instruments' : 'Aucun instrument connecté'}">
                    <span class="action-icon">🔌❌</span>
                    <div class="action-content">
                        <div class="action-label">Déconnecter tout</div>
                        <div class="action-description">
                            ${hasConnected ? `${connectedCount} connecté${connectedCount > 1 ? 's' : ''}` : 'Aucun'}
                        </div>
                    </div>
                </button>
                
                <!-- Divider -->
                <div class="action-divider"></div>
                
                <!-- Réinitialiser la configuration -->
                <button class="action-btn action-reset" 
                        onclick="this.resetConfiguration()"
                        title="Réinitialiser la configuration des instruments">
                    <span class="action-icon">🔄</span>
                    <div class="action-content">
                        <div class="action-label">Réinitialiser</div>
                        <div class="action-description">Configuration par défaut</div>
                    </div>
                </button>
                
                <!-- Effacer le cache -->
                <button class="action-btn action-clear" 
                        onclick="this.clearCache()"
                        title="Effacer le cache des profils">
                    <span class="action-icon">🗑️</span>
                    <div class="action-content">
                        <div class="action-label">Effacer le cache</div>
                        <div class="action-description">Profils SysEx</div>
                    </div>
                </button>
                
            </div>
            
            <!-- Statistiques rapides -->
            <div class="actions-stats">
                <div class="stat-item">
                    <span class="stat-value">${instruments.length}</span>
                    <span class="stat-label">Total</span>
                </div>
                <div class="stat-item">
                    <span class="stat-value">${connectedCount}</span>
                    <span class="stat-label">Connectés</span>
                </div>
                <div class="stat-item">
                    <span class="stat-value">${instruments.length - connectedCount}</span>
                    <span class="stat-label">Disponibles</span>
                </div>
            </div>
            
        </div>
    `;
}

/**
 * PHASE 2.3 - Construire la liste groupée par type de connexion (COMPLET)
 * @param {Array} instruments - Instruments à afficher
 * @param {Object} data - Données complètes
 * @returns {string} HTML de la liste groupée
 */
buildGroupedInstrumentsList(instruments, data) {
    // Grouper par type de connexion
    const groups = {
        usb: [],
        wifi: [],
        bluetooth: [],
        other: []
    };
    
    instruments.forEach(instrument => {
        const type = instrument.connectionType || 'other';
        if (groups[type]) {
            groups[type].push(instrument);
        } else {
            groups.other.push(instrument);
        }
    });
    
    // Template pour chaque groupe
    const buildGroup = (type, items, icon, label, color) => {
        const count = items.length;
        const connectedCount = items.filter(i => i.connected).length;
        
        // Ne pas afficher les groupes vides si option activée
        if (count === 0 && !this.displayConfig.showEmptyGroups) {
            return '';
        }
        
        return `
            <div class="instrument-group" data-connection-type="${type}">
                
                <!-- En-tête du groupe -->
                <div class="group-header" style="border-left-color: ${color}">
                    <div class="group-info">
                        <span class="group-icon">${icon}</span>
                        <h4 class="group-title">${label}</h4>
                        <span class="group-count">
                            (${connectedCount}/${count})
                        </span>
                    </div>
                    
                    <div class="group-actions">
                        <!-- Toggle collapse -->
                        <button class="group-toggle" 
                                onclick="this.toggleGroup('${type}')"
                                title="Déplier/Replier">
                            <span class="toggle-icon">▼</span>
                        </button>
                    </div>
                </div>
                
                <!-- Liste des instruments du groupe -->
                <div class="group-content" data-group="${type}">
                    ${count === 0 ? `
                        <div class="group-empty">
                            <p>Aucun instrument ${label.toLowerCase()} détecté</p>
                        </div>
                    ` : `
                        <div class="instruments-grid ${this.displayConfig.compactMode ? 'compact' : ''}">
                            ${items.map(instrument => this.buildInstrumentCard(instrument, data)).join('')}
                        </div>
                    `}
                </div>
                
            </div>
        `;
    };
    
    // Construire tous les groupes
    return `
        <div class="instruments-grouped-list">
            ${buildGroup('usb', groups.usb, '🔌', 'USB', this.connectionColors.usb || '#3498db')}
            ${buildGroup('wifi', groups.wifi, '📶', 'WiFi / Réseau', this.connectionColors.wifi || '#2ecc71')}
            ${buildGroup('bluetooth', groups.bluetooth, '📘', 'Bluetooth', this.connectionColors.bluetooth || '#9b59b6')}
            ${groups.other.length > 0 ? buildGroup('other', groups.other, '🔗', 'Autres', '#95a5a6') : ''}
        </div>
    `;
}

// ============================================================================
// MÉTHODES HELPERS POUR ACTIONS GLOBALES
// ============================================================================

/**
 * Calibrer tous les instruments connectés
 */
calibrateAll() {
    const instruments = Array.from(this.data.instruments || []).filter(i => i.connected);
    
    if (instruments.length === 0) {
        this.showNotification('Aucun instrument connecté', 'warning');
        return;
    }
    
    // Confirmation
    if (!confirm(`Calibrer ${instruments.length} instrument${instruments.length > 1 ? 's' : ''} ?`)) {
        return;
    }
    
    // Émettre événement pour chaque instrument
    instruments.forEach(instrument => {
        this.eventBus.emit('instrument:calibrate', { deviceId: instrument.id });
    });
    
    this.showNotification(`Calibration de ${instruments.length} instrument${instruments.length > 1 ? 's' : ''} lancée`, 'success');
}

/**
 * Tester tous les instruments (note C4)
 */
testAll() {
    const instruments = Array.from(this.data.instruments || []).filter(i => i.connected);
    
    if (instruments.length === 0) {
        this.showNotification('Aucun instrument connecté', 'warning');
        return;
    }
    
    instruments.forEach((instrument, index) => {
        // Décaler légèrement chaque test
        setTimeout(() => {
            this.eventBus.emit('instrument:test', { deviceId: instrument.id });
        }, index * 100);
    });
    
    this.showNotification(`Test de ${instruments.length} instrument${instruments.length > 1 ? 's' : ''}`, 'info');
}

/**
 * Déconnecter tous les instruments
 */
disconnectAll() {
    const instruments = Array.from(this.data.instruments || []).filter(i => i.connected);
    
    if (instruments.length === 0) {
        this.showNotification('Aucun instrument connecté', 'warning');
        return;
    }
    
    // Confirmation
    if (!confirm(`Déconnecter tous les instruments (${instruments.length}) ?`)) {
        return;
    }
    
    instruments.forEach(instrument => {
        this.eventBus.emit('instrument:disconnect', { deviceId: instrument.id });
    });
    
    this.showNotification(`Déconnexion de ${instruments.length} instrument${instruments.length > 1 ? 's' : ''}`, 'success');
}

/**
 * Réinitialiser la configuration
 */
resetConfiguration() {
    if (!confirm('Réinitialiser toute la configuration des instruments ?')) {
        return;
    }
    
    // Réinitialiser les paramètres locaux
    this.displayConfig = {
        showDisconnected: true,
        compactMode: false,
        showMetrics: true,
        showTechnicalDetails: false,
        groupByConnection: false,
        autoRefresh: true,
        refreshRate: 2000
    };
    
    // Sauvegarder dans localStorage
    localStorage.removeItem('instrumentViewConfig');
    
    // Re-render
    this.render();
    
    this.showNotification('Configuration réinitialisée', 'success');
}

/**
 * Effacer le cache des profils
 */
clearCache() {
    if (!confirm('Effacer le cache des profils SysEx ?')) {
        return;
    }
    
    // Émettre événement vers controller
    this.eventBus.emit('instrument:clearCache');
    
    this.showNotification('Cache effacé', 'success');
}

/**
 * Toggle monitoring
 */
toggleMonitoring() {
    this.localState.monitoringEnabled = !this.localState.monitoringEnabled;
    
    if (this.localState.monitoringEnabled) {
        this.startMonitoring();
    } else {
        this.stopMonitoring();
    }
    
    // Re-render status bar seulement
    const statusBar = document.querySelector('.instrument-status-bar');
    if (statusBar) {
        statusBar.outerHTML = this.buildInstrumentStatusBar(this.data);
    }
}

/**
 * Démarrer le monitoring
 */
startMonitoring() {
    if (this.localState.refreshInterval) {
        clearInterval(this.localState.refreshInterval);
    }
    
    this.localState.refreshInterval = setInterval(() => {
        if (this.localState.monitoringEnabled && this.displayConfig.autoRefresh) {
            this.eventBus.emit('instrument:refresh');
        }
    }, this.displayConfig.refreshRate);
}

/**
 * Arrêter le monitoring
 */
stopMonitoring() {
    if (this.localState.refreshInterval) {
        clearInterval(this.localState.refreshInterval);
        this.localState.refreshInterval = null;
    }
}

/**
 * Toggle groupe dans liste groupée
 * @param {string} groupType - Type du groupe (usb, wifi, etc.)
 */
toggleGroup(groupType) {
    const groupContent = document.querySelector(`.group-content[data-group="${groupType}"]`);
    const toggleIcon = document.querySelector(`.group-toggle[onclick*="${groupType}"] .toggle-icon`);
    
    if (!groupContent || !toggleIcon) return;
    
    const isCollapsed = groupContent.classList.toggle('collapsed');
    toggleIcon.textContent = isCollapsed ? '▶' : '▼';
    
    // Sauvegarder l'état
    if (!this.localState.collapsedGroups) {
        this.localState.collapsedGroups = new Set();
    }
    
    if (isCollapsed) {
        this.localState.collapsedGroups.add(groupType);
    } else {
        this.localState.collapsedGroups.delete(groupType);
    }
}




/**
 * PHASE 3.1 - Construire l'onglet de configuration (REMPLACE PLACEHOLDER)
 * @param {Object} instrument - Instrument
 * @returns {string} HTML de l'onglet configuration
 */
buildInstrumentConfigTab(instrument) {
    // Récupérer les valeurs actuelles ou défaut
    const midiChannel = instrument.midiChannel || 1;
    const programChange = instrument.programChange || 0;
    const bankSelect = instrument.bankSelect || 0;
    const latencyOffset = instrument.latencyOffset || 0;
    const transpose = instrument.transpose || 0;
    const velocityCurve = instrument.velocityCurve || 'linear';
    
    // Vérifier si des modifications ont été faites
    const hasChanges = instrument.configModified || false;
    
    return `
        <div class="instrument-config-tab">
            
            <!-- Section: Paramètres MIDI -->
            <div class="config-section">
                <h5>📡 Paramètres MIDI</h5>
                
                <div class="config-grid">
                    
                    <!-- Canal MIDI -->
                    <div class="config-field">
                        <label for="midi-channel-${instrument.id}">Canal MIDI</label>
                        <select id="midi-channel-${instrument.id}" 
                                class="config-input"
                                onchange="this.updateInstrumentConfig('${instrument.id}', 'midiChannel', parseInt(this.value))">
                            ${Array.from({length: 16}, (_, i) => i + 1).map(ch => `
                                <option value="${ch}" ${ch === midiChannel ? 'selected' : ''}>
                                    Canal ${ch}
                                </option>
                            `).join('')}
                        </select>
                        <span class="config-hint">Canal MIDI pour cet instrument</span>
                    </div>
                    
                    <!-- Program Change -->
                    <div class="config-field">
                        <label for="program-change-${instrument.id}">Program Change</label>
                        <input type="number" 
                               id="program-change-${instrument.id}"
                               class="config-input" 
                               min="0" 
                               max="127" 
                               value="${programChange}"
                               onchange="this.updateInstrumentConfig('${instrument.id}', 'programChange', parseInt(this.value))">
                        <span class="config-hint">Programme MIDI (0-127)</span>
                    </div>
                    
                    <!-- Bank Select -->
                    <div class="config-field">
                        <label for="bank-select-${instrument.id}">Bank Select</label>
                        <input type="number" 
                               id="bank-select-${instrument.id}"
                               class="config-input" 
                               min="0" 
                               max="127" 
                               value="${bankSelect}"
                               onchange="this.updateInstrumentConfig('${instrument.id}', 'bankSelect', parseInt(this.value))">
                        <span class="config-hint">Banque de sons (0-127)</span>
                    </div>
                    
                </div>
            </div>
            
            <!-- Section: Latence -->
            <div class="config-section">
                <h5>⚡ Compensation de latence</h5>
                
                <div class="config-grid">
                    
                    <!-- Offset manuel -->
                    <div class="config-field full-width">
                        <label for="latency-offset-${instrument.id}">
                            Offset manuel
                            <span class="config-value">${latencyOffset} ms</span>
                        </label>
                        <input type="range" 
                               id="latency-offset-${instrument.id}"
                               class="config-slider" 
                               min="-200" 
                               max="200" 
                               step="1"
                               value="${latencyOffset}"
                               oninput="this.updateLatencyDisplay('${instrument.id}', this.value)"
                               onchange="this.updateInstrumentConfig('${instrument.id}', 'latencyOffset', parseInt(this.value))">
                        <div class="slider-labels">
                            <span>-200ms</span>
                            <span>0ms</span>
                            <span>+200ms</span>
                        </div>
                        <span class="config-hint">Ajustement manuel de la latence (négatif = avancer, positif = retarder)</span>
                    </div>
                    
                    <!-- Auto-calibration -->
                    <div class="config-field">
                        <button class="btn btn-primary btn-block"
                                onclick="this.calibrateInstrument('${instrument.id}')">
                            <span class="btn-icon">🔬</span>
                            <span>Calibration automatique</span>
                        </button>
                        <span class="config-hint">Mesure automatique de la latence réelle</span>
                    </div>
                    
                </div>
            </div>
            
            <!-- Section: Options avancées -->
            <div class="config-section">
                <h5>🎹 Options avancées</h5>
                
                <div class="config-grid">
                    
                    <!-- Transpose -->
                    <div class="config-field">
                        <label for="transpose-${instrument.id}">
                            Transpose
                            <span class="config-value">${transpose > 0 ? '+' : ''}${transpose} demi-tons</span>
                        </label>
                        <input type="range" 
                               id="transpose-${instrument.id}"
                               class="config-slider" 
                               min="-12" 
                               max="12" 
                               step="1"
                               value="${transpose}"
                               oninput="this.updateTransposeDisplay('${instrument.id}', this.value)"
                               onchange="this.updateInstrumentConfig('${instrument.id}', 'transpose', parseInt(this.value))">
                        <div class="slider-labels">
                            <span>-12</span>
                            <span>0</span>
                            <span>+12</span>
                        </div>
                        <span class="config-hint">Décalage en demi-tons</span>
                    </div>
                    
                    <!-- Velocity Curve -->
                    <div class="config-field">
                        <label for="velocity-curve-${instrument.id}">Courbe de vélocité</label>
                        <select id="velocity-curve-${instrument.id}" 
                                class="config-input"
                                onchange="this.updateInstrumentConfig('${instrument.id}', 'velocityCurve', this.value)">
                            <option value="linear" ${velocityCurve === 'linear' ? 'selected' : ''}>Linéaire</option>
                            <option value="log" ${velocityCurve === 'log' ? 'selected' : ''}>Logarithmique</option>
                            <option value="exp" ${velocityCurve === 'exp' ? 'selected' : ''}>Exponentielle</option>
                            <option value="custom" ${velocityCurve === 'custom' ? 'selected' : ''}>Personnalisée</option>
                        </select>
                        <span class="config-hint">Type de réponse à la vélocité</span>
                    </div>
                    
                </div>
            </div>
            
            <!-- Section: Actions -->
            <div class="config-actions">
                
                <!-- Indicateur modifications -->
                ${hasChanges ? `
                    <div class="config-warning">
                        ⚠️ Modifications non sauvegardées
                    </div>
                ` : ''}
                
                <div class="action-buttons">
                    
                    <!-- Sauvegarder -->
                    <button class="btn btn-primary ${!hasChanges ? 'disabled' : ''}"
                            onclick="this.saveInstrumentConfig('${instrument.id}')"
                            ${!hasChanges ? 'disabled' : ''}>
                        <span class="btn-icon">💾</span>
                        <span>Sauvegarder</span>
                    </button>
                    
                    <!-- Réinitialiser -->
                    <button class="btn btn-secondary"
                            onclick="this.resetInstrumentConfig('${instrument.id}')">
                        <span class="btn-icon">🔄</span>
                        <span>Réinitialiser</span>
                    </button>
                    
                    <!-- Tester -->
                    <button class="btn btn-secondary"
                            onclick="this.testInstrument('${instrument.id}')">
                        <span class="btn-icon">🎵</span>
                        <span>Tester</span>
                    </button>
                    
                </div>
            </div>
            
        </div>
    `;
}

/**
 * PHASE 3.2 - Construire l'onglet de journal (REMPLACE PLACEHOLDER)
 * @param {Object} instrument - Instrument
 * @returns {string} HTML de l'onglet journal
 */
buildInstrumentLogTab(instrument) {
    // Récupérer les événements du journal (simulé pour l'instant)
    const events = instrument.eventLog || this.getInstrumentEvents(instrument.id);
    
    // Filtres actifs
    const activeFilter = this.localState.logFilters?.[instrument.id] || 'all';
    
    // Filtrer les événements
    const filteredEvents = activeFilter === 'all' ? events : 
        events.filter(e => e.type === activeFilter);
    
    return `
        <div class="instrument-log-tab">
            
            <!-- Barre de filtres -->
            <div class="log-filters">
                <button class="filter-btn ${activeFilter === 'all' ? 'active' : ''}"
                        onclick="this.setLogFilter('${instrument.id}', 'all')">
                    Tout
                </button>
                <button class="filter-btn ${activeFilter === 'note' ? 'active' : ''}"
                        onclick="this.setLogFilter('${instrument.id}', 'note')">
                    🎵 Notes
                </button>
                <button class="filter-btn ${activeFilter === 'cc' ? 'active' : ''}"
                        onclick="this.setLogFilter('${instrument.id}', 'cc')">
                    🎛️ CC
                </button>
                <button class="filter-btn ${activeFilter === 'sysex' ? 'active' : ''}"
                        onclick="this.setLogFilter('${instrument.id}', 'sysex')">
                    📟 SysEx
                </button>
                <button class="filter-btn ${activeFilter === 'error' ? 'active' : ''}"
                        onclick="this.setLogFilter('${instrument.id}', 'error')">
                    ❌ Erreurs
                </button>
            </div>
            
            <!-- Liste des événements -->
            <div class="log-events">
                ${filteredEvents.length === 0 ? `
                    <div class="log-empty">
                        <p>Aucun événement ${activeFilter !== 'all' ? 'de ce type' : ''}</p>
                    </div>
                ` : `
                    ${filteredEvents.map(event => this.buildLogEvent(event)).join('')}
                `}
            </div>
            
            <!-- Actions -->
            <div class="log-actions">
                <button class="btn btn-secondary btn-sm"
                        onclick="this.exportLog('${instrument.id}')">
                    <span class="btn-icon">💾</span>
                    <span>Exporter</span>
                </button>
                
                <button class="btn btn-secondary btn-sm"
                        onclick="this.clearLog('${instrument.id}')">
                    <span class="btn-icon">🗑️</span>
                    <span>Effacer</span>
                </button>
                
                <button class="btn btn-secondary btn-sm"
                        onclick="this.refreshLog('${instrument.id}')">
                    <span class="btn-icon">🔄</span>
                    <span>Rafraîchir</span>
                </button>
            </div>
            
        </div>
    `;
}

/**
 * Helper: Construire un événement de log
 * @param {Object} event - Événement
 * @returns {string} HTML de l'événement
 */
buildLogEvent(event) {
    const icon = {
        note: '🎵',
        cc: '🎛️',
        sysex: '📟',
        connection: '🔌',
        error: '❌',
        info: 'ℹ️'
    }[event.type] || 'ℹ️';
    
    const typeClass = `log-event-${event.type}`;
    const timestamp = this.formatLogTimestamp(event.timestamp);
    
    return `
        <div class="log-event ${typeClass}">
            <span class="event-icon">${icon}</span>
            <span class="event-timestamp">${timestamp}</span>
            <span class="event-message">${event.message}</span>
            ${event.details ? `
                <span class="event-details">${event.details}</span>
            ` : ''}
        </div>
    `;
}

/**
 * Helper: Obtenir les événements d'un instrument (simulé)
 * @param {string} instrumentId - ID instrument
 * @returns {Array} Événements
 */
getInstrumentEvents(instrumentId) {
    // Simuler des événements pour démonstration
    const now = Date.now();
    return [
        {
            type: 'connection',
            timestamp: now - 60000,
            message: 'Connected'
        },
        {
            type: 'sysex',
            timestamp: now - 59000,
            message: 'SysEx Identity Reply',
            details: 'Manufacturer: 0x42, Model: 0x01'
        },
        {
            type: 'note',
            timestamp: now - 30000,
            message: 'Note On - C4 (vel: 100)'
        },
        {
            type: 'note',
            timestamp: now - 29800,
            message: 'Note Off - C4'
        },
        {
            type: 'cc',
            timestamp: now - 20000,
            message: 'CC 1 - Value: 64',
            details: 'Modulation Wheel'
        }
    ];
}

/**
 * Helper: Formater timestamp pour log
 * @param {number} timestamp - Timestamp
 * @returns {string} Timestamp formaté
 */
formatLogTimestamp(timestamp) {
    const date = new Date(timestamp);
    const hours = date.getHours().toString().padStart(2, '0');
    const minutes = date.getMinutes().toString().padStart(2, '0');
    const seconds = date.getSeconds().toString().padStart(2, '0');
    const ms = date.getMilliseconds().toString().padStart(3, '0');
    return `${hours}:${minutes}:${seconds}.${ms}`;
}

// ============================================================================
// MÉTHODES HELPERS POUR CONFIG TAB
// ============================================================================

/**
 * Mettre à jour la configuration d'un instrument
 * @param {string} instrumentId - ID instrument
 * @param {string} key - Clé config
 * @param {any} value - Valeur
 */
updateInstrumentConfig(instrumentId, key, value) {
    // Émettre événement vers controller
    this.eventBus.emit('instrument:config:update', {
        deviceId: instrumentId,
        key: key,
        value: value
    });
    
    // Marquer comme modifié
    const instrument = this.data.instruments.find(i => i.id === instrumentId);
    if (instrument) {
        instrument.configModified = true;
    }
}

/**
 * Sauvegarder la configuration
 * @param {string} instrumentId - ID instrument
 */
saveInstrumentConfig(instrumentId) {
    this.eventBus.emit('instrument:config:save', { deviceId: instrumentId });
    this.showNotification('Configuration sauvegardée', 'success');
}

/**
 * Réinitialiser la configuration
 * @param {string} instrumentId - ID instrument
 */
resetInstrumentConfig(instrumentId) {
    if (!confirm('Réinitialiser la configuration de cet instrument ?')) {
        return;
    }
    
    this.eventBus.emit('instrument:config:reset', { deviceId: instrumentId });
    this.showNotification('Configuration réinitialisée', 'success');
}

/**
 * Mettre à jour l'affichage latency
 */
updateLatencyDisplay(instrumentId, value) {
    const valueSpan = document.querySelector(`#latency-offset-${instrumentId}`).previousElementSibling.querySelector('.config-value');
    if (valueSpan) {
        valueSpan.textContent = `${value} ms`;
    }
}

/**
 * Mettre à jour l'affichage transpose
 */
updateTransposeDisplay(instrumentId, value) {
    const valueSpan = document.querySelector(`#transpose-${instrumentId}`).previousElementSibling.querySelector('.config-value');
    if (valueSpan) {
        const sign = value > 0 ? '+' : '';
        valueSpan.textContent = `${sign}${value} demi-tons`;
    }
}

// ============================================================================
// MÉTHODES HELPERS POUR LOG TAB
// ============================================================================

/**
 * Définir le filtre de log
 * @param {string} instrumentId - ID instrument
 * @param {string} filter - Filtre
 */
setLogFilter(instrumentId, filter) {
    if (!this.localState.logFilters) {
        this.localState.logFilters = {};
    }
    
    this.localState.logFilters[instrumentId] = filter;
    
    // Re-render seulement l'onglet log
    const logTab = document.querySelector(`[data-instrument="${instrumentId}"] .instrument-log-tab`);
    if (logTab && logTab.parentElement) {
        logTab.parentElement.innerHTML = this.buildInstrumentLogTab(
            this.data.instruments.find(i => i.id === instrumentId)
        );
    }
}

/**
 * Exporter le log
 * @param {string} instrumentId - ID instrument
 */
exportLog(instrumentId) {
    const events = this.getInstrumentEvents(instrumentId);
    const text = events.map(e => 
        `${this.formatLogTimestamp(e.timestamp)} - [${e.type}] ${e.message}${e.details ? ' - ' + e.details : ''}`
    ).join('\n');
    
    const blob = new Blob([text], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `instrument-${instrumentId}-log-${Date.now()}.txt`;
    a.click();
    URL.revokeObjectURL(url);
    
    this.showNotification('Log exporté', 'success');
}

/**
 * Effacer le log
 * @param {string} instrumentId - ID instrument
 */
clearLog(instrumentId) {
    if (!confirm('Effacer tous les événements du journal ?')) {
        return;
    }
    
    this.eventBus.emit('instrument:log:clear', { deviceId: instrumentId });
    this.showNotification('Journal effacé', 'success');
}

/**
 * Rafraîchir le log
 * @param {string} instrumentId - ID instrument
 */
refreshLog(instrumentId) {
    this.eventBus.emit('instrument:log:refresh', { deviceId: instrumentId });
}

// Description:
//   Méthodes Phase 4 - Interactions UI
//   - toggleInstrumentExpansion
//   - showInstrumentMenu
//   - switchInstrumentTab
//   - toggleShowDisconnected
//   - toggleCompactMode
//   - toggleShowMetrics
// ============================================================================

/**
 * PHASE 4.1 - Déplier/Replier une carte d'instrument (COMPLET)
 * @param {string} instrumentId - ID de l'instrument
 */
toggleInstrumentExpansion(instrumentId) {
    // Toggle dans le Set
    if (this.localState.expandedInstruments.has(instrumentId)) {
        this.localState.expandedInstruments.delete(instrumentId);
    } else {
        this.localState.expandedInstruments.add(instrumentId);
    }
    
    // Trouver la carte dans le DOM
    const card = document.querySelector(`[data-instrument-id="${instrumentId}"]`);
    if (!card) {
        console.warn('Instrument card not found:', instrumentId);
        return;
    }
    
    // Toggle classe expanded
    const isExpanded = card.classList.toggle('expanded');
    
    // Mettre à jour l'icône du bouton
    const expandBtn = card.querySelector('.action-btn.expand');
    if (expandBtn) {
        expandBtn.innerHTML = `
            <span>${isExpanded ? '🔽' : '🔼'}</span>
        `;
        expandBtn.title = isExpanded ? 'Réduire' : 'Détails';
    }
    
    // Si on déplie, charger le profil si pas encore fait
    if (isExpanded) {
        const instrument = this.data.instruments.find(i => i.id === instrumentId);
        if (instrument && instrument.connected && !instrument.profileLoaded) {
            this.eventBus.emit('instrument:getProfile', { deviceId: instrumentId });
        }
    }
    
    // Animation smooth
    if (isExpanded) {
        card.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }
}

/**
 * PHASE 4.2 - Afficher le menu contextuel d'un instrument (COMPLET)
 * @param {string} instrumentId - ID de l'instrument
 * @param {Event} event - Événement click
 */
showInstrumentMenu(instrumentId, event) {
    event.preventDefault();
    event.stopPropagation();
    
    const instrument = this.data.instruments.find(i => i.id === instrumentId);
    if (!instrument) return;
    
    // Position du menu
    const x = event.clientX || event.pageX;
    const y = event.clientY || event.pageY;
    
    // Options du menu selon l'état de l'instrument
    const menuOptions = [];
    
    // Connexion/Déconnexion
    if (instrument.connected) {
        menuOptions.push({
            label: 'Déconnecter',
            icon: '🔌❌',
            action: 'disconnect',
            shortcut: ''
        });
    } else {
        menuOptions.push({
            label: 'Connecter',
            icon: '🔌✅',
            action: 'connect',
            shortcut: ''
        });
    }
    
    menuOptions.push({ separator: true });
    
    // Actions disponibles si connecté
    if (instrument.connected) {
        menuOptions.push({
            label: 'Tester',
            icon: '🎵',
            action: 'test',
            shortcut: ''
        });
        
        menuOptions.push({
            label: 'Calibrer',
            icon: '🔬',
            action: 'calibrate',
            shortcut: ''
        });
        
        menuOptions.push({ separator: true });
        
        menuOptions.push({
            label: 'Configuration',
            icon: '⚙️',
            action: 'config',
            shortcut: ''
        });
        
        menuOptions.push({
            label: 'Voir Métriques',
            icon: '📊',
            action: 'metrics',
            shortcut: ''
        });
        
        menuOptions.push({ separator: true });
        
        menuOptions.push({
            label: 'Exporter Profil',
            icon: '💾',
            action: 'export',
            shortcut: ''
        });
    }
    
    menuOptions.push({
        label: 'Supprimer',
        icon: '🗑️',
        action: 'delete',
        shortcut: '',
        danger: true
    });
    
    // Afficher le menu contextuel
    this.showContextMenu(x, y, menuOptions, (action) => {
        this.handleInstrumentMenuAction(instrumentId, action);
    });
}

/**
 * Helper: Gérer les actions du menu instrument
 * @param {string} instrumentId - ID instrument
 * @param {string} action - Action
 */
handleInstrumentMenuAction(instrumentId, action) {
    const instrument = this.data.instruments.find(i => i.id === instrumentId);
    if (!instrument) return;
    
    switch (action) {
        case 'connect':
            this.eventBus.emit('instrument:connect', { deviceId: instrumentId });
            break;
            
        case 'disconnect':
            this.eventBus.emit('instrument:disconnect', { deviceId: instrumentId });
            break;
            
        case 'test':
            this.testInstrument(instrumentId);
            break;
            
        case 'calibrate':
            this.calibrateInstrument(instrumentId);
            break;
            
        case 'config':
            // Déplier et aller à l'onglet config
            this.localState.expandedInstruments.add(instrumentId);
            this.render();
            setTimeout(() => {
                this.switchInstrumentTab(instrumentId, 'config');
            }, 100);
            break;
            
        case 'metrics':
            // Déplier et aller à l'onglet métriques
            this.localState.expandedInstruments.add(instrumentId);
            this.render();
            setTimeout(() => {
                this.switchInstrumentTab(instrumentId, 'metrics');
            }, 100);
            break;
            
        case 'export':
            this.exportInstrumentProfile(instrumentId);
            break;
            
        case 'delete':
            this.deleteInstrument(instrumentId);
            break;
    }
}

/**
 * Helper: Afficher un menu contextuel générique
 * @param {number} x - Position X
 * @param {number} y - Position Y
 * @param {Array} options - Options du menu
 * @param {Function} callback - Callback quand action sélectionnée
 */
showContextMenu(x, y, options, callback) {
    // Supprimer menu existant
    const existingMenu = document.querySelector('.context-menu');
    if (existingMenu) {
        existingMenu.remove();
    }
    
    // Créer le menu
    const menu = document.createElement('div');
    menu.className = 'context-menu';
    menu.style.left = `${x}px`;
    menu.style.top = `${y}px`;
    
    // Construire les options
    menu.innerHTML = options.map(opt => {
        if (opt.separator) {
            return '<div class="context-menu-separator"></div>';
        }
        
        const dangerClass = opt.danger ? 'danger' : '';
        const disabledClass = opt.disabled ? 'disabled' : '';
        
        return `
            <div class="context-menu-item ${dangerClass} ${disabledClass}" 
                 data-action="${opt.action}"
                 ${opt.disabled ? 'data-disabled="true"' : ''}>
                <span class="item-icon">${opt.icon || ''}</span>
                <span class="item-label">${opt.label}</span>
                ${opt.shortcut ? `<span class="item-shortcut">${opt.shortcut}</span>` : ''}
            </div>
        `;
    }).join('');
    
    // Ajouter au DOM
    document.body.appendChild(menu);
    
    // Ajuster position si hors écran
    const rect = menu.getBoundingClientRect();
    if (rect.right > window.innerWidth) {
        menu.style.left = `${x - rect.width}px`;
    }
    if (rect.bottom > window.innerHeight) {
        menu.style.top = `${y - rect.height}px`;
    }
    
    // Gérer les clics sur les items
    menu.querySelectorAll('.context-menu-item:not(.disabled)').forEach(item => {
        item.addEventListener('click', () => {
            const action = item.getAttribute('data-action');
            if (action && callback) {
                callback(action);
            }
            menu.remove();
        });
    });
    
    // Fermer au clic ailleurs
    const closeMenu = (e) => {
        if (!menu.contains(e.target)) {
            menu.remove();
            document.removeEventListener('click', closeMenu);
        }
    };
    
    setTimeout(() => {
        document.addEventListener('click', closeMenu);
    }, 10);
    
    // Fermer à l'échap
    const closeOnEscape = (e) => {
        if (e.key === 'Escape') {
            menu.remove();
            document.removeEventListener('keydown', closeOnEscape);
        }
    };
    document.addEventListener('keydown', closeOnEscape);
}

/**
 * PHASE 4.3 - Changer d'onglet dans une carte étendue (COMPLET)
 * @param {string} instrumentId - ID de l'instrument
 * @param {string} tabName - Nom de l'onglet (info, metrics, config, log)
 */
switchInstrumentTab(instrumentId, tabName) {
    const card = document.querySelector(`[data-instrument-id="${instrumentId}"]`);
    if (!card) return;
    
    // Trouver les boutons d'onglets
    const tabButtons = card.querySelectorAll('.tab-btn');
    const tabPanes = card.querySelectorAll('.tab-pane');
    
    // Désactiver tous les onglets
    tabButtons.forEach(btn => btn.classList.remove('active'));
    tabPanes.forEach(pane => pane.classList.remove('active'));
    
    // Activer l'onglet sélectionné
    const activeButton = card.querySelector(`.tab-btn[onclick*="'${tabName}'"]`);
    const activePane = card.querySelector(`.tab-pane[data-tab="${tabName}"]`);
    
    if (activeButton) activeButton.classList.add('active');
    if (activePane) activePane.classList.add('active');
    
    // Mémoriser l'onglet actif pour cet instrument
    if (!this.localState.activeInstrumentTabs) {
        this.localState.activeInstrumentTabs = {};
    }
    this.localState.activeInstrumentTabs[instrumentId] = tabName;
}

/**
 * PHASE 4.4 - Toggle affichage instruments déconnectés (COMPLET)
 */
toggleShowDisconnected() {
    this.displayConfig.showDisconnected = !this.displayConfig.showDisconnected;
    
    // Sauvegarder dans localStorage
    this.saveDisplayConfig();
    
    // Re-render
    this.render();
}

/**
 * PHASE 4.5 - Toggle mode compact (COMPLET)
 */
toggleCompactMode() {
    this.displayConfig.compactMode = !this.displayConfig.compactMode;
    
    // Sauvegarder dans localStorage
    this.saveDisplayConfig();
    
    // Re-render
    this.render();
}

/**
 * PHASE 4.6 - Toggle affichage métriques (COMPLET)
 */
toggleShowMetrics() {
    this.displayConfig.showMetrics = !this.displayConfig.showMetrics;
    
    // Sauvegarder dans localStorage
    this.saveDisplayConfig();
    
    // Re-render ou juste toggle CSS
    const cards = document.querySelectorAll('.instrument-card');
    cards.forEach(card => {
        const metrics = card.querySelector('.instrument-metrics');
        if (metrics) {
            metrics.style.display = this.displayConfig.showMetrics ? 'flex' : 'none';
        }
    });
}

/**
 * Helper: Sauvegarder la config d'affichage
 */
saveDisplayConfig() {
    try {
        localStorage.setItem('instrumentViewConfig', JSON.stringify(this.displayConfig));
    } catch (e) {
        console.warn('Failed to save display config:', e);
    }
}

/**
 * Helper: Charger la config d'affichage
 */
loadDisplayConfig() {
    try {
        const saved = localStorage.getItem('instrumentViewConfig');
        if (saved) {
            this.displayConfig = {
                ...this.displayConfig,
                ...JSON.parse(saved)
            };
        }
    } catch (e) {
        console.warn('Failed to load display config:', e);
    }
}

// ============================================================================
// MÉTHODES HELPERS SUPPLÉMENTAIRES
// ============================================================================

/**
 * Tester un instrument (envoyer note C4)
 * @param {string} instrumentId - ID instrument
 */
testInstrument(instrumentId) {
    this.eventBus.emit('instrument:test', { 
        deviceId: instrumentId,
        note: 60, // C4
        velocity: 100,
        duration: 500 // 500ms
    });
    
    this.showNotification('Test envoyé', 'info');
}

/**
 * Calibrer un instrument
 * @param {string} instrumentId - ID instrument
 */
calibrateInstrument(instrumentId) {
    this.eventBus.emit('instrument:calibrate', { deviceId: instrumentId });
    this.showNotification('Calibration lancée...', 'info');
}

/**
 * Exporter le profil d'un instrument
 * @param {string} instrumentId - ID instrument
 */
exportInstrumentProfile(instrumentId) {
    const instrument = this.data.instruments.find(i => i.id === instrumentId);
    if (!instrument) return;
    
    // Récupérer le profil complet
    this.eventBus.emit('instrument:getProfile', { 
        deviceId: instrumentId,
        callback: (profile) => {
            const data = {
                instrument: {
                    id: instrument.id,
                    name: instrument.name,
                    model: instrument.model,
                    type: instrument.type
                },
                profile: profile,
                exportDate: new Date().toISOString()
            };
            
            const json = JSON.stringify(data, null, 2);
            const blob = new Blob([json], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            
            const a = document.createElement('a');
            a.href = url;
            a.download = `instrument-${instrumentId}-profile-${Date.now()}.json`;
            a.click();
            
            URL.revokeObjectURL(url);
            
            this.showNotification('Profil exporté', 'success');
        }
    });
}

/**
 * Supprimer un instrument
 * @param {string} instrumentId - ID instrument
 */
deleteInstrument(instrumentId) {
    const instrument = this.data.instruments.find(i => i.id === instrumentId);
    if (!instrument) return;
    
    if (!confirm(`Supprimer l'instrument "${instrument.name || instrumentId}" ?`)) {
        return;
    }
    
    // Déconnecter d'abord si connecté
    if (instrument.connected) {
        this.eventBus.emit('instrument:disconnect', { deviceId: instrumentId });
    }
    
    // Supprimer de la liste
    this.eventBus.emit('instrument:delete', { deviceId: instrumentId });
    
    this.showNotification('Instrument supprimé', 'success');
}

/**
 * Afficher une notification
 * @param {string} message - Message
 * @param {string} type - Type (success, error, warning, info)
 */
showNotification(message, type = 'info') {
    this.eventBus.emit('notification:show', {
        type: type,
        message: message,
        duration: 3000
    });
}

// ============================================================================
// INITIALISATION
// ============================================================================

/**
 * Initialiser la vue (appelé dans constructor de BaseView)
 */
init() {
    // Charger la config d'affichage
    this.loadDisplayConfig();
    
    // Démarrer le monitoring auto si activé
    if (this.displayConfig.autoRefresh && this.localState.monitoringEnabled) {
        this.startMonitoring();
    }
    
    // Appeler init parent
    if (super.init) {
        super.init();
    }
}

/**
 * Nettoyer avant destruction
 */
destroy() {
    // Arrêter le monitoring
    this.stopMonitoring();
    
    // Nettoyer les menus contextuels
    const menus = document.querySelectorAll('.context-menu');
    menus.forEach(menu => menu.remove());
    
    // Appeler destroy parent
    if (super.destroy) {
        super.destroy();
    }
}

// Description:
//   Méthodes Phase 6 - Animations & Visuels
//   - Animation connexion/déconnexion
//   - Drag & Drop pour réorganiser
//   - Tooltips avancés
// ============================================================================

/**
 * PHASE 6.1 - Initialiser les animations de connexion (COMPLET)
 */
initConnectionAnimations() {
    // Écouter les événements de connexion/déconnexion
    this.eventBus.on('device:connection:success', (data) => {
        this.animateConnection(data.deviceId);
    });
    
    this.eventBus.on('device:disconnection:success', (data) => {
        this.animateDisconnection(data.deviceId);
    });
    
    this.eventBus.on('device:connected', (data) => {
        this.animateConnection(data.device_id);
    });
    
    this.eventBus.on('device:disconnected', (data) => {
        this.animateDisconnection(data.device_id);
    });
}

/**
 * Animer la connexion d'un instrument
 * @param {string} instrumentId - ID instrument
 */
animateConnection(instrumentId) {
    const card = document.querySelector(`[data-instrument-id="${instrumentId}"]`);
    if (!card) return;
    
    // Ajouter classe d'animation
    card.classList.add('connecting');
    
    // Animation pulse
    setTimeout(() => {
        card.classList.remove('connecting');
        card.classList.add('connected-pulse');
        
        // Retirer après animation
        setTimeout(() => {
            card.classList.remove('connected-pulse');
        }, 1500);
    }, 500);
    
    // Mémoriser l'animation
    this.localState.connectionAnimations.set(instrumentId, {
        type: 'connect',
        timestamp: Date.now()
    });
}

/**
 * Animer la déconnexion d'un instrument
 * @param {string} instrumentId - ID instrument
 */
animateDisconnection(instrumentId) {
    const card = document.querySelector(`[data-instrument-id="${instrumentId}"]`);
    if (!card) return;
    
    // Animation fade out
    card.classList.add('disconnecting');
    
    setTimeout(() => {
        card.classList.remove('disconnecting');
        card.classList.add('disconnected-fade');
        
        // Retirer après animation
        setTimeout(() => {
            card.classList.remove('disconnected-fade');
        }, 1000);
    }, 300);
    
    // Mémoriser l'animation
    this.localState.connectionAnimations.set(instrumentId, {
        type: 'disconnect',
        timestamp: Date.now()
    });
}

/**
 * PHASE 6.2 - Initialiser le Drag & Drop (COMPLET)
 */
initDragAndDrop() {
    // Attendre que le DOM soit prêt
    setTimeout(() => {
        this.setupDraggableCards();
    }, 100);
}

/**
 * Configurer les cartes draggables
 */
setupDraggableCards() {
    const cards = document.querySelectorAll('.instrument-card');
    
    cards.forEach((card, index) => {
        // Rendre draggable
        card.setAttribute('draggable', 'true');
        card.setAttribute('data-index', index);
        
        // Events drag
        card.addEventListener('dragstart', (e) => this.handleDragStart(e));
        card.addEventListener('dragend', (e) => this.handleDragEnd(e));
        card.addEventListener('dragover', (e) => this.handleDragOver(e));
        card.addEventListener('drop', (e) => this.handleDrop(e));
        card.addEventListener('dragenter', (e) => this.handleDragEnter(e));
        card.addEventListener('dragleave', (e) => this.handleDragLeave(e));
        
        // Ajouter handle de drag visuel
        this.addDragHandle(card);
    });
}

/**
 * Ajouter un handle de drag à une carte
 * @param {HTMLElement} card - Carte
 */
addDragHandle(card) {
    // Vérifier si le handle existe déjà
    if (card.querySelector('.drag-handle')) return;
    
    const handle = document.createElement('div');
    handle.className = 'drag-handle';
    handle.innerHTML = '⋮⋮';
    handle.title = 'Glisser pour réorganiser';
    
    const header = card.querySelector('.instrument-card-header');
    if (header) {
        header.insertBefore(handle, header.firstChild);
    }
}

/**
 * Gérer le début du drag
 * @param {DragEvent} e - Event
 */
handleDragStart(e) {
    const card = e.currentTarget;
    
    // Stocker l'ID de l'instrument
    const instrumentId = card.getAttribute('data-instrument-id');
    e.dataTransfer.effectAllowed = 'move';
    e.dataTransfer.setData('text/plain', instrumentId);
    
    // Ajouter classe dragging
    card.classList.add('dragging');
    
    // Stocker l'élément dragué
    this.draggedElement = card;
    
    // Rendre les autres cartes droppables
    document.querySelectorAll('.instrument-card').forEach(c => {
        if (c !== card) {
            c.classList.add('drop-target');
        }
    });
}

/**
 * Gérer la fin du drag
 * @param {DragEvent} e - Event
 */
handleDragEnd(e) {
    const card = e.currentTarget;
    
    // Retirer classes
    card.classList.remove('dragging');
    
    document.querySelectorAll('.instrument-card').forEach(c => {
        c.classList.remove('drop-target', 'drag-over');
    });
    
    this.draggedElement = null;
}

/**
 * Gérer le drag over
 * @param {DragEvent} e - Event
 */
handleDragOver(e) {
    e.preventDefault();
    e.dataTransfer.dropEffect = 'move';
    return false;
}

/**
 * Gérer l'entrée dans une zone drop
 * @param {DragEvent} e - Event
 */
handleDragEnter(e) {
    const card = e.currentTarget;
    if (card !== this.draggedElement) {
        card.classList.add('drag-over');
    }
}

/**
 * Gérer la sortie d'une zone drop
 * @param {DragEvent} e - Event
 */
handleDragLeave(e) {
    const card = e.currentTarget;
    card.classList.remove('drag-over');
}

/**
 * Gérer le drop
 * @param {DragEvent} e - Event
 */
handleDrop(e) {
    e.preventDefault();
    e.stopPropagation();
    
    const dropTarget = e.currentTarget;
    
    if (dropTarget === this.draggedElement) return;
    
    // Récupérer les indices
    const draggedIndex = parseInt(this.draggedElement.getAttribute('data-index'));
    const targetIndex = parseInt(dropTarget.getAttribute('data-index'));
    
    // Réorganiser dans le data
    const instruments = [...this.data.instruments];
    const [draggedItem] = instruments.splice(draggedIndex, 1);
    instruments.splice(targetIndex, 0, draggedItem);
    
    // Sauvegarder l'ordre custom
    this.saveCustomOrder(instruments.map(i => i.id));
    
    // Re-render
    this.data.instruments = instruments;
    this.render();
    
    // Réinitialiser drag & drop
    this.initDragAndDrop();
    
    return false;
}

/**
 * Sauvegarder l'ordre personnalisé
 * @param {Array} order - Tableau d'IDs dans l'ordre
 */
saveCustomOrder(order) {
    try {
        localStorage.setItem('instrumentsCustomOrder', JSON.stringify(order));
    } catch (e) {
        console.warn('Failed to save custom order:', e);
    }
}

/**
 * Charger l'ordre personnalisé
 * @returns {Array|null} Ordre ou null
 */
loadCustomOrder() {
    try {
        const saved = localStorage.getItem('instrumentsCustomOrder');
        return saved ? JSON.parse(saved) : null;
    } catch (e) {
        console.warn('Failed to load custom order:', e);
        return null;
    }
}

/**
 * Appliquer l'ordre personnalisé aux instruments
 * @param {Array} instruments - Instruments
 * @returns {Array} Instruments réorganisés
 */
applyCustomOrder(instruments) {
    const order = this.loadCustomOrder();
    if (!order) return instruments;
    
    // Créer une map pour accès rapide
    const instrumentsMap = new Map(instruments.map(i => [i.id, i]));
    
    // Réorganiser selon l'ordre sauvegardé
    const ordered = [];
    order.forEach(id => {
        if (instrumentsMap.has(id)) {
            ordered.push(instrumentsMap.get(id));
            instrumentsMap.delete(id);
        }
    });
    
    // Ajouter les nouveaux instruments non présents dans l'ordre
    instrumentsMap.forEach(instrument => {
        ordered.push(instrument);
    });
    
    return ordered;
}

/**
 * PHASE 6.3 - Initialiser les tooltips avancés (COMPLET)
 */
initAdvancedTooltips() {
    // Délégation d'événements pour les tooltips
    document.addEventListener('mouseover', (e) => {
        const element = e.target.closest('[data-tooltip]');
        if (element) {
            this.showAdvancedTooltip(element);
        }
    });
    
    document.addEventListener('mouseout', (e) => {
        const element = e.target.closest('[data-tooltip]');
        if (element) {
            this.hideAdvancedTooltip();
        }
    });
}

/**
 * Afficher un tooltip avancé
 * @param {HTMLElement} element - Élément
 */
showAdvancedTooltip(element) {
    // Annuler tooltip existant
    if (this.tooltipTimeout) {
        clearTimeout(this.tooltipTimeout);
    }
    
    // Délai d'apparition
    this.tooltipTimeout = setTimeout(() => {
        const tooltipData = element.getAttribute('data-tooltip');
        const tooltipType = element.getAttribute('data-tooltip-type') || 'simple';
        
        // Créer tooltip
        const tooltip = this.createAdvancedTooltip(tooltipData, tooltipType, element);
        
        // Positionner
        this.positionTooltip(tooltip, element);
        
        // Ajouter au DOM
        document.body.appendChild(tooltip);
        
        // Animation d'entrée
        setTimeout(() => {
            tooltip.classList.add('visible');
        }, 10);
        
        // Stocker référence
        this.currentTooltip = tooltip;
    }, 500); // 500ms de délai
}

/**
 * Créer un tooltip avancé
 * @param {string} data - Données tooltip (peut être JSON)
 * @param {string} type - Type de tooltip
 * @param {HTMLElement} element - Élément source
 * @returns {HTMLElement} Tooltip
 */
createAdvancedTooltip(data, type, element) {
    const tooltip = document.createElement('div');
    tooltip.className = `advanced-tooltip tooltip-${type}`;
    
    try {
        // Essayer de parser comme JSON
        const jsonData = JSON.parse(data);
        tooltip.innerHTML = this.buildTooltipContent(jsonData, type);
    } catch {
        // Simple texte
        tooltip.innerHTML = `<div class="tooltip-content">${data}</div>`;
    }
    
    return tooltip;
}

/**
 * Construire le contenu d'un tooltip riche
 * @param {Object} data - Données
 * @param {string} type - Type
 * @returns {string} HTML
 */
buildTooltipContent(data, type) {
    if (type === 'instrument') {
        return `
            <div class="tooltip-header">
                <span class="tooltip-icon">${data.icon || '🎹'}</span>
                <strong>${data.name || 'Instrument'}</strong>
            </div>
            <div class="tooltip-body">
                ${data.connectionType ? `<div>📡 ${data.connectionType.toUpperCase()}</div>` : ''}
                ${data.latency ? `<div>⚡ ${data.latency}ms latency</div>` : ''}
                ${data.noteRange ? `<div>🎵 ${data.noteRange}</div>` : ''}
                ${data.polyphony ? `<div>🔊 ${data.polyphony} polyphony</div>` : ''}
            </div>
            <div class="tooltip-footer">
                <div class="tooltip-shortcuts">
                    <div><kbd>Click</kbd> Sélectionner</div>
                    <div><kbd>Double-click</kbd> Connecter</div>
                    <div><kbd>Right-click</kbd> Menu</div>
                </div>
            </div>
        `;
    }
    
    // Type simple par défaut
    return `<div class="tooltip-content">${data.text || JSON.stringify(data)}</div>`;
}

/**
 * Positionner un tooltip
 * @param {HTMLElement} tooltip - Tooltip
 * @param {HTMLElement} element - Élément source
 */
positionTooltip(tooltip, element) {
    const rect = element.getBoundingClientRect();
    const tooltipRect = tooltip.getBoundingClientRect();
    
    // Position par défaut : au-dessus
    let top = rect.top - tooltipRect.height - 10;
    let left = rect.left + (rect.width / 2) - (tooltipRect.width / 2);
    
    // Ajustements si hors écran
    if (top < 10) {
        // En dessous
        top = rect.bottom + 10;
        tooltip.classList.add('below');
    }
    
    if (left < 10) {
        left = 10;
    } else if (left + tooltipRect.width > window.innerWidth - 10) {
        left = window.innerWidth - tooltipRect.width - 10;
    }
    
    tooltip.style.top = `${top}px`;
    tooltip.style.left = `${left}px`;
}

/**
 * Masquer le tooltip
 */
hideAdvancedTooltip() {
    // Annuler timeout si en attente
    if (this.tooltipTimeout) {
        clearTimeout(this.tooltipTimeout);
        this.tooltipTimeout = null;
    }
    
    // Retirer tooltip si existant
    if (this.currentTooltip) {
        this.currentTooltip.classList.remove('visible');
        
        setTimeout(() => {
            if (this.currentTooltip && this.currentTooltip.parentNode) {
                this.currentTooltip.remove();
            }
            this.currentTooltip = null;
        }, 200);
    }
}

/**
 * Ajouter des tooltips avancés aux cartes
 */
addTooltipsToCards() {
    const cards = document.querySelectorAll('.instrument-card');
    
    cards.forEach(card => {
        const instrumentId = card.getAttribute('data-instrument-id');
        const instrument = this.data.instruments.find(i => i.id === instrumentId);
        
        if (!instrument) return;
        
        // Données du tooltip
        const tooltipData = {
            name: instrument.name || instrumentId,
            icon: this.getConnectionIcon(instrument.connectionType),
            connectionType: instrument.connectionType,
            latency: instrument.latency,
            noteRange: instrument.noteRange ? 
                `${instrument.noteRange.min}-${instrument.noteRange.max}` : null,
            polyphony: instrument.maxPolyphony || instrument.polyphony
        };
        
        card.setAttribute('data-tooltip', JSON.stringify(tooltipData));
        card.setAttribute('data-tooltip-type', 'instrument');
    });
}

/**
 * Helper: Obtenir l'icône de connexion
 * @param {string} type - Type connexion
 * @returns {string} Icône
 */
getConnectionIcon(type) {
    const icons = {
        usb: '🔌',
        wifi: '📶',
        bluetooth: '📘'
    };
    return icons[type] || '🔗';
}

// ============================================================================
// INITIALISATION COMPLÈTE - À APPELER DANS init()
// ============================================================================

/**
 * Initialiser toutes les fonctionnalités visuelles
 */
initVisualFeatures() {
    // Animations connexion
    this.initConnectionAnimations();
    
    // Drag & Drop
    this.initDragAndDrop();
    
    // Tooltips
    this.initAdvancedTooltips();
    
    // Ajouter tooltips aux cartes
    setTimeout(() => {
        this.addTooltipsToCards();
    }, 100);
    
    // Appliquer l'ordre personnalisé si existant
    const order = this.loadCustomOrder();
    if (order && this.data.instruments) {
        this.data.instruments = this.applyCustomOrder(this.data.instruments);
    }
}

/**
 * Réinitialiser les features visuelles après render
 */
reinitVisualFeatures() {
    this.initDragAndDrop();
    this.addTooltipsToCards();
}






buildEmptyInstrumentsState(data) {
    const {
        isScanning = false,
        lastScanTime = null,
        connectionFilter = 'all',
        showDisconnected = true
    } = data;
    
    // Message différent selon le filtre actif
    let emptyMessage = '';
    let emptyIcon = '🎹';
    let emptyTitle = 'Aucun instrument détecté';
    
    switch (connectionFilter) {
        case 'usb':
            emptyIcon = '🔌';
            emptyTitle = 'Aucun instrument USB';
            emptyMessage = 'Aucun instrument MIDI USB n\'est actuellement connecté à votre système.';
            break;
        case 'wifi':
            emptyIcon = '📶';
            emptyTitle = 'Aucun instrument WiFi';
            emptyMessage = 'Aucun instrument MIDI WiFi/réseau n\'est actuellement détecté.';
            break;
        case 'bluetooth':
            emptyIcon = '📘';
            emptyTitle = 'Aucun instrument Bluetooth';
            emptyMessage = 'Aucun instrument MIDI Bluetooth n\'est actuellement connecté.';
            break;
        case 'connected':
            emptyIcon = '🔌❌';
            emptyTitle = 'Aucun instrument connecté';
            emptyMessage = 'Vous avez des instruments disponibles mais aucun n\'est connecté.';
            break;
        default:
            emptyMessage = 'Aucun instrument MIDI n\'a été détecté sur votre système.';
    }
    
    // Si en train de scanner
    if (isScanning) {
        return `
            <div class="empty-instruments-state scanning">
                <div class="empty-content">
                    <div class="empty-icon scanning-icon">
                        <div class="spinner"></div>
                        🔍
                    </div>
                    <h3>Scan en cours...</h3>
                    <p>Recherche d'instruments MIDI en cours. Veuillez patienter.</p>
                </div>
            </div>
        `;
    }
    
    return `
        <div class="empty-instruments-state">
            <div class="empty-content">
                
                <!-- Icône placeholder -->
                <div class="empty-icon">
                    ${emptyIcon}
                </div>
                
                <!-- Titre et message -->
                <h3>${emptyTitle}</h3>
                <p class="empty-message">${emptyMessage}</p>
                
                <!-- Dernière scan info si disponible -->
                ${lastScanTime ? `
                    <p class="last-scan">
                        Dernier scan : ${this.formatDate(lastScanTime, { relative: true })}
                    </p>
                ` : ''}
                
                <!-- Actions recommandées -->
                <div class="empty-actions">
                    
                    <!-- Bouton scan principal -->
                    <button class="btn btn-primary btn-scan" 
                            onclick="app.eventBus.emit('instrument:scan')"
                            title="Scanner les instruments MIDI disponibles">
                        <span class="btn-icon">🔍</span>
                        <span class="btn-label">Scanner les instruments</span>
                    </button>
                    
                    <!-- Bouton refresh -->
                    <button class="btn btn-secondary" 
                            onclick="app.eventBus.emit('instrument:refresh')"
                            title="Rafraîchir la liste">
                        <span class="btn-icon">🔄</span>
                        <span class="btn-label">Rafraîchir</span>
                    </button>
                    
                </div>
                
                <!-- Instructions détaillées -->
                <div class="empty-instructions">
                    <h4>💡 Comment connecter un instrument :</h4>
                    <div class="instruction-list">
                        
                        <!-- USB -->
                        <div class="instruction-item">
                            <div class="instruction-icon">🔌</div>
                            <div class="instruction-content">
                                <strong>USB</strong>
                                <p>Connectez votre instrument via un câble USB MIDI. Il sera détecté automatiquement.</p>
                            </div>
                        </div>
                        
                        <!-- WiFi/Réseau -->
                        <div class="instruction-item">
                            <div class="instruction-icon">📶</div>
                            <div class="instruction-content">
                                <strong>WiFi / Réseau</strong>
                                <p>Assurez-vous que votre instrument réseau (RTP-MIDI) est sur le même réseau local.</p>
                            </div>
                        </div>
                        
                        <!-- Bluetooth -->
                        <div class="instruction-item">
                            <div class="instruction-icon">📘</div>
                            <div class="instruction-content">
                                <strong>Bluetooth</strong>
                                <p>Activez le Bluetooth sur votre Raspberry Pi et appairez votre instrument Bluetooth MIDI.</p>
                            </div>
                        </div>
                        
                    </div>
                </div>
                
                <!-- Lien aide -->
                <div class="empty-help">
                    <a href="#" onclick="app.modalController?.show('help', { topic: 'instruments' }); return false;">
                        📚 Consulter le guide de connexion des instruments
                    </a>
                </div>
                
            </div>
        </div>
    `;
}



renderInstrumentCard(instrument) {
    return `
        <div class="instrument-card" data-device-id="${instrument.deviceId}">
            <!-- ... sections existantes ... -->
            
            <!-- ✅ NOUVEAU: CC Capabilities -->
            ${this.renderCCCapabilities(instrument)}
            
            <!-- ✅ NOUVEAU: Air Capabilities -->
            ${this.renderAirCapabilities(instrument)}
            
            <!-- ✅ NOUVEAU: Light Capabilities -->
            ${this.renderLightCapabilities(instrument)}
            
            <!-- ✅ NOUVEAU: Sensors -->
            ${this.renderSensors(instrument)}
            
            <!-- ✅ NOUVEAU: Sync -->
            ${this.renderSyncClock(instrument)}
        </div>
    `;
}

renderCCCapabilities(instrument) {
    if (!instrument.ccCapabilities) return '';
    
    const { count, list, names } = instrument.ccCapabilities;
    
    return `
        <div class="cc-capabilities">
            <h4>🎛️ Contrôleurs MIDI (${count})</h4>
            <div class="cc-list">
                ${list.map((cc, i) => `
                    <span class="cc-badge">
                        CC ${cc}: ${names[i]}
                    </span>
                `).join('')}
            </div>
        </div>
    `;
}

renderAirCapabilities(instrument) {
    if (!instrument.airCapabilities || !instrument.airCapabilities.hasBreathControl) {
        return '';
    }
    
    const { cc, range, sensitivity, curve } = instrument.airCapabilities;
    
    return `
        <div class="air-capabilities">
            <h4>💨 Contrôle Breath</h4>
            <p>CC ${cc} (${curve})</p>
            <p>Range: ${range.min}-${range.max}</p>
            <p>Sensitivity: ${sensitivity}</p>
        </div>
    `;
}

renderLightCapabilities(instrument) {
    if (!instrument.lightCapabilities || !instrument.lightCapabilities.hasLights) {
        return '';
    }
    
    const { count, type, protocol, animations } = instrument.lightCapabilities;
    
    return `
        <div class="light-capabilities">
            <h4>💡 Lumières</h4>
            <p>${count} LED(s) ${type} (${protocol})</p>
            <p>Animations: ${animations.join(', ')}</p>
        </div>
    `;
}

renderSensors(instrument) {
    if (!instrument.sensorsFeedback || instrument.sensorsFeedback.count === 0) {
        return '';
    }
    
    const { sensors } = instrument.sensorsFeedback;
    
    return `
        <div class="sensors-feedback">
            <h4>📊 Capteurs (${sensors.length})</h4>
            <div class="sensors-list">
                ${sensors.map(sensor => `
                    <div class="sensor-item">
                        <span class="sensor-name">${sensor.name}</span>
                        <progress 
                            value="${sensor.value}" 
                            max="${sensor.max}"
                            class="sensor-value"
                        ></progress>
                        <span class="sensor-value-text">${sensor.value}</span>
                    </div>
                `).join('')}
            </div>
        </div>
    `;
}

renderSyncClock(instrument) {
    if (!instrument.syncClock) return '';
    
    const { midiClock, mtc, internalTempo } = instrument.syncClock;
    
    if (!midiClock && !mtc && internalTempo === 0) return '';
    
    return `
        <div class="sync-clock">
            <h4>⏱️ Sync & Clock</h4>
            <p>
                ${midiClock ? '✓ MIDI Clock' : '✗ MIDI Clock'}<br>
                ${mtc ? '✓ MTC' : '✗ MTC'}<br>
                ${internalTempo > 0 ? `Tempo: ${internalTempo} BPM` : ''}
            </p>
        </div>
    `;
}





    /**
     * Construire le template HTML principal
     * @param {Object} data - Données des instruments et état
     * @returns {string} HTML généré
     */
    buildTemplate(data) {
        const {
            instruments = [],
            filteredInstruments = [],
            connectionFilter = 'all',
            viewMode = 'all',
            showDisconnected = true,
            detailedStats = {},
            healthStatus = {},
            performanceMetrics = {}
        } = data;
        
        return `
            <div class="instruments-container">
                
                <!-- Header avec statistiques et contrôles -->
                ${this.buildInstrumentHeader(data)}
                
                <!-- Corps principal -->
                <div class="instruments-body">
                    
                    <!-- Panneau principal des instruments -->
                    <div class="instruments-main-panel">
                        
                        <!-- Barre d'outils et filtres -->
                        ${this.buildInstrumentToolbar(data)}
                        
                        <!-- Liste/grille des instruments -->
                        ${this.buildInstrumentsList(filteredInstruments, data)}
                        
                    </div>
                    
                    <!-- Panneau latéral de monitoring -->
                    <div class="instruments-side-panel">
                        
                        <!-- État de santé global -->
                        ${this.buildHealthPanel(healthStatus)}
                        
                        <!-- Métriques de performance -->
                        ${this.buildPerformancePanel(performanceMetrics)}
                        
                        <!-- Détails de l'instrument sélectionné -->
                        ${this.buildInstrumentDetails(data)}
                        
                        <!-- Actions globales -->
                        ${this.buildGlobalActions(data)}
                        
                    </div>
                    
                </div>
                
                <!-- Barre de statut avec monitoring -->
                ${this.buildInstrumentStatusBar(data)}
                
            </div>
        `;
    }

    /**
     * Construire l'en-tête avec statistiques
     * @param {Object} data - Données
     * @returns {string} HTML de l'en-tête
     */
    buildInstrumentHeader(data) {
        const { detailedStats = {}, stats = {} } = data;
        const statsToUse = Object.keys(detailedStats).length > 0 ? detailedStats : stats;
        
        return `
            <div class="instrument-header">
                
                <!-- Titre et indicateur de statut -->
                <div class="header-title">
                    <h2>🎹 Instruments MIDI</h2>
                    <div class="status-indicator ${this.getGlobalHealthClass(data.healthStatus)}">
                        <span class="status-dot"></span>
                        <span class="status-text">${this.getGlobalHealthText(data.healthStatus)}</span>
                    </div>
                </div>
                
                <!-- Statistiques rapides -->
                <div class="header-stats">
                    <div class="stat-card">
                        <div class="stat-value">${statsToUse.total || 0}</div>
                        <div class="stat-label">Total</div>
                    </div>
                    
                    <div class="stat-card connected">
                        <div class="stat-value">${statsToUse.connected || 0}</div>
                        <div class="stat-label">Connectés</div>
                    </div>
                    
                    <div class="stat-card sysex">
                        <div class="stat-value">${statsToUse.sysexCapable || 0}</div>
                        <div class="stat-label">DIY SysEx</div>
                    </div>
                    
                    <div class="stat-card">
                        <div class="stat-value">${Math.round(statsToUse.averageLatency || 0)}ms</div>
                        <div class="stat-label">Latence moy.</div>
                    </div>
                </div>
                
                <!-- Actions principales -->
                <div class="header-actions">
                    <button class="btn btn-primary" onclick="app.controllers.instrument.startDiscovery()" 
                            title="Détecter de nouveaux instruments">
                        🔍 Détecter
                    </button>
                    
                    <button class="btn btn-secondary" onclick="app.controllers.instrument.scanInstruments()" 
                            title="Scanner tous les ports MIDI">
                        🔄 Scanner
                    </button>
                    
                    <button class="btn btn-secondary" onclick="this.toggleMonitoring()" 
                            title="Basculer le monitoring">
                        ${this.localState.monitoringEnabled ? '⏸️ Pause' : '▶️ Reprendre'}
                    </button>
                </div>
                
            </div>
        `;
    }

    /**
     * Construire la barre d'outils
     * @param {Object} data - Données
     * @returns {string} HTML de la barre d'outils
     */
    buildInstrumentToolbar(data) {
        const { filters = {}, connectionFilter = 'all', viewMode = 'all' } = data;
        const activeType = filters.type || 'all';
        const activeConnection = filters.connection || connectionFilter;
        
        return `
            <div class="instruments-toolbar">
                
                <!-- Filtres de type -->
                <div class="connection-filters">
                    <label>Type :</label>
                    <div class="filter-buttons">
                        <button class="filter-btn ${activeType === 'all' ? 'active' : ''}" 
                                onclick="app.controllers.instrument.setTypeFilter('all')">
                            Tous
                        </button>
                        <button class="filter-btn ${activeType === 'diy' ? 'active' : ''}" 
                                onclick="app.controllers.instrument.setTypeFilter('diy')">
                            🔧 DIY
                        </button>
                        <button class="filter-btn ${activeType === 'standard' ? 'active' : ''}" 
                                onclick="app.controllers.instrument.setTypeFilter('standard')">
                            Standard
                        </button>
                    </div>
                </div>
                
                <!-- Filtres de connexion -->
                <div class="connection-filters">
                    <label>Connexion :</label>
                    <div class="filter-buttons">
                        ${this.buildConnectionFilterButtons(activeConnection)}
                    </div>
                </div>
                
                <!-- Mode d'affichage -->
                <div class="view-mode-filters">
                    <label>État :</label>
                    <div class="filter-buttons">
                        <button class="filter-btn ${activeConnection === 'all' ? 'active' : ''}" 
                                onclick="app.controllers.instrument.setConnectionFilter('all')">
                            Tous
                        </button>
                        <button class="filter-btn ${activeConnection === 'connected' ? 'active' : ''}" 
                                onclick="app.controllers.instrument.setConnectionFilter('connected')">
                            Connectés
                        </button>
                        <button class="filter-btn ${activeConnection === 'disconnected' ? 'active' : ''}" 
                                onclick="app.controllers.instrument.setConnectionFilter('disconnected')">
                            Déconnectés
                        </button>
                    </div>
                </div>
                
                <!-- Options d'affichage -->
                <div class="display-options">
                    <label class="option-toggle">
                        <input type="checkbox" ${this.displayConfig.showDisconnected ? 'checked' : ''} 
                               onchange="this.toggleShowDisconnected()">
                        <span>Afficher déconnectés</span>
                    </label>
                    
                    <label class="option-toggle">
                        <input type="checkbox" ${this.displayConfig.compactMode ? 'checked' : ''} 
                               onchange="this.toggleCompactMode()">
                        <span>Mode compact</span>
                    </label>
                    
                    <label class="option-toggle">
                        <input type="checkbox" ${this.displayConfig.showMetrics ? 'checked' : ''} 
                               onchange="this.toggleShowMetrics()">
                        <span>Métriques</span>
                    </label>
                </div>
                
                <!-- Actions rapides -->
                <div class="quick-actions">
                    <button class="btn btn-sm" onclick="app.controllers.instrument.calibrateAll?.()" 
                            title="Calibrer la latence de tous les instruments">
                        🔬 Calibrer
                    </button>
                    
                    <button class="btn btn-sm" onclick="this.exportInstrumentConfig()" 
                            title="Exporter la configuration">
                        💾 Exporter
                    </button>
                    
                    <button class="btn btn-sm" onclick="this.importInstrumentConfig()" 
                            title="Importer une configuration">
                        📁 Importer
                    </button>
                </div>
                
            </div>
        `;
    }

    /**
     * Construire les boutons de filtre de connexion
     * @param {string} activeFilter - Filtre actif
     * @returns {string} HTML des boutons
     */
    buildConnectionFilterButtons(activeFilter) {
        const filters = [
            { value: 'all', label: 'Tous', icon: '🔗' },
            { value: 'usb', label: 'USB', icon: '🔌' },
            { value: 'wifi', label: 'WiFi', icon: '📶' },
            { value: 'bluetooth', label: 'Bluetooth', icon: '📘' }
        ];
        
        return filters.map(filter => `
            <button class="filter-btn ${activeFilter === filter.value ? 'active' : ''}" 
                    onclick="app.controllers.instrument.setConnectionFilter('${filter.value}')"
                    style="border-color: ${this.connectionColors[filter.value] || '#ddd'}">
                ${filter.icon} ${filter.label}
            </button>
        `).join('');
    }

    /**
     * Construire la liste des instruments
     * @param {Array} instruments - Instruments à afficher
     * @param {Object} data - Données complètes
     * @returns {string} HTML de la liste
     */
    buildInstrumentsList(instruments, data) {
        if (!instruments || instruments.length === 0) {
            return this.buildEmptyInstrumentsState(data);
        }
        
        // Grouper par connexion si activé
        if (this.displayConfig.groupByConnection) {
            return this.buildGroupedInstrumentsList(instruments, data);
        }
        
        const gridClass = this.displayConfig.compactMode ? 'instruments-grid compact' : 'instruments-grid';
        
        return `
            <div class="${gridClass}">
                ${instruments.map(instrument => this.buildInstrumentCard(instrument, data)).join('')}
            </div>
        `;
    }

    /**
     * Construire une carte d'instrument (VERSION COMPLÈTE)
     * @param {Object} instrument - Instrument
     * @param {Object} data - Données complètes
     * @returns {string} HTML de la carte
     */
    buildInstrumentCard(instrument, data) {
        const isSelected = this.localState.selectedInstruments.has(instrument.id);
        const isExpanded = this.localState.expandedInstruments.has(instrument.id);
        const connectionColor = this.connectionColors[instrument.connection] || '#95a5a6';
        
        const cardClasses = [
            'instrument-card',
            instrument.connected ? 'connected' : 'disconnected',
            instrument.sysexCapable ? 'sysex-capable' : '',
            isSelected ? 'selected' : '',
            isExpanded ? 'expanded' : '',
            this.displayConfig.compactMode ? 'compact' : ''
        ].filter(Boolean).join(' ');
        
        return `
            <div class="${cardClasses}" 
                 data-instrument-id="${instrument.id}"
                 data-connection="${instrument.connection || 'unknown'}"
                 style="border-left-color: ${connectionColor}"
                 onclick="this.handleInstrumentClick('${instrument.id}', event)">
                
                <!-- En-tête de la carte -->
                <div class="card-header">
                    
                    <!-- Statut et connexion -->
                    <div class="instrument-status">
                        <div class="connection-indicator ${instrument.connected ? 'connected' : 'disconnected'}" 
                             style="background-color: ${connectionColor}"
                             title="${(instrument.connection || 'unknown').toUpperCase()} - ${instrument.connected ? 'Connecté' : 'Déconnecté'}">
                        </div>
                        
                        <div class="instrument-type">
                            ${this.getInstrumentIcon(instrument.type)}
                        </div>
                    </div>
                    
                    <!-- Informations principales -->
                    <div class="instrument-info">
                        <h4 class="instrument-name" title="${this.escapeHtml(instrument.name)}">
                            ${this.escapeHtml(instrument.name)}
                        </h4>
                        <div class="instrument-details">
                            <span class="instrument-type-text">${instrument.type || 'Unknown'}</span>
                            ${instrument.manufacturer ? `<span class="separator">•</span><span>${this.escapeHtml(instrument.manufacturer)}</span>` : ''}
                        </div>
                        
                        <!-- ✨ NOUVEAU: Badges SysEx -->
                        ${instrument.sysexCapable ? `
                            <div class="instrument-badges">
                                <span class="badge badge-sysex">🔧 SysEx DIY</span>
                                ${instrument.identityReceived ? '<span class="badge badge-success">✓ Identifié</span>' : ''}
                                ${instrument.mappingReceived ? '<span class="badge badge-info">🗺️ Mapping</span>' : ''}
                            </div>
                        ` : ''}
                    </div>
                    
                    <!-- Actions de la carte -->
                    <div class="card-actions">
                        ${this.buildInstrumentCardActions(instrument)}
                    </div>
                    
                </div>
                
                <!-- ✨ NOUVEAU: Capacités SysEx (si DIY et identifié) -->
                ${instrument.sysexCapable && instrument.identityReceived ? this.buildSysExCapabilities(instrument) : ''}
                
                <!-- Métriques rapides -->
                ${this.displayConfig.showMetrics ? this.buildInstrumentMetrics(instrument) : ''}
                
                <!-- Corps de la carte (visible si étendue) -->
                ${isExpanded ? this.buildInstrumentCardBody(instrument, data) : ''}
                
            </div>
        `;
    }

    /**
     * ✨ NOUVEAU: Construire les capacités SysEx
     * @param {Object} instrument - Instrument
     * @returns {string} HTML des capacités
     */
    buildSysExCapabilities(instrument) {
        const {
            noteRange = { min: 0, max: 127 },
            polyphony = 1,
            hasAir = false,
            hasLights = false,
            hasSensors = false,
            hasCC = false
        } = instrument;
        
        return `
            <div class="sysex-capabilities">
                <h5 class="capabilities-title">Capacités</h5>
                <div class="capabilities-grid">
                    <div class="capability">
                        <span class="capability-icon">🎹</span>
                        <span class="capability-label">Notes</span>
                        <span class="capability-value">${noteRange.min}-${noteRange.max}</span>
                    </div>
                    
                    <div class="capability">
                        <span class="capability-icon">🎵</span>
                        <span class="capability-label">Polyphonie</span>
                        <span class="capability-value">${polyphony}</span>
                    </div>
                    
                    ${hasAir ? `
                        <div class="capability active">
                            <span class="capability-icon">💨</span>
                            <span class="capability-label">Air</span>
                            <span class="capability-value">✓</span>
                        </div>
                    ` : ''}
                    
                    ${hasLights ? `
                        <div class="capability active">
                            <span class="capability-icon">💡</span>
                            <span class="capability-label">Lumières</span>
                            <span class="capability-value">✓</span>
                        </div>
                    ` : ''}
                    
                    ${hasSensors ? `
                        <div class="capability active">
                            <span class="capability-icon">📡</span>
                            <span class="capability-label">Capteurs</span>
                            <span class="capability-value">✓</span>
                        </div>
                    ` : ''}
                    
                    ${hasCC ? `
                        <div class="capability active">
                            <span class="capability-icon">🎚️</span>
                            <span class="capability-label">CC MIDI</span>
                            <span class="capability-value">✓</span>
                        </div>
                    ` : ''}
                </div>
            </div>
        `;
    }

    /**
     * Construire les actions d'une carte d'instrument (AMÉLIORÉ)
     * @param {Object} instrument - Instrument
     * @returns {string} HTML des actions
     */
    buildInstrumentCardActions(instrument) {
        const actions = [];
        
        // ✨ NOUVEAU: Interroger (si SysEx et pas encore interrogé)
        if (instrument.sysexCapable && instrument.connected && !instrument.identityReceived) {
            actions.push(`
                <button class="action-btn query" 
                        onclick="event.stopPropagation(); app.controllers.instrument.queryInstrumentCapabilities('${instrument.id}')" 
                        title="Interroger capacités SysEx">
                    📡
                </button>
            `);
        }
        
        // ✨ NOUVEAU: Mapping (si SysEx et mapping reçu)
        if (instrument.sysexCapable && instrument.mappingReceived) {
            actions.push(`
                <button class="action-btn mapping" 
                        onclick="event.stopPropagation(); app.controllers.instrument.showNoteMappingModal('${instrument.id}')" 
                        title="Voir mapping notes">
                    🗺️
                </button>
            `);
        }
        
        // Connexion/Déconnexion
        if (instrument.connected) {
            actions.push(`
                <button class="action-btn disconnect" 
                        onclick="event.stopPropagation(); app.controllers.instrument.disconnectInstrument?.('${instrument.id}')" 
                        title="Déconnecter">
                    🔌❌
                </button>
            `);
        } else {
            actions.push(`
                <button class="action-btn connect" 
                        onclick="event.stopPropagation(); app.controllers.instrument.connectInstrument?.('${instrument.id}')" 
                        title="Connecter">
                    🔌✅
                </button>
            `);
        }
        
        // Test
        if (instrument.connected) {
            actions.push(`
                <button class="action-btn test" 
                        onclick="event.stopPropagation(); app.controllers.instrument.testInstrument('${instrument.id}')" 
                        title="Tester">
                    🎵
                </button>
            `);
        }
        
        // Calibrer (si connecté)
        if (instrument.connected) {
            actions.push(`
                <button class="action-btn calibrate" 
                        onclick="event.stopPropagation(); app.controllers.instrument.calibrateInstrument('${instrument.id}')" 
                        title="Calibrer latence">
                    🔧
                </button>
            `);
        }
        
        // Détails
        actions.push(`
            <button class="action-btn expand" 
                    onclick="event.stopPropagation(); this.toggleInstrumentExpansion('${instrument.id}')" 
                    title="Détails">
                ${this.localState.expandedInstruments.has(instrument.id) ? '🔽' : '🔼'}
            </button>
        `);
        
        // Menu
        actions.push(`
            <button class="action-btn menu" 
                    onclick="event.stopPropagation(); this.showInstrumentMenu('${instrument.id}', event)" 
                    title="Plus d'actions">
                ⋮
            </button>
        `);
        
        return actions.join('');
    }

    /**
     * Construire les métriques rapides d'un instrument
     * @param {Object} instrument - Instrument
     * @returns {string} HTML des métriques
     */
    buildInstrumentMetrics(instrument) {
        const { latency = 0, jitter = 0 } = instrument;
        const latencyClass = latency > 50 ? 'warning' : latency > 100 ? 'error' : 'good';
        
        return `
            <div class="instrument-metrics">
                <div class="metric">
                    <span class="metric-icon">⚡</span>
                    <span class="metric-label">Latence</span>
                    <span class="metric-value ${latencyClass}">${latency.toFixed(1)} ms</span>
                </div>
                
                ${jitter > 0 ? `
                    <div class="metric">
                        <span class="metric-icon">📊</span>
                        <span class="metric-label">Jitter</span>
                        <span class="metric-value">${jitter.toFixed(1)} ms</span>
                    </div>
                ` : ''}
            </div>
        `;
    }

    /**
     * Construire le corps étendu d'une carte d'instrument
     * @param {Object} instrument - Instrument
     * @param {Object} data - Données complètes
     * @returns {string} HTML du corps
     */
    buildInstrumentCardBody(instrument, data) {
        return `
            <div class="card-body">
                
                <!-- Onglets de contenu -->
                <div class="content-tabs">
                    <button class="tab-btn active" onclick="this.switchInstrumentTab('${instrument.id}', 'info')">
                        ℹ️ Informations
                    </button>
                    <button class="tab-btn" onclick="this.switchInstrumentTab('${instrument.id}', 'metrics')">
                        📊 Métriques
                    </button>
                    <button class="tab-btn" onclick="this.switchInstrumentTab('${instrument.id}', 'config')">
                        ⚙️ Configuration
                    </button>
                    <button class="tab-btn" onclick="this.switchInstrumentTab('${instrument.id}', 'log')">
                        📝 Journal
                    </button>
                </div>
                
                <!-- Contenu des onglets -->
                <div class="tab-content">
                    <div class="tab-pane active" data-tab="info">
                        ${this.buildInstrumentInfoTab(instrument)}
                    </div>
                    <div class="tab-pane" data-tab="metrics">
                        ${this.buildInstrumentMetricsTab(instrument)}
                    </div>
                    <div class="tab-pane" data-tab="config">
                        ${this.buildInstrumentConfigTab(instrument)}
                    </div>
                    <div class="tab-pane" data-tab="log">
                        ${this.buildInstrumentLogTab(instrument)}
                    </div>
                </div>
                
            </div>
        `;
    }

    /**
     * Construire l'onglet d'informations d'un instrument
     * @param {Object} instrument - Instrument
     * @returns {string} HTML de l'onglet
     */
    buildInstrumentInfoTab(instrument) {
        return `
            <div class="instrument-info-details">
                
                <!-- Informations générales -->
                <div class="info-section">
                    <h5>📋 Général</h5>
                    <div class="info-grid">
                        <div class="info-item">
                            <label>ID :</label>
                            <span>${instrument.id}</span>
                        </div>
                        <div class="info-item">
                            <label>Modèle :</label>
                            <span>${instrument.model || 'Non spécifié'}</span>
                        </div>
                        <div class="info-item">
                            <label>Version :</label>
                            <span>${instrument.version || 'N/A'}</span>
                        </div>
                        <div class="info-item">
                            <label>Port :</label>
                            <span>${instrument.port || 'N/A'}</span>
                        </div>
                        ${instrument.serialNumber ? `
                        <div class="info-item">
                            <label>Numéro de série :</label>
                            <span>${instrument.serialNumber}</span>
                        </div>
                        ` : ''}
                    </div>
                </div>
                
                <!-- Spécifications techniques -->
                <div class="info-section">
                    <h5>🔧 Spécifications</h5>
                    <div class="info-grid">
                        <div class="info-item">
                            <label>Plage de notes :</label>
                            <span>${instrument.noteRange ? `${instrument.noteRange.min}-${instrument.noteRange.max}` : 'N/A'}</span>
                        </div>
                        <div class="info-item">
                            <label>Canaux :</label>
                            <span>${instrument.channels ? instrument.channels.join(', ') : 'N/A'}</span>
                        </div>
                        <div class="info-item">
                            <label>Polyphonie max :</label>
                            <span>${instrument.maxPolyphony || instrument.polyphony || 'N/A'}</span>
                        </div>
                        <div class="info-item">
                            <label>Latence :</label>
                            <span>${instrument.latency || 0}ms</span>
                        </div>
                    </div>
                </div>
                
                <!-- État et historique -->
                <div class="info-section">
                    <h5>📊 État</h5>
                    <div class="info-grid">
                        <div class="info-item">
                            <label>Dernière connexion :</label>
                            <span>${instrument.lastSeen ? this.formatDate(instrument.lastSeen, { relative: true }) : 'Jamais'}</span>
                        </div>
                        <div class="info-item">
                            <label>Temps de fonctionnement :</label>
                            <span>${this.formatUptime(instrument.uptime || 0)}</span>
                        </div>
                    </div>
                </div>
                
            </div>
        `;
    }

    /**
     * Construire l'onglet de métriques d'un instrument
     * @param {Object} instrument - Instrument
     * @returns {string} HTML de l'onglet
     */
    buildInstrumentMetricsTab(instrument) {
        return `
            <div class="instrument-metrics-details">
                <div class="metrics-section">
                    <h5>⚡ Performance</h5>
                    <div class="metrics-grid">
                        <div class="metric-card">
                            <div class="metric-value">${instrument.latency || 0}ms</div>
                            <div class="metric-label">Latence</div>
                        </div>
                        ${instrument.jitter ? `
                        <div class="metric-card">
                            <div class="metric-value">${instrument.jitter}ms</div>
                            <div class="metric-label">Jitter</div>
                        </div>
                        ` : ''}
                    </div>
                </div>
            </div>
        `;
    }

    /**
     * Construire l'onglet de configuration
     * @param {Object} instrument - Instrument
     * @returns {string} HTML
     */
    buildInstrumentConfigTab(instrument) {
        return `<div class="config-placeholder">Configuration à venir...</div>`;
    }

    /**
     * Construire l'onglet de journal
     * @param {Object} instrument - Instrument
     * @returns {string} HTML
     */
    buildInstrumentLogTab(instrument) {
        return `<div class="log-placeholder">Journal à venir...</div>`;
    }

    /**
     * Construire le panneau de santé globale
     * @param {Object} healthStatus - État de santé
     * @returns {string} HTML du panneau
     */
    buildHealthPanel(healthStatus) {
        if (!healthStatus || Object.keys(healthStatus).length === 0) {
            return '';
        }
        
        return `
            <div class="health-panel">
                <h4>🏥 État de santé</h4>
                
                <div class="health-score">
                    <div class="score-circle ${healthStatus.overall}">
                        <span class="score-value">${healthStatus.score || 0}</span>
                        <span class="score-max">/100</span>
                    </div>
                    <div class="score-status">
                        ${this.getHealthStatusText(healthStatus.overall)}
                    </div>
                </div>
                
            </div>
        `;
    }

    /**
     * Construire le panneau de performance
     * @param {Object} performanceMetrics - Métriques de performance
     * @returns {string} HTML du panneau
     */
    buildPerformancePanel(performanceMetrics) {
        if (!performanceMetrics || Object.keys(performanceMetrics).length === 0) {
            return '';
        }
        
        return `
            <div class="performance-panel">
                <h4>📊 Performance système</h4>
                
                <div class="performance-metrics">
                    
                    <div class="metric-item">
                        <div class="metric-icon">🎹</div>
                        <div class="metric-info">
                            <div class="metric-value">${performanceMetrics.activeInstruments || 0}</div>
                            <div class="metric-label">Instruments actifs</div>
                        </div>
                    </div>
                    
                    <div class="metric-item">
                        <div class="metric-icon">⚡</div>
                        <div class="metric-info">
                            <div class="metric-value">${Math.round(performanceMetrics.averageLatency || 0)}ms</div>
                            <div class="metric-label">Latence moyenne</div>
                        </div>
                    </div>
                    
                </div>
                
            </div>
        `;
    }

    /**
     * Construire les détails de l'instrument sélectionné
     * @param {Object} data - Données
     * @returns {string} HTML des détails
     */
    buildInstrumentDetails(data) {
        const selectedIds = Array.from(this.localState.selectedInstruments);
        
        if (selectedIds.length === 0) {
            return `
                <div class="details-panel">
                    <div class="no-selection">
                        <div class="placeholder-icon">🎹</div>
                        <p>Sélectionnez un instrument pour voir les détails</p>
                    </div>
                </div>
            `;
        }
        
        return '<div class="details-panel"><p>Détails instrument sélectionné</p></div>';
    }

    /**
     * Construire les actions globales
     * @param {Object} data - Données
     * @returns {string} HTML des actions
     */
    buildGlobalActions(data) {
        return `
            <div class="global-actions-panel">
                <h4>⚙️ Actions globales</h4>
                
                <div class="action-buttons">
                    
                    <div class="action-group">
                        <h5>Connexions</h5>
                        <button class="btn btn-block btn-primary" onclick="app.controllers.instrument.startDiscovery()">
                            🔍 Détecter nouveaux
                        </button>
                        <button class="btn btn-block btn-secondary" onclick="app.controllers.instrument.scanInstruments()">
                            🔄 Scanner
                        </button>
                    </div>
                    
                </div>
                
            </div>
        `;
    }

    /**
     * Construire la barre de statut
     * @param {Object} data - Données
     * @returns {string} HTML de la barre de statut
     */
    buildInstrumentStatusBar(data) {
        const totalInstruments = (data.instruments || data.filteredInstruments || []).length;
        const selectedCount = this.localState.selectedInstruments.size;
        const lastUpdate = this.formatDate(new Date(this.localState.lastUpdate), { relative: true });
        
        return `
            <div class="instrument-status-bar">
                
                <div class="status-info">
                    <span>${totalInstruments} instrument(s)</span>
                    ${selectedCount > 0 ? `<span> • ${selectedCount} sélectionné(s)</span>` : ''}
                    <span> • Dernière maj: ${lastUpdate}</span>
                </div>
                
                <div class="monitoring-indicators">
                    <div class="indicator ${this.localState.monitoringEnabled ? 'active' : 'inactive'}">
                        <span class="indicator-dot"></span>
                        <span>Monitoring</span>
                    </div>
                </div>
                
            </div>
        `;
    }

    /**
     * Construire l'état vide des instruments
     * @param {Object} data - Données
     * @returns {string} HTML de l'état vide
     */
    buildEmptyInstrumentsState(data) {
        return `
            <div class="empty-instruments-state">
                <div class="empty-icon">🎹</div>
                <h3>Aucun instrument détecté</h3>
                <p>Connectez des instruments MIDI pour commencer.</p>
                <div class="empty-actions">
                    <button class="btn btn-primary" onclick="app.controllers.instrument.startDiscovery()">
                        🔍 Détecter des instruments
                    </button>
                </div>
            </div>
        `;
    }

    /**
     * ✨ NOUVEAU: Affiche le modal de mapping notes
     * @param {Object} instrument - Instrument avec noteMapping
     */
    showNoteMappingModal(instrument) {
        if (!instrument.noteMapping || !instrument.noteMapping.entries) {
            console.warn('No note mapping available for', instrument.name);
            return;
        }
        
        const { entries } = instrument.noteMapping;
        
        const modalHTML = `
            <div class="modal-overlay" id="note-mapping-modal">
                <div class="modal-content modal-large">
                    <div class="modal-header">
                        <h2>🗺️ Mapping Notes - ${this.escapeHtml(instrument.name)}</h2>
                        <button class="btn-close-modal" aria-label="Fermer">×</button>
                    </div>
                    
                    <div class="modal-body">
                        <div class="mapping-info">
                            <p><strong>${entries.length}</strong> entrées de mapping</p>
                        </div>
                        
                        <div class="mapping-table-container">
                            <table class="mapping-table">
                                <thead>
                                    <tr>
                                        <th>MIDI Note</th>
                                        <th>Note</th>
                                        <th>Sortie Physique</th>
                                        <th>Nom</th>
                                        <th>État</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    ${entries.map(entry => this.buildMappingRow(entry)).join('')}
                                </tbody>
                            </table>
                        </div>
                    </div>
                    
                    <div class="modal-footer">
                        <button class="btn btn-secondary btn-close-modal">Fermer</button>
                    </div>
                </div>
            </div>
        `;
        
        document.body.insertAdjacentHTML('beforeend', modalHTML);
        this.setupModalEvents();
    }

    /**
     * ✨ NOUVEAU: Construit une ligne du tableau de mapping
     */
    buildMappingRow(entry) {
        const { midiNote, physicalId, name, enabled } = entry;
        const noteName = this.getMidiNoteName(midiNote);
        const statusClass = enabled ? 'enabled' : 'disabled';
        const statusIcon = enabled ? '✅' : '❌';
        
        return `
            <tr class="mapping-row ${statusClass}">
                <td class="text-center"><strong>${midiNote}</strong></td>
                <td>${noteName}</td>
                <td class="text-center">${physicalId}</td>
                <td>${this.escapeHtml(name)}</td>
                <td class="text-center">
                    <span class="status-badge ${statusClass}">${statusIcon}</span>
                </td>
            </tr>
        `;
    }

    /**
     * ✨ NOUVEAU: Configure les événements du modal
     */
    setupModalEvents() {
        const modal = document.getElementById('note-mapping-modal');
        if (!modal) return;
        
        modal.querySelectorAll('.btn-close-modal').forEach(btn => {
            btn.addEventListener('click', () => this.closeModal());
        });
        
        modal.querySelector('.modal-overlay').addEventListener('click', (e) => {
            if (e.target.classList.contains('modal-overlay')) {
                this.closeModal();
            }
        });
        
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape') this.closeModal();
        });
    }

    /**
     * ✨ NOUVEAU: Ferme le modal
     */
    closeModal() {
        const modal = document.getElementById('note-mapping-modal');
        if (modal) modal.remove();
    }

    // ===== MÉTHODES D'INTERACTION =====

    setupInstrumentInteractions() {
        // Configuration sera faite ici
    }

    startMonitoring() {
        if (this.localState.refreshInterval) return;
        
        this.localState.refreshInterval = setInterval(() => {
            if (this.localState.monitoringEnabled) {
                this.updateMonitoringData();
            }
        }, this.displayConfig.refreshRate);
    }

    stopMonitoring() {
        if (this.localState.refreshInterval) {
            clearInterval(this.localState.refreshInterval);
            this.localState.refreshInterval = null;
        }
    }

    toggleMonitoring() {
        this.localState.monitoringEnabled = !this.localState.monitoringEnabled;
        this.render(this.data);
    }

    handleInstrumentClick(instrumentId, event) {
        const multiSelect = event.ctrlKey || event.metaKey;
        
        if (multiSelect) {
            if (this.localState.selectedInstruments.has(instrumentId)) {
                this.localState.selectedInstruments.delete(instrumentId);
            } else {
                this.localState.selectedInstruments.add(instrumentId);
            }
        } else {
            this.localState.selectedInstruments.clear();
            this.localState.selectedInstruments.add(instrumentId);
        }
        
        this.updateSelectionDisplay();
    }

    toggleInstrumentExpansion(instrumentId) {
        if (this.localState.expandedInstruments.has(instrumentId)) {
            this.localState.expandedInstruments.delete(instrumentId);
        } else {
            this.localState.expandedInstruments.add(instrumentId);
        }
        
        this.render(this.data);
    }

    updateMonitoringData() {
        this.localState.lastUpdate = Date.now();
    }

    updateSelectionDisplay() {
        const cards = this.getElements('.instrument-card');
        cards.forEach(card => {
            const instrumentId = card.dataset.instrumentId;
            if (this.localState.selectedInstruments.has(instrumentId)) {
                card.classList.add('selected');
            } else {
                card.classList.remove('selected');
            }
        });
    }

    toggleShowDisconnected() {
        this.displayConfig.showDisconnected = !this.displayConfig.showDisconnected;
        this.render(this.data);
    }

    toggleCompactMode() {
        this.displayConfig.compactMode = !this.displayConfig.compactMode;
        this.render(this.data);
    }

    toggleShowMetrics() {
        this.displayConfig.showMetrics = !this.displayConfig.showMetrics;
        this.render(this.data);
    }

    // ===== MÉTHODES UTILITAIRES =====

    getInstrumentIcon(type) {
        const icons = {
            'Cordes': '🎻',
            'Vents': '🎺',
            'Percussions': '🥁',
            'Clavier': '🎹',
            'Électronique': '🎛️'
        };
        return icons[type] || '🎵';
    }

    getGlobalHealthClass(healthStatus) {
        if (!healthStatus) return 'unknown';
        return healthStatus.overall || 'unknown';
    }

    getGlobalHealthText(healthStatus) {
        if (!healthStatus) return 'Inconnu';
        const statusTexts = {
            good: 'Excellent',
            warning: 'Attention',
            critical: 'Critique',
            unknown: 'Inconnu'
        };
        return statusTexts[healthStatus.overall] || 'Inconnu';
    }

    getHealthStatusText(status) {
        const texts = {
            good: '✅ Tout va bien',
            warning: '⚠️ Attention requise',
            critical: '🚨 Action urgente',
            unknown: '❓ État inconnu'
        };
        return texts[status] || texts.unknown;
    }

    getMidiNoteName(note) {
        const noteNames = ['Do', 'Do#', 'Ré', 'Ré#', 'Mi', 'Fa', 'Fa#', 'Sol', 'Sol#', 'La', 'La#', 'Si'];
        const octave = Math.floor(note / 12) - 1;
        const noteName = noteNames[note % 12];
        return `${noteName}${octave}`;
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    formatDate(date, options = {}) {
        if (!date) return 'N/A';
        const d = new Date(date);
        if (options.relative) {
            const now = Date.now();
            const diff = now - d.getTime();
            if (diff < 60000) return 'À l\'instant';
            if (diff < 3600000) return `Il y a ${Math.floor(diff / 60000)}m`;
            if (diff < 86400000) return `Il y a ${Math.floor(diff / 3600000)}h`;
            return d.toLocaleDateString('fr-FR');
        }
        return d.toLocaleString('fr-FR');
    }

    formatUptime(uptime) {
        if (uptime < 60000) {
            return `${Math.round(uptime / 1000)}s`;
        } else if (uptime < 3600000) {
            return `${Math.round(uptime / 60000)}min`;
        } else {
            return `${Math.round(uptime / 3600000)}h`;
        }
    }
}