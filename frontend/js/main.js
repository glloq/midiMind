// ============================================================================
// Fichier: frontend/js/main.js
// Version: v3.2.0 - FIXED INTERFACE DISPLAY
// Date: 2025-10-22
// Projet: MidiMind v3.0 - SystÃ¨me d'Orchestration MIDI
// ============================================================================
// MODIFICATIONS v3.2.0:
// âœ… Ajout d'un timeout pour forcer l'affichage si l'init bloque
// âœ… Affichage de l'interface mÃªme si l'initialisation est incomplÃ¨te
// âœ… Meilleure gestion des erreurs d'initialisation
// ============================================================================


// Attendre que le DOM soit chargÃ©
document.addEventListener('DOMContentLoaded', async () => {
    console.log('ðŸš€ Starting MidiMind v3.1.0 (Performance Mode)...');
    
    try {
        // =====================================================================
        // Ã‰TAPE 0: VÃ‰RIFIER PERFORMANCE CONFIG
        // =====================================================================
        
        if (typeof PerformanceConfig === 'undefined') {
            throw new Error('PerformanceConfig not loaded! Check index.html script order.');
        }
        
        console.log('âœ“ PerformanceConfig loaded:', {
            targetFPS: PerformanceConfig.rendering.targetFPS,
            maxHistory: PerformanceConfig.memory.maxHistorySize,
            maxCache: PerformanceConfig.memory.maxCacheSize,
            keyboardMode: PerformanceConfig.keyboard.mode
        });
        
        // =====================================================================
        // Ã‰TAPE 1: ACTIVER MODE PERFORMANCE
        // =====================================================================
        
        // âœ“ Ajouter classe performance-mode au body
        if (!PerformanceConfig.ui.enableTransitions) {
            document.body.classList.add('performance-mode');
            console.log('âœ“ Performance mode activated (transitions disabled)');
        }
        
        // âœ“ DÃ©sactiver smooth scroll
        if (!PerformanceConfig.rendering.enableSmoothScrolling) {
            document.documentElement.style.scrollBehavior = 'auto';
            const mainContainer = document.querySelector('.app-main');
            if (mainContainer) {
                mainContainer.style.scrollBehavior = 'auto';
            }
            console.log('âœ“ Smooth scrolling disabled');
        }
        
        // =====================================================================
        // Ã‰TAPE 2: VÃ‰RIFIER APPLICATION CLASS
        // =====================================================================
        
        if (typeof Application === 'undefined') {
            throw new Error('Application class not loaded. Check index.html script order.');
        }
        
        // =====================================================================
        // Ã‰TAPE 3: CRÃ‰ER INSTANCE APPLICATION
        // =====================================================================
        
        const app = new Application();
        
        // Rendre app globale pour accÃ¨s depuis la console et les autres scripts
        window.app = app;
        console.log('âœ“ Application instance created');
        
        // =====================================================================
        // Ã‰TAPE 4: VÃ‰RIFIER MÃ‰THODE INIT
        // =====================================================================
        
        if (typeof app.init !== 'function') {
            throw new Error('Application.init() method not found');
        }
        
        // =====================================================================
        // Ã‰TAPE 5: AFFICHER LOADING INDICATOR
        // =====================================================================
        
        showLoadingIndicator();
        
        // =====================================================================
        // Ã‰TAPE 6: INITIALISER L'APPLICATION AVEC TIMEOUT
        // =====================================================================
        
        console.log('âš™ï¸ Initializing application...');
        
        // NOUVEAU: Timeout de sÃ©curitÃ© pour forcer l'affichage aprÃ¨s 5 secondes
        let initTimeout = setTimeout(() => {
            console.warn('âš ï¸ Initialization timeout - forcing interface display');
            forceShowInterface();
        }, 5000);
        
        try {
            await app.init();
            
            // Annuler le timeout si l'init rÃ©ussit
            clearTimeout(initTimeout);
            
            // VÃ©rifier que l'initialisation a rÃ©ussi
            if (!app.state.initialized || !app.state.ready) {
                console.warn('âš ï¸ Application initialization incomplete - showing interface anyway');
                forceShowInterface();
            } else {
                // =====================================================================
                // Ã‰TAPE 7: AFFICHER L'INTERFACE & MASQUER LOADING
                // =====================================================================
                
                hideLoadingIndicator();
                
                // CRITIQUE: Afficher l'Ã©lÃ©ment #app qui est cachÃ© par dÃ©faut
                const appElement = document.getElementById('app');
                if (appElement) {
                    appElement.style.display = 'block';
                    console.log('âœ“ Application interface displayed');
                } else {
                    console.warn('âš ï¸ #app element not found in DOM');
                }
                
                console.log('âœ… MidiMind v3.1.0 initialized successfully (Performance Mode)');
                console.log('ðŸ“Š Performance Stats:', {
                    antiAliasing: PerformanceConfig.rendering.enableAntiAliasing ? 'ON' : 'OFF',
                    targetFPS: PerformanceConfig.rendering.targetFPS,
                    maxNotes: PerformanceConfig.rendering.maxVisibleNotes,
                    cacheSize: `${PerformanceConfig.memory.maxCacheSize}MB`,
                    historyLevels: PerformanceConfig.memory.maxHistorySize
                });
                
                // Ã‰mettre Ã©vÃ©nement pour signaler que l'app est prÃªte
                if (window.eventBus) {
                    window.eventBus.emit('app:ready', { 
                        app,
                        performanceMode: true,
                        version: '3.1.0'
                    });
                }
            }
            
        } catch (initError) {
            // Annuler le timeout
            clearTimeout(initTimeout);
            
            console.error('âŒ Application initialization failed:', initError);
            console.error('Stack trace:', initError.stack);
            
            // TOUJOURS afficher l'interface pour permettre le debug
            forceShowInterface();
            
            // Afficher une erreur dÃ©taillÃ©e Ã  l'utilisateur
            showErrorMessage(`Ã‰chec d'initialisation: ${initError.message}`);
        }
        
        // =====================================================================
        // Ã‰TAPE 8: AFFICHER INFO PERFORMANCE EN CONSOLE
        // =====================================================================
        
        displayPerformanceInfo();
        
    } catch (error) {
        console.error('âŒ Fatal error during startup:', error);
        hideLoadingIndicator();
        
        // Forcer l'affichage mÃªme en cas d'erreur fatale
        forceShowInterface();
        
        showErrorMessage(`Erreur fatale: ${error.message}`);
    }
});

