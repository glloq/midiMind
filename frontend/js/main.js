// ============================================================================
// Fichier: frontend/js/main.js
// Version: v3.1.0 - PERFORMANCE OPTIMIZED
// Date: 2025-10-16
// Projet: MidiMind v3.0 - SystÃ¨me d'Orchestration MIDI
// ============================================================================
// MODIFICATIONS v3.1.0:
// âœ“ Activation mode performance
// âœ“ DÃ©sactivation smooth scroll
// âœ“ Validation PerformanceConfig chargÃ©
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
        // ÉTAPE 6: INITIALISER L'APPLICATION
        // =====================================================================
        
        console.log('⚙️ Initializing application...');
        
        try {
            await app.init();
            
            // Vérifier que l'initialisation a réussi
            if (!app.state.initialized || !app.state.ready) {
                throw new Error('Application initialization failed - state not ready');
            }
            
            // =====================================================================
            // ÉTAPE 7: AFFICHER L'INTERFACE & MASQUER LOADING
            // =====================================================================
            
            hideLoadingIndicator();
            
            // CRITIQUE: Afficher l'élément #app qui est caché par défaut
            const appElement = document.getElementById('app');
            if (appElement) {
                appElement.style.display = 'block';
                console.log('✓ Application interface displayed');
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
            
            // =====================================================================
            // ÉTAPE 8: AFFICHER INFO PERFORMANCE EN CONSOLE
            // =====================================================================
            
            displayPerformanceInfo();
            
        } catch (initError) {
            console.error('❌ Application initialization failed:', initError);
            console.error('Stack trace:', initError.stack);
            hideLoadingIndicator();
            
            // Afficher une erreur détaillée à l'utilisateur
            showErrorMessage(`Échec d'initialisation: ${initError.message}`);
            
            // Afficher quand même l'interface pour permettre le debug
            const appElement = document.getElementById('app');
            if (appElement) {
                appElement.style.display = 'block';
                console.log('⚠️ Interface displayed despite errors for debugging');
            }
        }
        
    } catch (error) {
        console.error('âŒ Failed to initialize MidiMind:', error);
        console.error('Stack trace:', error.stack);
        
        // Afficher une erreur Ã  l'utilisateur
        showErrorMessage(error.message);
        
        // Ne pas bloquer complÃ¨tement - permettre le debug
        console.log('Application failed but console remains available for debugging');
        console.log('Try: window.app, window.PerformanceConfig');
    }
});

// =============================================================================
// FONCTIONS UTILITAIRES
// =============================================================================

/**
 * Affiche un indicateur de chargement
 */
function showLoadingIndicator() {
    const loading = document.getElementById('loading-indicator');
    if (loading) {
        loading.style.display = 'flex';
    } else {
        // CrÃ©er un indicateur si n'existe pas
        const indicator = document.createElement('div');
        indicator.id = 'loading-indicator';
        indicator.className = 'loading-indicator';
        indicator.innerHTML = `
            <div class="spinner"></div>
            <p style="margin-top: 20px; color: #ecf0f1;">Chargement de MIDI Mind...</p>
        `;
        document.body.appendChild(indicator);
    }
}

/**
 * Masque l'indicateur de chargement
 */
function hideLoadingIndicator() {
    const loading = document.getElementById('loading-indicator');
    if (loading) {
        loading.style.display = 'none';
    }
}

/**
 * Affiche un message d'erreur Ã  l'utilisateur
 */
function showErrorMessage(message) {
    hideLoadingIndicator();
    
    const errorDiv = document.createElement('div');
    errorDiv.className = 'error-message';
    errorDiv.innerHTML = `
        <div style="
            position: fixed;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            background: #2c3e50;
            border: 2px solid #FF6B6B;
            border-radius: 8px;
            padding: 30px;
            max-width: 500px;
            z-index: 10000;
            box-shadow: 0 10px 40px rgba(0,0,0,0.3);
        ">
            <h2 style="color: #FF6B6B; margin-bottom: 15px;">âŒ Erreur d'initialisation</h2>
            <p style="color: #ecf0f1; margin-bottom: 20px;">${message}</p>
            <details style="color: #95a5a6; font-size: 12px; margin-top: 15px;">
                <summary style="cursor: pointer; margin-bottom: 10px;">DÃ©tails techniques</summary>
                <pre style="background: #1a1a1a; padding: 10px; border-radius: 4px; overflow: auto;">
VÃ©rifications Ã  effectuer:
1. Tous les scripts sont-ils chargÃ©s dans le bon ordre ?
2. PerformanceConfig.js est-il chargÃ© en premier ?
3. Le backend WebSocket est-il accessible ?
4. Console DevTools pour plus d'infos
                </pre>
            </details>
            <button onclick="window.location.reload()" style="
                margin-top: 20px;
                padding: 10px 20px;
                background: #4ECDC4;
                color: #1a1a1a;
                border: none;
                border-radius: 4px;
                cursor: pointer;
                font-weight: 600;
            ">
                Recharger la page
            </button>
        </div>
    `;
    
    document.body.appendChild(errorDiv);
}

