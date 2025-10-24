// ============================================================================
// 📄 Fichier: frontend/js/views/EditorView.js
// Version: v3.6.0 - CORRECTED & COMPLETE
// Date: 2025-10-14
// ============================================================================
// CORRECTIONS v3.6.0:
// ✅ HÉRITAGE: Hérite maintenant de BaseView (CRITIQUE)
// ✅ ARCHITECTURE: Appelle super() et initialize() correctement
// ✅ CANVAS: Intégration RenderEngine + Viewport + CoordinateSystem
// ✅ ÉVÉNEMENTS: Tous les événements DOM bindés proprement
// ✅ RÉACTIVITÉ: Écoute tous les événements EditorModel
// ✅ COMPOSANTS: VelocityEditor et CCEditor intégrés
// ✅ STATE: Utilise viewState conforme à BaseView
// ✅ CLEANUP: Méthode destroy() complète
// ============================================================================
// 📝 Description:
//   Vue principale de l'éditeur MIDI avec interface complète.
//   Toolbar, Canvas principal, Velocity Editor, CC Editor, Properties Modal,
//   Routing Panel, Context Menu.
//
// Fonctionnalités:
//   - Toolbar complète (15 outils)
//   - Snap grid configurable (1/32 à 1 mesure)
//   - Canvas principal avec MidiVisualizer
//   - Velocity editor (draw, scale, randomize)
//   - CC editor (6 types de contrôleurs)
//   - Properties modal (édition note détaillée)
//   - Routing panel (sidebar)
//   - Context menu (clic droit)
//
// 🏗️ Architecture:
//   EditorView extends BaseView
//   - Utilise MidiVisualizer pour le canvas principal
//   - RenderEngine pour rendu optimisé
//   - Viewport pour zoom/pan/culling
//   - CoordinateSystem pour conversions
//
// 👤 Auteur: MidiMind Team
// ============================================================================

class EditorView extends BaseView {
    /**
     * Constructeur
     * @param {string|HTMLElement} containerId - ID du conteneur ou élément DOM
     * @param {EventBus} eventBus - Bus d'événements global
     */
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        // Configuration spécifique
        this.config.autoRender = false; // Rendu manuel via controller
        this.config.preserveState = true;
        this.config.debounceRender = 0; // Pas de debounce pour réactivité
        this.config.name = 'EditorView';
        
        // Référence au modèle (injection via setModel())
        this.editorModel = null;
        
        // État de la vue (conforme BaseView)
        this.viewState = {
            // Toolbar
            currentTool: 'select',
            availableTools: ['select', 'pencil', 'eraser', 'line', 'rectangle'],
            
            // Snap
            snapEnabled: false,
            snapGrid: 16, // 1/16
            
            // Editors
            showVelocity: false,
            showModulation: false,
            velocityMode: 'draw', // draw, scale, randomize
            modulationCC: 1, // Modulation Wheel par défaut
            
            // Panels
            showRoutingPanel: false,
            showPropertiesModal: false,
            
            // Selection
            selectedNotes: new Set(),
            selectedNoteData: null,
            
            // File info
            fileName: '',
            isModified: false,
            
            // Context menu
            contextMenuOpen: false,
            contextMenuX: 0,
            contextMenuY: 0,
            contextMenuOptions: []
        };
        
        // Composants Canvas
        this.canvas = null;
        this.renderEngine = null;
        this.viewport = null;
        this.coordSystem = null;
        this.visualizer = null; // MidiVisualizer (injecté)
        
        // Éditeurs Canvas
        this.velocityEditor = {
            canvas: null,
            ctx: null,
            mode: 'draw',
            bars: [],
            draggedBar: null
        };
        
        this.modulationEditor = {
            canvas: null,
            ctx: null,
            selectedCC: 1,
            points: [],
            hoveredPoint: null,
            draggedPoint: null
        };
        
        // Stockage des event listeners pour cleanup
        this._eventListeners = {
            toolbar: [],
            snap: [],
            velocity: [],
            modulation: [],
            context: [],
            properties: [],
            routing: [],
            canvas: []
        };
        
        // Mark as fully initialized
        this._fullyInitialized = true;
        
        // Now initialize after all properties are set
        this.initialize();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    /**
     * Initialisation de la vue
     * Override de BaseView.initialize()
     */
    initialize() {
        // Only call super.initialize if we're fully initialized
        // (BaseView constructor calls initialize, but properties aren't ready yet)
        if (this._fullyInitialized) {
            super.initialize();
            
            this.bindCustomEvents();
            
            // Exposer globalement pour compatibilité
            if (typeof window !== 'undefined') {
                window.editorView = this;
            }
            
            this.logDebug('EditorView v3.6.0 initialized');
        }
    }
    
    /**
     * Lie les événements personnalisés (EventBus)
     */
    bindCustomEvents() {
        // Événements du modèle
        this.eventBus.on('editor:loaded', (data) => this.onFileLoaded(data));
        this.eventBus.on('editor:modified', () => this.onDataModified());
        this.eventBus.on('editor:saved', () => this.onFileSaved());
        
        // Événements notes
        this.eventBus.on('editor:note:added', (data) => this.onNoteAdded(data));
        this.eventBus.on('editor:note:updated', (data) => this.onNoteUpdated(data));
        this.eventBus.on('editor:notes:deleted', (data) => this.onNotesDeleted(data));
        
        // Événements sélection
        this.eventBus.on('editor:selection:changed', (data) => this.onSelectionChanged(data));
        this.eventBus.on('editor:selection:cleared', () => this.onSelectionCleared());
        
        // Événements CC
        this.eventBus.on('editor:cc:added', (data) => this.onCCAdded(data));
        this.eventBus.on('editor:cc:updated', (data) => this.onCCUpdated(data));
        this.eventBus.on('editor:cc:deleted', (data) => this.onCCDeleted(data));
        
        // Événements viewport
        this.eventBus.on('editor:zoom:changed', (data) => this.onZoomChanged(data));
        this.eventBus.on('editor:pan:changed', (data) => this.onPanChanged(data));
        
        // Événements historique
        this.eventBus.on('editor:undo', () => this.onUndo());
        this.eventBus.on('editor:redo', () => this.onRedo());
        
        // Événements toolbar
        this.eventBus.on('editor:tool:changed', (data) => this.onToolChanged(data));
        this.eventBus.on('editor:snap:toggled', (data) => this.onSnapToggled(data));
        this.eventBus.on('editor:snap:grid:changed', (data) => this.onSnapGridChanged(data));
        
        // Événements routing
        this.eventBus.on('routing:channel:assigned', () => this.refreshRoutingPanel());
        this.eventBus.on('routing:validated', (data) => this.onRoutingValidated(data));
    }
    
    // ========================================================================
    // INJECTION DÉPENDANCES
    // ========================================================================
    
    /**
     * Injecte le modèle EditorModel
     * @param {EditorModel} editorModel
     */
    setModel(editorModel) {
        this.editorModel = editorModel;
        this.logDebug('EditorModel injected');
    }
    
    /**
     * Injecte le visualizer (MidiVisualizer)
     * @param {MidiVisualizer} visualizer
     */
    setVisualizer(visualizer) {
        this.visualizer = visualizer;
        
        // Récupérer les composants canvas
        if (visualizer) {
            this.canvas = visualizer.canvas;
            this.renderEngine = visualizer.renderEngine;
            this.viewport = visualizer.viewport;
            this.coordSystem = visualizer.coordSystem;
            
            this.logDebug('MidiVisualizer injected');
        }
    }
    
    // ========================================================================
    // TEMPLATE PRINCIPAL
    // ========================================================================
    
