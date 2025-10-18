// ============================================================================
// Fichier: frontend/scripts/views/components/Modal.js
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi  
// ============================================================================
// Description:
//   Composant de fenêtre modale réutilisable.
//   Supporte différents types, tailles et animations.
//
// Auteur: midiMind Team
// Date: 2025-10-04
// Version: 3.0.0
// ============================================================================

class Modal {
    constructor(config = {}) {
        // Configuration par défaut
        this.config = {
            title: config.title || '',
            content: config.content || '',
            size: config.size || 'medium',          // small, medium, large, fullscreen
            type: config.type || 'default',         // default, info, success, warning, error, confirm
            closable: config.closable !== false,
            backdrop: config.backdrop !== false,
            keyboard: config.keyboard !== false,     // Fermer avec Escape
            animated: config.animated !== false,
            centered: config.centered !== false,
            scrollable: config.scrollable || false,
            buttons: config.buttons || [],
            onShow: config.onShow || null,
            onShown: config.onShown || null,
            onHide: config.onHide || null,
            onHidden: config.onHidden || null,
            ...config
        };
        
        // État
        this.state = {
            isOpen: false,
            isAnimating: false
        };
        
        // Éléments DOM
        this.elements = {
            container: null,
            backdrop: null,
            modal: null,
            header: null,
            body: null,
            footer: null
        };
        
        // ID unique
        this.id = `modal-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
        
        // Pile de modales (pour gérer plusieurs modales)
        Modal.stack = Modal.stack || [];
        
        this.init();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    init() {
        this.createElement();
        this.attachEvents();
    }
    
    /**
     * Créer les éléments DOM de la modale
     */
    createElement() {
        // Conteneur principal
        this.elements.container = document.createElement('div');
        this.elements.container.className = 'modal-container';
        this.elements.container.id = this.id;
        this.elements.container.style.display = 'none';
        
        // Backdrop
        if (this.config.backdrop) {
            this.elements.backdrop = document.createElement('div');
            this.elements.backdrop.className = 'modal-backdrop';
            this.elements.container.appendChild(this.elements.backdrop);
        }
        
        // Modale
        this.elements.modal = document.createElement('div');
        this.elements.modal.className = `modal modal-${this.config.size} modal-${this.config.type}`;
        
        if (this.config.centered) {
            this.elements.modal.classList.add('modal-centered');
        }
        
        // Structure interne
        this.elements.modal.innerHTML = this.buildModalContent();
        
        // Récupérer les références
        this.elements.header = this.elements.modal.querySelector('.modal-header');
        this.elements.body = this.elements.modal.querySelector('.modal-body');
        this.elements.footer = this.elements.modal.querySelector('.modal-footer');
        
        // Ajouter au conteneur
        this.elements.container.appendChild(this.elements.modal);
        
        // Ajouter au body
        document.body.appendChild(this.elements.container);
        
        // Appliquer les styles
        this.applyStyles();
    }
    
    /**
     * Construire le contenu HTML de la modale
     */
    buildModalContent() {
        let html = '<div class="modal-content">';
        
        // Header
        if (this.config.title || this.config.closable) {
            html += '<div class="modal-header">';
            
            if (this.config.title) {
                html += `<h3 class="modal-title">${this.getIcon()}${this.config.title}</h3>`;
            }
            
            if (this.config.closable) {
                html += `
                    <button class="modal-close" aria-label="Fermer">
                        <span aria-hidden="true">&times;</span>
                    </button>
                `;
            }
            
            html += '</div>';
        }
        
        // Body
        html += `<div class="modal-body ${this.config.scrollable ? 'modal-body-scrollable' : ''}">`;
        html += this.config.content;
        html += '</div>';
        
        // Footer avec boutons
        if (this.config.buttons.length > 0 || this.config.type === 'confirm') {
            html += '<div class="modal-footer">';
            html += this.buildButtons();
            html += '</div>';
        }
        
        html += '</div>';
        
        return html;
    }
    
    /**
     * Construire les boutons
     */
    buildButtons() {
        let buttons = '';
        
        // Boutons par défaut pour confirm
        if (this.config.type === 'confirm' && this.config.buttons.length === 0) {
            this.config.buttons = [
                {
                    text: 'Annuler',
                    class: 'btn-secondary',
                    action: 'cancel'
                },
                {
                    text: 'Confirmer',
                    class: 'btn-primary',
                    action: 'confirm'
                }
            ];
        }
        
        // Générer les boutons
        this.config.buttons.forEach((button, index) => {
            const btnClass = button.class || 'btn-default';
            const btnText = button.text || 'Button';
            const btnAction = button.action || `button-${index}`;
            
            buttons += `
                <button class="btn ${btnClass}" data-action="${btnAction}">
                    ${button.icon ? `<span class="btn-icon">${button.icon}</span>` : ''}
                    ${btnText}
                </button>
            `;
        });
        
        return buttons;
    }
    
    /**
     * Obtenir l'icône selon le type
     */
    getIcon() {
        const icons = {
            'info': 'ℹ️',
            'success': '✅',
            'warning': '⚠️',
            'error': '❌',
            'confirm': '❓',
            'default': ''
        };
        
        const icon = icons[this.config.type] || '';
        return icon ? `<span class="modal-icon">${icon}</span>` : '';
    }
    
    /**
     * Appliquer les styles CSS
     */
    applyStyles() {
        if (document.getElementById('modal-styles')) return;
        
        const style = document.createElement('style');
        style.id = 'modal-styles';
        style.textContent = `
            .modal-container {
                position: fixed;
                top: 0;
                left: 0;
                width: 100%;
                height: 100%;
                z-index: 9999;
                display: flex;
                align-items: center;
                justify-content: center;
            }
            
            .modal-backdrop {
                position: absolute;
                top: 0;
                left: 0;
                width: 100%;
                height: 100%;
                background: rgba(0, 0, 0, 0.5);
                backdrop-filter: blur(5px);
            }
            
            .modal {
                position: relative;
                background: linear-gradient(145deg, #2a2a3e, #1a1a2e);
                border-radius: 12px;
                box-shadow: 0 10px 40px rgba(0, 0, 0, 0.5);
                max-height: 90vh;
                display: flex;
                flex-direction: column;
                opacity: 0;
                transform: scale(0.9) translateY(20px);
                transition: all 0.3s ease;
            }
            
            .modal.show {
                opacity: 1;
                transform: scale(1) translateY(0);
            }
            
            /* Tailles */
            .modal-small { width: 400px; max-width: 90%; }
            .modal-medium { width: 600px; max-width: 90%; }
            .modal-large { width: 900px; max-width: 90%; }
            .modal-fullscreen {
                width: 95%;
                height: 95vh;
                max-width: none;
            }
            
            .modal-centered {
                margin: auto;
            }
            
            .modal-content {
                display: flex;
                flex-direction: column;
                height: 100%;
            }
            
            .modal-header {
                padding: 20px;
                border-bottom: 1px solid rgba(255, 255, 255, 0.1);
                display: flex;
                align-items: center;
                justify-content: space-between;
                flex-shrink: 0;
            }
            
            .modal-title {
                margin: 0;
                font-size: 20px;
                font-weight: 600;
                color: #fff;
                display: flex;
                align-items: center;
                gap: 10px;
            }
            
            .modal-icon {
                font-size: 24px;
            }
            
            .modal-close {
                background: none;
                border: none;
                color: rgba(255, 255, 255, 0.6);
                font-size: 28px;
                cursor: pointer;
                padding: 0;
                width: 32px;
                height: 32px;
                display: flex;
                align-items: center;
                justify-content: center;
                border-radius: 4px;
                transition: all 0.2s ease;
            }
            
            .modal-close:hover {
                background: rgba(255, 255, 255, 0.1);
                color: #fff;
            }
            
            .modal-body {
                padding: 20px;
                flex: 1;
                overflow-y: auto;
                color: rgba(255, 255, 255, 0.9);
            }
            
            .modal-body-scrollable {
                max-height: 60vh;
                overflow-y: auto;
            }
            
            .modal-body::-webkit-scrollbar {
                width: 8px;
            }
            
            .modal-body::-webkit-scrollbar-track {
                background: rgba(255, 255, 255, 0.05);
                border-radius: 4px;
            }
            
            .modal-body::-webkit-scrollbar-thumb {
                background: rgba(255, 255, 255, 0.2);
                border-radius: 4px;
            }
            
            .modal-footer {
                padding: 16px 20px;
                border-top: 1px solid rgba(255, 255, 255, 0.1);
                display: flex;
                gap: 12px;
                justify-content: flex-end;
                flex-shrink: 0;
            }
            
            /* Types de modales */
            .modal-info .modal-header {
                background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            }
            
            .modal-success .modal-header {
                background: linear-gradient(135deg, #84fab0 0%, #8fd3f4 100%);
            }
            
            .modal-warning .modal-header {
                background: linear-gradient(135deg, #f7b733 0%, #fc4a1a 100%);
            }
            
            .modal-error .modal-header {
                background: linear-gradient(135deg, #eb3349 0%, #f45c43 100%);
            }
            
            /* Boutons dans la modale */
            .modal .btn {
                padding: 8px 16px;
                border-radius: 6px;
                border: none;
                cursor: pointer;
                font-size: 14px;
                font-weight: 500;
                transition: all 0.2s ease;
                display: inline-flex;
                align-items: center;
                gap: 6px;
            }
            
            .modal .btn-primary {
                background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                color: white;
            }
            
            .modal .btn-secondary {
                background: rgba(255, 255, 255, 0.1);
                color: rgba(255, 255, 255, 0.8);
                border: 1px solid rgba(255, 255, 255, 0.2);
            }
            
            .modal .btn-danger {
                background: linear-gradient(135deg, #eb3349 0%, #f45c43 100%);
                color: white;
            }
            
            .modal .btn:hover {
                transform: translateY(-1px);
                box-shadow: 0 4px 12px rgba(0, 0, 0, 0.2);
            }
            
            /* Animation shake pour erreur */
            @keyframes shake {
                0%, 100% { transform: translateX(0); }
                10%, 30%, 50%, 70%, 90% { transform: translateX(-5px); }
                20%, 40%, 60%, 80% { transform: translateX(5px); }
            }
            
            .modal.shake {
                animation: shake 0.5s;
            }
            
            /* Responsive */
            @media (max-width: 768px) {
                .modal-small,
                .modal-medium,
                .modal-large {
                    width: 95%;
                    max-width: none;
                }
                
                .modal-body {
                    max-height: 60vh;
                }
            }
        `;
        
        document.head.appendChild(style);
    }
    
    // ========================================================================
    // ÉVÉNEMENTS
    // ========================================================================
    
    attachEvents() {
        // Fermeture par clic sur backdrop
        if (this.config.backdrop && this.config.closable) {
            this.elements.backdrop.addEventListener('click', () => this.hide());
        }
        
        // Bouton de fermeture
        const closeBtn = this.elements.modal.querySelector('.modal-close');
        if (closeBtn) {
            closeBtn.addEventListener('click', () => this.hide());
        }
        
        // Boutons du footer
        const buttons = this.elements.modal.querySelectorAll('.modal-footer button');
        buttons.forEach(button => {
            button.addEventListener('click', (e) => this.handleButtonClick(e));
        });
        
        // Fermeture avec Escape
        if (this.config.keyboard) {
            this.keyHandler = (e) => {
                if (e.key === 'Escape' && this.state.isOpen) {
                    // Vérifier si c'est la modale au dessus de la pile
                    if (Modal.stack[Modal.stack.length - 1] === this) {
                        this.hide();
                    }
                }
            };
            document.addEventListener('keydown', this.keyHandler);
        }
    }
    
    /**
     * Gérer le clic sur un bouton
     */
    handleButtonClick(event) {
        const button = event.target.closest('button');
        const action = button.dataset.action;
        
        // Chercher la configuration du bouton
        const buttonConfig = this.config.buttons.find(b => b.action === action);
        
        // Exécuter le callback si défini
        if (buttonConfig && buttonConfig.callback) {
            const result = buttonConfig.callback(this);
            
            // Fermer si le callback retourne true
            if (result !== false) {
                this.hide();
            }
        } else {
            // Actions par défaut
            switch (action) {
                case 'cancel':
                case 'close':
                    this.hide();
                    break;
                case 'confirm':
                    if (this.config.onConfirm) {
                        this.config.onConfirm();
                    }
                    this.hide();
                    break;
                default:
                    this.hide();
            }
        }
    }
    
    // ========================================================================
    // AFFICHAGE / MASQUAGE
    // ========================================================================
    
    /**
     * Afficher la modale
     */
    show() {
        if (this.state.isOpen || this.state.isAnimating) return;
        
        this.state.isAnimating = true;
        
        // Callback onShow
        if (this.config.onShow) {
            this.config.onShow(this);
        }
        
        // Ajouter à la pile
        Modal.stack.push(this);
        
        // Afficher le conteneur
        this.elements.container.style.display = 'flex';
        
        // Force reflow
        this.elements.container.offsetHeight;
        
        // Ajouter la classe show avec animation
        requestAnimationFrame(() => {
            this.elements.modal.classList.add('show');
            
            setTimeout(() => {
                this.state.isOpen = true;
                this.state.isAnimating = false;
                
                // Callback onShown
                if (this.config.onShown) {
                    this.config.onShown(this);
                }
                
                // Focus sur le premier élément focusable
                this.focusFirstElement();
            }, 300);
        });
        
        // Empêcher le scroll du body
        document.body.style.overflow = 'hidden';
    }
    
    /**
     * Masquer la modale
     */
    hide() {
        if (!this.state.isOpen || this.state.isAnimating) return;
        
        this.state.isAnimating = true;
        
        // Callback onHide
        if (this.config.onHide) {
            const result = this.config.onHide(this);
            if (result === false) {
                this.state.isAnimating = false;
                return;
            }
        }
        
        // Retirer la classe show
        this.elements.modal.classList.remove('show');
        
        setTimeout(() => {
            // Masquer le conteneur
            this.elements.container.style.display = 'none';
            
            this.state.isOpen = false;
            this.state.isAnimating = false;
            
            // Retirer de la pile
            const index = Modal.stack.indexOf(this);
            if (index > -1) {
                Modal.stack.splice(index, 1);
            }
            
            // Réactiver le scroll si plus de modales
            if (Modal.stack.length === 0) {
                document.body.style.overflow = '';
            }
            
            // Callback onHidden
            if (this.config.onHidden) {
                this.config.onHidden(this);
            }
        }, 300);
    }
    
    /**
     * Basculer l'affichage
     */
    toggle() {
        if (this.state.isOpen) {
            this.hide();
        } else {
            this.show();
        }
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES
    // ========================================================================
    
    /**
     * Mettre à jour le titre
     */
    setTitle(title) {
        this.config.title = title;
        const titleElement = this.elements.header?.querySelector('.modal-title');
        if (titleElement) {
            titleElement.innerHTML = this.getIcon() + title;
        }
    }
    
    /**
     * Mettre à jour le contenu
     */
    setContent(content) {
        this.config.content = content;
        if (this.elements.body) {
            this.elements.body.innerHTML = content;
        }
    }
    
    /**
     * Ajouter du contenu
     */
    appendContent(content) {
        if (this.elements.body) {
            if (typeof content === 'string') {
                this.elements.body.insertAdjacentHTML('beforeend', content);
            } else if (content instanceof HTMLElement) {
                this.elements.body.appendChild(content);
            }
        }
    }
    
    /**
     * Mettre à jour les boutons
     */
    setButtons(buttons) {
        this.config.buttons = buttons;
        
        if (this.elements.footer) {
            this.elements.footer.innerHTML = this.buildButtons();
            
            // Réattacher les événements
            const btns = this.elements.footer.querySelectorAll('button');
            btns.forEach(button => {
                button.addEventListener('click', (e) => this.handleButtonClick(e));
            });
        }
    }
    
    /**
     * Secouer la modale (effet d'erreur)
     */
    shake() {
        this.elements.modal.classList.add('shake');
        setTimeout(() => {
            this.elements.modal.classList.remove('shake');
        }, 500);
    }
    
    /**
     * Focus sur le premier élément focusable
     */
    focusFirstElement() {
        const focusable = this.elements.modal.querySelectorAll(
            'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])'
        );
        
        if (focusable.length > 0) {
            focusable[0].focus();
        }
    }
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    /**
     * Détruire la modale
     */
    destroy() {
        // Masquer si ouverte
        if (this.state.isOpen) {
            this.hide();
        }
        
        // Retirer les événements
        if (this.keyHandler) {
            document.removeEventListener('keydown', this.keyHandler);
        }
        
        // Retirer du DOM après animation
        setTimeout(() => {
            if (this.elements.container && this.elements.container.parentNode) {
                this.elements.container.parentNode.removeChild(this.elements.container);
            }
        }, 300);
    }
    
    // ========================================================================
    // MÉTHODES STATIQUES
    // ========================================================================
    
    /**
     * Afficher une alerte
     */
    static alert(message, title = 'Information', type = 'info') {
        const modal = new Modal({
            title: title,
            content: `<p>${message}</p>`,
            type: type,
            size: 'small',
            buttons: [{
                text: 'OK',
                class: 'btn-primary',
                action: 'close'
            }]
        });
        
        modal.show();
        return modal;
    }
    
    /**
     * Afficher une confirmation
     */
    static confirm(message, title = 'Confirmation', onConfirm = null, onCancel = null) {
        const modal = new Modal({
            title: title,
            content: `<p>${message}</p>`,
            type: 'confirm',
            size: 'small',
            onConfirm: onConfirm,
            onHide: onCancel
        });
        
        modal.show();
        return modal;
    }
    
    /**
     * Afficher un prompt
     */
    static prompt(message, title = 'Saisie', defaultValue = '', onSubmit = null) {
        const inputId = `prompt-input-${Date.now()}`;
        
        const modal = new Modal({
            title: title,
            content: `
                <p>${message}</p>
                <input type="text" 
                       id="${inputId}" 
                       class="form-control" 
                       value="${defaultValue}"
                       style="width: 100%; padding: 8px; margin-top: 10px;">
            `,
            type: 'default',
            size: 'small',
            buttons: [
                {
                    text: 'Annuler',
                    class: 'btn-secondary',
                    action: 'cancel'
                },
                {
                    text: 'OK',
                    class: 'btn-primary',
                    callback: () => {
                        const input = document.getElementById(inputId);
                        if (onSubmit) {
                            onSubmit(input.value);
                        }
                        return true;
                    }
                }
            ]
        });
        
        modal.show();
        
        // Focus sur l'input
        setTimeout(() => {
            const input = document.getElementById(inputId);
            if (input) {
                input.focus();
                input.select();
            }
        }, 400);
        
        return modal;
    }
    
    /**
     * Fermer toutes les modales
     */
    static closeAll() {
        [...Modal.stack].forEach(modal => modal.hide());
    }
}