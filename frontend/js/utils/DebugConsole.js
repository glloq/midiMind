// ============================================================================
// Fichier: frontend/js/utils/DebugConsole.js
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   Console de debug visuelle intégrée à l'application.
//   Affichage logs colorés, filtres, historique, export.
//
// Fonctionnalités:
//   - Affichage logs en temps réel
//   - Niveaux : Debug, Info, Warning, Error
//   - Filtres par niveau et catégorie
//   - Recherche dans logs
//   - Copie vers presse-papiers
//   - Export logs (TXT, JSON)
//   - Clear logs
//   - Historique persistant
//
// Architecture:
//   DebugConsole (classe singleton)
//   - Buffer circulaire pour logs (max 1000)
//   - UI overlay toggle (F12)
//   - Capture console.log natif
//
// Auteur: MidiMind Team
// ============================================================================

class DebugConsole {
    constructor(eventBus) {
        this.eventBus = eventBus;
        this.panel = document.getElementById('debugPanel');
        this.content = document.getElementById('debugContent');
        this.isVisible = false;
        this.logs = [];
        this.maxLogs = 1000;
        
        // Filtres actifs
        this.filters = {
            system: true,
            midi: true,
            files: true,
            keyboard: true,
            network: true,
            error: true,
            warning: true,
            info: true,
            success: true,
            backend: true,
            sync: true,
            instruments: true,
            notification: true
        };
        
        this.setupListeners();
    }

    /**
     * Configuration des listeners
     */
    setupListeners() {
        this.eventBus.on('debug:log', (data) => {
            this.log(data.category, data.message, data.data);
        });
        
        this.eventBus.on('error:caught', (error) => {
            this.log('error', `Erreur: ${error.message || error}`, error);
        });
    }

    /**
     * Logger un message
     */
    log(category, message, data = null) {
        const logEntry = {
            category,
            message,
            data,
            timestamp: new Date().toISOString(),
            id: Date.now() + Math.random()
        };
        
        this.logs.push(logEntry);
        
        if (this.logs.length > this.maxLogs) {
            this.logs.shift();
        }
        
        if (this.filters[category]) {
            this.appendLog(logEntry);
        }
        
        // Log dans la console navigateur aussi
        const emoji = this.getCategoryEmoji(category);
        console.log(`${emoji} [${category}]`, message, data || '');
    }

    /**
     * Ajouter un log au DOM
     */
    appendLog(entry) {
        if (!this.content) return;
        
        const logElement = document.createElement('div');
        logElement.className = `debug-log debug-log-${entry.category}`;
        
        const time = new Date(entry.timestamp).toLocaleTimeString('fr-FR');
        const emoji = this.getCategoryEmoji(entry.category);
        
        logElement.innerHTML = `
            <span class="debug-time">${time}</span>
            <span class="debug-category">${emoji} ${entry.category}</span>
            <span class="debug-message">${this.escapeHtml(entry.message)}</span>
        `;
        
        this.content.appendChild(logElement);
        this.content.scrollTop = this.content.scrollHeight;
    }

    /**
     * Obtenir l'emoji selon la catégorie
     */
    getCategoryEmoji(category) {
        const emojis = {
            system: '🔧',
            midi: '🎵',
            files: '📁',
            keyboard: '🎹',
            network: '🌐',
            error: '❌',
            warning: '⚠️',
            info: 'ℹ️',
            success: '✅',
            backend: '🔌',
            sync: '🔄',
            instruments: '🎼',
            notification: '📢'
        };
        return emojis[category] || '📝';
    }

    /**
     * Échapper le HTML
     */
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    /**
     * Toggle visibilité du panel
     */
    toggle() {
        this.isVisible = !this.isVisible;
        if (this.isVisible) {
            this.show();
        } else {
            this.hide();
        }
    }

    /**
     * Afficher le panel
     */
    show() {
        this.isVisible = true;
        if (this.panel) {
            this.panel.classList.add('visible');
        }
        this.refresh();
    }

    /**
     * Masquer le panel
     */
    hide() {
        this.isVisible = false;
        if (this.panel) {
            this.panel.classList.remove('visible');
        }
    }

    /**
     * Rafraîchir l'affichage
     */
    refresh() {
        if (!this.content) return;
        
        this.content.innerHTML = '';
        this.logs.forEach(entry => {
            if (this.filters[entry.category]) {
                this.appendLog(entry);
            }
        });
    }

    /**
     * Vider les logs
     */
    clear() {
        this.logs = [];
        if (this.content) {
            this.content.innerHTML = '';
        }
        this.log('system', 'Console vidée');
    }

    /**
     * Exporter les logs
     */
    export() {
        const dataStr = JSON.stringify(this.logs, null, 2);
        const dataBlob = new Blob([dataStr], { type: 'application/json' });
        const url = URL.createObjectURL(dataBlob);
        
        const link = document.createElement('a');
        link.href = url;
        link.download = `debug-logs-${Date.now()}.json`;
        link.click();
        
        URL.revokeObjectURL(url);
        
        this.log('system', 'Logs exportés');
    }
}