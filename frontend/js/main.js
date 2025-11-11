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
            console.log('âœ“ #app force displayed');
        } else {
            console.error('âœ— #app NOT FOUND');
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
        // Ã‰TAPE 2: VÃ‰RIFIER ET INITIALISER EVENTBUS GLOBAL
        // =====================================================================
        
        if (typeof EventBus === 'undefined') {
            throw new Error('EventBus class not loaded. Check index.html script order.');
        }
        
        // âœ… CRITIQUE: CrÃ©er EventBus GLOBAL avant tout le reste
        if (!window.eventBus) {
            window.eventBus = new EventBus();
            console.log('âœ“ EventBus initialized globally');
        } else {
            console.log('âœ“ EventBus already exists (reusing existing instance)');
        }
        
        // VÃ©rification de sÃ©curitÃ©
        if (!window.eventBus || typeof window.eventBus.emit !== 'function') {
            throw new Error('EventBus initialization failed - invalid instance');
        }
        
        console.log('âœ“ EventBus verified:', {
            hasOn: typeof window.eventBus.on === 'function',
            hasEmit: typeof window.eventBus.emit === 'function',
            hasOff: typeof window.eventBus.off === 'function'
        });
        console.log('✅ EventBus verified:', {
            hasOn: typeof window.eventBus.on === 'function',
            hasEmit: typeof window.eventBus.emit === 'function',
            hasOff: typeof window.eventBus.off === 'function'
        });
        
        // =====================================================================
        // ✅ NOUVEAU: ÉTAPE 2.1: INITIALISER LOGGER GLOBAL
        // =====================================================================
        
        if (typeof Logger === 'undefined') {
            throw new Error('Logger class not loaded. Check index.html script order.');
        }
        
        if (!window.logger) {
            console.log('📝 Creating global Logger...');
            window.logger = new Logger({
                level: 'info',
                enableConsole: true,
                enableEventBus: true,
                eventBus: window.eventBus
            });
            console.log('✅ Logger created and initialized');
        } else {
            console.log('✅ Logger already exists (reusing existing instance)');
        }
        
        // =====================================================================
        // ✅ NOUVEAU: ÉTAPE 2.2: INITIALISER NOTIFICATIONMANAGER GLOBAL
        // =====================================================================
        
        if (typeof NotificationManager === 'undefined') {
            console.warn('⚠️ NotificationManager class not loaded (optional)');
        } else if (!window.notificationManager) {
            console.log('🔔 Creating global NotificationManager...');
            window.notificationManager = new NotificationManager();
            console.log('✅ NotificationManager created and initialized');
        } else {
            console.log('✅ NotificationManager already exists (reusing existing instance)');
        }
        
        // =====================================================================
        // ÉTAPE 3: VÉRIFIER APPLICATION CLASS
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
        console.log('âœ“ Application instance created');
        
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
                    historyLevels: PerformanceConfig.memory.maxHistorySize,
                    keyboardMode: PerformanceConfig.keyboard.mode
                });
                
                // Afficher le rÃ©capitulatif des performances
                showPerformanceInfo();
            }
        } catch (initError) {
            // Annuler le timeout en cas d'erreur
            clearTimeout(initTimeout);
            
            console.error('âŒ Initialization error:', initError);
            
            // NOUVEAU: Forcer l'affichage mÃªme en cas d'erreur d'init
            console.warn('âš ï¸ Forcing interface display despite initialization error');
            forceShowInterface();
            
            // Re-throw l'erreur pour qu'elle soit capturÃ©e par le catch externe
            throw initError;
        }
        
    } catch (error) {
        console.error('âŒ Fatal initialization error:', error);
        console.error('Stack trace:', error.stack);
        
        // NOUVEAU: Toujours forcer l'affichage de l'interface
        forceShowInterface();
        
        // Afficher une erreur utilisateur conviviale
        showErrorMessage(
            'Erreur d\'initialisation',
            `Une erreur s'est produite lors du chargement de l'application: ${error.message}`,
            'Veuillez recharger la page ou consulter la console pour plus de dÃ©tails.'
        );
    }
});

// =============================================================================
// LOADING INDICATOR
// =============================================================================

/**
 * Affiche l'indicateur de chargement
 */
