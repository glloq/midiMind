// ============================================================================
// Fichier: frontend/scripts/views/RoutingView.js
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Vue principale pour l'interface de routage MIDI.
//   Affiche la matrice de routage et les contrôles par canal.
//
// Auteur: midiMind Team
// Date: 2025-10-04
// Version: 3.0.0
// ============================================================================

class RoutingView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        // Configuration spécifique
        this.config.autoRender = true;
        this.config.preserveState = true;
        
        // Composants enfants
        this.routingMatrix = null;
        
        // État local de la vue
        this.viewState = {
            selectedChannel: null,
            showAdvanced: false,
            compactMode: false
        };
        
        this.initialize();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    initialize() {
        this.bindCustomEvents();
    }
    
    bindCustomEvents() {
        // Écouter les changements du modèle de routage
        this.eventBus.on('routing:channel-assigned', () => this.updateView());
        this.eventBus.on('routing:channel-muted', () => this.updateView());
        this.eventBus.on('routing:channel-solo', () => this.updateView());
        this.eventBus.on('routing:devices-updated', () => this.render(this.data));
        this.eventBus.on('routing:preset-loaded', () => this.render(this.data));
    }
    
    // ========================================================================
    // RENDU
    // ========================================================================
    
    /**
     * Construire le template HTML
     */
    buildTemplate(data) {
        const { 
            channels = [], 
            devices = [], 
            masterVolume = 100,
            presets = [],
            activePreset = null
        } = data;
        
        return `
            <div class="routing-view">
                <!-- En-tête avec titre et actions -->
                <div class="routing-header">
                    <div class="routing-title">
                        <h2>🔀 Routage MIDI</h2>
                        <span class="routing-subtitle">
                            ${channels.filter(ch => ch.device).length}/16 canaux assignés
                        </span>
                    </div>
                    
                    <div class="routing-actions">
                        <!-- Presets -->
                        <div class="preset-controls">
                            <select class="preset-selector" id="presetSelector">
                                <option value="">-- Preset --</option>
                                ${this.buildPresetOptions(presets, activePreset)}
                            </select>
                            <button class="btn btn-sm" onclick="app.routingController.savePreset()">
                                💾 Save
                            </button>
                            <button class="btn btn-sm btn-danger" onclick="app.routingController.deleteCurrentPreset()">
                                🗑️
                            </button>
                        </div>
                        
                        <!-- Actions globales -->
                        <div class="global-actions">
                            <button class="btn btn-sm" onclick="app.routingController.muteAll()">
                                🔇 Mute All
                            </button>
                            <button class="btn btn-sm" onclick="app.routingController.unmuteAll()">
                                🔊 Unmute All
                            </button>
                            <button class="btn btn-sm btn-danger" onclick="app.routingController.resetAll()">
                                🔄 Reset
                            </button>
                        </div>
                        
                        <!-- Mode d'affichage -->
                        <div class="view-mode-toggle">
                            <button class="btn btn-sm ${!this.viewState.compactMode ? 'active' : ''}"
                                    onclick="app.routingView.setCompactMode(false)">
                                📊 Détaillé
                            </button>
                            <button class="btn btn-sm ${this.viewState.compactMode ? 'active' : ''}"
                                    onclick="app.routingView.setCompactMode(true)">
                                📱 Compact
                            </button>
                        </div>
                    </div>
                </div>
                
                <!-- Volume Master -->
                <div class="master-volume-section">
                    <label class="master-volume-label">
                        🎚️ Volume Master
                    </label>
                    <div class="volume-slider-container">
                        <input type="range" 
                               class="volume-slider master-volume-slider"
                               min="0" max="127" 
                               value="${masterVolume}"
                               oninput="app.routingController.setMasterVolume(this.value)">
                        <span class="volume-value">${Math.round((masterVolume / 127) * 100)}%</span>
                    </div>
                </div>
                
                <!-- Contenu principal -->
                <div class="routing-content">
                    ${this.viewState.compactMode ? 
                        this.buildCompactView(channels, devices) :
                        this.buildDetailedView(channels, devices)
                    }
                </div>
                
                <!-- Matrice de routage (mode détaillé uniquement) -->
                ${!this.viewState.compactMode ? `
                    <div class="routing-matrix-container" id="routingMatrixContainer">
                        <!-- La matrice sera insérée ici -->
                    </div>
                ` : ''}
                
                <!-- Panneau de détails du canal sélectionné -->
                ${this.viewState.selectedChannel ? 
                    this.buildChannelDetails(channels[this.viewState.selectedChannel - 1]) : ''
                }
            </div>
        `;
    }
    
    /**
     * Construire la vue détaillée (avec tous les contrôles)
     */
    buildDetailedView(channels, devices) {
        return `
            <div class="channels-grid detailed">
                ${channels.map(channel => `
                    <div class="channel-card ${channel.device ? 'assigned' : ''} 
                                ${channel.active ? 'active' : ''}"
                         data-channel="${channel.number}"
                         style="border-color: ${channel.color};">
                        
                        <!-- En-tête du canal -->
                        <div class="channel-header">
                            <div class="channel-number" style="background: ${channel.color};">
                                ${channel.number}
                            </div>
                            <div class="channel-name">
                                ${channel.name}
                            </div>
                            <div class="channel-indicators">
                                ${channel.active ? '<span class="indicator-active">●</span>' : ''}
                                ${channel.muted ? '<span class="indicator-mute">M</span>' : ''}
                                ${channel.solo ? '<span class="indicator-solo">S</span>' : ''}
                            </div>
                        </div>
                        
                        <!-- Device assigné -->
                        <div class="channel-device">
                            <select class="device-selector" 
                                    onchange="app.routingController.assignChannelToDevice(${channel.number}, this.value)">
                                <option value="">-- Non assigné --</option>
                                ${devices.map(device => `
                                    <option value="${device.id}" 
                                            ${channel.device === device.id ? 'selected' : ''}>
                                        ${device.name} ${device.status === 'connected' ? '✓' : '✗'}
                                    </option>
                                `).join('')}
                            </select>
                        </div>
                        
                        <!-- Contrôles Mute/Solo -->
                        <div class="channel-controls">
                            <button class="btn-mute ${channel.muted ? 'active' : ''}"
                                    onclick="app.routingController.muteChannel(${channel.number})">
                                M
                            </button>
                            <button class="btn-solo ${channel.solo ? 'active' : ''}"
                                    onclick="app.routingController.soloChannel(${channel.number})">
                                S
                            </button>
                        </div>
                        
                        <!-- Volume -->
                        <div class="channel-volume">
                            <label>Vol</label>
                            <input type="range" 
                                   class="volume-slider"
                                   min="0" max="127" 
                                   value="${channel.volume}"
                                   oninput="app.routingController.setChannelVolume(${channel.number}, this.value)">
                            <span class="volume-value">${Math.round((channel.volume / 127) * 100)}%</span>
                        </div>
                        
                        <!-- Pan -->
                        <div class="channel-pan">
                            <label>Pan</label>
                            <input type="range" 
                                   class="pan-slider"
                                   min="0" max="127" 
                                   value="${channel.pan}"
                                   oninput="app.routingController.setChannelPan(${channel.number}, this.value)">
                            <span class="pan-value">${this.formatPan(channel.pan)}</span>
                        </div>
                        
                        <!-- Transposition -->
                        <div class="channel-transpose">
                            <label>Transpose</label>
                            <input type="number" 
                                   class="transpose-input"
                                   min="-24" max="24" 
                                   value="${channel.transpose}"
                                   onchange="app.routingController.setChannelTranspose(${channel.number}, this.value)">
                            <span class="transpose-unit">st</span>
                        </div>
                        
                        <!-- Statistiques -->
                        <div class="channel-stats">
                            <span class="stat-notes">♪ ${channel.noteCount}</span>
                            ${channel.lastNote ? 
                                `<span class="stat-last-note">Last: ${this.getNoteName(channel.lastNote)}</span>` 
                                : ''
                            }
                        </div>
                    </div>
                `).join('')}
            </div>
        `;
    }
    
    /**
     * Construire la vue compacte (minimaliste)
     */
    buildCompactView(channels, devices) {
        return `
            <div class="channels-list compact">
                <table class="channels-table">
                    <thead>
                        <tr>
                            <th>Ch</th>
                            <th>Device</th>
                            <th>M</th>
                            <th>S</th>
                            <th>Vol</th>
                            <th>Pan</th>
                            <th>Tr</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${channels.map(channel => `
                            <tr class="channel-row ${channel.device ? 'assigned' : ''}"
                                data-channel="${channel.number}">
                                <td class="channel-number">
                                    <span style="color: ${channel.color};">●</span> ${channel.number}
                                </td>
                                <td class="channel-device">
                                    <select class="device-selector-compact" 
                                            onchange="app.routingController.assignChannelToDevice(${channel.number}, this.value)">
                                        <option value="">--</option>
                                        ${devices.map(device => `
                                            <option value="${device.id}" 
                                                    ${channel.device === device.id ? 'selected' : ''}>
                                                ${device.name}
                                            </option>
                                        `).join('')}
                                    </select>
                                </td>
                                <td>
                                    <button class="btn-mute-compact ${channel.muted ? 'active' : ''}"
                                            onclick="app.routingController.muteChannel(${channel.number})">
                                        ${channel.muted ? '✓' : ''}
                                    </button>
                                </td>
                                <td>
                                    <button class="btn-solo-compact ${channel.solo ? 'active' : ''}"
                                            onclick="app.routingController.soloChannel(${channel.number})">
                                        ${channel.solo ? '✓' : ''}
                                    </button>
                                </td>
                                <td>
                                    <input type="number" 
                                           class="volume-input-compact"
                                           min="0" max="127" 
                                           value="${channel.volume}"
                                           onchange="app.routingController.setChannelVolume(${channel.number}, this.value)">
                                </td>
                                <td>
                                    <input type="number" 
                                           class="pan-input-compact"
                                           min="0" max="127" 
                                           value="${channel.pan}"
                                           onchange="app.routingController.setChannelPan(${channel.number}, this.value)">
                                </td>
                                <td>
                                    <input type="number" 
                                           class="transpose-input-compact"
                                           min="-24" max="24" 
                                           value="${channel.transpose}"
                                           onchange="app.routingController.setChannelTranspose(${channel.number}, this.value)">
                                </td>
                            </tr>
                        `).join('')}
                    </tbody>
                </table>
            </div>
        `;
    }
    
    /**
     * Construire les options de presets
     */
    buildPresetOptions(presets, activePreset) {
        return presets.map(preset => `
            <option value="${preset.id}" ${preset.id === activePreset ? 'selected' : ''}>
                ${preset.name}
            </option>
        `).join('');
    }
    
    /**
     * Construire le panneau de détails d'un canal
     */
    buildChannelDetails(channel) {
        if (!channel) return '';
        
        return `
            <div class="channel-details-panel">
                <div class="details-header">
                    <h3>Canal ${channel.number} - ${channel.name}</h3>
                    <button class="btn-close" onclick="app.routingView.closeChannelDetails()">✕</button>
                </div>
                
                <div class="details-content">
                    <!-- Configuration avancée -->
                    <div class="details-section">
                        <h4>Configuration MIDI</h4>
                        
                        <div class="detail-row">
                            <label>Program Change:</label>
                            <input type="number" min="0" max="127" 
                                   value="${channel.programChange || 0}"
                                   onchange="app.routingController.setChannelProgram(${channel.number}, this.value)">
                        </div>
                        
                        <div class="detail-row">
                            <label>Bank Select:</label>
                            <input type="number" min="0" max="127" 
                                   value="${channel.bankSelect || 0}"
                                   onchange="app.routingController.setChannelBank(${channel.number}, this.value)">
                        </div>
                    </div>
                    
                    <!-- Effets -->
                    <div class="details-section">
                        <h4>Effets</h4>
                        
                        <div class="detail-row">
                            <label>Reverb:</label>
                            <input type="range" min="0" max="127" 
                                   value="${channel.effects?.reverb || 0}"
                                   onchange="app.routingController.setChannelEffect(${channel.number}, 'reverb', this.value)">
                        </div>
                        
                        <div class="detail-row">
                            <label>Chorus:</label>
                            <input type="range" min="0" max="127" 
                                   value="${channel.effects?.chorus || 0}"
                                   onchange="app.routingController.setChannelEffect(${channel.number}, 'chorus', this.value)">
                        </div>
                        
                        <div class="detail-row">
                            <label>Delay:</label>
                            <input type="range" min="0" max="127" 
                                   value="${channel.effects?.delay || 0}"
                                   onchange="app.routingController.setChannelEffect(${channel.number}, 'delay', this.value)">
                        </div>
                    </div>
                    
                    <!-- Statistiques -->
                    <div class="details-section">
                        <h4>Statistiques</h4>
                        <p>Notes jouées: ${channel.noteCount}</p>
                        <p>Dernière note: ${channel.lastNote ? this.getNoteName(channel.lastNote) : 'Aucune'}</p>
                        <p>Dernière activité: ${channel.lastActivity ? 
                            new Date(channel.lastActivity).toLocaleTimeString() : 'Jamais'}</p>
                    </div>
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // APRÈS RENDU
    // ========================================================================
    
    afterRender() {
        // Créer le composant RoutingMatrix si nécessaire
        if (!this.viewState.compactMode) {
            const container = document.getElementById('routingMatrixContainer');
            if (container && typeof RoutingMatrix !== 'undefined') {
                this.routingMatrix = new RoutingMatrix(container, this.data);
            }
        }
        
        // Attacher les événements personnalisés
        this.attachChannelEvents();
    }
    
    attachChannelEvents() {
        // Click sur les cartes de canaux pour sélection
        const cards = this.container.querySelectorAll('.channel-card');
        cards.forEach(card => {
            card.addEventListener('click', (e) => {
                if (e.target.tagName !== 'BUTTON' && e.target.tagName !== 'INPUT' && e.target.tagName !== 'SELECT') {
                    const channelNumber = parseInt(card.dataset.channel);
                    this.selectChannel(channelNumber);
                }
            });
        });
    }
    
    // ========================================================================
    // ACTIONS
    // ========================================================================
    
    /**
     * Sélectionner un canal
     */
    selectChannel(channelNumber) {
        this.viewState.selectedChannel = channelNumber;
        this.render(this.data);
    }
    
    /**
     * Fermer le panneau de détails
     */
    closeChannelDetails() {
        this.viewState.selectedChannel = null;
        this.render(this.data);
    }
    
    /**
     * Changer le mode d'affichage
     */
    setCompactMode(compact) {
        this.viewState.compactMode = compact;
        this.render(this.data);
    }
    
    /**
     * Mettre à jour la vue (sans re-render complet)
     */
    updateView() {
        // Mise à jour partielle des éléments DOM
        // Plus efficace que re-render complet
        
        // Exemple: mettre à jour les indicateurs mute/solo
        if (this.data.channels) {
            this.data.channels.forEach(channel => {
                const card = this.container.querySelector(`[data-channel="${channel.number}"]`);
                if (card) {
                    // Mettre à jour les classes
                    card.classList.toggle('muted', channel.muted);
                    card.classList.toggle('solo', channel.solo);
                    
                    // Mettre à jour les boutons
                    const muteBtn = card.querySelector('.btn-mute, .btn-mute-compact');
                    const soloBtn = card.querySelector('.btn-solo, .btn-solo-compact');
                    
                    if (muteBtn) {
                        muteBtn.classList.toggle('active', channel.muted);
                    }
                    if (soloBtn) {
                        soloBtn.classList.toggle('active', channel.solo);
                    }
                }
            });
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Formater la valeur de pan
     */
    formatPan(value) {
        if (value === 64) return 'C';
        if (value < 64) return `L${64 - value}`;
        return `R${value - 64}`;
    }
    
    /**
     * Obtenir le nom d'une note MIDI
     */
    getNoteName(noteNumber) {
        if (typeof MidiConstants !== 'undefined') {
            return MidiConstants.getNoteName(noteNumber);
        }
        
        const notes = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const octave = Math.floor(noteNumber / 12) - 1;
        const note = notes[noteNumber % 12];
        return `${note}${octave}`;
    }
}