// =============================================================================
// FONCTION: FORCER L'AFFICHAGE DE L'INTERFACE
// =============================================================================

function forceShowInterface() {
    console.log('ðŸ”§ Forcing interface display...');
    
    hideLoadingIndicator();
    
    // Afficher l'Ã©lÃ©ment #app
    const appElement = document.getElementById('app');
    if (appElement) {
        appElement.style.display = 'block';
        console.log('âœ“ Interface forcibly displayed');
    } else {
        console.error('âŒ Cannot find #app element');
    }
    
    // Afficher la page home par dÃ©faut
    const homePage = document.getElementById('home');
    if (homePage) {
        homePage.style.display = 'block';
        homePage.classList.add('active');
    }
    
    // Initialiser au moins la navigation de base
    if (window.app && !window.app.state.initialized) {
        try {
            // Navigation manuelle minimale
            window.addEventListener('hashchange', handleBasicNavigation);
            handleBasicNavigation();
            console.log('âœ“ Basic navigation initialized');
        } catch (e) {
            console.error('Failed to initialize basic navigation:', e);
        }
    }
}

// =============================================================================
// NAVIGATION DE BASE (fallback si l'app ne s'initialise pas)
// =============================================================================

function handleBasicNavigation() {
    const hash = window.location.hash.slice(1) || 'home';
    const page = hash.split('/')[0];
    
    console.log('ðŸ“ Basic navigation to:', page);
    
    // Masquer toutes les pages
    const pages = ['home', 'editor', 'routing', 'keyboard', 'instruments', 'system'];
    pages.forEach(p => {
        const element = document.getElementById(p);
        if (element) {
            element.style.display = 'none';
            element.classList.remove('active');
        }
    });
    
    // Afficher la page demandÃ©e
    const pageElement = document.getElementById(page);
    if (pageElement) {
        pageElement.style.display = 'block';
        pageElement.classList.add('active');
    }
    
    // Mettre Ã  jour les liens de navigation
    document.querySelectorAll('.nav-item').forEach(item => {
        item.classList.remove('active');
        if (item.dataset.page === page) {
            item.classList.add('active');
        }
    });
}

// =============================================================================
// FONCTION: AFFICHER L'INDICATEUR DE CHARGEMENT
// =============================================================================

