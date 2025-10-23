// ===== NAVIGATION CONTROLLER - ContrÃ´leur de navigation et gestion des pages =====
// ================================================================================
// GÃ¨re toute la navigation de l'application :
// - Affichage et transition entre les pages (home, files, instruments, keyboard, system)
// - Gestion de l'historique et navigation (back/forward)
// - Mise Ã  jour dynamique du contenu des pages
// - Coordination entre les vues et contrÃ´leurs
// - Gestion des Ã©tats de navigation et URL
// - Animation des transitions entre pages
// - Raccourcis clavier pour la navigation
// ================================================================================


class NavigationController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Ã‰tat de navigation
        this.currentPage = 'home';
        this.previousPage = null;
        this.navigationHistory = ['home'];
        this.historyIndex = 0;
        
        // Configuration des pages
        this.pages = {
            home: {
                id: 'home-page',
                title: 'ðŸ  Accueil',
                icon: 'ðŸ ',
                shortcut: 'h',
                requiresData: true,
                cacheable: false
            },
            files: {
                id: 'files-page', 
                title: 'ðŸ“ Fichiers',
                icon: 'ðŸ“',
                shortcut: 'f',
                requiresData: true,
                cacheable: true
            },
            instruments: {
                id: 'instruments-page',
                title: 'ðŸŽ¼ Instruments', 
                icon: 'ðŸŽ¼',
                shortcut: 'i',
                requiresData: true,
                cacheable: true
            },
            keyboard: {
                id: 'keyboard-page',
                title: 'ðŸŽ¹ Clavier',
                icon: 'ðŸŽ¹', 
                shortcut: 'k',
                requiresData: true,
                cacheable: false
            },
            system: {
                id: 'system-page',
                title: 'âš™ï¸ SystÃ¨me',
                icon: 'âš™ï¸',
                shortcut: 's',
                requiresData: true,
                cacheable: true
            }
        };
        
        // Ã‰tat des transitions
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
            slideDirection: 'horizontal', // horizontal, vertical, fade
            parallax: false,
            preloadNext: true
        };
        
        this.initializeNavigation();
    }

    /**
     * Configuration des Ã©vÃ©nements
     */
    bindEvents() {
        // Ã‰couter les changements de modÃ¨les pour mise Ã  jour automatique
        this.eventBus.on('model:changed', (data) => {
            this.handleModelChange(data);
        });
        
        // Ã‰couter les Ã©vÃ©nements de navigation
        this.eventBus.on('navigation:page_request', (data) => {
            this.showPage(data.page, data.options);
        });
        
        this.eventBus.on('navigation:back', () => {
            this.goBack();
        });
        
        this.eventBus.on('navigation:forward', () => {
            this.goForward();
        });
        
        // Ã‰couter les Ã©vÃ©nements de donnÃ©es
        this.eventBus.on('file:added', () => {
            this.invalidatePageCache(['home', 'files']);
        });
        
        this.eventBus.on('instrument:updated', () => {
            this.invalidatePageCache(['home', 'instruments', 'keyboard']);
        });
        
        this.eventBus.on('playlist:updated', () => {
            this.invalidatePageCache(['home', 'files']);
        });
    }

    /**
     * Initialise le systÃ¨me de navigation
     */
    initializeNavigation() {
        // Configurer les raccourcis clavier
        this.setupKeyboardShortcuts();
        
        // GÃ©rer l'historique du navigateur
        this.setupBrowserHistory();
        
        // Afficher la page d'accueil
        this.showPage('home', { skipHistory: true });
        
        this.logDebug('navigation', 'SystÃ¨me de navigation initialisÃ©');
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
        // GÃ©rer les changements d'URL
        window.addEventListener('popstate', (event) => {
            if (event.state && event.state.page) {
                this.showPage(event.state.page, { skipHistory: true, skipPushState: true });
            }
        });
        
        // DÃ©finir l'Ã©tat initial
        history.replaceState({ page: this.currentPage }, '', `#${this.currentPage}`);
    }

    // ===== NAVIGATION PRINCIPALE =====

    /**
     * Affiche une page spÃ©cifique
     * @param {string} pageKey - ClÃ© de la page Ã  afficher
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
        
        // Ã‰viter les transitions inutiles
        if (pageKey === this.currentPage && !forceRefresh) {
            this.logDebug('navigation', `Page dÃ©jÃ  active: ${pageKey}`);
            return true;
        }
        
        // VÃ©rifier si une transition est en cours
        if (this.transitionState.inProgress) {
            this.logDebug('navigation', 'Transition dÃ©jÃ  en cours, ignorÃ©e');
            return false;
        }
        
        this.logDebug('navigation', `Navigation vers: ${pageKey}`);
        
        try {
            // PrÃ©parer la transition
            this.transitionState.inProgress = true;
            this.previousPage = this.currentPage;
            
            // Ã‰mettre l'Ã©vÃ©nement de dÃ©but de navigation
            this.eventBus.emit('navigation:page_changing', {
                from: this.currentPage,
                to: pageKey
            });
            
            // Effectuer la transition
            const success = await this.performPageTransition(pageKey, animationDirection);
            
            if (success) {
                // Mettre Ã  jour l'Ã©tat de navigation
                this.currentPage = pageKey;
                
                // GÃ©rer l'historique
                if (!skipHistory) {
                    this.addToHistory(pageKey);
                }
                
                // Mettre Ã  jour l'URL du navigateur
                if (!skipPushState) {
                    history.pushState({ page: pageKey }, '', `#${pageKey}`);
                }
                
                // Mettre Ã  jour l'interface de navigation
                this.updateNavigationUI();
                
                // Ã‰mettre l'Ã©vÃ©nement de fin de navigation
                this.eventBus.emit('navigation:page_changed', {
                    from: this.previousPage,
                    to: pageKey,
                    page: pageKey
                });
                
                this.logDebug('navigation', `Navigation rÃ©ussie: ${this.previousPage} â†’ ${pageKey}`);
                return true;
            }
            
        } catch (error) {
            this.logDebug('navigation', `Erreur navigation: ${error.message}`);
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
     * @returns {Promise<boolean>} - SuccÃ¨s de la transition
     */
    async performPageTransition(pageKey, animationDirection) {
        const pageConfig = this.pages[pageKey];
        const currentPageElement = document.getElementById(this.pages[this.currentPage].id);
        const targetPageElement = document.getElementById(pageConfig.id);
        
        if (!currentPageElement || !targetPageElement) {
            this.logDebug('navigation', 'Ã‰lÃ©ments de page manquants');
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
            }
            
            // Mettre Ã  jour le contenu de la page cible
            targetPageElement.innerHTML = pageContent;
            
            // Animation d'entrÃ©e de la nouvelle page
            if (this.animationConfig.enableTransitions) {
                await this.animatePageIn(targetPageElement, animationDirection);
            } else {
                targetPageElement.classList.add('active');
            }
            
            return true;
            
        } catch (error) {
            this.logDebug('navigation', `Erreur transition: ${error.message}`);
            return false;
        }
    }

    /**
     * Anime la sortie d'une page
     * @param {HTMLElement} pageElement - Ã‰lÃ©ment de la page
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
                resolve();
            };
        });
    }

    /**
     * Anime l'entrÃ©e d'une page
     * @param {HTMLElement} pageElement - Ã‰lÃ©ment de la page
     * @param {string} direction - Direction de l'animation
     * @returns {Promise} - Promise de fin d'animation
     */
    animatePageIn(pageElement, direction) {
        return new Promise((resolve) => {
            // PrÃ©parer l'Ã©lÃ©ment
            pageElement.style.opacity = '0';
            pageElement.style.transform = 'translateX(20%)';
            pageElement.classList.add('active');
            
            // Animation d'entrÃ©e
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
     * @param {string} pageKey - ClÃ© de la page
     * @returns {Promise<string>} - Contenu HTML de la page
     */
    async getPageContent(pageKey) {
        const pageConfig = this.pages[pageKey];
        
        // VÃ©rifier le cache
        if (pageConfig.cacheable && this.pageCache.has(pageKey)) {
            const cachedContent = this.pageCache.get(pageKey);
            this.logDebug('navigation', `Contenu rÃ©cupÃ©rÃ© du cache: ${pageKey}`);
            return cachedContent;
        }
        
        // GÃ©nÃ©rer le contenu
        const content = await this.generatePageContent(pageKey);
        
        // Mettre en cache si nÃ©cessaire
        if (pageConfig.cacheable) {
            this.cachePageContent(pageKey, content);
        }
        
        return content;
    }

    /**
     * GÃ©nÃ¨re le contenu d'une page
     * @param {string} pageKey - ClÃ© de la page
     * @returns {Promise<string>} - Contenu HTML gÃ©nÃ©rÃ©
     */
    async generatePageContent(pageKey) {
        const view = this.getView(pageKey);
        
        if (!view) {
            this.logDebug('navigation', `Vue manquante pour: ${pageKey}`);
            return this.getErrorPageContent(pageKey);
        }
        
        try {
            // Obtenir les donnÃ©es nÃ©cessaires
            const data = this.getPageData(pageKey);
            
            // GÃ©nÃ©rer le contenu via la vue
            const content = view.buildTemplate(data);
            
            this.logDebug('navigation', `Contenu gÃ©nÃ©rÃ© pour: ${pageKey}`);
            return content;
            
        } catch (error) {
            this.logDebug('navigation', `Erreur gÃ©nÃ©ration contenu ${pageKey}: ${error.message}`);
            return this.getErrorPageContent(pageKey, error);
        }
    }

    /**
     * Obtient les donnÃ©es nÃ©cessaires pour une page
     * @param {string} pageKey - ClÃ© de la page
     * @returns {Object} - DonnÃ©es pour la page
     */
    getPageData(pageKey) {
        const stateModel = this.getModel('state');
        const fileModel = this.getModel('file');
        const instrumentModel = this.getModel('instrument');
        const playlistModel = this.getModel('playlist');
        
        // DonnÃ©es communes Ã  toutes les pages
        const commonData = {
            currentPage: pageKey,
            currentFile: stateModel.get('currentFile'),
            currentPlaylist: stateModel.get('currentPlaylist'),
            selectorMode: stateModel.get('selectorMode') || 'file',
            isPlaying: stateModel.get('isPlaying') || false,
            settings: stateModel.get('settings') || {}
        };
        
        // DonnÃ©es spÃ©cifiques par page
        switch (pageKey) {
            case 'home':
                return {
                    ...commonData,
                    recentFiles: fileModel.getRecentFiles(),
                    connectedInstruments: instrumentModel.getConnectedInstruments(),
                    playlists: playlistModel.get('playlists') || [],
                    systemStats: this.getSystemStats()
                };
                
            case 'files':
                return {
                    ...commonData,
                    files: fileModel.getFilesInCurrentPath(),
                    currentPath: fileModel.get('currentPath'),
                    selectedFiles: fileModel.get('selectedFiles') || [],
                    selectedFolders: fileModel.get('selectedFolders') || [],
                    playlists: playlistModel.get('playlists') || []
                };
                
            case 'instruments':
                return {
                    ...commonData,
                    instruments: instrumentModel.get('instruments') || [],
                    connectedCount: instrumentModel.getConnectedInstruments().length,
                    discoveryInProgress: instrumentModel.get('discoveryInProgress') || false
                };
                
            case 'keyboard':
                return {
                    ...commonData,
                    connectedInstruments: instrumentModel.getConnectedInstruments(),
                    selectedInstrument: stateModel.get('selectedKeyboardInstrument'),
                    velocity: stateModel.get('keyboardVelocity') || 64,
                    keyboardView: stateModel.get('keyboardView') || { start: 48, end: 84 },
                    speakerMode: stateModel.get('speakerMode') || false
                };
                
            case 'system':
                return {
                    ...commonData,
                    systemConfig: stateModel.get('systemConfig'),
                    systemHealth: this.getSystemHealth(),
                    connectedInstruments: instrumentModel.getConnectedInstruments(),
                    systemStats: this.getSystemStats()
                };
                
            default:
                return commonData;
        }
    }

    /**
     * GÃ©nÃ¨re le contenu d'erreur pour une page
     * @param {string} pageKey - ClÃ© de la page
     * @param {Error} error - Erreur optionnelle
     * @returns {string} - Contenu HTML d'erreur
     */
    getErrorPageContent(pageKey, error = null) {
        const pageConfig = this.pages[pageKey];
        return `
            <div class="error-page" style="text-align: center; padding: 60px 20px; color: #6c757d;">
                <div style="font-size: 4rem; margin-bottom: 20px;">âš ï¸</div>
                <h2 style="margin-bottom: 16px; color: #dc3545;">Erreur de chargement</h2>
                <p style="margin-bottom: 20px;">
                    La page "${pageConfig?.title || pageKey}" n'a pas pu Ãªtre chargÃ©e.
                </p>
                ${error ? `
                    <details style="margin: 20px 0; text-align: left; max-width: 600px; margin-left: auto; margin-right: auto;">
                        <summary style="cursor: pointer; color: #007bff;">DÃ©tails de l'erreur</summary>
                        <pre style="background: #f8f9fa; padding: 16px; border-radius: 8px; margin-top: 10px; text-align: left; overflow-x: auto;">
${error.message}
${error.stack ? '\n' + error.stack : ''}
                        </pre>
                    </details>
                ` : ''}
                <button class="btn btn-primary" onclick="app.navigationController.refreshCurrentPage()">
                    ðŸ”„ RÃ©essayer
                </button>
                <button class="btn btn-secondary" onclick="app.navigationController.showPage('home')" style="margin-left: 10px;">
                    ðŸ  Retour Ã  l'accueil
                </button>
            </div>
        `;
    }

    // ===== GESTION DU CACHE =====

    /**
     * Met en cache le contenu d'une page
     * @param {string} pageKey - ClÃ© de la page
     * @param {string} content - Contenu Ã  mettre en cache
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
            this.logDebug('navigation', `Cache expirÃ©: ${pageKey}`);
        }, this.defaultCacheDuration);
        
        this.cacheTimeouts.set(pageKey, timeout);
        
        this.logDebug('navigation', `Contenu mis en cache: ${pageKey}`);
    }

    /**
     * Invalide le cache de certaines pages
     * @param {Array<string>} pageKeys - Pages Ã  invalider
     */
    invalidatePageCache(pageKeys = []) {
        pageKeys.forEach(pageKey => {
            if (this.pageCache.has(pageKey)) {
                this.pageCache.delete(pageKey);
                
                if (this.cacheTimeouts.has(pageKey)) {
                    clearTimeout(this.cacheTimeouts.get(pageKey));
                    this.cacheTimeouts.delete(pageKey);
                }
                
                this.logDebug('navigation', `Cache invalidÃ©: ${pageKey}`);
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
        this.logDebug('navigation', 'Tout le cache a Ã©tÃ© vidÃ©');
    }

    // ===== HISTORIQUE DE NAVIGATION =====

    /**
     * Ajoute une page Ã  l'historique
     * @param {string} pageKey - Page Ã  ajouter
     */
    addToHistory(pageKey) {
        // Supprimer les entrÃ©es futures si on navigue depuis le milieu de l'historique
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
     * Navigue vers la page prÃ©cÃ©dente
     */
    goBack() {
        if (this.historyIndex > 0) {
            this.historyIndex--;
            const pageKey = this.navigationHistory[this.historyIndex];
            this.showPage(pageKey, { skipHistory: true, animationDirection: 'back' });
            this.logDebug('navigation', `Navigation arriÃ¨re vers: ${pageKey}`);
        } else {
            this.showNotification('Aucune page prÃ©cÃ©dente', 'info');
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

    // ===== MÃ‰THODES PUBLIQUES =====

    /**
     * RafraÃ®chit la page actuelle
     */
    refreshCurrentPage() {
        this.invalidatePageCache([this.currentPage]);
        this.showPage(this.currentPage, { forceRefresh: true });
        this.logDebug('navigation', `Page rafraÃ®chie: ${this.currentPage}`);
    }

    /**
     * RafraÃ®chit une page spÃ©cifique
     * @param {string} pageKey - Page Ã  rafraÃ®chir
     */
    refreshPageView(pageKey) {
        this.invalidatePageCache([pageKey]);
        
        // Si c'est la page actuelle, la rafraÃ®chir
        if (pageKey === this.currentPage) {
            this.refreshCurrentPage();
        }
    }

    /**
     * Obtient la page actuelle
     * @returns {string} - ClÃ© de la page actuelle
     */
    getCurrentPage() {
        return this.currentPage;
    }

    /**
     * VÃ©rifie si une page peut Ãªtre mise en cache
     * @param {string} pageKey - ClÃ© de la page
     * @returns {boolean} - Page cacheable ou non
     */
    isPageCacheable(pageKey) {
        return this.pages[pageKey]?.cacheable || false;
    }

    /**
     * Met Ã  jour l'interface de navigation
     */
    updateNavigationUI() {
        // Mettre Ã  jour les liens de navigation actifs
        document.querySelectorAll('.nav-link').forEach(link => {
            link.classList.remove('active');
        });
        
        // Trouver et activer le lien correspondant
        const activeLink = document.querySelector(`.nav-link[onclick*="${this.currentPage}"]`);
        if (activeLink) {
            activeLink.classList.add('active');
        }
        
        // Mettre Ã  jour le titre de la page
        const pageConfig = this.pages[this.currentPage];
        if (pageConfig) {
            document.title = `${pageConfig.title} - MIDI Orchestrion`;
        }
    }

    /**
     * GÃ¨re les changements de modÃ¨le pour mise Ã  jour automatique
     * @param {Object} data - DonnÃ©es du changement
     */
    handleModelChange(data) {
        // Invalider le cache des pages concernÃ©es selon le modÃ¨le modifiÃ©
        const cacheInvalidationMap = {
            'FileModel': ['home', 'files'],
            'InstrumentModel': ['home', 'instruments', 'keyboard'],
            'PlaylistModel': ['home', 'files'],
            'StateModel': [this.currentPage] // Toujours rafraÃ®chir la page actuelle
        };
        
        const pagesToInvalidate = cacheInvalidationMap[data.model] || [];
        if (pagesToInvalidate.length > 0) {
            this.invalidatePageCache(pagesToInvalidate);
            
            // RafraÃ®chir la page actuelle si elle est concernÃ©e
            if (pagesToInvalidate.includes(this.currentPage)) {
                // DÃ©lai court pour Ã©viter les rafraÃ®chissements trop frÃ©quents
                clearTimeout(this.refreshTimeout);
                this.refreshTimeout = setTimeout(() => {
                    this.refreshCurrentPage();
                }, 100);
            }
        }
    }

    // ===== UTILITAIRES =====

    /**
     * Obtient les statistiques systÃ¨me
     * @returns {Object} - Statistiques systÃ¨me
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
     * Obtient l'Ã©tat de santÃ© du systÃ¨me
     * @returns {string} - Ã‰tat de santÃ© (good/warning/error)
     */
    getSystemHealth() {
        const fileModel = this.getModel('file');
        const instrumentModel = this.getModel('instrument');
        
        const hasFiles = (fileModel.get('files') || []).length > 0;
        const hasInstruments = (instrumentModel.get('instruments') || []).length > 0;
        
        if (hasFiles && hasInstruments) return 'good';
        if (hasFiles || hasInstruments) return 'warning';
        return 'error';
    }

    /**
     * Nettoie les ressources du contrÃ´leur
     */
    destroy() {
        this.clearAllCache();
        
        if (this.refreshTimeout) {
            clearTimeout(this.refreshTimeout);
        }
        
        // Nettoyer les Ã©vÃ©nements
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

window.NavigationController = NavigationController;