// ============================================================================
// Fichier: frontend/js/main.js
// Version: v3.3.0 - EVENTBUS GLOBAL INITIALIZATION FIX
// Date: 2025-11-05
// Projet: MidiMind v3.1 - Système d'Orchestration MIDI
// ============================================================================
// MODIFICATIONS v3.3.0:
// ✅ CRITIQUE: Initialisation EventBus GLOBAL avant Application
// ✅ Vérification EventBus après initialisation
// ✅ Fix du problème "EventBus is null"
//
// MODIFICATIONS v3.2.0:
// ✅ Ajout d'un timeout pour forcer l'affichage si l'init bloque
// ✅ Affichage de l'interface même si l'initialisation est incomplète
// ✅ Meilleure gestion des erreurs d'initialisation
// ============================================================================


// Attendre que le DOM soit chargé
document.addEventListener('DOMContentLoaded', async () => {
    console.log('🚀 Starting MidiMind v3.1.0 (Performance Mode)...');
    
    try {
        // =====================================================================
        // ÉTAPE 0: VÉRIFIER PERFORMANCE CONFIG
        // =====================================================================
        
        if (typeof PerformanceConfig === 'undefined') {
            throw new Error('PerformanceConfig not loaded! Check index.html script order.');
        }
        
        console.log('✔ PerformanceConfig loaded:', {
            targetFPS: PerformanceConfig.rendering.targetFPS,
            maxHistory: PerformanceConfig.memory.maxHistorySize,
            maxCache: PerformanceConfig.memory.maxCacheSize,
            keyboardMode: PerformanceConfig.keyboard.mode
        });
        
        // =====================================================================
        // ÉTAPE 1: ACTIVER MODE PERFORMANCE
        // =====================================================================
        
        // ✔ Ajouter classe performance-mode au body
        if (!PerformanceConfig.ui.enableTransitions) {
            document.body.classList.add('performance-mode');
            console.log('✔ Performance mode activated (transitions disabled)');
        }
        
        // ✔ Désactiver smooth scroll
        if (!PerformanceConfig.rendering.enableSmoothScrolling) {
            document.documentElement.style.scrollBehavior = 'auto';
            const mainContainer = document.querySelector('.app-main');
            if (mainContainer) {
                mainContainer.style.scrollBehavior = 'auto';
            }
            console.log('✔ Smooth scrolling disabled');
        }
        
        // =====================================================================
        // ÉTAPE 2: VÉRIFIER ET INITIALISER EVENTBUS GLOBAL
        // =====================================================================
        
        if (typeof EventBus === 'undefined') {
            throw new Error('EventBus class not loaded. Check index.html script order.');
        }
        
        // ✅ CRITIQUE: Créer EventBus GLOBAL avant tout le reste
        if (!window.eventBus) {
            window.eventBus = new EventBus();
            console.log('✔ EventBus initialized globally');
        } else {
            console.log('✔ EventBus already exists (reusing existing instance)');
        }
        
        // Vérification de sécurité
        if (!window.eventBus || typeof window.eventBus.emit !== 'function') {
            throw new Error('EventBus initialization failed - invalid instance');
        }
        
        console.log('✔ EventBus verified:', {
            hasOn: typeof window.eventBus.on === 'function',
            hasEmit: typeof window.eventBus.emit === 'function',
            hasOff: typeof window.eventBus.off === 'function'
        });
        
        // =====================================================================
        // ÉTAPE 3: VÉRIFIER APPLICATION CLASS
        // =====================================================================
        
        if (typeof Application === 'undefined') {
            throw new Error('Application class not loaded. Check index.html script order.');
        }
        
        // =====================================================================
        // ÉTAPE 4: CRÉER INSTANCE APPLICATION
        // =====================================================================
        
        const app = new Application();
        
        // Rendre app globale pour accès depuis la console et les autres scripts
        window.app = app;
        console.log('✔ Application instance created');
        
        // Vérification supplémentaire EventBus
        if (!window.eventBus) {
            console.error('❌ CRITICAL: EventBus was lost during Application initialization!');
            throw new Error('EventBus disappeared after Application creation');
        }
        
        // =====================================================================
        // ÉTAPE 5: VÉRIFIER MÉTHODE INIT
        // =====================================================================
        
        if (typeof app.init !== 'function') {
            throw new Error('Application.init() method not found');
        }
        
        // =====================================================================
        // ÉTAPE 6: AFFICHER LOADING INDICATOR
        // =====================================================================
        
        showLoadingIndicator();
        
        // =====================================================================
        // ÉTAPE 7: INITIALISER L'APPLICATION AVEC TIMEOUT
        // =====================================================================
        
        console.log('⚙️ Initializing application...');
        
        // NOUVEAU: Timeout de sécurité pour forcer l'affichage après 5 secondes
        let initTimeout = setTimeout(() => {
            console.warn('⚠️ Initialization timeout - forcing interface display');
            forceShowInterface();
        }, 5000);
        
        try {
            await app.init();
            
            // Annuler le timeout si l'init réussit
            clearTimeout(initTimeout);
            
            // Vérifier que l'initialisation a réussi
            if (!app.state.initialized || !app.state.ready) {
                console.warn('⚠️ Application initialization incomplete - showing interface anyway');
                forceShowInterface();
            } else {
                // =====================================================================
                // ÉTAPE 8: AFFICHER L'INTERFACE & MASQUER LOADING
                // =====================================================================
                
                hideLoadingIndicator();
                
                // CRITIQUE: Afficher l'élément #app qui est caché par défaut
                const appElement = document.getElementById('app');
                if (appElement) {
                    appElement.style.display = 'block';
                    console.log('✔ Application interface displayed');
                } else {
                    console.warn('⚠️ #app element not found in DOM');
                }
                
                console.log('✅ MidiMind v3.1.0 initialized successfully (Performance Mode)');
                console.log('📊 Performance Stats:', {
                    antiAliasing: PerformanceConfig.rendering.enableAntiAliasing ? 'ON' : 'OFF',
                    targetFPS: PerformanceConfig.rendering.targetFPS,
                    maxNotes: PerformanceConfig.rendering.maxVisibleNotes,
                    cacheSize: `${PerformanceConfig.memory.maxCacheSize}MB`,
                    historyLevels: PerformanceConfig.memory.maxHistorySize
                });
                
                // Émettre événement pour signaler que l'app est prête
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
            
            console.error('❌ Application initialization failed:', initError);
            console.error('Stack trace:', initError.stack);
            
            // TOUJOURS afficher l'interface pour permettre le debug
            forceShowInterface();
            
            // Afficher une erreur détaillée à l'utilisateur
            showErrorMessage(`Échec d'initialisation: ${initError.message}`);
        }
        
        // =====================================================================
        // ÉTAPE 9: AFFICHER INFO PERFORMANCE EN CONSOLE
        // =====================================================================
        
        displayPerformanceInfo();
        
    } catch (error) {
        console.error('❌ Fatal error during startup:', error);
        hideLoadingIndicator();
        
        // Forcer l'affichage même en cas d'erreur fatale
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
 * Force l'affichage de l'interface même en cas d'erreur
 */
function forceShowInterface() {
    console.log('🔧 Forcing interface display...');
    
    // Masquer loading
    hideLoadingIndicator();
    
    // Afficher #app
    const appElement = document.getElementById('app');
    if (appElement) {
        appElement.style.display = 'block';
        console.log('✔ Interface forcefully displayed');
    } else {
        console.error('❌ Cannot find #app element');
    }
}

/**
 * Affiche un message d'erreur à l'utilisateur
 */
function showErrorMessage(message) {
    // Créer un élément d'erreur si pas déjà présent
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
        <h3 style="margin-top: 0;">⚠️ Erreur de Démarrage</h3>
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
╔══════════════════════════════════════════════════════╗
║                                                      ║
║       🎹 MIDI MIND v3.1.0 - PERFORMANCE MODE 🚀      ║
║                                                      ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║  RENDERING                                           ║
║  • Target FPS: ${PerformanceConfig.rendering.targetFPS.toString().padEnd(38)} ║
║  • Anti-aliasing: ${(PerformanceConfig.rendering.enableAntiAliasing ? 'ON' : 'OFF').padEnd(33)} ║
║  • Max notes visible: ${PerformanceConfig.rendering.maxVisibleNotes.toString().padEnd(28)} ║
║  • Animations: ${(PerformanceConfig.rendering.enableAnimations ? 'ON' : 'OFF').padEnd(37)} ║
║                                                      ║
║  MEMORY                                              ║
║  • Max cache: ${PerformanceConfig.memory.maxCacheSize.toString().padEnd(36)} MB ║
║  • History levels: ${PerformanceConfig.memory.maxHistorySize.toString().padEnd(31)} ║
║  • Aggressive GC: ${(PerformanceConfig.memory.aggressiveGC ? 'ON' : 'OFF').padEnd(34)} ║
║                                                      ║
║  KEYBOARD                                            ║
║  • Mode: ${PerformanceConfig.keyboard.mode.padEnd(43)} ║
║  • Recording: ${(PerformanceConfig.keyboard.enableRecording ? 'ON' : 'OFF').padEnd(38)} ║
║  • Playback: ${(PerformanceConfig.keyboard.enablePlayback ? 'ON' : 'OFF').padEnd(39)} ║
║                                                      ║
║  ROUTING                                             ║
║  • Complex routing: ${(PerformanceConfig.routing.allowComplexRouting ? 'ON' : 'OFF').padEnd(30)} ║
║  • Auto-assign: ${(PerformanceConfig.routing.enableAutoRouting ? 'ON' : 'OFF').padEnd(36)} ║
║  • Max routes: ${PerformanceConfig.routing.maxRoutes.toString().padEnd(37)} ║
║                                                      ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║  💡 Tips:                                            ║
║  • Utilisez window.app pour accéder à l'application ║
║  • Utilisez window.eventBus pour l'EventBus global  ║
║  • Utilisez window.PerformanceConfig pour config    ║
║  • Pressez F12 pour ouvrir DevTools                 ║
║                                                      ║
╚══════════════════════════════════════════════════════╝
    `;
    
    console.log(info);
    
    // Ajouter également des métadonnées pour debug
    console.group('🔍 Performance Configuration Details');
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

// Capturer les erreurs non gérées
window.addEventListener('error', (event) => {
    const errorMessage = event.error && event.error.message 
        ? event.error.message 
        : (event.message || 'Unknown error');
    
    // Ignorer erreur ResizeObserver (bénigne)
    if (errorMessage.includes('ResizeObserver')) {
        return;
    }
    
    console.error('🔴 Unhandled error:', event.error || errorMessage);
    
    if (window.app && window.app.debugConsole) {
        window.app.debugConsole.log('error', 
            `Unhandled error: ${errorMessage}`, 
            'error'
        );
    }
});

// Capturer les promesses rejetées
window.addEventListener('unhandledrejection', (event) => {
    console.error('🔴 Unhandled promise rejection:', event.reason);
    
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
     * Affiche l'état actuel de l'application
     */
    showAppState() {
        if (!window.app) {
            console.warn('Application not initialized yet');
            return;
        }
        
        console.group('📱 Application State');
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
        
        console.group('📊 Performance Statistics');
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
            console.log('🗑️ Running garbage collection...');
            window.gc();
            console.log('✔ GC complete');
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
     * Vérifie l'état d'EventBus
     */
    checkEventBus() {
        console.group('🔌 EventBus Status');
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
console.log('🔧 Debug utilities available: window.debugUtils');
console.log('   • showAppState()');
console.log('   • showPerformanceStats()');
console.log('   • togglePerformanceMode()');
console.log('   • forceGC()');
console.log('   • forceShowInterface()');
console.log('   • checkEventBus()');