// ===== NAVIGATION CONTROLLER - Contrôleur de navigation et gestion des pages =====
// ================================================================================
// Fichier: frontend/js/controllers/NavigationController.js
// Version: v3.7.0 - CORRECTION IDS PAGES
// Date: 2025-10-24
// ================================================================================
// CORRECTIONS v3.7.0:
// ✅ IDs de pages corrigés pour correspondre au HTML (home, editor, routing, etc.)
// ✅ Ajout gestion display:none/block pour affichage correct des pages
// ✅ Correction getView() pour mapper correctement les vues
// ================================================================================

class NavigationController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // État de navigation
        this.currentPage = 'home';
        this.previousPage = null;
        this.navigationHistory = ['home'];
        this.historyIndex = 0;
        
        // Configuration des pages - IDS CORRIGES
        this.pages = {
            home: {
                id: 'home',  // ✅ Correspond à <div id="home">
                title: '🏠 Accueil',
                icon: '🏠',
                shortcut: 'h',
                requiresData: true,
                cacheable: false,
                viewKey: 'home'  // Clé pour récupérer la vue
            },
            editor: {
                id: 'editor',  // ✅ Correspond à <div id="editor">
                title: '✏️ Éditeur',
                icon: '✏️',
                shortcut: 'e',
                requiresData: true,
                cacheable: false,
                viewKey: 'editor'
            },
            routing: {
                id: 'routing',  // ✅ Correspond à <div id="routing">
                title: '🔀 Routage',
                icon: '🔀',
                shortcut: 'r',
                requiresData: true,
                cacheable: true,
                viewKey: 'routing'
            },
            keyboard: {
                id: 'keyboard',  // ✅ Correspond à <div id="keyboard">
                title: '🎹 Clavier',
                icon: '🎹',
                shortcut: 'k',
                requiresData: true,
                cacheable: false,
                viewKey: 'keyboard'
            },
            instruments: {
                id: 'instruments',  // ✅ Correspond à <div id="instruments">
                title: '🎸 Instruments',
                icon: '🎸',
                shortcut: 'i',
                requiresData: true,
                cacheable: true,
                viewKey: 'instrument'  // Note: la vue s'appelle 'instrument' pas 'instruments'
            },
            system: {
                id: 'system',  // ✅ Correspond à <div id="system">
                title: '⚙️ Système',
                icon: '⚙️',
                shortcut: 's',
                requiresData: true,
                cacheable: true,
                viewKey: 'system'
            }
        };
        
        // État des transitions
        this.transitionState = {
            inProgress: false,
            duration: 300, // ms
            easing: 'cubic-bezier(0.4, 0, 0.2, 1)'
        };
        
        // Cache des pages
        this.pageCache = new Map();
        this.cacheTimeouts = new Map();
        this.defaultCacheDuration = 30000; // 30 secondes
        
        // Configuration des animations
        this.animationConfig = {
            enableTransitions: true,
            slideDirection: 'horizontal',
            parallax: false,
            preloadNext: true
        };
        
        this.initializeNavigation();
    }

    /**
     * Configuration des événements
     */
    bindEvents() {
        // Écouter les changements de modèles pour mise à jour automatique
        this.eventBus.on('model:changed', (data) => {
            this.handleModelChange(data);
        });
        
        // Écouter les événements de navigation
        this.eventBus.on('navigation:page_request', (data) => {
            this.showPage(data.page, data.options);
        });
        
        this.eventBus.on('navigation:back', () => {
            this.goBack();
        });
        
        this.eventBus.on('navigation:forward', () => {
            this.goForward();
        });
        
        // Écouter les événements de données
        this.eventBus.on('file:added', () => {
            this.invalidatePageCache(['home', 'editor']);
        });
        
        this.eventBus.on('instrument:updated', () => {
            this.invalidatePageCache(['home', 'instruments', 'keyboard']);
        });
        
        this.eventBus.on('playlist:updated', () => {
            this.invalidatePageCache(['home', 'editor']);
        });
    }

    /**
     * Initialise le système de navigation
     */
    initializeNavigation() {
        // Configurer les raccourcis clavier
        this.setupKeyboardShortcuts();
        
        // Gérer l'historique du navigateur
        this.setupBrowserHistory();
        
        // Gérer les clics sur la navigation
        this.setupNavigationLinks();
        
        // Afficher la page d'accueil
        this.showPage('home', { skipHistory: true });
        
        this.logDebug('navigation', 'Système de navigation initialisé');
    }

    /**
     * Configure les liens de navigation
     */
    setupNavigationLinks() {
        document.querySelectorAll('.nav-item').forEach(link => {
            link.addEventListener('click', (e) => {
                e.preventDefault();
                const page = link.getAttribute('data-page');
                if (page) {
                    this.showPage(page);
                }
            });
        });
    }

    /**
     * Configure les raccourcis clavier pour la navigation
     */
    setupKeyboardShortcuts() {
        document.addEventListener('keydown', (event) => {
            // Ignorer si on est dans un champ de saisie
            if (event.target.tagName === 'INPUT' || event.target.tagName === 'TEXTAREA') {
                return;
            }
            
            // Raccourcis avec Alt
            if (event.altKey) {
                Object.entries(this.pages).forEach(([pageKey, pageConfig]) => {
                    if (event.key.toLowerCase() === pageConfig.shortcut) {
                        event.preventDefault();
                        this.showPage(pageKey);
                    }
                });
                
                // Navigation historique
                if (event.key === 'ArrowLeft') {
                    event.preventDefault();
                    this.goBack();
                } else if (event.key === 'ArrowRight') {
                    event.preventDefault();
                    this.goForward();
                }
            }
        });
    }

    /**
     * Configure la gestion de l'historique du navigateur
     */
    setupBrowserHistory() {
        // Gérer les changements d'URL
        window.addEventListener('popstate', (event) => {
            if (event.state && event.state.page) {
                this.showPage(event.state.page, { skipHistory: true, skipPushState: true });
            }
        });
        
        // Définir l'état initial
        history.replaceState({ page: this.currentPage }, '', `#${this.currentPage}`);
    }

    // ===== NAVIGATION PRINCIPALE =====

    /**
     * Affiche une page spécifique
     * @param {string} pageKey - Clé de la page à afficher
     * @param {Object} options - Options de navigation
     */
    async showPage(pageKey, options = {}) {
        const {
            skipHistory = false,
            skipPushState = false,
            forceRefresh = false,
            animationDirection = null
        } = options;
        
        // Valider la page
        if (!this.pages[pageKey]) {
            this.logDebug('navigation', `Page invalide: ${pageKey}`);
            this.showNotification('Page introuvable', 'error');
            return false;
        }
        
        // Éviter les transitions inutiles
        if (pageKey === this.currentPage && !forceRefresh) {
            this.logDebug('navigation', `Page déjà active: ${pageKey}`);
            return true;
        }
        
        // Vérifier si une transition est en cours
        if (this.transitionState.inProgress) {
            this.logDebug('navigation', 'Transition déjà en cours, ignorée');
            return false;
        }
        
        this.logDebug('navigation', `Navigation vers: ${pageKey}`);
        
        try {
            // Préparer la transition
            this.transitionState.inProgress = true;
            this.previousPage = this.currentPage;
            
            // Émettre l'événement de début de navigation
            this.eventBus.emit('navigation:page_changing', {
                from: this.currentPage,
                to: pageKey
            });
            
            // Effectuer la transition
            const success = await this.performPageTransition(pageKey, animationDirection);
            
            if (success) {
                // Mettre à jour l'état de navigation
                this.currentPage = pageKey;
                
                // Gérer l'historique
                if (!skipHistory) {
                    this.addToHistory(pageKey);
                }
                
                // Mettre à jour l'URL du navigateur
                if (!skipPushState) {
                    history.pushState({ page: pageKey }, '', `#${pageKey}`);
                }
                
                // Mettre à jour l'interface de navigation
                this.updateNavigationUI();
                
                // Émettre l'événement de fin de navigation
                this.eventBus.emit('navigation:page_changed', {
                    from: this.previousPage,
                    to: pageKey,
                    page: pageKey
                });
                
                this.logDebug('navigation', `Navigation réussie: ${this.previousPage} → ${pageKey}`);
                return true;
            }
            
        } catch (error) {
            this.logDebug('navigation', `Erreur navigation: ${error.message}`);
            console.error('Navigation error:', error);
            this.showNotification('Erreur de navigation', 'error');
            return false;
            
        } finally {
            this.transitionState.inProgress = false;
        }
        
        return false;
    }

    /**
     * Effectue la transition entre les pages
     * @param {string} pageKey - Page de destination
     * @param {string} animationDirection - Direction de l'animation
     * @returns {Promise<boolean>} - Succès de la transition
     */
    async performPageTransition(pageKey, animationDirection) {
        const pageConfig = this.pages[pageKey];
        const currentPageElement = document.getElementById(this.pages[this.currentPage].id);
        const targetPageElement = document.getElementById(pageConfig.id);
        
        if (!currentPageElement || !targetPageElement) {
            this.logDebug('navigation', `Éléments de page manquants: ${pageKey}`);
            console.error('Missing page elements:', { 
                currentId: this.pages[this.currentPage].id,
                targetId: pageConfig.id,
                currentExists: !!currentPageElement,
                targetExists: !!targetPageElement
            });
            return false;
        }
        
        try {
            // Obtenir le contenu de la page
            const pageContent = await this.getPageContent(pageKey);
            
            // Animation de sortie de la page actuelle
            if (this.animationConfig.enableTransitions) {
                await this.animatePageOut(currentPageElement, animationDirection);
            } else {
                currentPageElement.classList.remove('active');
                currentPageElement.style.display = 'none';
            }
            
            // Mettre à jour le contenu de la page cible
            targetPageElement.innerHTML = pageContent;
            
            // Animation d'entrée de la nouvelle page
            if (this.animationConfig.enableTransitions) {
                await this.animatePageIn(targetPageElement, animationDirection);
            } else {
                targetPageElement.style.display = 'block';
                targetPageElement.classList.add('active');
            }
            
            return true;
            
        } catch (error) {
            this.logDebug('navigation', `Erreur transition: ${error.message}`);
            console.error('Transition error:', error);
            return false;
        }
    }

    /**
     * Anime la sortie d'une page
     * @param {HTMLElement} pageElement - Élément de la page
     * @param {string} direction - Direction de l'animation
     * @returns {Promise} - Promise de fin d'animation
     */
    animatePageOut(pageElement, direction) {
        return new Promise((resolve) => {
            const animation = pageElement.animate([
                { opacity: 1, transform: 'translateX(0%)' },
                { opacity: 0, transform: 'translateX(-20%)' }
            ], {
                duration: this.transitionState.duration,
                easing: this.transitionState.easing,
                fill: 'forwards'
            });
            
            animation.onfinish = () => {
                pageElement.classList.remove('active');
                pageElement.style.display = 'none';
                resolve();
            };
        });
    }

    /**
     * Anime l'entrée d'une page
     * @param {HTMLElement} pageElement - Élément de la page
     * @param {string} direction - Direction de l'animation
     * @returns {Promise} - Promise de fin d'animation
     */
    animatePageIn(pageElement, direction) {
        return new Promise((resolve) => {
            // Préparer l'élément
            pageElement.style.display = 'block';
            pageElement.style.opacity = '0';
            pageElement.style.transform = 'translateX(20%)';
            pageElement.classList.add('active');
            
            // Animation d'entrée
            const animation = pageElement.animate([
                { opacity: 0, transform: 'translateX(20%)' },
                { opacity: 1, transform: 'translateX(0%)' }
            ], {
                duration: this.transitionState.duration,
                easing: this.transitionState.easing,
                fill: 'forwards'
            });
            
            animation.onfinish = () => {
                pageElement.style.opacity = '';
                pageElement.style.transform = '';
                resolve();
            };
        });
    }

    // ===== GESTION DU CONTENU =====

    /**
     * Obtient le contenu d'une page
     * @param {string} pageKey - Clé de la page
     * @returns {Promise<string>} - Contenu HTML de la page
     */
    async getPageContent(pageKey) {
        const pageConfig = this.pages[pageKey];
        
        // Vérifier le cache
        if (pageConfig.cacheable && this.pageCache.has(pageKey)) {
            const cachedContent = this.pageCache.get(pageKey);
            this.logDebug('navigation', `Contenu récupéré du cache: ${pageKey}`);
            return cachedContent;
        }
        
        // Générer le contenu
        const content = await this.generatePageContent(pageKey);
        
        // Mettre en cache si nécessaire
        if (pageConfig.cacheable) {
            this.cachePageContent(pageKey, content);
        }
        
        return content;
    }

    /**
     * Génère le contenu d'une page
     * @param {string} pageKey - Clé de la page
     * @returns {Promise<string>} - Contenu HTML généré
     */
    async generatePageContent(pageKey) {
        const pageConfig = this.pages[pageKey];
        const view = this.getView(pageConfig.viewKey);
        
        if (!view) {
            this.logDebug('navigation', `Vue manquante pour: ${pageKey} (viewKey: ${pageConfig.viewKey})`);
            console.warn(`View not found for page ${pageKey}, viewKey: ${pageConfig.viewKey}`);
            return this.getErrorPageContent(pageKey);
        }
        
        try {
            // Obtenir les données nécessaires
            const data = this.getPageData(pageKey);
            
            // Générer le contenu via la vue
            const content = view.buildTemplate(data);
            
            this.logDebug('navigation', `Contenu généré pour: ${pageKey}`);
            return content;
            
        } catch (error) {
            this.logDebug('navigation', `Erreur génération contenu ${pageKey}: ${error.message}`);
            console.error(`Error generating content for ${pageKey}:`, error);
            return this.getErrorPageContent(pageKey, error);
        }
    }

    /**
     * Obtient les données nécessaires pour une page
     * @param {string} pageKey - Clé de la page
     * @returns {Object} - Données pour la page
     */
    getPageData(pageKey) {
        const stateModel = this.getModel('state');
        const fileModel = this.getModel('file');
        const instrumentModel = this.getModel('instrument');
        const playlistModel = this.getModel('playlist');
        const editorModel = this.getModel('editor');
        const routingModel = this.getModel('routing');
        const systemModel = this.getModel('system');
        
        // Données communes à toutes les pages
        const commonData = {
            currentPage: pageKey,
            currentFile: stateModel?.get('currentFile'),
            currentPlaylist: stateModel?.get('currentPlaylist'),
            selectorMode: stateModel?.get('selectorMode') || 'file',
            isPlaying: stateModel?.get('isPlaying') || false,
            settings: stateModel?.get('settings') || {}
        };
        
        // Données spécifiques par page
        switch (pageKey) {
            case 'home':
                return {
                    ...commonData,
                    recentFiles: fileModel?.getRecentFiles() || [],
                    connectedInstruments: instrumentModel?.getConnectedInstruments() || [],
                    playlists: playlistModel?.get('playlists') || [],
                    systemStats: this.getSystemStats()
                };
                
            case 'editor':
                return {
                    ...commonData,
                    currentFile: stateModel?.get('currentFile'),
                    tracks: editorModel?.get('tracks') || [],
                    selectedNotes: editorModel?.get('selectedNotes') || [],
                    zoom: editorModel?.get('zoom') || 1.0,
                    mode: editorModel?.get('mode') || 'select'
                };
                
            case 'routing':
                return {
                    ...commonData,
                    routingMatrix: routingModel?.get('matrix') || [],
                    inputDevices: routingModel?.get('inputDevices') || [],
                    outputDevices: routingModel?.get('outputDevices') || []
                };
                
            case 'instruments':
                return {
                    ...commonData,
                    instruments: instrumentModel?.get('instruments') || [],
                    connectedCount: instrumentModel?.getConnectedInstruments()?.length || 0,
                    discoveryInProgress: instrumentModel?.get('discoveryInProgress') || false
                };
                
            case 'keyboard':
                return {
                    ...commonData,
                    connectedInstruments: instrumentModel?.getConnectedInstruments() || [],
                    selectedInstrument: stateModel?.get('selectedKeyboardInstrument'),
                    velocity: stateModel?.get('keyboardVelocity') || 64,
                    keyboardView: stateModel?.get('keyboardView') || { start: 48, end: 84 },
                    speakerMode: stateModel?.get('speakerMode') || false
                };
                
            case 'system':
                return {
                    ...commonData,
                    systemConfig: systemModel?.get('config') || stateModel?.get('systemConfig'),
                    systemHealth: this.getSystemHealth(),
                    connectedInstruments: instrumentModel?.getConnectedInstruments() || [],
                    systemStats: this.getSystemStats()
                };
                
            default:
                return commonData;
        }
    }

    /**
     * Génère le contenu d'erreur pour une page
     * @param {string} pageKey - Clé de la page
     * @param {Error} error - Erreur optionnelle
     * @returns {string} - Contenu HTML d'erreur
     */
    getErrorPageContent(pageKey, error = null) {
        const pageConfig = this.pages[pageKey];
        return `
            <div class="error-page" style="text-align: center; padding: 60px 20px; color: #6c757d;">
                <div style="font-size: 4rem; margin-bottom: 20px;">⚠️</div>
                <h2 style="margin-bottom: 16px; color: #dc3545;">Erreur de chargement</h2>
                <p style="margin-bottom: 20px;">
                    La page "${pageConfig?.title || pageKey}" n'a pas pu être chargée.
                </p>
                ${error ? `
                    <details style="margin: 20px 0; text-align: left; max-width: 600px; margin-left: auto; margin-right: auto;">
                        <summary style="cursor: pointer; color: #007bff;">Détails de l'erreur</summary>
                        <pre style="background: #f8f9fa; padding: 16px; border-radius: 8px; margin-top: 10px; text-align: left; overflow-x: auto;">
${error.message}
${error.stack ? '\n' + error.stack : ''}
                        </pre>
                    </details>
                ` : ''}
                <button class="btn btn-primary" onclick="app.navigationController.refreshCurrentPage()">
                    🔄 Réessayer
                </button>
                <button class="btn btn-secondary" onclick="app.navigationController.showPage('home')" style="margin-left: 10px;">
                    🏠 Retour à l'accueil
                </button>
            </div>
        `;
    }

    // ===== GESTION DU CACHE =====

    /**
     * Met en cache le contenu d'une page
     * @param {string} pageKey - Clé de la page
     * @param {string} content - Contenu à mettre en cache
     */
    cachePageContent(pageKey, content) {
        this.pageCache.set(pageKey, content);
        
        // Programmer l'expiration du cache
        if (this.cacheTimeouts.has(pageKey)) {
            clearTimeout(this.cacheTimeouts.get(pageKey));
        }
        
        const timeout = setTimeout(() => {
            this.pageCache.delete(pageKey);
            this.cacheTimeouts.delete(pageKey);
            this.logDebug('navigation', `Cache expiré: ${pageKey}`);
        }, this.defaultCacheDuration);
        
        this.cacheTimeouts.set(pageKey, timeout);
        
        this.logDebug('navigation', `Contenu mis en cache: ${pageKey}`);
    }

    /**
     * Invalide le cache de certaines pages
     * @param {Array<string>} pageKeys - Pages à invalider
     */
    invalidatePageCache(pageKeys = []) {
        pageKeys.forEach(pageKey => {
            if (this.pageCache.has(pageKey)) {
                this.pageCache.delete(pageKey);
                
                if (this.cacheTimeouts.has(pageKey)) {
                    clearTimeout(this.cacheTimeouts.get(pageKey));
                    this.cacheTimeouts.delete(pageKey);
                }
                
                this.logDebug('navigation', `Cache invalidé: ${pageKey}`);
            }
        });
    }

    /**
     * Vide tout le cache
     */
    clearAllCache() {
        this.pageCache.clear();
        this.cacheTimeouts.forEach(timeout => clearTimeout(timeout));
        this.cacheTimeouts.clear();
        this.logDebug('navigation', 'Tout le cache a été vidé');
    }

    // ===== HISTORIQUE DE NAVIGATION =====

    /**
     * Ajoute une page à l'historique
     * @param {string} pageKey - Page à ajouter
     */
    addToHistory(pageKey) {
        // Supprimer les entrées futures si on navigue depuis le milieu de l'historique
        this.navigationHistory = this.navigationHistory.slice(0, this.historyIndex + 1);
        
        // Ajouter la nouvelle page
        this.navigationHistory.push(pageKey);
        this.historyIndex = this.navigationHistory.length - 1;
        
        // Limiter la taille de l'historique
        const maxHistorySize = 50;
        if (this.navigationHistory.length > maxHistorySize) {
            this.navigationHistory = this.navigationHistory.slice(-maxHistorySize);
            this.historyIndex = this.navigationHistory.length - 1;
        }
    }

    /**
     * Navigue vers la page précédente
     */
    goBack() {
        if (this.historyIndex > 0) {
            this.historyIndex--;
            const pageKey = this.navigationHistory[this.historyIndex];
            this.showPage(pageKey, { skipHistory: true, animationDirection: 'back' });
            this.logDebug('navigation', `Navigation arrière vers: ${pageKey}`);
        } else {
            this.showNotification('Aucune page précédente', 'info');
        }
    }

    /**
     * Navigue vers la page suivante
     */
    goForward() {
        if (this.historyIndex < this.navigationHistory.length - 1) {
            this.historyIndex++;
            const pageKey = this.navigationHistory[this.historyIndex];
            this.showPage(pageKey, { skipHistory: true, animationDirection: 'forward' });
            this.logDebug('navigation', `Navigation avant vers: ${pageKey}`);
        } else {
            this.showNotification('Aucune page suivante', 'info');
        }
    }

    // ===== MÉTHODES PUBLIQUES =====

    /**
     * Rafraîchit la page actuelle
     */
    refreshCurrentPage() {
        this.invalidatePageCache([this.currentPage]);
        this.showPage(this.currentPage, { forceRefresh: true });
        this.logDebug('navigation', `Page rafraîchie: ${this.currentPage}`);
    }

    /**
     * Rafraîchit une page spécifique
     * @param {string} pageKey - Page à rafraîchir
     */
    refreshPageView(pageKey) {
        this.invalidatePageCache([pageKey]);
        
        // Si c'est la page actuelle, la rafraîchir
        if (pageKey === this.currentPage) {
            this.refreshCurrentPage();
        }
    }

    /**
     * Obtient la page actuelle
     * @returns {string} - Clé de la page actuelle
     */
    getCurrentPage() {
        return this.currentPage;
    }

    /**
     * Vérifie si une page peut être mise en cache
     * @param {string} pageKey - Clé de la page
     * @returns {boolean} - Page cacheable ou non
     */
    isPageCacheable(pageKey) {
        return this.pages[pageKey]?.cacheable || false;
    }

    /**
     * Met à jour l'interface de navigation
     */
    updateNavigationUI() {
        // Mettre à jour les liens de navigation actifs
        document.querySelectorAll('.nav-item').forEach(link => {
            link.classList.remove('active');
        });
        
        // Trouver et activer le lien correspondant
        const activeLink = document.querySelector(`.nav-item[data-page="${this.currentPage}"]`);
        if (activeLink) {
            activeLink.classList.add('active');
        }
        
        // Mettre à jour le titre de la page
        const pageConfig = this.pages[this.currentPage];
        if (pageConfig) {
            document.title = `${pageConfig.title} - MIDI Mind`;
        }
    }

    /**
     * Gère les changements de modèle pour mise à jour automatique
     * @param {Object} data - Données du changement
     */
    handleModelChange(data) {
        // Invalider le cache des pages concernées selon le modèle modifié
        const cacheInvalidationMap = {
            'FileModel': ['home', 'editor'],
            'InstrumentModel': ['home', 'instruments', 'keyboard'],
            'PlaylistModel': ['home', 'editor'],
            'EditorModel': ['editor'],
            'RoutingModel': ['routing'],
            'SystemModel': ['system'],
            'StateModel': [this.currentPage] // Toujours rafraîchir la page actuelle
        };
        
        const pagesToInvalidate = cacheInvalidationMap[data.model] || [];
        if (pagesToInvalidate.length > 0) {
            this.invalidatePageCache(pagesToInvalidate);
            
            // Rafraîchir la page actuelle si elle est concernée
            if (pagesToInvalidate.includes(this.currentPage)) {
                // Délai court pour éviter les rafraîchissements trop fréquents
                clearTimeout(this.refreshTimeout);
                this.refreshTimeout = setTimeout(() => {
                    this.refreshCurrentPage();
                }, 100);
            }
        }
    }

    // ===== UTILITAIRES =====

    /**
     * Obtient les statistiques système
     * @returns {Object} - Statistiques système
     */
    getSystemStats() {
        return {
            uptime: Math.floor((performance.now() - (window.app?.startTime || 0)) / 1000),
            memoryUsage: performance.memory ? Math.round(performance.memory.usedJSHeapSize / 1024 / 1024) : 0,
            pagesLoaded: this.navigationHistory.length,
            cacheSize: this.pageCache.size
        };
    }

    /**
     * Obtient l'état de santé du système
     * @returns {string} - État de santé (good/warning/error)
     */
    getSystemHealth() {
        const fileModel = this.getModel('file');
        const instrumentModel = this.getModel('instrument');
        
        const hasFiles = (fileModel?.get('files') || []).length > 0;
        const hasInstruments = (instrumentModel?.get('instruments') || []).length > 0;
        
        if (hasFiles && hasInstruments) return 'good';
        if (hasFiles || hasInstruments) return 'warning';
        return 'error';
    }

    /**
     * Nettoie les ressources du contrôleur
     */
    destroy() {
        this.clearAllCache();
        
        if (this.refreshTimeout) {
            clearTimeout(this.refreshTimeout);
        }
        
        // Nettoyer les événements
        document.removeEventListener('keydown', this.keyboardHandler);
        window.removeEventListener('popstate', this.popstateHandler);
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = NavigationController;
}

if (typeof window !== 'undefined') {
    window.NavigationController = NavigationController;
}