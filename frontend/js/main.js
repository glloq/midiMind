// ============================================================================
// Fichier: frontend/js/main.js
// Version: v3.0.1 - CORRECTED
// Date: 2025-10-10
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// CORRECTIONS v3.0.1:
// ✓ Fixed app.initialize is not a function
// ✓ Proper Application instantiation
// ✓ Error handling on initialization
// ✓ Loading indicator
// ============================================================================

// Attendre que le DOM soit chargé
document.addEventListener('DOMContentLoaded', async () => {
    console.log('🚀 Starting midiMind v3.0...');
    
    try {
        // Vérifier que Application est définie
        if (typeof Application === 'undefined') {
            throw new Error('Application class not loaded. Check index.html script order.');
        }
        
        // Créer l'instance de l'application
        const app = new Application();
        
        // Rendre app globale pour accès depuis la console et les autres scripts
        window.app = app;
        
        // Vérifier que la méthode initialize existe
        if (typeof app.init !== 'function') {
            throw new Error('Application.init() method not found');
        }
        
        // Afficher un indicateur de chargement
        showLoadingIndicator();
        
        // Initialiser l'application
        console.log('⚙️ Initializing application...');
        await app.init();
        
        // Masquer l'indicateur de chargement
        hideLoadingIndicator();
        
        console.log('✅ midiMind v3.0 initialized successfully');
        
        // Émettre un événement pour signaler que l'app est prête
        if (window.EventBus) {
            window.EventBus.emit('app:ready', { app });
        }
        
    } catch (error) {
        console.error('❌ Failed to initialize midiMind:', error);
        console.error('Stack trace:', error.stack);
        
        // Afficher une erreur à l'utilisateur
        showErrorMessage(error.message);
        
        // Ne pas bloquer complètement - permettre le debug
        console.log('Application failed but console remains available for debugging');
    }
});

/**
 * Affiche un indicateur de chargement
 */
function showLoadingIndicator() {
    const loading = document.getElementById('loading-indicator');
    if (loading) {
        loading.style.display = 'flex';
    } else {
        // Créer un indicateur si n'existe pas
        const indicator = document.createElement('div');
        indicator.id = 'loading-indicator';
        indicator.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.8);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 9999;
        `;
        indicator.innerHTML = `
            <div style="text-align: center; color: white;">
                <div class="spinner" style="
                    border: 4px solid #f3f3f3;
                    border-top: 4px solid #667eea;
                    border-radius: 50%;
                    width: 40px;
                    height: 40px;
                    animation: spin 1s linear infinite;
                    margin: 0 auto 20px;
                "></div>
                <p style="font-size: 18px;">Loading midiMind...</p>
            </div>
        `;
        document.body.appendChild(indicator);
        
        // Ajouter l'animation CSS si pas déjà présente
        if (!document.getElementById('spinner-style')) {
            const style = document.createElement('style');
            style.id = 'spinner-style';
            style.textContent = `
                @keyframes spin {
                    0% { transform: rotate(0deg); }
                    100% { transform: rotate(360deg); }
                }
            `;
            document.head.appendChild(style);
        }
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
 * Affiche un message d'erreur à l'utilisateur
 * @param {string} message - Message d'erreur
 */
function showErrorMessage(message) {
    hideLoadingIndicator();
    
    const errorDiv = document.createElement('div');
    errorDiv.style.cssText = `
        position: fixed;
        top: 50%;
        left: 50%;
        transform: translate(-50%, -50%);
        background: #ff4444;
        color: white;
        padding: 20px 30px;
        border-radius: 8px;
        box-shadow: 0 4px 20px rgba(0,0,0,0.3);
        z-index: 10000;
        max-width: 500px;
        text-align: center;
    `;
    errorDiv.innerHTML = `
        <h3 style="margin: 0 0 10px 0;">⚠️ Initialization Error</h3>
        <p style="margin: 0 0 15px 0;">${message}</p>
        <button onclick="location.reload()" style="
            background: white;
            color: #ff4444;
            border: none;
            padding: 10px 20px;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
        ">Reload Page</button>
        <p style="margin: 15px 0 0 0; font-size: 12px;">
            Check the console (F12) for more details
        </p>
    `;
    document.body.appendChild(errorDiv);
}

// ============================================================================
// GESTION DES ERREURS GLOBALES
// ============================================================================

/**
 * Capture les erreurs JavaScript non gérées
 */
window.addEventListener('error', (event) => {
    console.error('Uncaught error:', event.error);
    
    // Ne pas afficher d'erreur si l'app est déjà initialisée
    if (window.app && window.app.initialized) {
        return;
    }
    
    // Sinon, afficher l'erreur
    if (event.error) {
        showErrorMessage(`JavaScript Error: ${event.error.message}`);
    }
});

/**
 * Capture les promesses rejetées non gérées
 */
window.addEventListener('unhandledrejection', (event) => {
    console.error('Unhandled promise rejection:', event.reason);
    
    // Ne pas afficher d'erreur si l'app est déjà initialisée
    if (window.app && window.app.initialized) {
        return;
    }
    
    // Sinon, afficher l'erreur
    showErrorMessage(`Promise Rejection: ${event.reason}`);
});

// ============================================================================
// HELPERS DE DEBUG (disponibles dans la console)
// ============================================================================

/**
 * Fonctions utilitaires disponibles dans la console
 */
window.debug = {
    /**
     * Affiche l'état de l'application
     */
    appState() {
        if (!window.app) {
            console.log('❌ Application not initialized');
            return;
        }
        
        console.log('✅ Application State:', {
            initialized: window.app.initialized || false,
            controllers: Object.keys(window.app.controllers || {}),
            models: Object.keys(window.app.models || {}),
            views: Object.keys(window.app.views || {})
        });
    },
    
    /**
     * Liste tous les contrôleurs
     */
    controllers() {
        if (!window.app || !window.app.controllers) {
            console.log('❌ No controllers available');
            return;
        }
        
        console.table(Object.keys(window.app.controllers).map(name => ({
            name,
            type: typeof window.app.controllers[name],
            available: window.app.controllers[name] !== null
        })));
    },
    
    /**
     * Teste le chargement des classes de base
     */
    checkBase() {
        const classes = [
            'EventBus',
            'Logger',
            'BaseModel',
            'BaseView',
            'BaseController',
            'Application'
        ];
        
        console.log('Checking base classes...');
        classes.forEach(className => {
            const exists = typeof window[className] !== 'undefined';
            console.log(`${exists ? '✅' : '❌'} ${className}: ${typeof window[className]}`);
        });
    },
    
    /**
     * Recharge l'application
     */
    reload() {
        location.reload();
    },
    
    /**
     * Affiche l'aide
     */
    help() {
        console.log(`
🛠️ midiMind Debug Commands:

debug.appState()      - Show application state
debug.controllers()   - List all controllers  
debug.checkBase()     - Check if base classes loaded
debug.reload()        - Reload the application
debug.help()          - Show this help

Examples:
  debug.appState()
  debug.controllers()
        `);
    }
};

// Afficher un message de bienvenue dans la console
console.log(`
%c midiMind v3.0 
%c Système d'Orchestration MIDI 
%c Debug commands available: type 'debug.help()' 
`, 
'color: #667eea; font-size: 20px; font-weight: bold;',
'color: #764ba2; font-size: 14px;',
'color: #999; font-size: 12px;'
);

// ============================================================================
// FIN DU FICHIER main.js
// ============================================================================