function showLoadingIndicator() {
    const loading = document.getElementById('loading-indicator');
    if (loading) {
        loading.style.display = 'flex';
    }
}

// =============================================================================
// FONCTION: MASQUER L'INDICATEUR DE CHARGEMENT
// =============================================================================

function hideLoadingIndicator() {
    const loading = document.getElementById('loading-indicator');
    if (loading) {
        loading.style.display = 'none';
    }
}

// =============================================================================
// FONCTION: AFFICHER UN MESSAGE D'ERREUR
// =============================================================================

function showErrorMessage(message) {
    // CrÃ©er un Ã©lÃ©ment d'erreur si nÃ©cessaire
    let errorBox = document.getElementById('init-error-box');
    if (!errorBox) {
        errorBox = document.createElement('div');
        errorBox.id = 'init-error-box';
        errorBox.style.cssText = `
            position: fixed;
            top: 20px;
            left: 50%;
            transform: translateX(-50%);
            background: #f44336;
            color: white;
            padding: 15px 25px;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
            z-index: 10000;
            max-width: 500px;
            font-family: system-ui, -apple-system, sans-serif;
        `;
        document.body.appendChild(errorBox);
    }
    
    errorBox.innerHTML = `
        <strong>âš ï¸ Erreur</strong><br>
        ${message}
        <br><br>
        <small>Ouvrez la console (F12) pour plus de dÃ©tails</small>
    `;
}

// =============================================================================
// FONCTION: AFFICHER INFO PERFORMANCE
// =============================================================================

function displayPerformanceInfo() {
    if (typeof PerformanceConfig === 'undefined') return;
    
    const info = `
â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—
â•‘                                                        â•‘
â•‘       ðŸŽ¹ MIDI MIND v3.1.0 - PERFORMANCE MODE ðŸš€       â•‘
â•‘                                                        â•‘
â• â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•£
â•‘                                                        â•‘
â•‘  RENDERING                                             â•‘
â•‘  â€¢ Target FPS: ${PerformanceConfig.rendering.targetFPS.toString().padEnd(38)} â•‘
â•‘  â€¢ Anti-aliasing: ${(PerformanceConfig.rendering.enableAntiAliasing ? 'ON' : 'OFF').padEnd(33)} â•‘
â•‘  â€¢ Max notes visible: ${PerformanceConfig.rendering.maxVisibleNotes.toString().padEnd(28)} â•‘
â•‘  â€¢ Animations: ${(PerformanceConfig.rendering.enableAnimations ? 'ON' : 'OFF').padEnd(37)} â•‘
â•‘                                                        â•‘
â•‘  MEMORY                                                â•‘
â•‘  â€¢ Max cache: ${PerformanceConfig.memory.maxCacheSize.toString().padEnd(36)} MB â•‘
â•‘  â€¢ History levels: ${PerformanceConfig.memory.maxHistorySize.toString().padEnd(31)} â•‘
â•‘  â€¢ Aggressive GC: ${(PerformanceConfig.memory.aggressiveGC ? 'ON' : 'OFF').padEnd(34)} â•‘
â•‘                                                        â•‘
â•‘  KEYBOARD                                              â•‘
â•‘  â€¢ Mode: ${PerformanceConfig.keyboard.mode.padEnd(43)} â•‘
â•‘  â€¢ Recording: ${(PerformanceConfig.keyboard.enableRecording ? 'ON' : 'OFF').padEnd(38)} â•‘
â•‘  â€¢ Playback: ${(PerformanceConfig.keyboard.enablePlayback ? 'ON' : 'OFF').padEnd(39)} â•‘
â•‘                                                        â•‘
â•‘  ROUTING                                               â•‘
â•‘  â€¢ Complex routing: ${(PerformanceConfig.routing.allowComplexRouting ? 'ON' : 'OFF').padEnd(30)} â•‘
â•‘  â€¢ Auto-assign: ${(PerformanceConfig.routing.enableAutoRouting ? 'ON' : 'OFF').padEnd(36)} â•‘
â•‘  â€¢ Max routes: ${PerformanceConfig.routing.maxRoutes.toString().padEnd(37)} â•‘
â•‘                                                        â•‘
â• â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•£
â•‘                                                        â•‘
â•‘  ðŸ’¡ Tips:                                              â•‘
â•‘  â€¢ Utilisez window.app pour accÃ©der Ã  l'application   â•‘
â•‘  â€¢ Utilisez window.PerformanceConfig pour voir config â•‘
â•‘  â€¢ Pressez F12 pour ouvrir DevTools                   â•‘
â•‘                                                        â•‘
â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    `;
    
    console.log(info);
    
    // Ajouter Ã©galement des mÃ©tadonnÃ©es pour debug
    console.group('ðŸ“ Performance Configuration Details');
    console.log('Rendering:', PerformanceConfig.rendering);
    console.log('Memory:', PerformanceConfig.memory);
    console.log('Editor:', PerformanceConfig.editor);
    console.log('Routing:', PerformanceConfig.routing);
    console.log('Keyboard:', PerformanceConfig.keyboard);
    console.log('UI:', PerformanceConfig.ui);
    console.log('Features:', PerformanceConfig.features);
    console.groupEnd();
}