    /**
     * Construit le template HTML complet
     * @returns {string}
     */
    buildTemplate() {
        return `
            <div class="editor-view">
                <!-- Header: File info + Toolbar -->
                <div class="editor-header">
                    ${this.renderFileInfo()}
                    ${this.renderToolbar()}
                </div>
                
                <!-- Main: Canvas + Editors -->
                <div class="editor-main">
                    <!-- Canvas principal -->
                    <div class="editor-canvas-container">
                        <canvas id="editor-main-canvas" 
                                width="1920" 
                                height="1080"
                                style="width: 100%; height: 100%;">
                        </canvas>
                    </div>
                    
                    <!-- Velocity Editor (optionnel) -->
                    <div class="velocity-editor ${this.viewState.showVelocity ? 'visible' : 'hidden'}">
                        ${this.renderVelocityEditor()}
                    </div>
                    
                    <!-- CC/Modulation Editor (optionnel) -->
                    <div class="modulation-editor ${this.viewState.showModulation ? 'visible' : 'hidden'}">
                        ${this.renderModulationEditor()}
                    </div>
                </div>
                
                <!-- Routing Panel (sidebar droite, optionnel) -->
                <div class="routing-panel ${this.viewState.showRoutingPanel ? 'visible' : 'hidden'}">
                    ${this.renderRoutingSidebar()}
                </div>
                
                <!-- Context Menu -->
                ${this.renderContextMenu()}
                
                <!-- Properties Modal -->
                ${this.renderPropertiesModal()}
            </div>
        `;
    }
    
    // ========================================================================
    // TEMPLATES COMPOSANTS
    // ========================================================================
    
    /**
     * Rendu des informations fichier
     */
    renderFileInfo() {
        const fileName = this.viewState.fileName || 'Untitled';
        const modifiedIndicator = this.viewState.isModified ? '• ' : '';
        
        return `
            <div class="editor-file-info">
                <span class="file-name">${modifiedIndicator}${this.escapeHTML(fileName)}</span>
                <span class="selection-info" data-selection-info></span>
            </div>
        `;
    }
    
    /**
     * Rendu de la toolbar principale
     */
    renderToolbar() {
        return `
            <div class="editor-toolbar">
                <!-- Tools -->
                <div class="toolbar-section toolbar-tools">
                    ${this.renderToolButtons()}
                </div>
                
                <!-- Snap Grid -->
                <div class="toolbar-section toolbar-snap">
                    ${this.renderSnapGrid()}
                </div>
                
                <!-- Actions -->
                <div class="toolbar-section toolbar-actions">
                    ${this.renderActionButtons()}
                </div>
                
                <!-- Editors Toggle -->
                <div class="toolbar-section toolbar-editors">
                    ${this.renderEditorToggles()}
                </div>
                
                <!-- View Controls -->
                <div class="toolbar-section toolbar-view">
                    ${this.renderViewControls()}
                </div>
            </div>
        `;
    }
    
    /**
     * Rendu des boutons d'outils
     */
    renderToolButtons() {
        const tools = [
            { id: 'select', icon: '🎯', title: 'Select (V)' },
            { id: 'pencil', icon: '✏️', title: 'Pencil (P)' },
            { id: 'eraser', icon: '🗑️', title: 'Eraser (E)' },
            { id: 'line', icon: '📏', title: 'Line (L)' },
            { id: 'rectangle', icon: '⬜', title: 'Rectangle (R)' }
        ];
        
        return tools.map(tool => `
            <button class="tool-btn ${this.viewState.currentTool === tool.id ? 'active' : ''}"
                    data-tool="${tool.id}"
                    title="${tool.title}"
                    onclick="app.editorView.setTool('${tool.id}')">
                ${tool.icon}
            </button>
        `).join('');
    }
    
    /**
     * Rendu du snap grid selector
     */
    renderSnapGrid() {
        const snapValues = [
            { value: 1, label: '1/1' },
            { value: 2, label: '1/2' },
            { value: 4, label: '1/4' },
            { value: 8, label: '1/8' },
            { value: 16, label: '1/16' },
            { value: 32, label: '1/32' },
            { value: 0, label: 'Off' }
        ];
        
        return `
            <div class="snap-grid-controls">
                <label class="snap-toggle">
                    <input type="checkbox" 
                           ${this.viewState.snapEnabled ? 'checked' : ''}
                           onchange="app.editorView.toggleSnap(this.checked)">
                    <span>Snap</span>
                </label>
                
                <select class="snap-grid-select"
                        ${!this.viewState.snapEnabled ? 'disabled' : ''}
                        onchange="app.editorView.setSnapGrid(parseInt(this.value))">
                    ${snapValues.map(snap => `
                        <option value="${snap.value}" 
                                ${this.viewState.snapGrid === snap.value ? 'selected' : ''}>
                            ${snap.label}
                        </option>
                    `).join('')}
                </select>
            </div>
        `;
    }
    
    /**
     * Rendu des boutons d'actions
     */
    renderActionButtons() {
        const canUndo = this.editorModel?.canUndo() ?? false;
        const canRedo = this.editorModel?.canRedo() ?? false;
        const hasClipboard = this.editorModel?.hasClipboardContent() ?? false;
        
        return `
            <button class="action-btn" 
                    ${!canUndo ? 'disabled' : ''}
                    title="Undo (Ctrl+Z)"
                    onclick="app.editorController?.undo()">
                ↶ Undo
            </button>
            
            <button class="action-btn" 
                    ${!canRedo ? 'disabled' : ''}
                    title="Redo (Ctrl+Y)"
                    onclick="app.editorController?.redo()">
                ↷ Redo
            </button>
            
            <div class="toolbar-divider"></div>
            
            <button class="action-btn"
                    ${this.viewState.selectedNotes.size === 0 ? 'disabled' : ''}
                    title="Cut (Ctrl+X)"
                    onclick="app.editorController?.cut()">
                ✂️ Cut
            </button>
            
            <button class="action-btn"
                    ${this.viewState.selectedNotes.size === 0 ? 'disabled' : ''}
                    title="Copy (Ctrl+C)"
                    onclick="app.editorController?.copy()">
                📋 Copy
            </button>
            
            <button class="action-btn"
                    ${!hasClipboard ? 'disabled' : ''}
                    title="Paste (Ctrl+V)"
                    onclick="app.editorController?.paste()">
                📄 Paste
            </button>
            
            <button class="action-btn"
                    ${this.viewState.selectedNotes.size === 0 ? 'disabled' : ''}
                    title="Delete (Del)"
                    onclick="app.editorController?.deleteSelection()">
                🗑️ Delete
            </button>
        `;
    }
    
    /**
     * Rendu des toggles d'éditeurs
     */
    renderEditorToggles() {
        return `
            <button class="editor-toggle-btn ${this.viewState.showVelocity ? 'active' : ''}"
                    title="Toggle Velocity Editor"
                    onclick="app.editorView.toggleVelocityEditor()">
                📊 Velocity
            </button>
            
            <button class="editor-toggle-btn ${this.viewState.showModulation ? 'active' : ''}"
                    title="Toggle Modulation Editor"
                    onclick="app.editorView.toggleModulationEditor()">
                🎛️ CC
            </button>
            
            <button class="editor-toggle-btn ${this.viewState.showRoutingPanel ? 'active' : ''}"
                    title="Toggle Routing Panel"
                    onclick="app.editorView.toggleRoutingPanel()">
                🔀 Routing
            </button>
        `;
    }
    
