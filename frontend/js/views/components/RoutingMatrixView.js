// ============================================================================
// Fichier: frontend/scripts/views/components/RoutingMatrixView.js
// Projet: midiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// Description:
//   Vue interactive de la matrice de routage MIDI
//   Permet d'assigner visuellement les canaux aux instruments
// ============================================================================

class RoutingMatrixView {
    constructor(container, routingManager, eventBus) {
        this.container = typeof container === 'string' ? 
            document.getElementById(container) : container;
        this.routingManager = routingManager;
        this.eventBus = eventBus;
        
        // État
        this.mode = 'matrix'; // 'matrix', 'list', 'compact'
        this.selectedChannel = null;
        this.hoveredCell = null;
        
        // Éléments DOM
        this.elements = {};
    }

    // ========================================================================
    // RENDU
    // ========================================================================

    render() {
        this.container.innerHTML = `
            <div class="routing-matrix-container">
                <!-- Header -->
                <div class="routing-header">
                    <h3>🎛️ MIDI Routing</h3>
                    <div class="routing-actions">
                        ${this.renderModeSelector()}
                        ${this.renderActions()}
                    </div>
                </div>
                
                <!-- Stats -->
                <div class="routing-stats">
                    ${this.renderStats()}
                </div>
                
                <!-- Matrice ou Liste -->
                <div class="routing-content" id="routingContent">
                    ${this.mode === 'matrix' ? 
                        this.renderMatrix() : 
                        this.renderList()}
                </div>
                
                <!-- Presets -->
                <div class="routing-presets">
                    ${this.renderPresets()}
                </div>
                
                <!-- Conflicts -->
                ${this.renderConflicts()}
            </div>
        `;
        
        this.cacheElements();
        this.attachEvents();
    }

    /**
     * Rendu du sélecteur de mode
     */
    renderModeSelector() {
        return `
            <div class="mode-selector">
                <button class="mode-btn ${this.mode === 'matrix' ? 'active' : ''}" 
                        data-mode="matrix" title="Matrix View">
                    ⊞ Matrix
                </button>
                <button class="mode-btn ${this.mode === 'list' ? 'active' : ''}" 
                        data-mode="list" title="List View">
                    ≡ List
                </button>
            </div>
        `;
    }

    /**
     * Rendu des actions
     */
    renderActions() {
        return `
            <div class="action-buttons">
                <button class="action-btn" id="autoRouteBtn" title="Auto Route">
                    🤖 Auto
                </button>
                <button class="action-btn" id="clearRouteBtn" title="Clear All">
                    🗑️ Clear
                </button>
                <button class="action-btn" id="savePresetBtn" title="Save Preset">
                    💾 Save
                </button>
            </div>
        `;
    }

    /**
     * Rendu des statistiques
     */
    renderStats() {
        const stats = this.routingManager.getStats();
        const percentage = stats.totalChannels > 0 ? 
            Math.round((stats.assignedChannels / stats.totalChannels) * 100) : 0;
        
        return `
            <div class="stat-item">
                <span class="stat-label">Channels:</span>
                <span class="stat-value">${stats.assignedChannels}/${stats.totalChannels}</span>
                <div class="stat-bar">
                    <div class="stat-fill" style="width: ${percentage}%"></div>
                </div>
            </div>
            <div class="stat-item">
                <span class="stat-label">Compatibility:</span>
                <span class="stat-value ${this.getCompatibilityClass(stats.compatibilityScore)}">
                    ${Math.round(stats.compatibilityScore * 100)}%
                </span>
            </div>
            <div class="stat-item">
                <span class="stat-label">Status:</span>
                <span class="stat-value ${this.routingManager.isValid ? 'valid' : 'invalid'}">
                    ${this.routingManager.isValid ? '✓ Valid' : '⚠ Issues'}
                </span>
            </div>
        `;
    }

