// ============================================================================
// Fichier: frontend/js/main.js
// Version: v3.3.0 - EVENTBUS GLOBAL INITIALIZATION FIX
// Date: 2025-11-05
// Projet: MidiMind v3.1 - SystÃ¨me d'Orchestration MIDI
// ============================================================================
// MODIFICATIONS v3.3.0:
// âœ… CRITIQUE: Initialisation EventBus GLOBAL avant Application
// âœ… VÃ©rification EventBus aprÃ¨s initialisation
// âœ… Fix du problÃ¨me "EventBus is null"
//
// MODIFICATIONS v3.2.0:
// âœ… Ajout d'un timeout pour forcer l'affichage si l'init bloque
// âœ… Affichage de l'interface mÃªme si l'initialisation est incomplÃ¨te
// âœ… Meilleure gestion des erreurs d'initialisation
// ============================================================================


// Attendre que le DOM soit chargÃ©
document.addEventListener('DOMContentLoaded', async () => {
    
    // FORCE: Display #app immediately
    setTimeout(() => {
        const appEl = document.getElementById('app');
        if (appEl) {
            appEl.style.display = 'block';
            console.log('✓ #app force displayed');
        } else {
            console.error('✗ #app NOT FOUND');
        }
    }, 0);
    console.log('ðŸš€ Starting MidiMind v3.1.0 (Performance Mode)...');
    
    try {
        // =====================================================================
        // Ã‰TAPE 0: VÃ‰RIFIER PERFORMANCE CONFIG
        // =====================================================================
        
        if (typeof PerformanceConfig === 'undefined') {
            throw new Error('PerformanceConfig not loaded! Check index.html script order.');
        }
        
        console.log('âœ” PerformanceConfig loaded:', {
            targetFPS: PerformanceConfig.rendering.targetFPS,
            maxHistory: PerformanceConfig.memory.maxHistorySize,
            maxCache: PerformanceConfig.memory.maxCacheSize,
            keyboardMode: PerformanceConfig.keyboard.mode
        });
        
        // =====================================================================
        // Ã‰TAPE 1: ACTIVER MODE PERFORMANCE
        // =====================================================================
        
        // âœ” Ajouter classe performance-mode au body
        if (!PerformanceConfig.ui.enableTransitions) {
            document.body.classList.add('performance-mode');
            console.log('âœ” Performance mode activated (transitions disabled)');
        }
        
        // âœ” DÃ©sactiver smooth scroll
        if (!PerformanceConfig.rendering.enableSmoothScrolling) {
            document.documentElement.style.scrollBehavior = 'auto';
            const mainContainer = document.querySelector('.app-main');
            if (mainContainer) {
                mainContainer.style.scrollBehavior = 'auto';
            }
            console.log('âœ” Smooth scrolling disabled');
        }
        
        // =====================================================================
        // Ã‰TAPE 2: VÃ‰RIFIER ET INITIALISER EVENTBUS GLOBAL
        // =====================================================================
        
        if (typeof EventBus === 'undefined') {
            throw new Error('EventBus class not loaded. Check index.html script order.');
        }
        
        // âœ… CRITIQUE: CrÃ©er EventBus GLOBAL avant tout le reste
        if (!window.eventBus) {
            window.eventBus = new EventBus();
            console.log('âœ” EventBus initialized globally');
        } else {
            console.log('âœ” EventBus already exists (reusing existing instance)');
        }
        
        // VÃ©rification de sÃ©curitÃ©
        if (!window.eventBus || typeof window.eventBus.emit !== 'function') {
            throw new Error('EventBus initialization failed - invalid instance');
        }
        
        console.log('âœ” EventBus verified:', {
            hasOn: typeof window.eventBus.on === 'function',
            hasEmit: typeof window.eventBus.emit === 'function',
            hasOff: typeof window.eventBus.off === 'function'
        });
        
        // =====================================================================
        // Ã‰TAPE 3: VÃ‰RIFIER APPLICATION CLASS
        // =====================================================================
        
        if (typeof Application === 'undefined') {
            throw new Error('Application class not loaded. Check index.html script order.');
        }
        
        // =====================================================================
        // Ã‰TAPE 4: CRÃ‰ER INSTANCE APPLICATION
        // =====================================================================
        
        const app = new Application();
        
        // Rendre app globale pour accÃ¨s depuis la console et les autres scripts
        window.app = app;
        console.log('âœ” Application instance created');
        
        // VÃ©rification supplÃ©mentaire EventBus
        if (!window.eventBus) {
            console.error('âŒ CRITICAL: EventBus was lost during Application initialization!');
            throw new Error('EventBus disappeared after Application creation');
        }
        
        // =====================================================================
        // Ã‰TAPE 5: VÃ‰RIFIER MÃ‰THODE INIT
        // =====================================================================
        
        if (typeof app.init !== 'function') {
            throw new Error('Application.init() method not found');
        }
        
        // =====================================================================
        // Ã‰TAPE 6: AFFICHER LOADING INDICATOR
        // =====================================================================
        
        showLoadingIndicator();
        
        // =====================================================================
        // Ã‰TAPE 7: INITIALISER L'APPLICATION AVEC TIMEOUT
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
                // Ã‰TAPE 8: AFFICHER L'INTERFACE & MASQUER LOADING
                // =====================================================================
                
                hideLoadingIndicator();
                
                // CRITIQUE: Afficher l'Ã©lÃ©ment #app qui est cachÃ© par dÃ©faut
                const appElement = document.getElementById('app');
                if (appElement) {
                    appElement.style.display = 'block';
                    console.log('âœ” Application interface displayed');
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
        // Ã‰TAPE 9: AFFICHER INFO PERFORMANCE EN CONSOLE
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
// UTILITAIRES D'INTERFACE
// =============================================================================

function showLoadingIndicator() {
    const loadingEl = document.getElementById('loading-indicator');
    if (loadingEl) {
        loadingEl.style.display = 'flex';
    }
}

function hideLoadingIndicator() {
    const loadingEl = document.getElementById('loading-indicator');
    if (loadingEl) {
        loadingEl.style.display = 'none';
    }
}

/**
 * Force l'affichage de l'interface mÃªme en cas d'erreur
 */
function forceShowInterface() {
    console.log('ðŸ”§ Forcing interface display...');
    
    // Masquer loading
    hideLoadingIndicator();
    
    // Afficher #app
    const appElement = document.getElementById('app');
    if (appElement) {
        appElement.style.display = 'block';
        console.log('âœ” Interface forcefully displayed');
    } else {
        console.error('âŒ Cannot find #app element');
    }
}

/**
 * Affiche un message d'erreur Ã  l'utilisateur
 */
function showErrorMessage(message) {
    // CrÃ©er un Ã©lÃ©ment d'erreur si pas dÃ©jÃ  prÃ©sent
    let errorEl = document.getElementById('startup-error');
    
    if (!errorEl) {
        errorEl = document.createElement('div');
        errorEl.id = 'startup-error';
        errorEl.style.cssText = `
            position: fixed;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            background: #ff4444;
            color: white;
            padding: 20px 30px;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
            z-index: 10000;
            max-width: 500px;
            text-align: center;
            font-family: system-ui, -apple-system, sans-serif;
        `;
        document.body.appendChild(errorEl);
    }
    
    errorEl.innerHTML = `
        <h3 style="margin-top: 0;">âš ï¸ Erreur de DÃ©marrage</h3>
        <p>${message}</p>
        <button onclick="location.reload()" style="
            background: white;
            color: #ff4444;
            border: none;
            padding: 10px 20px;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
            margin-top: 10px;
        ">Recharger</button>
        <button onclick="document.getElementById('startup-error').remove()" style="
            background: transparent;
            color: white;
            border: 2px solid white;
            padding: 10px 20px;
            border-radius: 4px;
            cursor: pointer;
            margin-top: 10px;
            margin-left: 10px;
        ">Continuer</button>
    `;
}

/**
 * Affiche les informations de performance dans la console
 */
function displayPerformanceInfo() {
    if (typeof PerformanceConfig === 'undefined') {
        return;
    }
    
    const info = `
â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—
â•‘                                                      â•‘
â•‘       ðŸŽ¹ MIDI MIND v3.1.0 - PERFORMANCE MODE ðŸš€      â•‘
â•‘                                                      â•‘
â• â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•£
â•‘                                                      â•‘
â•‘  RENDERING                                           â•‘
â•‘  â€¢ Target FPS: ${PerformanceConfig.rendering.targetFPS.toString().padEnd(38)} â•‘
â•‘  â€¢ Anti-aliasing: ${(PerformanceConfig.rendering.enableAntiAliasing ? 'ON' : 'OFF').padEnd(33)} â•‘
â•‘  â€¢ Max notes visible: ${PerformanceConfig.rendering.maxVisibleNotes.toString().padEnd(28)} â•‘
â•‘  â€¢ Animations: ${(PerformanceConfig.rendering.enableAnimations ? 'ON' : 'OFF').padEnd(37)} â•‘
â•‘                                                      â•‘
â•‘  MEMORY                                              â•‘
â•‘  â€¢ Max cache: ${PerformanceConfig.memory.maxCacheSize.toString().padEnd(36)} MB â•‘
â•‘  â€¢ History levels: ${PerformanceConfig.memory.maxHistorySize.toString().padEnd(31)} â•‘
â•‘  â€¢ Aggressive GC: ${(PerformanceConfig.memory.aggressiveGC ? 'ON' : 'OFF').padEnd(34)} â•‘
â•‘                                                      â•‘
â•‘  KEYBOARD                                            â•‘
â•‘  â€¢ Mode: ${PerformanceConfig.keyboard.mode.padEnd(43)} â•‘
â•‘  â€¢ Recording: ${(PerformanceConfig.keyboard.enableRecording ? 'ON' : 'OFF').padEnd(38)} â•‘
â•‘  â€¢ Playback: ${(PerformanceConfig.keyboard.enablePlayback ? 'ON' : 'OFF').padEnd(39)} â•‘
â•‘                                                      â•‘
â•‘  ROUTING                                             â•‘
â•‘  â€¢ Complex routing: ${(PerformanceConfig.routing.allowComplexRouting ? 'ON' : 'OFF').padEnd(30)} â•‘
â•‘  â€¢ Auto-assign: ${(PerformanceConfig.routing.enableAutoRouting ? 'ON' : 'OFF').padEnd(36)} â•‘
â•‘  â€¢ Max routes: ${PerformanceConfig.routing.maxRoutes.toString().padEnd(37)} â•‘
â•‘                                                      â•‘
â• â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•£
â•‘                                                      â•‘
â•‘  ðŸ’¡ Tips:                                            â•‘
â•‘  â€¢ Utilisez window.app pour accÃ©der Ã  l'application â•‘
â•‘  â€¢ Utilisez window.eventBus pour l'EventBus global  â•‘
â•‘  â€¢ Utilisez window.PerformanceConfig pour config    â•‘
â•‘  â€¢ Pressez F12 pour ouvrir DevTools                 â•‘
â•‘                                                      â•‘
â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    `;
    
    console.log(info);
    
    // Ajouter Ã©galement des mÃ©tadonnÃ©es pour debug
    console.group('ðŸ” Performance Configuration Details');
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
    const errorMessage = event.error && event.error.message 
        ? event.error.message 
        : (event.message || 'Unknown error');
    
    // Ignorer erreur ResizeObserver (bÃ©nigne)
    if (errorMessage.includes('ResizeObserver')) {
        return;
    }
    
    console.error('ðŸ”´ Unhandled error:', event.error || errorMessage);
    
    if (window.app && window.app.debugConsole) {
        window.app.debugConsole.log('error', 
            `Unhandled error: ${errorMessage}`, 
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
        
        console.group('ðŸ“Š Performance Statistics');
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
            console.log('âœ” GC complete');
        } else {
            console.warn('GC not available. Start Chrome with --expose-gc flag.');
        }
    },
    
    /**
     * Force l'affichage de l'interface
     */
    forceShowInterface() {
        forceShowInterface();
    },
    
    /**
     * VÃ©rifie l'Ã©tat d'EventBus
     */
    checkEventBus() {
        console.group('ðŸ”Œ EventBus Status');
        console.log('Exists:', !!window.eventBus);
        console.log('Type:', typeof window.eventBus);
        if (window.eventBus) {
            console.log('Has emit:', typeof window.eventBus.emit === 'function');
            console.log('Has on:', typeof window.eventBus.on === 'function');
            console.log('Has off:', typeof window.eventBus.off === 'function');
            console.log('Listeners count:', Object.keys(window.eventBus.listeners || {}).length);
        }
        console.groupEnd();
    }
};

// Afficher les utilitaires disponibles
console.log('ðŸ”§ Debug utilities available: window.debugUtils');
console.log('   â€¢ showAppState()');
console.log('   â€¢ showPerformanceStats()');
console.log('   â€¢ togglePerformanceMode()');
console.log('   â€¢ forceGC()');
console.log('   â€¢ forceShowInterface()');
console.log('   â€¢ checkEventBus()');