function showLoadingIndicator() {
    const loadingDiv = document.createElement('div');
    loadingDiv.id = 'app-loading';
    loadingDiv.innerHTML = `
        <div class="loading-content">
            <div class="loading-spinner"></div>
            <div class="loading-text">Chargement de MidiMind...</div>
        </div>
    `;
    loadingDiv.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background: rgba(0, 0, 0, 0.8);
        display: flex;
        align-items: center;
        justify-content: center;
        z-index: 10000;
    `;
    document.body.appendChild(loadingDiv);
}

/**
 * Masque l'indicateur de chargement
 */
function hideLoadingIndicator() {
    const loadingDiv = document.getElementById('app-loading');
    if (loadingDiv) {
        loadingDiv.remove();
    }
}

/**
 * NOUVEAU: Force l'affichage de l'interface mÃªme si l'init Ã©choue
 */
function forceShowInterface() {
    console.log('ðŸ”§ Forcing interface display...');
    
    // Masquer le loading
    hideLoadingIndicator();
    
    // Afficher #app
    const appElement = document.getElementById('app');
    if (appElement) {
        appElement.style.display = 'block';
        console.log('âœ“ #app forced to display');
    } else {
        console.error('âœ— #app element not found - cannot force display');
    }
    
    // Afficher la navigation au minimum
    const nav = document.querySelector('.app-nav');
    if (nav) {
        nav.style.display = 'block';
        console.log('âœ“ Navigation forced to display');
    }
    
    console.log('âœ“ Interface forced to display (degraded mode)');
}

// =============================================================================
// ERROR DISPLAY
// =============================================================================

/**
 * Affiche un message d'erreur convivial
 */
function showErrorMessage(title, message, details) {
    const errorDiv = document.createElement('div');
    errorDiv.className = 'error-message';
    errorDiv.innerHTML = `
        <div class="error-content">
            <h2>âŒ ${title}</h2>
            <p>${message}</p>
            ${details ? `<p class="error-details">${details}</p>` : ''}
            <button onclick="location.reload()">Recharger la page</button>
        </div>
    `;
    errorDiv.style.cssText = `
        position: fixed;
        top: 50%;
        left: 50%;
        transform: translate(-50%, -50%);
        background: white;
        padding: 2rem;
        border-radius: 8px;
        box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
        z-index: 10001;
        max-width: 500px;
    `;
    document.body.appendChild(errorDiv);
}

// =============================================================================
// PERFORMANCE INFO DISPLAY
// =============================================================================

/**
 * Affiche les informations de performance dans la console
 */
function showPerformanceInfo() {
    const info = `
â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—
â•‘                                                  â•‘
â•‘  ðŸŽµ MidiMind v3.1.0 - Performance Mode ActivÃ©    â•‘
â•‘                                                  â•‘
â• â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•£
â•‘                                                  â•‘
â•‘  RENDERING                                       â•‘
â•‘  â€¢ FPS target: ${PerformanceConfig.rendering.targetFPS.toString().padEnd(36)} â•‘
â•‘  â€¢ Max notes: ${PerformanceConfig.rendering.maxVisibleNotes.toString().padEnd(37)} â•‘
â•‘  â€¢ Anti-aliasing: ${(PerformanceConfig.rendering.enableAntiAliasing ? 'ON' : 'OFF').padEnd(31)} â•‘
â•‘  â€¢ Smooth scroll: ${(PerformanceConfig.rendering.enableSmoothScrolling ? 'ON' : 'OFF').padEnd(31)} â•‘
â•‘                                                  â•‘
â•‘  MEMORY                                          â•‘
â•‘  â€¢ Max cache: ${PerformanceConfig.memory.maxCacheSize.toString().padEnd(36)} MB â•‘
â•‘  â€¢ History levels: ${PerformanceConfig.memory.maxHistorySize.toString().padEnd(31)} â•‘
â•‘  â€¢ Aggressive GC: ${(PerformanceConfig.memory.aggressiveGC ? 'ON' : 'OFF').padEnd(34)} â•‘
â•‘                                                  â•‘
â•‘  KEYBOARD                                        â•‘
â•‘  â€¢ Mode: ${PerformanceConfig.keyboard.mode.padEnd(43)} â•‘
â•‘  â€¢ Recording: ${(PerformanceConfig.keyboard.enableRecording ? 'ON' : 'OFF').padEnd(38)} â•‘
â•‘  â€¢ Playback: ${(PerformanceConfig.keyboard.enablePlayback ? 'ON' : 'OFF').padEnd(39)} â•‘
â•‘                                                  â•‘
â•‘  ROUTING                                         â•‘
â•‘  â€¢ Complex routing: ${(PerformanceConfig.routing.allowComplexRouting ? 'ON' : 'OFF').padEnd(30)} â•‘
â•‘  â€¢ Auto-assign: ${(PerformanceConfig.routing.enableAutoRouting ? 'ON' : 'OFF').padEnd(36)} â•‘
â•‘  â€¢ Max routes: ${PerformanceConfig.routing.maxRoutes.toString().padEnd(37)} â•‘
â•‘                                                  â•‘
â• â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•£
â•‘                                                  â•‘
â•‘  ðŸ’¡ Tips:                                        â•‘
â•‘  â€¢ Utilisez window.app pour accÃ©der Ã  l'application â•‘
â•‘  â€¢ Utilisez window.eventBus pour l'EventBus global  â•‘
â•‘  â€¢ Utilisez window.PerformanceConfig pour config    â•‘
â•‘  â€¢ Pressez F12 pour ouvrir DevTools             â•‘
â•‘                                                  â•‘
â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
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