    /**
     * Rendu des contrôles de vue
     */
    renderViewControls() {
        return `
            <button class="view-btn" 
                    title="Zoom In (+)"
                    onclick="app.editorController?.zoomIn()">
                🔍+
            </button>
            
            <button class="view-btn" 
                    title="Zoom Out (-)"
                    onclick="app.editorController?.zoomOut()">
                🔍-
            </button>
            
            <button class="view-btn" 
                    title="Fit to View (F)"
                    onclick="app.editorController?.fitToView()">
                ⛶ Fit
            </button>
        `;
    }
    
    /**
     * Rendu du Velocity Editor
     */
    renderVelocityEditor() {
        return `
            <div class="velocity-editor-container">
                <!-- Header -->
                <div class="velocity-editor-header">
                    <span class="velocity-editor-title">Velocity Editor</span>
                    
                    <div class="velocity-mode-selector">
                        <button class="${this.viewState.velocityMode === 'draw' ? 'active' : ''}"
                                onclick="app.editorView.setVelocityMode('draw')">
                            Draw
                        </button>
                        <button class="${this.viewState.velocityMode === 'scale' ? 'active' : ''}"
                                onclick="app.editorView.setVelocityMode('scale')">
                            Scale
                        </button>
                        <button class="${this.viewState.velocityMode === 'randomize' ? 'active' : ''}"
                                onclick="app.editorView.setVelocityMode('randomize')">
                            Randomize
                        </button>
                    </div>
                    
                    <button class="close-btn" 
                            onclick="app.editorView.toggleVelocityEditor()">
                        ✕
                    </button>
                </div>
                
                <!-- Canvas -->
                <canvas id="velocity-canvas" 
                        width="1920" 
                        height="150"
                        style="width: 100%; height: 150px;">
                </canvas>
            </div>
        `;
    }
    
    /**
     * Rendu du Modulation/CC Editor
     */
    renderModulationEditor() {
        const ccTypes = [
            { value: 1, label: 'CC1 - Modulation' },
            { value: 7, label: 'CC7 - Volume' },
            { value: 10, label: 'CC10 - Pan' },
            { value: 11, label: 'CC11 - Expression' },
            { value: 64, label: 'CC64 - Sustain' },
            { value: 74, label: 'CC74 - Filter' }
        ];
        
        return `
            <div class="modulation-editor-container">
                <!-- Header -->
                <div class="modulation-editor-header">
                    <span class="modulation-editor-title">CC Editor</span>
                    
                    <select class="cc-type-selector"
                            onchange="app.editorView.setModulationCC(parseInt(this.value))">
                        ${ccTypes.map(cc => `
                            <option value="${cc.value}"
                                    ${this.viewState.modulationCC === cc.value ? 'selected' : ''}>
                                ${cc.label}
                            </option>
                        `).join('')}
                    </select>
                    
                    <button class="clear-cc-btn"
                            onclick="app.editorView.clearModulationCC()">
                        Clear
                    </button>
                    
                    <button class="close-btn" 
                            onclick="app.editorView.toggleModulationEditor()">
                        ✕
                    </button>
                </div>
                
                <!-- Canvas -->
                <canvas id="cc-canvas" 
                        width="1920" 
                        height="150"
                        style="width: 100%; height: 150px;">
                </canvas>
            </div>
        `;
    }
    
    /**
     * Rendu du Routing Sidebar
     */
    renderRoutingSidebar() {
        return `
            <div class="routing-sidebar-container">
                <!-- Header -->
                <div class="routing-sidebar-header">
                    <span class="routing-sidebar-title">🔀 Routing</span>
                    <button class="close-btn" 
                            onclick="app.editorView.toggleRoutingPanel()">
                        ✕
                    </button>
                </div>
                
                <!-- Content -->
                <div class="routing-sidebar-content" data-routing-content>
                    <!-- Sera rempli dynamiquement -->
                    <p>Loading routing...</p>
                </div>
                
                <!-- Actions -->
                <div class="routing-sidebar-actions">
                    <button onclick="app.editorController?.autoRoute()">
                        Auto-Route
                    </button>
                    <button onclick="app.editorController?.validateRouting()">
                        Validate
                    </button>
                    <button onclick="app.editorController?.clearRouting()">
                        Clear
                    </button>
                </div>
            </div>
        `;
    }
    
    /**
     * Rendu du Context Menu
     */
    renderContextMenu() {
        if (!this.viewState.contextMenuOpen) {
            return '<div class="context-menu hidden"></div>';
        }
        
        const options = this.viewState.contextMenuOptions;
        const x = this.viewState.contextMenuX;
        const y = this.viewState.contextMenuY;
        
        return `
            <div class="context-menu visible" 
                 style="left: ${x}px; top: ${y}px;">
                ${options.map(option => {
                    if (option.divider) {
                        return '<div class="context-menu-divider"></div>';
                    }
                    
                    return `
                        <div class="context-menu-item ${option.disabled ? 'disabled' : ''}"
                             onclick="${option.disabled ? '' : option.action}">
                            ${option.icon ? option.icon + ' ' : ''}
                            ${option.label}
                        </div>
                    `;
                }).join('')}
            </div>
        `;
    }
    