    /**
     * Rendu de la matrice
     */
    renderMatrix() {
        const channels = this.routingManager.midiChannels;
        const instruments = this.routingManager.instruments;
        
        if (channels.length === 0) {
            return '<div class="empty-state">No MIDI channels detected</div>';
        }
        
        if (instruments.length === 0) {
            return '<div class="empty-state">No instruments available</div>';
        }
        
        let html = '<div class="routing-matrix">';
        
        // Header avec instruments
        html += '<div class="matrix-header">';
        html += '<div class="matrix-cell header-cell corner">CH / Inst</div>';
        
        instruments.forEach(instrument => {
            const isUsed = Array.from(this.routingManager.routing.assignments.values())
                .some(a => a.instrumentId === instrument.id);
            
            html += `
                <div class="matrix-cell header-cell instrument-header ${isUsed ? 'used' : ''}"
                     data-instrument="${instrument.id}">
                    <div class="instrument-icon">${this.getInstrumentIcon(instrument.type)}</div>
                    <div class="instrument-name">${instrument.name}</div>
                    <div class="instrument-status ${instrument.state}">${instrument.state}</div>
                </div>
            `;
        });
        
        html += '</div>';
        
        // Lignes pour chaque canal
        channels.forEach(channel => {
            const assignment = this.routingManager.getAssignment(channel.number);
            
            html += '<div class="matrix-row" data-channel="' + channel.number + '">';
            
            // Header du canal
            html += `
                <div class="matrix-cell channel-header">
                    <div class="channel-number">CH${channel.number + 1}</div>
                    <div class="channel-info">
                        <div class="channel-instrument">${channel.instrument}</div>
                        <div class="channel-notes">${channel.noteCount} notes</div>
                    </div>
                </div>
            `;
            
            // Cellules pour chaque instrument
            instruments.forEach(instrument => {
                const isAssigned = assignment && assignment.instrumentId === instrument.id;
                const compatibility = this.routingManager.checkCompatibility(channel, instrument);
                const compatScore = Math.round(compatibility.score * 100);
                
                html += `
                    <div class="matrix-cell routing-cell ${isAssigned ? 'assigned' : ''}"
                         data-channel="${channel.number}"
                         data-instrument="${instrument.id}"
                         data-compatibility="${compatScore}">
                        ${isAssigned ? 
                            `<div class="assignment-marker">
                                <span class="marker-icon">✓</span>
                                <span class="compat-score">${compatScore}%</span>
                            </div>` : 
                            `<div class="compat-indicator" style="opacity: ${compatibility.score}">
                                ${compatScore}%
                            </div>`
                        }
                    </div>
                `;
            });
            
            html += '</div>';
        });
        
        html += '</div>';
        
        return html;
    }

    /**
     * Rendu en liste
     */
    renderList() {
        const channels = this.routingManager.midiChannels;
        
        if (channels.length === 0) {
            return '<div class="empty-state">No MIDI channels detected</div>';
        }
        
        let html = '<div class="routing-list">';
        
        channels.forEach(channel => {
            const assignment = this.routingManager.getAssignment(channel.number);
            
            html += `
                <div class="routing-item ${assignment ? 'assigned' : 'unassigned'}"
                     data-channel="${channel.number}">
                    <div class="item-channel">
                        <div class="channel-badge">CH${channel.number + 1}</div>
                        <div class="channel-details">
                            <div class="channel-name">${channel.instrument}</div>
                            <div class="channel-meta">${channel.noteCount} notes • ${this.formatNoteRange(channel.noteRange)}</div>
                        </div>
                    </div>
                    
                    <div class="item-arrow">→</div>
                    
                    <div class="item-instrument">
                        ${assignment ? `
                            <div class="assigned-instrument">
                                <div class="instrument-icon">${this.getInstrumentIcon(assignment.instrument.type)}</div>
                                <div class="instrument-details">
                                    <div class="instrument-name">${assignment.instrument.name}</div>
                                    <div class="compat-badge ${this.getCompatibilityClass(assignment.compatibility.score)}">
                                        ${Math.round(assignment.compatibility.score * 100)}% match
                                    </div>
                                </div>
                                <button class="unassign-btn" data-channel="${channel.number}">✕</button>
                            </div>
                        ` : `
                            <select class="instrument-select" data-channel="${channel.number}">
                                <option value="">Select instrument...</option>
                                ${this.renderInstrumentOptions(channel)}
                            </select>
                        `}
                    </div>
                </div>
            `;
        });
        
        html += '</div>';
        
        return html;
    }

    /**
     * Rendu des options d'instruments pour un select
     */
    renderInstrumentOptions(channel) {
        return this.routingManager.instruments
            .map(instrument => {
                const compatibility = this.routingManager.checkCompatibility(channel, instrument);
                const score = Math.round(compatibility.score * 100);
                
                return `
                    <option value="${instrument.id}" data-compat="${score}">
                        ${instrument.name} (${score}% match)
                    </option>
                `;
            })
            .sort((a, b) => {
                const aScore = parseInt(a.match(/data-compat="(\d+)"/)[1]);
                const bScore = parseInt(b.match(/data-compat="(\d+)"/)[1]);
                return bScore - aScore;
            })
            .join('');
    }

    /**
     * Rendu des presets
     */
    renderPresets() {
        const presets = this.routingManager.presets;
        
        return `
            <div class="presets-section">
                <h4>Presets</h4>
                <div class="presets-list">
                    ${presets.length === 0 ? 
                        '<div class="no-presets">No presets saved</div>' :
                        presets.map(preset => `
                            <div class="preset-item" data-preset="${preset.id}">
                                <span class="preset-name">${preset.name}</span>
                                <span class="preset-info">${preset.metadata.assignmentCount} assignments</span>
                                <div class="preset-actions">
                                    <button class="preset-btn apply-preset" data-preset="${preset.id}">Apply</button>
                                    <button class="preset-btn delete-preset" data-preset="${preset.id}">Delete</button>
                                </div>
                            </div>
                        `).join('')
                    }
                </div>
            </div>
        `;
    }

