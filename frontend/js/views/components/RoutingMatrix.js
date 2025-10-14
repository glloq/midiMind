// ============================================================================
// Fichier: frontend/scripts/views/components/RoutingMatrix.js
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Composant de matrice de routage visuelle pour l'assignation canaux/devices.
//   Affiche une grille interactive pour le routage MIDI.
//
// Auteur: midiMind Team
// Date: 2025-10-04
// Version: 3.0.0
// ============================================================================

class RoutingMatrix {
    constructor(container, config = {}) {
        this.container = typeof container === 'string' ? 
            document.getElementById(container) : container;
        
        // Configuration
        this.config = {
            channels: 16,
            showChannelNames: true,
            showDeviceStatus: true,
            allowMultiSelect: false,
            animateChanges: true,
            colorByChannel: true,
            ...config
        };
        
        // Données
        this.data = {
            channels: [],
            devices: [],
            routing: new Map()
        };
        
        // État
        this.state = {
            selectedChannel: null,
            selectedDevice: null,
            isDragging: false,
            hoveredCell: null
        };
        
        // Callbacks
        this.onAssign = config.onAssign || null;
        this.onUnassign = config.onUnassign || null;
        this.onMute = config.onMute || null;
        this.onSolo = config.onSolo || null;
        
        // Couleurs par canal
        this.channelColors = [
            '#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A',
            '#98D8C8', '#F7DC6F', '#BB8FCE', '#85C1E2',
            '#F8B739', '#52C7B8', '#FF8C94', '#A8E6CF',
            '#FFD93D', '#BCB3E5', '#FAB1A0', '#81C784'
        ];
        
        this.init();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    init() {
        if (!this.container) {
            console.error('RoutingMatrix: Container not found');
            return;
        }
        
        this.render();
        this.attachEvents();
    }
    
    // ========================================================================
    // RENDU
    // ========================================================================
    
    render() {
        const html = `
            <div class="routing-matrix">
                <div class="matrix-header">
                    ${this.buildHeader()}
                </div>
                <div class="matrix-body">
                    ${this.buildBody()}
                </div>
                <div class="matrix-legend">
                    ${this.buildLegend()}
                </div>
            </div>
        `;
        
        this.container.innerHTML = html;
        this.applyStyles();
        
        // Animation d'entrée si activée
        if (this.config.animateChanges) {
            this.animateIn();
        }
    }
    
    /**
     * Construire l'en-tête avec les devices
     */
    buildHeader() {
        let html = '<div class="matrix-row matrix-header-row">';
        
        // Cellule vide en haut à gauche
        html += '<div class="matrix-cell matrix-corner-cell"></div>';
        
        // En-têtes des devices
        this.data.devices.forEach(device => {
            const statusClass = device.status === 'connected' ? 'connected' : 'disconnected';
            const statusIcon = device.status === 'connected' ? '🟢' : '🔴';
            
            html += `
                <div class="matrix-cell matrix-device-header ${statusClass}" 
                     data-device="${device.id}"
                     title="${device.name} - ${device.type || 'Unknown'}">
                    <div class="device-name">${this.truncate(device.name, 12)}</div>
                    ${this.config.showDeviceStatus ? 
                        `<div class="device-status">${statusIcon}</div>` : ''
                    }
                </div>
            `;
        });
        
        // Colonne pour les contrôles Mute/Solo
        html += '<div class="matrix-cell matrix-controls-header">M/S</div>';
        
        html += '</div>';
        return html;
    }
    
    /**
     * Construire le corps de la matrice
     */
    buildBody() {
        let html = '';
        
        // Pour chaque canal
        for (let ch = 1; ch <= this.config.channels; ch++) {
            const channel = this.data.channels[ch - 1] || this.getDefaultChannel(ch);
            const color = this.config.colorByChannel ? this.channelColors[ch - 1] : '#667eea';
            
            html += '<div class="matrix-row matrix-channel-row">';
            
            // En-tête du canal
            html += `
                <div class="matrix-cell matrix-channel-header" 
                     data-channel="${ch}"
                     style="border-left: 4px solid ${color};">
                    <div class="channel-number">${ch}</div>
                    ${this.config.showChannelNames ? 
                        `<div class="channel-name">${channel.name || `Ch ${ch}`}</div>` : ''
                    }
                </div>
            `;
            
            // Cellules de routage
            this.data.devices.forEach(device => {
                const isAssigned = this.isChannelAssignedToDevice(ch, device.id);
                const isActive = channel.active || false;
                
                html += `
                    <div class="matrix-cell matrix-routing-cell 
                              ${isAssigned ? 'assigned' : ''} 
                              ${isActive ? 'active' : ''}"
                         data-channel="${ch}"
                         data-device="${device.id}"
                         style="${isAssigned ? `background-color: ${color}20; border-color: ${color};` : ''}">
                        ${isAssigned ? 
                            `<div class="routing-indicator" style="background: ${color};">✓</div>` : 
                            '<div class="routing-indicator empty">○</div>'
                        }
                    </div>
                `;
            });
            
            // Contrôles Mute/Solo
            html += `
                <div class="matrix-cell matrix-control-cell">
                    <button class="matrix-mute-btn ${channel.muted ? 'active' : ''}" 
                            data-channel="${ch}"
                            title="Mute Channel ${ch}">
                        M
                    </button>
                    <button class="matrix-solo-btn ${channel.solo ? 'active' : ''}" 
                            data-channel="${ch}"
                            title="Solo Channel ${ch}">
                        S
                    </button>
                </div>
            `;
            
            html += '</div>';
        }
        
        return html;
    }
    
    /**
     * Construire la légende
     */
    buildLegend() {
        return `
            <div class="matrix-legend-content">
                <div class="legend-item">
                    <div class="legend-icon assigned">✓</div>
                    <span>Assigné</span>
                </div>
                <div class="legend-item">
                    <div class="legend-icon empty">○</div>
                    <span>Non assigné</span>
                </div>
                <div class="legend-item">
                    <div class="legend-icon active-indicator"></div>
                    <span>Activité MIDI</span>
                </div>
                <div class="legend-item">
                    <button class="matrix-mute-btn">M</button>
                    <span>Mute</span>
                </div>
                <div class="legend-item">
                    <button class="matrix-solo-btn">S</button>
                    <span>Solo</span>
                </div>
            </div>
        `;
    }
    
    /**
     * Appliquer les styles CSS
     */
    applyStyles() {
        const style = document.createElement('style');
        style.textContent = `
            .routing-matrix {
                background: rgba(255, 255, 255, 0.02);
                border-radius: 12px;
                padding: 16px;
                overflow-x: auto;
            }
            
            .matrix-header {
                position: sticky;
                top: 0;
                z-index: 10;
                background: inherit;
            }
            
            .matrix-row {
                display: flex;
                align-items: center;
                min-height: 48px;
            }
            
            .matrix-cell {
                flex: 0 0 80px;
                height: 48px;
                display: flex;
                flex-direction: column;
                align-items: center;
                justify-content: center;
                border: 1px solid rgba(255, 255, 255, 0.1);
                margin: 2px;
                border-radius: 6px;
                position: relative;
                transition: all 0.2s ease;
            }
            
            .matrix-corner-cell {
                flex: 0 0 120px;
                background: transparent;
                border: none;
            }
            
            .matrix-channel-header {
                flex: 0 0 120px;
                flex-direction: row;
                justify-content: flex-start;
                padding: 0 12px;
                gap: 8px;
                background: rgba(255, 255, 255, 0.03);
            }
            
            .channel-number {
                font-weight: bold;
                font-size: 14px;
            }
            
            .channel-name {
                font-size: 11px;
                opacity: 0.7;
            }
            
            .matrix-device-header {
                background: rgba(255, 255, 255, 0.05);
                font-size: 11px;
                padding: 4px;
            }
            
            .device-name {
                text-align: center;
                overflow: hidden;
                text-overflow: ellipsis;
                white-space: nowrap;
            }
            
            .device-status {
                font-size: 8px;
                margin-top: 2px;
            }
            
            .matrix-routing-cell {
                cursor: pointer;
                user-select: none;
            }
            
            .matrix-routing-cell:hover {
                background: rgba(255, 255, 255, 0.1);
                transform: scale(1.05);
            }
            
            .matrix-routing-cell.assigned {
                border-width: 2px;
            }
            
            .matrix-routing-cell.active {
                animation: pulse 0.5s ease;
            }
            
            .routing-indicator {
                width: 24px;
                height: 24px;
                border-radius: 50%;
                display: flex;
                align-items: center;
                justify-content: center;
                font-size: 14px;
                font-weight: bold;
                color: white;
            }
            
            .routing-indicator.empty {
                background: transparent;
                color: rgba(255, 255, 255, 0.3);
                border: 1px solid rgba(255, 255, 255, 0.2);
            }
            
            .matrix-control-cell {
                flex: 0 0 80px;
                flex-direction: row;
                gap: 4px;
            }
            
            .matrix-mute-btn,
            .matrix-solo-btn {
                width: 28px;
                height: 28px;
                border-radius: 4px;
                border: 1px solid rgba(255, 255, 255, 0.2);
                background: transparent;
                color: rgba(255, 255, 255, 0.5);
                cursor: pointer;
                font-size: 12px;
                font-weight: bold;
                transition: all 0.2s ease;
            }
            
            .matrix-mute-btn:hover,
            .matrix-solo-btn:hover {
                background: rgba(255, 255, 255, 0.1);
            }
            
            .matrix-mute-btn.active {
                background: #dc3545;
                color: white;
                border-color: #dc3545;
            }
            
            .matrix-solo-btn.active {
                background: #ffc107;
                color: #000;
                border-color: #ffc107;
            }
            
            .matrix-legend {
                margin-top: 16px;
                padding-top: 16px;
                border-top: 1px solid rgba(255, 255, 255, 0.1);
            }
            
            .matrix-legend-content {
                display: flex;
                gap: 24px;
                align-items: center;
                font-size: 12px;
                opacity: 0.7;
            }
            
            .legend-item {
                display: flex;
                align-items: center;
                gap: 8px;
            }
            
            .legend-icon {
                width: 20px;
                height: 20px;
                display: flex;
                align-items: center;
                justify-content: center;
            }
            
            .active-indicator {
                width: 12px;
                height: 12px;
                background: #4ecdc4;
                border-radius: 50%;
                animation: pulse 1s infinite;
            }
            
            @keyframes pulse {
                0% { opacity: 1; transform: scale(1); }
                50% { opacity: 0.7; transform: scale(1.1); }
                100% { opacity: 1; transform: scale(1); }
            }
            
            /* Mode compact pour petits écrans */
            @media (max-width: 768px) {
                .matrix-cell {
                    flex: 0 0 60px;
                    height: 40px;
                }
                
                .matrix-channel-header {
                    flex: 0 0 80px;
                }
                
                .channel-name {
                    display: none;
                }
                
                .device-name {
                    font-size: 10px;
                }
            }
        `;
        
        if (!document.getElementById('routing-matrix-styles')) {
            style.id = 'routing-matrix-styles';
            document.head.appendChild(style);
        }
    }
    
    // ========================================================================
    // ÉVÉNEMENTS
    // ========================================================================
    
    attachEvents() {
        // Click sur les cellules de routage
        this.container.addEventListener('click', (e) => {
            const cell = e.target.closest('.matrix-routing-cell');
            if (cell) {
                this.handleCellClick(cell);
            }
            
            // Click sur Mute
            const muteBtn = e.target.closest('.matrix-mute-btn');
            if (muteBtn) {
                this.handleMuteClick(muteBtn);
            }
            
            // Click sur Solo
            const soloBtn = e.target.closest('.matrix-solo-btn');
            if (soloBtn) {
                this.handleSoloClick(soloBtn);
            }
        });
        
        // Hover sur les cellules
        this.container.addEventListener('mouseover', (e) => {
            const cell = e.target.closest('.matrix-routing-cell');
            if (cell) {
                this.handleCellHover(cell);
            }
        });
        
        this.container.addEventListener('mouseout', (e) => {
            const cell = e.target.closest('.matrix-routing-cell');
            if (cell) {
                this.handleCellHoverOut(cell);
            }
        });
        
        // Drag & Drop (optionnel)
        if (this.config.enableDragDrop) {
            this.attachDragDropEvents();
        }
    }
    
    /**
     * Gérer le click sur une cellule
     */
    handleCellClick(cell) {
        const channel = parseInt(cell.dataset.channel);
        const deviceId = cell.dataset.device;
        const isAssigned = cell.classList.contains('assigned');
        
        if (isAssigned) {
            // Désassigner
            this.unassignChannel(channel);
        } else {
            // Assigner
            this.assignChannelToDevice(channel, deviceId);
        }
    }
    
    /**
     * Gérer le click sur Mute
     */
    handleMuteClick(button) {
        const channel = parseInt(button.dataset.channel);
        const isMuted = button.classList.contains('active');
        
        button.classList.toggle('active');
        
        if (this.onMute) {
            this.onMute(channel, !isMuted);
        }
    }
    
    /**
     * Gérer le click sur Solo
     */
    handleSoloClick(button) {
        const channel = parseInt(button.dataset.channel);
        const isSolo = button.classList.contains('active');
        
        button.classList.toggle('active');
        
        if (this.onSolo) {
            this.onSolo(channel, !isSolo);
        }
    }
    
    /**
     * Gérer le hover sur une cellule
     */
    handleCellHover(cell) {
        const channel = parseInt(cell.dataset.channel);
        const deviceId = cell.dataset.device;
        
        // Highlight la ligne et la colonne
        this.highlightChannelRow(channel);
        this.highlightDeviceColumn(deviceId);
        
        this.state.hoveredCell = { channel, deviceId };
    }
    
    /**
     * Gérer la sortie du hover
     */
    handleCellHoverOut(cell) {
        this.clearHighlights();
        this.state.hoveredCell = null;
    }
    
    // ========================================================================
    // ACTIONS
    // ========================================================================
    
    /**
     * Assigner un canal à un device
     */
    assignChannelToDevice(channel, deviceId) {
        // Mettre à jour les données
        this.data.routing.set(channel, deviceId);
        
        // Mettre à jour l'affichage
        const cell = this.container.querySelector(
            `.matrix-routing-cell[data-channel="${channel}"][data-device="${deviceId}"]`
        );
        
        if (cell) {
            // Désassigner les autres devices pour ce canal si pas multi-select
            if (!this.config.allowMultiSelect) {
                this.container.querySelectorAll(
                    `.matrix-routing-cell[data-channel="${channel}"].assigned`
                ).forEach(c => {
                    c.classList.remove('assigned');
                    c.innerHTML = '<div class="routing-indicator empty">○</div>';
                });
            }
            
            // Assigner le nouveau
            const color = this.channelColors[channel - 1];
            cell.classList.add('assigned');
            cell.style.backgroundColor = `${color}20`;
            cell.style.borderColor = color;
            cell.innerHTML = `<div class="routing-indicator" style="background: ${color};">✓</div>`;
            
            if (this.config.animateChanges) {
                this.animateAssignment(cell);
            }
        }
        
        // Callback
        if (this.onAssign) {
            this.onAssign(channel, deviceId);
        }
    }
    
    /**
     * Désassigner un canal
     */
    unassignChannel(channel) {
        const deviceId = this.data.routing.get(channel);
        
        if (deviceId) {
            this.data.routing.delete(channel);
            
            const cell = this.container.querySelector(
                `.matrix-routing-cell[data-channel="${channel}"][data-device="${deviceId}"]`
            );
            
            if (cell) {
                cell.classList.remove('assigned');
                cell.style.backgroundColor = '';
                cell.style.borderColor = '';
                cell.innerHTML = '<div class="routing-indicator empty">○</div>';
                
                if (this.config.animateChanges) {
                    this.animateUnassignment(cell);
                }
            }
            
            if (this.onUnassign) {
                this.onUnassign(channel, deviceId);
            }
        }
    }
    
    /**
     * Mettre à jour l'activité d'un canal
     */
    updateChannelActivity(channel, active) {
        const cells = this.container.querySelectorAll(
            `.matrix-routing-cell[data-channel="${channel}"]`
        );
        
        cells.forEach(cell => {
            if (active) {
                cell.classList.add('active');
                setTimeout(() => cell.classList.remove('active'), 200);
            }
        });
    }
    
    // ========================================================================
    // MISE À JOUR DES DONNÉES
    // ========================================================================
    
    /**
     * Mettre à jour les données complètes
     */
    updateData(data) {
        this.data = {
            channels: data.channels || [],
            devices: data.devices || [],
            routing: new Map()
        };
        
        // Reconstruire la map de routage
        if (data.channels) {
            data.channels.forEach(ch => {
                if (ch.device) {
                    this.data.routing.set(ch.number, ch.device);
                }
            });
        }
        
        this.render();
    }
    
    /**
     * Mettre à jour uniquement les devices
     */
    updateDevices(devices) {
        this.data.devices = devices;
        this.render();
    }
    
    /**
     * Mettre à jour uniquement les canaux
     */
    updateChannels(channels) {
        this.data.channels = channels;
        
        // Mettre à jour les boutons Mute/Solo sans re-render complet
        channels.forEach(ch => {
            const muteBtn = this.container.querySelector(
                `.matrix-mute-btn[data-channel="${ch.number}"]`
            );
            const soloBtn = this.container.querySelector(
                `.matrix-solo-btn[data-channel="${ch.number}"]`
            );
            
            if (muteBtn) {
                muteBtn.classList.toggle('active', ch.muted);
            }
            if (soloBtn) {
                soloBtn.classList.toggle('active', ch.solo);
            }
        });
    }
    
    // ========================================================================
    // ANIMATIONS
    // ========================================================================
    
    animateIn() {
        const cells = this.container.querySelectorAll('.matrix-cell');
        cells.forEach((cell, index) => {
            cell.style.opacity = '0';
            cell.style.transform = 'scale(0.9)';
            
            setTimeout(() => {
                cell.style.transition = 'all 0.3s ease';
                cell.style.opacity = '1';
                cell.style.transform = 'scale(1)';
            }, index * 10);
        });
    }
    
    animateAssignment(cell) {
        cell.style.transform = 'scale(1.2)';
        setTimeout(() => {
            cell.style.transform = 'scale(1)';
        }, 200);
    }
    
    animateUnassignment(cell) {
        cell.style.transform = 'scale(0.8)';
        setTimeout(() => {
            cell.style.transform = 'scale(1)';
        }, 200);
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Vérifier si un canal est assigné à un device
     */
    isChannelAssignedToDevice(channel, deviceId) {
        return this.data.routing.get(channel) === deviceId;
    }
    
    /**
     * Obtenir le canal par défaut
     */
    getDefaultChannel(number) {
        return {
            number: number,
            name: `Channel ${number}`,
            muted: false,
            solo: false,
            active: false
        };
    }
    
    /**
     * Tronquer un texte
     */
    truncate(text, maxLength) {
        if (text.length <= maxLength) return text;
        return text.substring(0, maxLength - 3) + '...';
    }
    
    /**
     * Highlight une ligne de canal
     */
    highlightChannelRow(channel) {
        const row = this.container.querySelector(
            `.matrix-channel-row:nth-child(${channel})`
        );
        if (row) {
            row.style.background = 'rgba(255, 255, 255, 0.05)';
        }
    }
    
    /**
     * Highlight une colonne de device
     */
    highlightDeviceColumn(deviceId) {
        const cells = this.container.querySelectorAll(
            `.matrix-routing-cell[data-device="${deviceId}"]`
        );
        cells.forEach(cell => {
            cell.style.background = 'rgba(255, 255, 255, 0.03)';
        });
    }
    
    /**
     * Effacer les highlights
     */
    clearHighlights() {
        // Effacer les lignes
        this.container.querySelectorAll('.matrix-channel-row').forEach(row => {
            row.style.background = '';
        });
        
        // Effacer les cellules
        this.container.querySelectorAll('.matrix-routing-cell').forEach(cell => {
            if (!cell.classList.contains('assigned')) {
                cell.style.background = '';
            }
        });
    }
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    destroy() {
        if (this.container) {
            this.container.innerHTML = '';
        }
    }
}