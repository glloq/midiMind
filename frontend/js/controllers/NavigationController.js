// ===== NAVIGATION CONTROLLER - ContrÃ´leur de navigation et gestion des pages =====
// ================================================================================
// Fichier: frontend/js/controllers/NavigationController.js
// Version: v3.7.1 - CORRECTION CHEMIN CONTROLLEUR
// Date: 2025-10-24
// ================================================================================
// CORRECTIONS v3.7.1:
// âœ… Correction app.navigationController â†’ app.controllers.navigation
// âœ… IDs de pages corrigÃ©s pour correspondre au HTML
// âœ… Ajout gestion display:none/block pour affichage correct des pages
// âœ… Correction getView() pour mapper correctement les vues
// ================================================================================

class NavigationController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Ã‰tat de navigation
        this.currentPage = 'home';
        this.previousPage = null;
        this.navigationHistory = ['home'];
        this.historyIndex = 0;
        
        // Configuration des pages - IDS CORRIGES
        this.pages = {
            home: {
                id: 'home',
                title: 'ðŸ  Accueil',
                icon: 'ðŸ ',
                shortcut: 'h',
                requiresData: true,
                cacheable: false,
                viewKey: 'home'
            },
            editor: {
                id: 'editor',
                title: 'âœï¸ Ã‰diteur',
                icon: 'âœï¸',
                shortcut: 'e',
                requiresData: true,
                cacheable: false,
                viewKey: 'editor'
            },
            routing: {
                id: 'routing',
                title: 'ðŸ”€ Routage',
                icon: 'ðŸ”€',
                shortcut: 'r',
                requiresData: true,
                cacheable: true,
                viewKey: 'routing'
            },
            keyboard: {
                id: 'keyboard',
                title: 'ðŸŽ¹ Clavier',
                icon: 'ðŸŽ¹',
                shortcut: 'k',
                requiresData: true,
                cacheable: false,
                viewKey: 'keyboard'
            },
            instruments: {
                id: 'instruments',
                title: 'ðŸŽ¸ Instruments',
                icon: 'ðŸŽ¸',
                shortcut: 'i',
                requiresData: true,
                cacheable: true,
                viewKey: 'instrument'
            },
            system: {
                id: 'system',
                title: 'âš™ï¸ SystÃ¨me',
                icon: 'âš™ï¸',
                shortcut: 's',
                requiresData: true,
                cacheable: true,
                viewKey: 'system'
            }
        };
        
        // Ã‰tat des transitions
        this.transitionState = {
            inProgress: false,
            duration: 300,
            easing: 'cubic-bezier(0.4, 0, 0.2, 1)'
        };
        
        // Cache des pages
        this.pageCache = new Map();
        this.cacheTimeouts = new Map();
        this.defaultCacheDuration = 30000;
        
        // Configuration des animations
        this.animationConfig = {
            enableTransitions: true,
            slideDirection: 'horizontal',
            parallax: false,
            preloadNext: true
        };
        
        this.initializeNavigation();
    }

    bindEvents() {
        this.eventBus.on('model:changed', (data) => {
            this.handleModelChange(data);
        });
        
        this.eventBus.on('navigation:page_request', (data) => {
            this.showPage(data.page, data.options);
        });
        
        this.eventBus.on('navigation:back', () => {
            this.goBack();
        });
        
        this.eventBus.on('navigation:forward', () => {
            this.goForward();
        });
        
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

    initializeNavigation() {
        this.setupKeyboardShortcuts();
        this.setupBrowserHistory();
        this.setupNavigationLinks();
        this.showPage('home', { skipHistory: true });
        this.logDebug('navigation', 'SystÃ¨me de navigation initialisÃ©');
    }

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

    setupKeyboardShortcuts() {
        document.addEventListener('keydown', (event) => {
            if (event.target.tagName === 'INPUT' || event.target.tagName === 'TEXTAREA') {
                return;
            }
            
            if (event.altKey) {
                Object.entries(this.pages).forEach(([pageKey, pageConfig]) => {
                    if (event.key.toLowerCase() === pageConfig.shortcut) {
                        event.preventDefault();
                        this.showPage(pageKey);
                    }
                });
                
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

    setupBrowserHistory() {
        window.addEventListener('popstate', (event) => {
            if (event.state && event.state.page) {
                this.showPage(event.state.page, { skipHistory: true, skipPushState: true });
            }
        });
        
        history.replaceState({ page: this.currentPage }, '', `#${this.currentPage}`);
    }

    async showPage(pageKey, options = {}) {
        const {
            skipHistory = false,
            skipPushState = false,
            forceRefresh = false,
            animationDirection = null
        } = options;
        
        if (!this.pages[pageKey]) {
            this.logDebug('navigation', `Page invalide: ${pageKey}`);
            this.showNotification('Page introuvable', 'error');
            return false;
        }
        
        if (pageKey === this.currentPage && !forceRefresh) {
            this.logDebug('navigation', `Page dÃ©jÃ  active: ${pageKey}`);
            return true;
        }
        
        if (this.transitionState.inProgress) {
            this.logDebug('navigation', 'Transition dÃ©jÃ  en cours, ignorÃ©e');
            return false;
        }
        
        this.logDebug('navigation', `Navigation vers: ${pageKey}`);
        
        try {
            this.transitionState.inProgress = true;
            this.previousPage = this.currentPage;
            
            this.eventBus.emit('navigation:page_changing', {
                from: this.currentPage,
                to: pageKey
            });
            
            const success = await this.performPageTransition(pageKey, animationDirection);
            
            if (success) {
                this.currentPage = pageKey;
                
                if (!skipHistory) {
                    this.addToHistory(pageKey);
                }
                
                if (!skipPushState) {
                    history.pushState({ page: pageKey }, '', `#${pageKey}`);
                }
                
                this.updateNavigationUI();
                
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
            console.error('Navigation error:', error);
            this.showNotification('Erreur de navigation', 'error');
            return false;
            
        } finally {
            this.transitionState.inProgress = false;
        }
        
        return false;
    }

    async performPageTransition(pageKey, animationDirection) {
        const pageConfig = this.pages[pageKey];
        const currentPageElement = document.getElementById(this.pages[this.currentPage].id);
        const targetPageElement = document.getElementById(pageConfig.id);
        
        if (!currentPageElement || !targetPageElement) {
            this.logDebug('navigation', `Ã‰lÃ©ments de page manquants: ${pageKey}`);
            console.error('Missing page elements:', { 
                currentId: this.pages[this.currentPage].id,
                targetId: pageConfig.id,
                currentExists: !!currentPageElement,
                targetExists: !!targetPageElement
            });
            return false;
        }
        
        try {
            const pageContent = await this.getPageContent(pageKey);
            
            if (this.animationConfig.enableTransitions) {
                await this.animatePageOut(currentPageElement, animationDirection);
            } else {
                currentPageElement.classList.remove('active');
                currentPageElement.style.display = 'none';
            }
            
            targetPageElement.innerHTML = pageContent;
            
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

    animatePageIn(pageElement, direction) {
        return new Promise((resolve) => {
            pageElement.style.display = 'block';
            pageElement.style.opacity = '0';
            pageElement.style.transform = 'translateX(20%)';
            pageElement.classList.add('active');
            
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

    async getPageContent(pageKey) {
        const pageConfig = this.pages[pageKey];
        
        if (pageConfig.cacheable && this.pageCache.has(pageKey)) {
            const cachedContent = this.pageCache.get(pageKey);
            this.logDebug('navigation', `Contenu rÃ©cupÃ©rÃ© du cache: ${pageKey}`);
            return cachedContent;
        }
        
        const content = await this.generatePageContent(pageKey);
        
        if (pageConfig.cacheable) {
            this.cachePageContent(pageKey, content);
        }
        
        return content;
    }

    async generatePageContent(pageKey) {
        const pageConfig = this.pages[pageKey];
        const view = this.getView(pageConfig.viewKey);
        
        if (!view) {
            this.logDebug('navigation', `Vue manquante pour: ${pageKey} (viewKey: ${pageConfig.viewKey})`);
            console.warn(`View not found for page ${pageKey}, viewKey: ${pageConfig.viewKey}`);
            return this.getErrorPageContent(pageKey);
        }
        
        try {
            const data = this.getPageData(pageKey);
            
            // Check if buildTemplate exists (BaseView-inherited views)
            if (typeof view.buildTemplate === 'function') {
                const content = view.buildTemplate(data);
                this.logDebug('navigation', `Content generated: ${pageKey} (buildTemplate)`);
                return content;
            }
            
            // Otherwise check if render() exists
            if (typeof view.render === 'function') {
                view.render();
                const content = view.container ? view.container.innerHTML : '';
                this.logDebug('navigation', `Content generated: ${pageKey} (render)`);
                return content;
            }
            
            // Last option: return current container
            if (view.container) {
                return view.container.innerHTML;
            }
            
            throw new Error(`View ${pageConfig.viewKey} has no buildTemplate, render, or container`);
            
        } catch (error) {
            this.logDebug('navigation', `Error generating content for ${pageKey}: ${error.message}`);
            console.error(`Error generating content for ${pageKey}:`, error);
            return this.getErrorPageContent(pageKey, error);
        }
    }

    getPageData(pageKey) {
        const stateModel = this.getModel('state');
        const fileModel = this.getModel('file');
        const instrumentModel = this.getModel('instrument');
        const playlistModel = this.getModel('playlist');
        const editorModel = this.getModel('editor');
        const routingModel = this.getModel('routing');
        const systemModel = this.getModel('system');
        
        const commonData = {
            currentPage: pageKey,
            currentFile: stateModel?.get('currentFile'),
            currentPlaylist: stateModel?.get('currentPlaylist'),
            selectorMode: stateModel?.get('selectorMode') || 'file',
            isPlaying: stateModel?.get('isPlaying') || false,
            settings: stateModel?.get('settings') || {}
        };
        
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
                <button class="btn btn-primary" onclick="app.controllers.navigation.refreshCurrentPage()">
                    ðŸ”„ RÃ©essayer
                </button>
                <button class="btn btn-secondary" onclick="app.controllers.navigation.showPage('home')" style="margin-left: 10px;">
                    ðŸ  Retour Ã  l'accueil
                </button>
            </div>
        `;
    }

    cachePageContent(pageKey, content) {
        this.pageCache.set(pageKey, content);
        
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

    clearAllCache() {
        this.pageCache.clear();
        this.cacheTimeouts.forEach(timeout => clearTimeout(timeout));
        this.cacheTimeouts.clear();
        this.logDebug('navigation', 'Tout le cache a Ã©tÃ© vidÃ©');
    }

    addToHistory(pageKey) {
        this.navigationHistory = this.navigationHistory.slice(0, this.historyIndex + 1);
        this.navigationHistory.push(pageKey);
        this.historyIndex = this.navigationHistory.length - 1;
        
        const maxHistorySize = 50;
        if (this.navigationHistory.length > maxHistorySize) {
            this.navigationHistory = this.navigationHistory.slice(-maxHistorySize);
            this.historyIndex = this.navigationHistory.length - 1;
        }
    }

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

    refreshCurrentPage() {
        this.invalidatePageCache([this.currentPage]);
        this.showPage(this.currentPage, { forceRefresh: true });
        this.logDebug('navigation', `Page rafraÃ®chie: ${this.currentPage}`);
    }

    refreshPageView(pageKey) {
        this.invalidatePageCache([pageKey]);
        
        if (pageKey === this.currentPage) {
            this.refreshCurrentPage();
        }
    }

    getCurrentPage() {
        return this.currentPage;
    }

    isPageCacheable(pageKey) {
        return this.pages[pageKey]?.cacheable || false;
    }

    updateNavigationUI() {
        document.querySelectorAll('.nav-item').forEach(link => {
            link.classList.remove('active');
        });
        
        const activeLink = document.querySelector(`.nav-item[data-page="${this.currentPage}"]`);
        if (activeLink) {
            activeLink.classList.add('active');
        }
        
        const pageConfig = this.pages[this.currentPage];
        if (pageConfig) {
            document.title = `${pageConfig.title} - MIDI Mind`;
        }
    }

    handleModelChange(data) {
        const cacheInvalidationMap = {
            'FileModel': ['home', 'editor'],
            'InstrumentModel': ['home', 'instruments', 'keyboard'],
            'PlaylistModel': ['home', 'editor'],
            'EditorModel': ['editor'],
            'RoutingModel': ['routing'],
            'SystemModel': ['system'],
            'StateModel': [this.currentPage]
        };
        
        const pagesToInvalidate = cacheInvalidationMap[data.model] || [];
        if (pagesToInvalidate.length > 0) {
            this.invalidatePageCache(pagesToInvalidate);
            
            if (pagesToInvalidate.includes(this.currentPage)) {
                clearTimeout(this.refreshTimeout);
                this.refreshTimeout = setTimeout(() => {
                    this.refreshCurrentPage();
                }, 100);
            }
        }
    }

    getSystemStats() {
        return {
            uptime: Math.floor((performance.now() - (window.app?.startTime || 0)) / 1000),
            memoryUsage: performance.memory ? Math.round(performance.memory.usedJSHeapSize / 1024 / 1024) : 0,
            pagesLoaded: this.navigationHistory.length,
            cacheSize: this.pageCache.size
        };
    }

    getSystemHealth() {
        const fileModel = this.getModel('file');
        const instrumentModel = this.getModel('instrument');
        
        const hasFiles = (fileModel?.get('files') || []).length > 0;
        const hasInstruments = (instrumentModel?.get('instruments') || []).length > 0;
        
        if (hasFiles && hasInstruments) return 'good';
        if (hasFiles || hasInstruments) return 'warning';
        return 'error';
    }

    destroy() {
        this.clearAllCache();
        
        if (this.refreshTimeout) {
            clearTimeout(this.refreshTimeout);
        }
        
        document.removeEventListener('keydown', this.keyboardHandler);
        window.removeEventListener('popstate', this.popstateHandler);
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = NavigationController;
}

if (typeof window !== 'undefined') {
    window.NavigationController = NavigationController;
}