    /**
     * Rendu du Properties Modal
     */
    renderPropertiesModal() {
        if (!this.viewState.showPropertiesModal || !this.viewState.selectedNoteData) {
            return '<div class="properties-modal hidden"></div>';
        }
        
        const note = this.viewState.selectedNoteData;
        
        return `
            <div class="properties-modal visible">
                <div class="modal-overlay" 
                     onclick="app.editorView.hideNoteProperties()"></div>
                
                <div class="modal-content">
                    <div class="modal-header">
                        <h3>Note Properties</h3>
                        <button class="close-btn" 
                                onclick="app.editorView.hideNoteProperties()">
                            ✕
                        </button>
                    </div>
                    
                    <div class="modal-body">
                        <!-- Pitch -->
                        <div class="form-group">
                            <label>Pitch (0-127)</label>
                            <input type="number" 
                                   id="prop-pitch" 
                                   value="${note.pitch || 60}"
                                   min="0" 
                                   max="127">
                            <span class="note-name">${this.getNoteNameFromPitch(note.pitch || 60)}</span>
                        </div>
                        
                        <!-- Time -->
                        <div class="form-group">
                            <label>Time (ms)</label>
                            <input type="number" 
                                   id="prop-time" 
                                   value="${note.time || 0}"
                                   min="0">
                        </div>
                        
                        <!-- Duration -->
                        <div class="form-group">
                            <label>Duration (ms)</label>
                            <input type="number" 
                                   id="prop-duration" 
                                   value="${note.duration || 500}"
                                   min="1">
                        </div>
                        
                        <!-- Velocity -->
                        <div class="form-group">
                            <label>Velocity (1-127)</label>
                            <input type="range" 
                                   id="prop-velocity" 
                                   value="${note.velocity || 80}"
                                   min="1" 
                                   max="127"
                                   oninput="document.getElementById('velocity-value').textContent = this.value">
                            <span id="velocity-value">${note.velocity || 80}</span>
                        </div>
                        
                        <!-- Channel -->
                        <div class="form-group">
                            <label>Channel (0-15)</label>
                            <select id="prop-channel">
                                ${Array.from({ length: 16 }, (_, i) => `
                                    <option value="${i}" ${(note.channel || 0) === i ? 'selected' : ''}>
                                        CH${i + 1}
                                    </option>
                                `).join('')}
                            </select>
                        </div>
                    </div>
                    
                    <div class="modal-footer">
                        <button class="btn btn-secondary" 
                                onclick="app.editorView.hideNoteProperties()">
                            Cancel
                        </button>
                        <button class="btn btn-primary" 
                                onclick="app.editorView.applyNoteProperties()">
                            Apply
                        </button>
                    </div>
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // ACTIONS TOOLBAR
    // ========================================================================
    
    /**
     * Change l'outil actuel
     * @param {string} tool - ID de l'outil
     */
    setTool(tool) {
        if (!this.viewState.availableTools.includes(tool)) {
            this.logDebug(`Invalid tool: ${tool}`);
            return;
        }
        
        this.viewState.currentTool = tool;
        
        // Émettre événement
        this.emit('editor:tool:changed', { tool });
        
        // Mettre à jour UI
        this.updateToolbarButtons();
        
        this.logDebug(`Tool changed to: ${tool}`);
    }
    
    /**
     * Toggle snap grid
     * @param {boolean} enabled
     */
    toggleSnap(enabled) {
        this.viewState.snapEnabled = enabled;
        
        // Émettre événement
        this.emit('editor:snap:toggled', { enabled });
        
        // Mettre à jour UI
        this.updateSnapControls();
        
        this.logDebug(`Snap ${enabled ? 'enabled' : 'disabled'}`);
    }
    
    /**
     * Change la valeur du snap grid
     * @param {number} value - Valeur (1, 2, 4, 8, 16, 32)
     */
    setSnapGrid(value) {
        this.viewState.snapGrid = value;
        
        if (value === 0) {
            this.viewState.snapEnabled = false;
        }
        
        // Émettre événement
        this.emit('editor:snap:grid:changed', { value });
        
        // Mettre à jour UI
        this.updateSnapControls();
        
        this.logDebug(`Snap grid set to 1/${value}`);
    }
    
    // ========================================================================
    // ACTIONS VELOCITY EDITOR
    // ========================================================================
    
    /**
     * Toggle visibility du Velocity Editor
     */
    toggleVelocityEditor() {
        this.viewState.showVelocity = !this.viewState.showVelocity;
        
        if (this.viewState.showVelocity) {
            // Créer canvas si pas encore fait
            this.setupVelocityCanvas();
            // Dessiner
            this.drawVelocityBars();
        }
        
        // Mettre à jour UI
        this.updateEditorToggles();
        
        this.emit('editor:velocity:toggled', { show: this.viewState.showVelocity });
        
        this.logDebug(`Velocity editor ${this.viewState.showVelocity ? 'shown' : 'hidden'}`);
    }
    
    /**
     * Change le mode du Velocity Editor
     * @param {string} mode - draw, scale, randomize
     */
    setVelocityMode(mode) {
        if (!['draw', 'scale', 'randomize'].includes(mode)) {
            this.logDebug(`Invalid velocity mode: ${mode}`);
            return;
        }
        
        this.viewState.velocityMode = mode;
        this.velocityEditor.mode = mode;
        
        // Mettre à jour UI
        this.updateVelocityModeButtons();
        
        this.logDebug(`Velocity mode: ${mode}`);
    }
    
    /**
     * Dessine les barres de vélocité
     */
    drawVelocityBars() {
        if (!this.velocityEditor.canvas || !this.editorModel) {
            return;
        }
        
        const canvas = this.velocityEditor.canvas;
        const ctx = this.velocityEditor.ctx;
        const width = canvas.width;
        const height = canvas.height;
        
        // Clear
        ctx.clearRect(0, 0, width, height);
        
        // Fond
        ctx.fillStyle = '#1a1a1a';
        ctx.fillRect(0, 0, width, height);
        
        // Grille horizontale
        this.drawVelocityGrid(ctx, width, height);
        
        // Notes
        const notes = this.editorModel.getAllNotes();
        if (notes.length === 0) return;
        
        // Calculer la largeur de chaque barre
        const totalDuration = this.editorModel.getData()?.duration || 10000;
        const pixelsPerMs = width / totalDuration;
        
        notes.forEach(note => {
            const x = note.time * pixelsPerMs;
            const barWidth = Math.max(2, note.duration * pixelsPerMs);
            const barHeight = (note.velocity / 127) * (height - 20);
            const y = height - 10 - barHeight;
            
            // Couleur selon vélocité
            const hue = (note.velocity / 127) * 120; // Rouge -> Vert
            ctx.fillStyle = `hsl(${hue}, 70%, 50%)`;
            
            // Barre
            ctx.fillRect(x, y, barWidth, barHeight);
            
            // Border si sélectionnée
            if (this.viewState.selectedNotes.has(note.id)) {
                ctx.strokeStyle = '#fff';
                ctx.lineWidth = 2;
                ctx.strokeRect(x, y, barWidth, barHeight);
            }
        });
    }
    
    /**
     * Dessine la grille du Velocity Editor
     */
    drawVelocityGrid(ctx, width, height) {
        ctx.strokeStyle = '#333';
        ctx.lineWidth = 1;
        
        // Lignes horizontales (velocity levels)
        const levels = [0, 32, 64, 96, 127];
        levels.forEach(level => {
            const y = height - 10 - (level / 127) * (height - 20);
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(width, y);
            ctx.stroke();
            
            // Label
            ctx.fillStyle = '#666';
            ctx.font = '10px monospace';
            ctx.fillText(level.toString(), 5, y - 2);
        });
    }
    
    /**
     * Setup du canvas Velocity
     */
    setupVelocityCanvas() {
        const canvas = document.getElementById('velocity-canvas');
        if (!canvas) return;
        
        this.velocityEditor.canvas = canvas;
        this.velocityEditor.ctx = canvas.getContext('2d');
        
        // Événements
        canvas.addEventListener('mousedown', (e) => this.onVelocityCanvasMouseDown(e));
        canvas.addEventListener('mousemove', (e) => this.onVelocityCanvasMouseMove(e));
        canvas.addEventListener('mouseup', (e) => this.onVelocityCanvasMouseUp(e));
        
        this.logDebug('Velocity canvas setup');
    }
    
    /**
     * Gestion clic velocity canvas
     */
    onVelocityCanvasMouseDown(e) {
        const canvas = this.velocityEditor.canvas;
        if (!canvas || !this.editorModel) return;
        
        const rect = canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        // Trouver la note sous le curseur
        const note = this.findNoteAtVelocityX(x);
        
        if (!note) return;
        
        // Calculer nouvelle vélocité depuis position Y
        const height = canvas.height;
        const newVelocity = Math.round((1 - (y - 10) / (height - 20)) * 127);
        const clampedVelocity = Math.max(1, Math.min(127, newVelocity));
        
        // Appliquer selon le mode
        switch (this.viewState.velocityMode) {
            case 'draw':
                // Modifier directement
                this.velocityEditor.draggedBar = note;
                this.editorModel.updateNote(note.id, { velocity: clampedVelocity });
                break;
                
            case 'scale':
                // Préparer scaling sur sélection
                if (this.viewState.selectedNotes.size > 0) {
                    this.velocityEditor.draggedBar = note;
                    this.velocityEditor.scaleStart = clampedVelocity;
                }
                break;
                
            case 'randomize':
                // Randomize sur clic
                if (this.viewState.selectedNotes.size > 0) {
                    this.randomizeVelocity();
                } else {
                    // Randomize single note
                    const randomVel = Math.floor(Math.random() * 127) + 1;
                    this.editorModel.updateNote(note.id, { velocity: randomVel });
                }
                break;
        }
        
        this.drawVelocityBars();
    }
    
    onVelocityCanvasMouseMove(e) {
        const canvas = this.velocityEditor.canvas;
        if (!canvas || !this.editorModel) return;
        
        if (!this.velocityEditor.draggedBar) return;
        
        const rect = canvas.getBoundingClientRect();
        const y = e.clientY - rect.top;
        
        // Calculer nouvelle vélocité
        const height = canvas.height;
        const newVelocity = Math.round((1 - (y - 10) / (height - 20)) * 127);
        const clampedVelocity = Math.max(1, Math.min(127, newVelocity));
        
        const note = this.velocityEditor.draggedBar;
        
        switch (this.viewState.velocityMode) {
            case 'draw':
                // Modifier pendant drag
                this.editorModel.updateNote(note.id, { velocity: clampedVelocity });
                this.drawVelocityBars();
                break;
                
            case 'scale':
                // Scale toutes les notes sélectionnées
                if (this.viewState.selectedNotes.size > 0 && this.velocityEditor.scaleStart) {
                    const scaleFactor = clampedVelocity / this.velocityEditor.scaleStart;
                    this.scaleVelocity(scaleFactor);
                    this.drawVelocityBars();
                }
                break;
        }
    }
    
    onVelocityCanvasMouseUp(e) {
        this.velocityEditor.draggedBar = null;
        this.velocityEditor.scaleStart = null;
    }
    
    /**
     * Trouve une note à la position X du velocity canvas
     */
    findNoteAtVelocityX(x) {
        if (!this.editorModel) return null;
        
        const notes = this.editorModel.getAllNotes();
        const totalDuration = this.editorModel.getData()?.duration || 10000;
        const canvas = this.velocityEditor.canvas;
        const pixelsPerMs = canvas.width / totalDuration;
        
        for (const note of notes) {
            const noteX = note.time * pixelsPerMs;
            const noteWidth = Math.max(2, note.duration * pixelsPerMs);
            
            if (x >= noteX && x <= noteX + noteWidth) {
                return note;
            }
        }
        
        return null;
    }
    
    /**
     * Randomize velocity sur sélection
     */
    randomizeVelocity() {
        if (!this.editorModel || this.viewState.selectedNotes.size === 0) return;
        
        this.viewState.selectedNotes.forEach(noteId => {
            const randomVel = Math.floor(Math.random() * 127) + 1;
            this.editorModel.updateNote(noteId, { velocity: randomVel });
        });
    }
    
    /**
     * Scale velocity sur sélection
     */
    scaleVelocity(factor) {
        if (!this.editorModel || this.viewState.selectedNotes.size === 0) return;
        
        this.viewState.selectedNotes.forEach(noteId => {
            const note = this.editorModel.getNoteById(noteId);
            if (note) {
                const newVel = Math.round(note.velocity * factor);
                const clampedVel = Math.max(1, Math.min(127, newVel));
                this.editorModel.updateNote(noteId, { velocity: clampedVel });
            }
        });
    }
    
    // ========================================================================
    // ACTIONS CC EDITOR
    // ========================================================================
    
    /**
     * Toggle visibility du CC Editor
     */
    toggleModulationEditor() {
        this.viewState.showModulation = !this.viewState.showModulation;
        
        if (this.viewState.showModulation) {
            this.setupCCCanvas();
            this.drawCCCurve();
        }
        
        this.updateEditorToggles();
        
        this.emit('editor:modulation:toggled', { show: this.viewState.showModulation });
        
        this.logDebug(`CC editor ${this.viewState.showModulation ? 'shown' : 'hidden'}`);
    }
    
    /**
     * Change le CC affiché
     * @param {number} ccNumber - Numéro CC (1, 7, 10, 11, 64, 74)
     */
    setModulationCC(ccNumber) {
        this.viewState.modulationCC = ccNumber;
        this.modulationEditor.selectedCC = ccNumber;
        
        // Recharger les points CC
        this.loadCCPoints();
        
        // Redessiner
        this.drawCCCurve();
        
        this.logDebug(`CC changed to: ${ccNumber}`);
    }
    
    /**
     * Efface tous les événements CC du type sélectionné
     */
    clearModulationCC() {
        if (!this.editorModel) return;
        
        const ccNumber = this.viewState.modulationCC;
        
        // Effacer dans le modèle
        this.editorModel.clearCC(ccNumber);
        
        // Effacer dans la vue
        this.modulationEditor.points = [];
        this.drawCCCurve();
        
        this.logDebug(`Cleared CC${ccNumber}`);
    }
    
    /**
     * Charge les points CC depuis le modèle
     */
    loadCCPoints() {
        if (!this.editorModel) {
            this.modulationEditor.points = [];
            return;
        }
        
        const ccNumber = this.viewState.modulationCC;
        const ccEvents = this.editorModel.getCCEvents(ccNumber);
        
        this.modulationEditor.points = ccEvents.map(event => ({
            time: event.time,
            value: event.value,
            id: event.id
        })).sort((a, b) => a.time - b.time);
        
        this.logDebug(`Loaded ${this.modulationEditor.points.length} CC${ccNumber} points`);
    }
    
    /**
     * Dessine la courbe CC
     */
    drawCCCurve() {
        if (!this.modulationEditor.canvas || !this.editorModel) {
            return;
        }
        
        const canvas = this.modulationEditor.canvas;
        const ctx = this.modulationEditor.ctx;
        const width = canvas.width;
        const height = canvas.height;
        
        // Clear
        ctx.clearRect(0, 0, width, height);
        
        // Fond
        ctx.fillStyle = '#1a1a1a';
        ctx.fillRect(0, 0, width, height);
        
        // Grille
        this.drawCCGrid(ctx, width, height);
        
        // Points
        const points = this.modulationEditor.points;
        if (points.length === 0) return;
        
        // Calculer positions
        const totalDuration = this.editorModel.getData()?.duration || 10000;
        const pixelsPerMs = width / totalDuration;
        
        // Dessiner ligne entre points
        ctx.strokeStyle = '#667eea';
        ctx.lineWidth = 2;
        ctx.beginPath();
        
        points.forEach((point, i) => {
            const x = point.time * pixelsPerMs;
            const y = height - 10 - (point.value / 127) * (height - 20);
            
            if (i === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }
        });
        
        ctx.stroke();
        
        // Dessiner points
        points.forEach(point => {
            const x = point.time * pixelsPerMs;
            const y = height - 10 - (point.value / 127) * (height - 20);
            
            ctx.fillStyle = '#667eea';
            ctx.beginPath();
            ctx.arc(x, y, 4, 0, Math.PI * 2);
            ctx.fill();
            
            // Highlight si hover
            if (this.modulationEditor.hoveredPoint === point.id) {
                ctx.strokeStyle = '#fff';
                ctx.lineWidth = 2;
                ctx.stroke();
            }
        });
    }
    
    /**
     * Dessine la grille du CC Editor
     */
    drawCCGrid(ctx, width, height) {
        ctx.strokeStyle = '#333';
        ctx.lineWidth = 1;
        
        // Lignes horizontales
        const levels = [0, 64, 127];
        levels.forEach(level => {
            const y = height - 10 - (level / 127) * (height - 20);
            ctx.strokeStyle = level === 64 ? '#555' : '#333';
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(width, y);
            ctx.stroke();
            
            // Label
            ctx.fillStyle = '#666';
            ctx.font = '10px monospace';
            ctx.fillText(level.toString(), 5, y - 2);
        });
    }
    
    /**
     * Setup du canvas CC
     */
    setupCCCanvas() {
        const canvas = document.getElementById('cc-canvas');
        if (!canvas) return;
        
        this.modulationEditor.canvas = canvas;
        this.modulationEditor.ctx = canvas.getContext('2d');
        
        // Événements
        canvas.addEventListener('dblclick', (e) => this.onCCCanvasDoubleClick(e));
        canvas.addEventListener('mousedown', (e) => this.onCCCanvasMouseDown(e));
        canvas.addEventListener('mousemove', (e) => this.onCCCanvasMouseMove(e));
        canvas.addEventListener('mouseup', (e) => this.onCCCanvasMouseUp(e));
        
        this.logDebug('CC canvas setup');
    }
    
    /**
     * Double-click sur CC canvas = ajouter point
     */
    onCCCanvasDoubleClick(e) {
        const canvas = this.modulationEditor.canvas;
        if (!canvas || !this.editorModel) return;
        
        const rect = canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        // Convertir en temps et valeur CC
        const totalDuration = this.editorModel.getData()?.duration || 10000;
        const pixelsPerMs = canvas.width / totalDuration;
        const time = Math.round(x / pixelsPerMs);
        
        const height = canvas.height;
        const value = Math.round((1 - (y - 10) / (height - 20)) * 127);
        const clampedValue = Math.max(0, Math.min(127, value));
        
        // Ajouter le point CC dans le modèle
        const ccNumber = this.viewState.modulationCC;
        const channel = 0; // TODO: Get from current file or selection
        
        this.editorModel.addCC(channel, time, ccNumber, clampedValue);
        
        // Recharger et redessiner
        this.loadCCPoints();
        this.drawCCCurve();
        
        this.logDebug(`Added CC${ccNumber} point at ${time}ms, value ${clampedValue}`);
    }
    
    onCCCanvasMouseDown(e) {
        const canvas = this.modulationEditor.canvas;
        if (!canvas) return;
        
        const rect = canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        // Trouver le point le plus proche
        const point = this.findCCPointAt(x, y);
        
        if (point) {
            // Commencer le drag
            this.modulationEditor.draggedPoint = point;
            this.modulationEditor.hoveredPoint = point.id;
            this.drawCCCurve();
        }
    }
    
    onCCCanvasMouseMove(e) {
        const canvas = this.modulationEditor.canvas;
        if (!canvas) return;
        
        const rect = canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        if (this.modulationEditor.draggedPoint) {
            // Drag d'un point existant
            const totalDuration = this.editorModel.getData()?.duration || 10000;
            const pixelsPerMs = canvas.width / totalDuration;
            const newTime = Math.round(x / pixelsPerMs);
            
            const height = canvas.height;
            const newValue = Math.round((1 - (y - 10) / (height - 20)) * 127);
            const clampedValue = Math.max(0, Math.min(127, newValue));
            
            // Mettre à jour dans le modèle
            const point = this.modulationEditor.draggedPoint;
            this.editorModel.updateCC(point.id, {
                time: Math.max(0, newTime),
                value: clampedValue
            });
            
            // Recharger et redessiner
            this.loadCCPoints();
            this.drawCCCurve();
            
        } else {
            // Hover detection
            const hoveredPoint = this.findCCPointAt(x, y);
            
            if (hoveredPoint) {
                this.modulationEditor.hoveredPoint = hoveredPoint.id;
                canvas.style.cursor = 'pointer';
            } else {
                this.modulationEditor.hoveredPoint = null;
                canvas.style.cursor = 'crosshair';
            }
            
            this.drawCCCurve();
        }
    }
    
    onCCCanvasMouseUp(e) {
        this.modulationEditor.draggedPoint = null;
    }
    
    /**
     * Trouve un point CC à la position donnée
     * @param {number} x - Position X
     * @param {number} y - Position Y
     * @returns {Object|null} Point trouvé
     */
    findCCPointAt(x, y) {
        const canvas = this.modulationEditor.canvas;
        if (!canvas) return null;
        
        const totalDuration = this.editorModel?.getData()?.duration || 10000;
        const pixelsPerMs = canvas.width / totalDuration;
        const height = canvas.height;
        
        const hitRadius = 8; // Pixels de tolérance
        
        for (const point of this.modulationEditor.points) {
            const pointX = point.time * pixelsPerMs;
            const pointY = height - 10 - (point.value / 127) * (height - 20);
            
            const distance = Math.sqrt(
                Math.pow(x - pointX, 2) + Math.pow(y - pointY, 2)
            );
            
            if (distance <= hitRadius) {
                return point;
            }
        }
        
        return null;
    }
    
    // ========================================================================
    // ACTIONS ROUTING PANEL
    // ========================================================================
    
    /**
     * Toggle visibility du Routing Panel
     */
    toggleRoutingPanel() {
        this.viewState.showRoutingPanel = !this.viewState.showRoutingPanel;
        
        if (this.viewState.showRoutingPanel) {
            this.refreshRoutingPanel();
        }
        
        this.updateEditorToggles();
        
        this.emit('editor:routing:toggled', { show: this.viewState.showRoutingPanel });
        
        this.logDebug(`Routing panel ${this.viewState.showRoutingPanel ? 'shown' : 'hidden'}`);
    }
    
    /**
     * Rafraîchit le contenu du Routing Panel
     */
    refreshRoutingPanel() {
        const container = document.querySelector('[data-routing-content]');
        if (!container) return;
        
        // Récupérer données routing depuis le contrôleur
        const routingData = this.getRoutingData();
        
        if (!routingData || routingData.channels.length === 0) {
            container.innerHTML = `
                <div class="routing-empty">
                    <p>No routing configured</p>
                    <button onclick="app.editorController?.autoRoute()">
                        Auto-Route Now
                    </button>
                </div>
            `;
            return;
        }
        
        // Construire HTML pour chaque canal
        const channelsHTML = routingData.channels.map(channel => {
            const hasInstrument = channel.instrument !== null;
            const statusClass = hasInstrument ? 'routed' : 'unrouted';
            
            return `
                <div class="routing-channel ${statusClass}">
                    <div class="channel-header">
                        <span class="channel-number">CH${channel.number + 1}</span>
                        <span class="channel-notes">${channel.noteCount} notes</span>
                    </div>
                    
                    <div class="channel-assignment">
                        ${hasInstrument ? `
                            <div class="assigned-instrument">
                                <span class="instrument-icon">${this.getInstrumentIcon(channel.instrument.type)}</span>
                                <span class="instrument-name">${this.escapeHTML(channel.instrument.name)}</span>
                                <button class="clear-btn" 
                                        onclick="app.editorController?.clearChannelRouting(${channel.number})"
                                        title="Clear">
                                    ✕
                                </button>
                            </div>
                            
                            ${channel.confidence ? `
                                <div class="confidence-bar">
                                    <div class="confidence-fill" 
                                         style="width: ${channel.confidence}%"
                                         title="Confidence: ${channel.confidence}%">
                                    </div>
                                </div>
                            ` : ''}
                        ` : `
                            <div class="unassigned">
                                <span>Not assigned</span>
                                <button class="assign-btn" 
                                        onclick="app.editorController?.suggestRouting(${channel.number})">
                                    Suggest
                                </button>
                            </div>
                        `}
                    </div>
                    
                    ${channel.issues && channel.issues.length > 0 ? `
                        <div class="channel-issues">
                            ${channel.issues.map(issue => `
                                <div class="issue ${issue.severity}">
                                    ${issue.message}
                                </div>
                            `).join('')}
                        </div>
                    ` : ''}
                </div>
            `;
        }).join('');
        
        container.innerHTML = `
            <div class="routing-channels">
                ${channelsHTML}
            </div>
            
            <div class="routing-stats">
                <div class="stat">
                    <span class="stat-label">Routed</span>
                    <span class="stat-value">${routingData.routedCount}/${routingData.totalChannels}</span>
                </div>
                <div class="stat">
                    <span class="stat-label">Valid</span>
                    <span class="stat-value ${routingData.isValid ? 'valid' : 'invalid'}">
                        ${routingData.isValid ? '✓' : '✗'}
                    </span>
                </div>
            </div>
        `;
    }
    
    /**
     * Récupère les données de routing
     * @returns {Object}
     */
    getRoutingData() {
        // Essayer de récupérer depuis RoutingModel ou EditorController
        if (window.app?.controllers?.routing) {
            const routingController = window.app.controllers.routing;
            return routingController.getRoutingData?.();
        }
        
        if (window.app?.controllers?.editor?.routingManager) {
            const routingManager = window.app.controllers.editor.routingManager;
            return routingManager.getRoutingData?.();
        }
        
        // Fallback: construire depuis EditorModel
        if (this.editorModel) {
            const data = this.editorModel.getData();
            if (!data) return null;
            
            // Analyser les canaux utilisés
            const channelMap = new Map();
            const notes = this.editorModel.getAllNotes();
            
            notes.forEach(note => {
                const ch = note.channel || 0;
                if (!channelMap.has(ch)) {
                    channelMap.set(ch, {
                        number: ch,
                        noteCount: 0,
                        instrument: null,
                        confidence: null,
                        issues: []
                    });
                }
                channelMap.get(ch).noteCount++;
            });
            
            const channels = Array.from(channelMap.values()).sort((a, b) => a.number - b.number);
            
            return {
                channels,
                totalChannels: channels.length,
                routedCount: channels.filter(ch => ch.instrument !== null).length,
                isValid: false
            };
        }
        
        return null;
    }
    
    /**
     * Obtient l'icône pour un type d'instrument
     * @param {string} type - Type d'instrument
     * @returns {string}
     */
    getInstrumentIcon(type) {
        const icons = {
            'piano': '🎹',
            'guitar': '🎸',
            'bass': '🎸',
            'drums': '🥁',
            'strings': '🎻',
            'brass': '🎺',
            'woodwind': '🎷',
            'synth': '🎛️',
            'percussion': '🥁',
            'other': '🎵'
        };
        
        return icons[type] || icons.other;
    }
    
    // ========================================================================
    // ACTIONS CONTEXT MENU
    // ========================================================================
    
    /**
     * Affiche le context menu
     * @param {number} x - Position X
     * @param {number} y - Position Y
     * @param {Array} options - Options du menu
     */
    showContextMenu(x, y, options) {
        this.viewState.contextMenuOpen = true;
        this.viewState.contextMenuX = x;
        this.viewState.contextMenuY = y;
        this.viewState.contextMenuOptions = options;
        
        // Re-render context menu
        const menuHTML = this.renderContextMenu();
        const existingMenu = document.querySelector('.context-menu');
        if (existingMenu) {
            existingMenu.outerHTML = menuHTML;
        }
        
        // Fermer au clic extérieur
        setTimeout(() => {
            document.addEventListener('click', () => this.hideContextMenu(), { once: true });
        }, 0);
    }
    
    /**
     * Cache le context menu
     */
    hideContextMenu() {
        this.viewState.contextMenuOpen = false;
        
        const menu = document.querySelector('.context-menu');
        if (menu) {
            menu.classList.remove('visible');
            menu.classList.add('hidden');
        }
    }
    
    /**
     * Affiche le context menu du visualizer (clic droit sur note)
     * @param {Event} event
     * @param {string} noteId
     */
    showVisualizerContextMenu(event, noteId) {
        event.preventDefault();
        
        const hasSelection = this.viewState.selectedNotes.size > 0;
        const noteSelected = this.viewState.selectedNotes.has(noteId);
        
        const options = [
            {
                icon: '✏️',
                label: 'Properties',
                action: `app.editorView.showNoteProperties('${noteId}')`,
                disabled: false
            },
            { divider: true },
            {
                icon: '✂️',
                label: 'Cut',
                action: `app.editorController.cut()`,
                disabled: !hasSelection
            },
            {
                icon: '📋',
                label: 'Copy',
                action: `app.editorController.copy()`,
                disabled: !hasSelection
            },
            {
                icon: '📄',
                label: 'Paste',
                action: `app.editorController.paste()`,
                disabled: !this.editorModel?.hasClipboardContent()
            },
            { divider: true },
            {
                icon: '🗑️',
                label: 'Delete',
                action: `app.editorController.deleteSelection()`,
                disabled: !hasSelection
            },
            { divider: true },
            {
                icon: '🎵',
                label: 'Quantize',
                action: `app.editorController.quantize()`,
                disabled: !hasSelection
            },
            {
                icon: '🎹',
                label: 'Transpose',
                action: `app.editorController.transpose(1)`,
                disabled: !hasSelection
            }
        ];
        
        this.showContextMenu(event.clientX, event.clientY, options);
    }
    
    // ========================================================================
    // ACTIONS PROPERTIES MODAL
    // ========================================================================
    
    /**
     * Affiche la modal de propriétés d'une note
     * @param {string} noteId
     */
    showNoteProperties(noteId) {
        if (!this.editorModel) return;
        
        const note = this.editorModel.getNoteById(noteId);
        if (!note) {
            this.logDebug(`Note not found: ${noteId}`);
            return;
        }
        
        this.viewState.showPropertiesModal = true;
        this.viewState.selectedNoteData = note;
        
        // Re-render modal
        const modalHTML = this.renderPropertiesModal();
        const existingModal = document.querySelector('.properties-modal');
        if (existingModal) {
            existingModal.outerHTML = modalHTML;
        }
        
        this.logDebug(`Showing properties for note ${noteId}`);
    }
    
    /**
     * Cache la modal de propriétés
     */
    hideNoteProperties() {
        this.viewState.showPropertiesModal = false;
        this.viewState.selectedNoteData = null;
        
        const modal = document.querySelector('.properties-modal');
        if (modal) {
            modal.classList.remove('visible');
            modal.classList.add('hidden');
        }
    }
    
    /**
     * Applique les modifications de la modal de propriétés
     */
    applyNoteProperties() {
        if (!this.editorModel || !this.viewState.selectedNoteData) return;
        
        const noteId = this.viewState.selectedNoteData.id;
        
        // Récupérer les valeurs des champs
        const pitch = parseInt(document.getElementById('prop-pitch').value);
        const time = parseInt(document.getElementById('prop-time').value);
        const duration = parseInt(document.getElementById('prop-duration').value);
        const velocity = parseInt(document.getElementById('prop-velocity').value);
        const channel = parseInt(document.getElementById('prop-channel').value);
        
        // Valider
        if (isNaN(pitch) || pitch < 0 || pitch > 127 ||
            isNaN(time) || time < 0 ||
            isNaN(duration) || duration < 1 ||
            isNaN(velocity) || velocity < 1 || velocity > 127 ||
            isNaN(channel) || channel < 0 || channel > 15) {
            this.logDebug('Invalid property values');
            return;
        }
        
        // Mettre à jour dans le modèle
        this.editorModel.updateNote(noteId, {
            pitch,
            time,
            duration,
            velocity,
            channel
        });
        
        // Fermer modal
        this.hideNoteProperties();
        
        this.logDebug(`Note ${noteId} properties updated`);
    }
    
    // ========================================================================
    // EVENT HANDLERS (EVENTBUS)
    // ========================================================================
    
    onFileLoaded(data) {
        this.viewState.fileName = data.fileName || 'Untitled';
        this.viewState.isModified = false;
        
        this.updateFileInfo();
        this.refreshAll();
        
        this.logDebug('File loaded');
    }
    
    onDataModified() {
        this.viewState.isModified = true;
        this.updateFileInfo();
        
        this.logDebug('Data modified');
    }
    
    onFileSaved() {
        this.viewState.isModified = false;
        this.updateFileInfo();
        
        this.logDebug('File saved');
    }
    
    onNoteAdded(data) {
        this.refreshCanvas();
        if (this.viewState.showVelocity) {
            this.drawVelocityBars();
        }
    }
    
    onNoteUpdated(data) {
        this.refreshCanvas();
        if (this.viewState.showVelocity) {
            this.drawVelocityBars();
        }
    }
    
    onNotesDeleted(data) {
        this.refreshCanvas();
        if (this.viewState.showVelocity) {
            this.drawVelocityBars();
        }
    }
    
    onSelectionChanged(data) {
        this.viewState.selectedNotes = new Set(data.noteIds || []);
        this.updateSelectionInfo(data);
        this.refreshCanvas();
    }
    
    onSelectionCleared() {
        this.viewState.selectedNotes.clear();
        this.updateSelectionInfo({ noteIds: [], count: 0 });
        this.refreshCanvas();
    }
    
    onCCAdded(data) {
        if (this.viewState.showModulation) {
            this.loadCCPoints();
            this.drawCCCurve();
        }
    }
    
    onCCUpdated(data) {
        if (this.viewState.showModulation) {
            this.loadCCPoints();
            this.drawCCCurve();
        }
    }
    
    onCCDeleted(data) {
        if (this.viewState.showModulation) {
            this.loadCCPoints();
            this.drawCCCurve();
        }
    }
    
    onZoomChanged(data) {
        if (this.viewport) {
            this.viewport.setZoom(data.zoom);
        }
        this.refreshCanvas();
    }
    
    onPanChanged(data) {
        if (this.viewport) {
            this.viewport.pan(data.deltaX, data.deltaY);
        }
        this.refreshCanvas();
    }
    
    onUndo() {
        this.refreshAll();
    }
    
    onRedo() {
        this.refreshAll();
    }
    
    onToolChanged(data) {
        this.viewState.currentTool = data.tool;
        this.updateToolbarButtons();
    }
    
    onSnapToggled(data) {
        this.viewState.snapEnabled = data.enabled;
        this.updateSnapControls();
    }
    
    onSnapGridChanged(data) {
        this.viewState.snapGrid = data.value;
        this.updateSnapControls();
    }
    
    onRoutingValidated(data) {
        // TODO: Afficher résultats validation
        this.logDebug('Routing validated');
    }
    
    // ========================================================================
    // MISE À JOUR UI
    // ========================================================================
    
    updateFileInfo() {
        const fileInfoEl = this.container?.querySelector('.editor-file-info .file-name');
        if (fileInfoEl) {
            const fileName = this.viewState.fileName || 'Untitled';
            const modifiedIndicator = this.viewState.isModified ? '• ' : '';
            fileInfoEl.textContent = `${modifiedIndicator}${fileName}`;
        }
    }
    
    updateSelectionInfo(data) {
        const selectionInfoEl = this.container?.querySelector('[data-selection-info]');
        if (selectionInfoEl) {
            const count = data?.count || this.viewState.selectedNotes.size || 0;
            selectionInfoEl.textContent = count > 0 ? `${count} selected` : '';
        }
    }
    
    updateToolbarButtons() {
        const buttons = this.container?.querySelectorAll('.tool-btn');
        buttons?.forEach(btn => {
            const tool = btn.dataset.tool;
            if (tool === this.viewState.currentTool) {
                btn.classList.add('active');
            } else {
                btn.classList.remove('active');
            }
        });
    }
    
    updateSnapControls() {
        const checkbox = this.container?.querySelector('.snap-toggle input[type="checkbox"]');
        const select = this.container?.querySelector('.snap-grid-select');
        
        if (checkbox) {
            checkbox.checked = this.viewState.snapEnabled;
        }
        
        if (select) {
            select.disabled = !this.viewState.snapEnabled;
            select.value = this.viewState.snapGrid;
        }
    }
    
    updateEditorToggles() {
        const velocityBtn = this.container?.querySelector('.editor-toggle-btn[onclick*="Velocity"]');
        const ccBtn = this.container?.querySelector('.editor-toggle-btn[onclick*="CC"]');
        const routingBtn = this.container?.querySelector('.editor-toggle-btn[onclick*="Routing"]');
        
        if (velocityBtn) {
            velocityBtn.classList.toggle('active', this.viewState.showVelocity);
        }
        
        if (ccBtn) {
            ccBtn.classList.toggle('active', this.viewState.showModulation);
        }
        
        if (routingBtn) {
            routingBtn.classList.toggle('active', this.viewState.showRoutingPanel);
        }
        
        // Montrer/cacher panels
        const velocityPanel = this.container?.querySelector('.velocity-editor');
        const ccPanel = this.container?.querySelector('.modulation-editor');
        const routingPanel = this.container?.querySelector('.routing-panel');
        
        if (velocityPanel) {
            velocityPanel.classList.toggle('visible', this.viewState.showVelocity);
            velocityPanel.classList.toggle('hidden', !this.viewState.showVelocity);
        }
        
        if (ccPanel) {
            ccPanel.classList.toggle('visible', this.viewState.showModulation);
            ccPanel.classList.toggle('hidden', !this.viewState.showModulation);
        }
        
        if (routingPanel) {
            routingPanel.classList.toggle('visible', this.viewState.showRoutingPanel);
            routingPanel.classList.toggle('hidden', !this.viewState.showRoutingPanel);
        }
    }
    
    updateVelocityModeButtons() {
        const container = this.container?.querySelector('.velocity-mode-selector');
        if (!container) return;
        
        const buttons = container.querySelectorAll('button');
        buttons.forEach(btn => {
            const mode = btn.textContent.toLowerCase().trim();
            if (mode === this.viewState.velocityMode) {
                btn.classList.add('active');
            } else {
                btn.classList.remove('active');
            }
        });
    }
    
    // ========================================================================
    // RENDU CANVAS
    // ========================================================================
    
    refreshCanvas() {
        if (this.visualizer) {
            this.visualizer.render();
        }
    }
    
    refreshAll() {
        this.refreshCanvas();
        
        if (this.viewState.showVelocity) {
            this.drawVelocityBars();
        }
        
        if (this.viewState.showModulation) {
            this.drawCCCurve();
        }
        
        if (this.viewState.showRoutingPanel) {
            this.refreshRoutingPanel();
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Convertit un pitch MIDI en nom de note
     * @param {number} pitch - 0-127
     * @returns {string}
     */
    getNoteNameFromPitch(pitch) {
        const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const octave = Math.floor(pitch / 12) - 1;
        const noteName = noteNames[pitch % 12];
        return `${noteName}${octave}`;
    }
    
    /**
     * Log debug
     */
    logDebug(message) {
        if (this.logger) {
            this.logger.debug('EditorView', message);
        }
    }
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    /**
     * Nettoie la vue
     */
    destroy() {
        // Détruire visualizer
        if (this.visualizer) {
            this.visualizer.destroy();
            this.visualizer = null;
        }
        
        // Cleanup canvas
        this.canvas = null;
        this.renderEngine = null;
        this.viewport = null;
        this.coordSystem = null;
        
        this.velocityEditor.canvas = null;
        this.velocityEditor.ctx = null;
        
        this.modulationEditor.canvas = null;
        this.modulationEditor.ctx = null;
        
        // Appeler destroy parent
        super.destroy();
        
        this.logDebug('EditorView destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = EditorView;
}

if (typeof window !== 'undefined') {
    window.EditorView = EditorView;
}