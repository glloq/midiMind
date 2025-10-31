// ===== NAVIGATION CONTROLLER - Contrôleur de navigation et gestion des pages =====
// ================================================================================
// Fichier: frontend/js/controllers/NavigationController.js
// Version: v3.7.2 - CORRECTION MAPPINGS VUES
// Date: 2025-10-31
// ================================================================================
// CORRECTIONS v3.7.2:
// ✅ Ajout page "files" pour correspondre au HTML
// ✅ Correction viewKeys: instrument (pas instruments)
// ✅ Gestion vues sans buildTemplate
// ================================================================================

class NavigationController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // État de navigation
        this.currentPage = 'home';
        this.previousPage = null;
        this.navigationHistory = ['home'];
        this.historyIndex = 0;
        
        // Configuration des pages - IDS ET VIEWKEYS CORRIGES
        this.pages = {
            home: {
                id: 'home',
                title: '🏠 Accueil',
                icon: '🏠',
                shortcut: 'h',
                requiresData: true,
                cacheable: false,
                viewKey: 'home'
            },
            files: {
                id: 'files',
                title: '📁 Fichiers',
                icon: '📁',
                shortcut: 'f',
                requiresData: true,
                cacheable: false,
                viewKey: 'file'
            },
            editor: {
                id: 'editor',
                title: '✏️ Éditeur',
                icon: '✏️',
                shortcut: 'e',
                requiresData: true,
                cacheable: false,
                viewKey: 'editor'
            },
            routing: {
                id: 'routing',
                title: '🔀 Routage',
                icon: '🔀',
                shortcut: 'r',
                requiresData: true,
                cacheable: true,
                viewKey: 'routing'
            },
            keyboard: {
                id: 'keyboard',
                title: '🎹 Clavier',
                icon: '🎹',
                shortcut: 'k',
                requiresData: true,
                cacheable: false,
                viewKey: 'keyboard'
            },
            instruments: {
                id: 'instruments',
                title: '🎸 Instruments',
                icon: '🎸',
                shortcut: 'i',
                requiresData: true,
                cacheable: true,
                viewKey: 'instrument'
            },
            system: {
                id: 'system',
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
        
        this.eventBus.on('file:updated', () => {
            this.invalidatePageCache(['home', 'files', 'editor']);
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
        this.logDebug('navigation', 'Système de navigation initialisé');
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
            }
        });
    }

    setupBrowserHistory() {
        window.addEventListener('popstate', (event) => {
            if (event.state && event.state.page) {
                this.showPage(event.state.page, { 
                    skipHistory: true, 
                    skipPushState: true 
                });
            }
        });
    }

    async showPage(pageKey, options = {}) {
        const {
            forceRefresh = false,
            skipHistory = false,
            skipPushState = false,
            animationDirection = 'forward'
        } = options;
        
        if (!this.pages[pageKey]) {
            this.logDebug('navigation', `Page introuvable: ${pageKey}`);
            this.showNotification('Page introuvable', 'error');
            return false;
        }
        
        if (pageKey === this.currentPage && !forceRefresh) {
            this.logDebug('navigation', `Page déjà active: ${pageKey}`);
            return true;
        }
        
        if (this.transitionState.inProgress) {
            this.logDebug('navigation', 'Transition déjà en cours, ignorée');
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
                
                this.logDebug('navigation', `Navigation réussie: ${this.previousPage} → ${pageKey}`);
                return true;
            }
            
        } catch (error) {
            this.logDebug('navigation', `Erreur navigation: ${error.message}`);
            console.error('Navigation error:', error);
            this.showNotification('Erreur lors du chargement de la page', 'error');
            return false;
            
        } finally {
            this.transitionState.inProgress = false;
        }
    }

    async performPageTransition(pageKey, animationDirection) {
        const pageConfig = this.pages[pageKey];
        const currentPageElement = document.getElementById(this.pages[this.currentPage].id);
        const targetPageElement = document.getElementById(pageConfig.id);
        
        if (!currentPageElement || !targetPageElement) {
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
            
            const animation = pageElement.animate([
                { opacity: 0, transform: 'translateX(20%)' },
                { opacity: 1, transform: 'translateX(0%)' }
            ], {
                duration: this.transitionState.duration,
                easing: this.transitionState.easing,
                fill: 'forwards'
            });
            
            animation.onfinish = () => {
                pageElement.classList.add('active');
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
            this.logDebug('navigation', `Contenu en cache pour: ${pageKey}`);
            return cachedContent;
        }
        
        const content = await this.generatePageContent(pageKey);
        
        if (pageConfig.cacheable && content) {
            this.pageCache.set(pageKey, content);
            this.scheduleCacheExpiry(pageKey);
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
                
            case 'files':
                return {
                    ...commonData,
                    files: fileModel?.getAll() || [],
                    currentPath: fileModel?.getCurrentPath() || '/midi',
                    selectedFile: fileModel?.getSelected()
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
        const errorMessage = error ? error.message : 'Page introuvable ou inaccessible';
        return `
            <div class="error-page">
                <div class="error-icon">⚠️</div>
                <h2>Erreur de chargement</h2>
                <p>La page "${pageKey}" n'a pas pu être chargée.</p>
                ${error ? `<p class="error-detail">${errorMessage}</p>` : ''}
                <button class="btn-primary" onclick="window.location.reload()">Recharger l'application</button>
            </div>
        `;
    }

    getSystemStats() {
        return {
            uptime: Date.now() - (window.app?.startTime || Date.now()),
            pageViews: this.navigationHistory.length,
            cacheSize: this.pageCache.size
        };
    }

    getSystemHealth() {
        const backend = window.backendService || window.app?.services?.backend;
        return {
            backendConnected: backend?.isConnected() || false,
            performanceGood: true,
            memoryUsage: performance.memory ? 
                (performance.memory.usedJSHeapSize / performance.memory.jsHeapSizeLimit * 100).toFixed(1) : 
                'N/A'
        };
    }

    updateNavigationUI() {
        document.querySelectorAll('.nav-item').forEach(link => {
            const linkPage = link.getAttribute('data-page');
            if (linkPage === this.currentPage) {
                link.classList.add('active');
            } else {
                link.classList.remove('active');
            }
        });
    }

    addToHistory(pageKey) {
        if (this.historyIndex < this.navigationHistory.length - 1) {
            this.navigationHistory = this.navigationHistory.slice(0, this.historyIndex + 1);
        }
        this.navigationHistory.push(pageKey);
        this.historyIndex = this.navigationHistory.length - 1;
    }

    goBack() {
        if (this.historyIndex > 0) {
            this.historyIndex--;
            const targetPage = this.navigationHistory[this.historyIndex];
            this.showPage(targetPage, { 
                skipHistory: true, 
                animationDirection: 'backward' 
            });
        }
    }

    goForward() {
        if (this.historyIndex < this.navigationHistory.length - 1) {
            this.historyIndex++;
            const targetPage = this.navigationHistory[this.historyIndex];
            this.showPage(targetPage, { 
                skipHistory: true, 
                animationDirection: 'forward' 
            });
        }
    }

    invalidatePageCache(pageKeys = null) {
        if (pageKeys) {
            pageKeys.forEach(key => {
                this.pageCache.delete(key);
                if (this.cacheTimeouts.has(key)) {
                    clearTimeout(this.cacheTimeouts.get(key));
                    this.cacheTimeouts.delete(key);
                }
            });
        } else {
            this.pageCache.clear();
            this.cacheTimeouts.forEach(timeout => clearTimeout(timeout));
            this.cacheTimeouts.clear();
        }
    }

    scheduleCacheExpiry(pageKey) {
        if (this.cacheTimeouts.has(pageKey)) {
            clearTimeout(this.cacheTimeouts.get(pageKey));
        }
        
        const timeout = setTimeout(() => {
            this.pageCache.delete(pageKey);
            this.cacheTimeouts.delete(pageKey);
            this.logDebug('navigation', `Cache expiré pour: ${pageKey}`);
        }, this.defaultCacheDuration);
        
        this.cacheTimeouts.set(pageKey, timeout);
    }

    handleModelChange(data) {
        const { modelName } = data;
        
        const affectedPages = {
            'file': ['home', 'files', 'editor'],
            'instrument': ['home', 'instruments', 'keyboard'],
            'playlist': ['home', 'editor'],
            'routing': ['routing'],
            'system': ['system']
        };
        
        if (affectedPages[modelName]) {
            this.invalidatePageCache(affectedPages[modelName]);
            
            if (affectedPages[modelName].includes(this.currentPage)) {
                this.showPage(this.currentPage, { forceRefresh: true });
            }
        }
    }

    logDebug(category, message, data = null) {
        if (this.debugConsole && typeof this.debugConsole.log === 'function') {
            this.debugConsole.log(category, message, data);
        }
    }

    showNotification(message, type = 'info') {
        if (this.notifications && typeof this.notifications.show === 'function') {
            this.notifications.show(message, type);
        }
    }
}

if (typeof window !== 'undefined') {
    window.NavigationController = NavigationController;
}