/**
 * Affiche les informations de performance dans la console
 */
function displayPerformanceInfo() {
    if (!window.PerformanceConfig) return;
    
    const info = `
â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—
â•‘                                                            â•‘
â•‘       ðŸŽ¹ MIDI MIND v3.1.0 - PERFORMANCE MODE ðŸš€          â•‘
â•‘                                                            â•‘
â• â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•£
â•‘                                                            â•‘
â•‘  RENDERING                                                 â•‘
â•‘  â€¢ Target FPS: ${PerformanceConfig.rendering.targetFPS.toString().padEnd(38)} â•‘
â•‘  â€¢ Anti-aliasing: ${(PerformanceConfig.rendering.enableAntiAliasing ? 'ON' : 'OFF').padEnd(33)} â•‘
â•‘  â€¢ Max notes visible: ${PerformanceConfig.rendering.maxVisibleNotes.toString().padEnd(28)} â•‘
â•‘  â€¢ Animations: ${(PerformanceConfig.rendering.enableAnimations ? 'ON' : 'OFF').padEnd(37)} â•‘
â•‘                                                            â•‘
â•‘  MEMORY                                                    â•‘
â•‘  â€¢ Max cache: ${PerformanceConfig.memory.maxCacheSize.toString().padEnd(36)} MB â•‘
â•‘  â€¢ History levels: ${PerformanceConfig.memory.maxHistorySize.toString().padEnd(31)} â•‘
â•‘  â€¢ Aggressive GC: ${(PerformanceConfig.memory.aggressiveGC ? 'ON' : 'OFF').padEnd(34)} â•‘
â•‘                                                            â•‘
â•‘  KEYBOARD                                                  â•‘
â•‘  â€¢ Mode: ${PerformanceConfig.keyboard.mode.padEnd(43)} â•‘
â•‘  â€¢ Recording: ${(PerformanceConfig.keyboard.enableRecording ? 'ON' : 'OFF').padEnd(38)} â•‘
â•‘  â€¢ Playback: ${(PerformanceConfig.keyboard.enablePlayback ? 'ON' : 'OFF').padEnd(39)} â•‘
â•‘                                                            â•‘
â•‘  ROUTING                                                   â•‘
â•‘  â€¢ Complex routing: ${(PerformanceConfig.routing.allowComplexRouting ? 'ON' : 'OFF').padEnd(30)} â•‘
â•‘  â€¢ Auto-assign: ${(PerformanceConfig.routing.enableAutoRouting ? 'ON' : 'OFF').padEnd(36)} â•‘
â•‘  â€¢ Max routes: ${PerformanceConfig.routing.maxRoutes.toString().padEnd(37)} â•‘
â•‘                                                            â•‘
â• â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•£
â•‘                                                            â•‘
â•‘  ðŸ’¡ Tips:                                                  â•‘
â•‘  â€¢ Utilisez window.app pour accÃ©der Ã  l'application       â•‘
â•‘  â€¢ Utilisez window.PerformanceConfig pour voir la config  â•‘
â•‘  â€¢ Pressez F12 pour ouvrir DevTools                       â•‘
â•‘                                                            â•‘
â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    `;
    
    console.log(info);
    
    // Ajouter Ã©galement des mÃ©tadonnÃ©es pour debug
    console.group('ðŸ“Š Performance Configuration Details');
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
        console.log('Backend Connected:', window.app.backend?.isConnected || false);
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
    }
};

// Afficher les utilitaires disponibles
console.log('ðŸ”§ Debug utilities available: window.debugUtils');
console.log('   â€¢ showAppState()');
console.log('   â€¢ showPerformanceStats()');
console.log('   â€¢ togglePerformanceMode()');
console.log('   â€¢ forceGC()');