    /**
     * Rendu des conflits
     */
    renderConflicts() {
        const conflicts = this.routingManager.getConflicts();
        
        if (conflicts.length === 0) return '';
        
        return `
            <div class="routing-conflicts">
                <h4>⚠️ Issues (${conflicts.length})</h4>
                <div class="conflicts-list">
                    ${conflicts.map(conflict => `
                        <div class="conflict-item ${conflict.type}">
                            <span class="conflict-icon">${this.getConflictIcon(conflict.type)}</span>
                            <span class="conflict-message">${conflict.message}</span>
                        </div>
                    `).join('')}
                </div>
            </div>
        `;
    }

    // ========================================================================
    // ÉVÉNEMENTS
    // ========================================================================

    cacheElements() {
        this.elements = {
            content: document.getElementById('routingContent')
        };
    }

    attachEvents() {
        // Mode selector
        document.querySelectorAll('.mode-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                this.mode = btn.dataset.mode;
                this.render();
            });
        });
        
        // Actions
        document.getElementById('autoRouteBtn')?.addEventListener('click', () => {
            this.autoRoute();
        });
        
        document.getElementById('clearRouteBtn')?.addEventListener('click', () => {
            this.clearRouting();
        });
        
        document.getElementById('savePresetBtn')?.addEventListener('click', () => {
            this.savePreset();
        });
        
        // Matrix cells
        document.querySelectorAll('.routing-cell').forEach(cell => {
            cell.addEventListener('click', () => {
                const channel = parseInt(cell.dataset.channel);
                const instrument = cell.dataset.instrument;
                this.toggleAssignment(channel, instrument);
            });
        });
        
        // List selects
        document.querySelectorAll('.instrument-select').forEach(select => {
            select.addEventListener('change', (e) => {
                const channel = parseInt(e.target.dataset.channel);
                const instrument = e.target.value;
                if (instrument) {
                    this.routingManager.assign(channel, instrument);
                    this.render();
                }
            });
        });
        
        // Unassign buttons
        document.querySelectorAll('.unassign-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const channel = parseInt(btn.dataset.channel);
                this.routingManager.unassign(channel);
                this.render();
            });
        });
        
        // Presets
        document.querySelectorAll('.apply-preset').forEach(btn => {
            btn.addEventListener('click', () => {
                const presetId = btn.dataset.preset;
                this.routingManager.applyPreset(presetId);
                this.render();
            });
        });
        
        document.querySelectorAll('.delete-preset').forEach(btn => {
            btn.addEventListener('click', () => {
                const presetId = btn.dataset.preset;
                if (confirm('Delete this preset?')) {
                    this.routingManager.deletePreset(presetId);
                    this.render();
                }
            });
        });
    }

    // ========================================================================
    // ACTIONS
    // ========================================================================

    toggleAssignment(channel, instrumentId) {
        const assignment = this.routingManager.getAssignment(channel);
        
        if (assignment && assignment.instrumentId === instrumentId) {
            // Unassign
            this.routingManager.unassign(channel);
        } else {
            // Assign
            this.routingManager.assign(channel, instrumentId);
        }
        
        this.render();
    }

    autoRoute() {
        this.routingManager.autoRoute();
        this.render();
    }

    clearRouting() {
        if (confirm('Clear all routing assignments?')) {
            this.routingManager.clearAll();
            this.render();
        }
    }

    savePreset() {
        const name = prompt('Preset name:');
        if (name) {
            this.routingManager.createPreset(name);
            this.render();
        }
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    getInstrumentIcon(type) {
        const icons = {
            'keyboard': '🎹',
            'piano': '🎹',
            'guitar': '🎸',
            'bass': '🎸',
            'drum': '🥁',
            'percussion': '🥁',
            'string': '🎻',
            'brass': '🎺',
            'wind': '🎷',
            'synth': '🎛️',
            'organ': '🎹'
        };
        return icons[type?.toLowerCase()] || '🎵';
    }

    getCompatibilityClass(score) {
        if (score >= 0.8) return 'high';
        if (score >= 0.5) return 'medium';
        return 'low';
    }

    getConflictIcon(type) {
        const icons = {
            'unassigned': '❌',
            'duplicate': '⚠️',
            'low-compatibility': '🔻'
        };
        return icons[type] || '⚠️';
    }

    formatNoteRange(range) {
        const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const minName = noteNames[range.min % 12] + Math.floor(range.min / 12);
        const maxName = noteNames[range.max % 12] + Math.floor(range.max / 12);
        return `${minName}-${maxName}`;
    }

    // ========================================================================
    // API PUBLIQUE
    // ========================================================================

    update() {
        this.render();
    }

    setMode(mode) {
        this.mode = mode;
        this.render();
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = RoutingMatrixView;
}

if (typeof window !== 'undefined') {
    window.RoutingMatrixView = RoutingMatrixView;  // ← AJOUTÉ
}