// =============================================================================
// GESTION ERREURS GLOBALES
// =============================================================================

// Capturer les erreurs non gÃ©rÃ©es
window.addEventListener('error', (event) => {
    console.error('ðŸ”´ Unhandled error:', event.error);
    
    if (window.app && window.app.debugConsole) {
        window.app.debugConsole.log('error', 
            `Unhandled error: ${event.error.message}`, 
            'error'
        );
    }
});

// Capturer les promesses rejetÃ©es
window.addEventListener('unhandledrejection', (event) => {
    console.error('ðŸ”´ Unhandled promise rejection:', event.reason);
    
    if (window.app && window.app.debugConsole) {
        window.app.debugConsole.log('error', 
            `Unhandled rejection: ${event.reason}`, 
            'error'
        );
    }
});

// =============================================================================
// UTILITAIRES DE DEBUG (accessibles depuis console)
// =============================================================================

window.debugUtils = {
    /**
     * Affiche l'Ã©tat actuel de l'application
     */
    showAppState() {
        if (!window.app) {
            console.warn('Application not initialized yet');
            return;
        }
        
        console.group('ðŸ“± Application State');
        console.log('Current Page:', window.location.hash || '#home');
        console.log('Initialized:', window.app.state?.initialized || false);
        console.log('Ready:', window.app.state?.ready || false);
        console.log('Backend Connected:', window.app.state?.backendConnected || false);
        console.log('Offline Mode:', window.app.state?.offlineMode || false);
        console.log('Models:', Object.keys(window.app.models || {}));
        console.log('Controllers:', Object.keys(window.app.controllers || {}));
        console.log('Views:', Object.keys(window.app.views || {}));
        console.groupEnd();
    },
    
    /**
     * Affiche les statistiques de performance
     */
    showPerformanceStats() {
        const stats = {
            memory: performance.memory ? {
                used: `${(performance.memory.usedJSHeapSize / 1024 / 1024).toFixed(2)} MB`,
                total: `${(performance.memory.totalJSHeapSize / 1024 / 1024).toFixed(2)} MB`,
                limit: `${(performance.memory.jsHeapSizeLimit / 1024 / 1024).toFixed(2)} MB`
            } : 'Not available',
            timing: performance.timing ? {
                loadTime: `${performance.timing.loadEventEnd - performance.timing.navigationStart} ms`,
                domReady: `${performance.timing.domContentLoadedEventEnd - performance.timing.navigationStart} ms`
            } : 'Not available'
        };
        
        console.group('ðŸ“ Performance Statistics');
        console.table(stats);
        console.groupEnd();
    },
    
    /**
     * Toggle mode performance
     */
    togglePerformanceMode() {
        document.body.classList.toggle('performance-mode');
        const isActive = document.body.classList.contains('performance-mode');
        console.log(`Performance mode: ${isActive ? 'ON' : 'OFF'}`);
    },
    
    /**
     * Force garbage collection (si disponible)
     */
    forceGC() {
        if (window.gc) {
            console.log('ðŸ—‘ï¸ Running garbage collection...');
            window.gc();
            console.log('âœ“ GC complete');
        } else {
            console.warn('GC not available. Start Chrome with --expose-gc flag.');
        }
    },
    
    /**
     * Force l'affichage de l'interface
     */
    forceShowInterface() {
        forceShowInterface();
    }
};

// Afficher les utilitaires disponibles
console.log('ðŸ”§ Debug utilities available: window.debugUtils');
console.log('   â€¢ showAppState()');
console.log('   â€¢ showPerformanceStats()');
console.log('   â€¢ togglePerformanceMode()');
console.log('   â€¢ forceGC()');
console.log('   â€¢ forceShowInterface()');