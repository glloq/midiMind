// ============================================================================
// Fichier: frontend/js/core/EventBus.js
// Version: 3.1.0 - Enrichi avec nouveaux événements API
// Date: 2025-10-28
// ============================================================================
// Auteur: midiMind Team
// ============================================================================

/**
 * @enum EventPriority
 * @description Niveaux de priorité des événements
 */
const EventPriority = {
    HIGH: 'high',       // Traité immédiatement (ex: MIDI messages)
    NORMAL: 'normal',   // Traité normalement (ex: UI updates)
    LOW: 'low'          // Traité quand le système est libre (ex: stats)
};

/**
 * @class EventBus
 * @description Bus d'événements centralisé avec gestion de priorités
 * 
 * NOUVEAUX ÉVÉNEMENTS v3.1.0:
 * - bluetooth:* (scan, paired, unpaired, signal)
 * - hotplug:* (device-added, device-removed, monitoring-started, monitoring-stopped)
 * - latency:* (updated, calibration-started, calibration-complete, enabled, disabled)
 * - preset:* (loaded, saved, deleted, exported)
 * - logger:* (level-changed)
 * - network:* (status-changed, interface-up, interface-down)
 * 
 * Architecture Phase 2:
 * ```
 * emit(event, data, priority) →
 *   ↓
 * [Dispatcher avec priorités]
 *   ↓
 * ┌─────┬────────┬─────┐
 * │HIGH │ NORMAL │ LOW │ (Files d'attente)
 * └─────┴────────┴─────┘
 *   ↓      ↓       ↓
 * Listeners (triés par priorité)
 * ```
 * 
 * Objectifs:
 * - Latence < 5ms pour événements HIGH
 * - Latence < 20ms pour événements NORMAL
 * - Latence < 100ms pour événements LOW
 */
class EventBus {
    constructor() {
        // Listeners organisés par événement
        this.listeners = new Map();
        
        // Files d'attente par priorité
        this.queues = {
            [EventPriority.HIGH]: [],
            [EventPriority.NORMAL]: [],
            [EventPriority.LOW]: []
        };
        
        // Configuration
        this.config = {
            enablePriorities: true,
            enableMetrics: true,
            enableThrottling: true,
            maxQueueSize: 1000,
            processingInterval: 10  // ms
        };
        
        // Métriques
        this.metrics = {
            eventsEmitted: 0,
            eventsProcessed: 0,
            eventsDropped: 0,
            averageLatency: {
                high: 0,
                normal: 0,
                low: 0
            },
            latencyHistory: {
                high: [],
                normal: [],
                low: []
            }
        };
        
        // Throttle cache
        this.throttleCache = new Map();
        
        // Debounce timers
        this.debounceTimers = new Map();
        
        // Traitement asynchrone
        this.processingTimer = null;
        this._lastCacheClean = null;
        
        // Documentation des événements disponibles
        this.eventDocumentation = this.initEventDocumentation();
        
        this.init();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    init() {
        console.log('🔄 EventBus v3.1.0 initialized with priorities and new API events');
        
        // Démarrer le traitement des queues
        this.startProcessing();
    }
    
    /**
     * Initialise la documentation des événements disponibles
     */
    initEventDocumentation() {
        return {
            // Événements existants
            'app:ready': 'Application prête',
            'app:error': 'Erreur application',
            'backend:connected': 'Backend connecté',
            'backend:disconnected': 'Backend déconnecté',
            'websocket:disconnected': 'WebSocket déconnecté',
            'navigation:changed': 'Navigation changée',
            
            // Événements périphériques
            'device:connected': 'Périphérique connecté',
            'device:disconnected': 'Périphérique déconnecté',
            'device:scan': 'Scan de périphériques',
            
            // Événements MIDI
            'midi:message': 'Message MIDI reçu',
            'midi:note-on': 'Note MIDI activée',
            'midi:note-off': 'Note MIDI désactivée',
            
            // Événements de routing
            'routing:route-added': 'Route ajoutée',
            'routing:route-removed': 'Route retirée',
            'routing:routes-cleared': 'Routes effacées',
            
            // Événements de playback
            'playback:playing': 'Lecture en cours',
            'playback:paused': 'Lecture en pause',
            'playback:stopped': 'Lecture arrêtée',
            'playback:position': 'Position de lecture',
            
            // NOUVEAUX ÉVÉNEMENTS BLUETOOTH
            'bluetooth:scan-started': 'Scan Bluetooth démarré',
            'bluetooth:scan-complete': 'Scan Bluetooth terminé',
            'bluetooth:device-found': 'Périphérique Bluetooth trouvé',
            'bluetooth:paired': 'Périphérique Bluetooth apparié',
            'bluetooth:unpaired': 'Périphérique Bluetooth désapparié',
            'bluetooth:signal-update': 'Mise à jour du signal Bluetooth',
            'bluetooth:signal-weak': 'Signal Bluetooth faible',
            'bluetooth:error': 'Erreur Bluetooth',
            
            // NOUVEAUX ÉVÉNEMENTS HOT-PLUG
            'hotplug:device-added': 'Périphérique ajouté (hot-plug)',
            'hotplug:device-removed': 'Périphérique retiré (hot-plug)',
            'hotplug:monitoring-started': 'Surveillance hot-plug démarrée',
            'hotplug:monitoring-stopped': 'Surveillance hot-plug arrêtée',
            'hotplug:status-update': 'Mise à jour statut hot-plug',
            
            // NOUVEAUX ÉVÉNEMENTS LATENCE
            'latency:updated': 'Latence mise à jour',
            'latency:calibration-started': 'Calibration de latence démarrée',
            'latency:calibration-progress': 'Progression calibration latence',
            'latency:calibration-complete': 'Calibration de latence terminée',
            'latency:calibration-failed': 'Échec calibration latence',
            'latency:compensation-enabled': 'Compensation de latence activée',
            'latency:compensation-disabled': 'Compensation de latence désactivée',
            'latency:offset-changed': 'Offset de latence changé',
            
            // NOUVEAUX ÉVÉNEMENTS PRESET
            'preset:loaded': 'Preset chargé',
            'preset:saved': 'Preset sauvegardé',
            'preset:deleted': 'Preset supprimé',
            'preset:exported': 'Preset exporté',
            'preset:imported': 'Preset importé',
            'preset:list-updated': 'Liste des presets mise à jour',
            'preset:error': 'Erreur preset',
            
            // NOUVEAUX ÉVÉNEMENTS LOGGER
            'logger:level-changed': 'Niveau de log changé',
            'logger:message': 'Message de log',
            'logger:error': 'Erreur de log',
            
            // NOUVEAUX ÉVÉNEMENTS NETWORK
            'network:status-changed': 'Statut réseau changé',
            'network:online': 'Réseau en ligne',
            'network:offline': 'Réseau hors ligne',
            'network:interface-up': 'Interface réseau activée',
            'network:interface-down': 'Interface réseau désactivée',
            'network:stats-updated': 'Statistiques réseau mises à jour',
            'network:error': 'Erreur réseau'
        };
    }
    
    startProcessing() {
        if (this.processingTimer) return;
        
        this.processingTimer = setInterval(() => {
            this.processQueues();
        }, this.config.processingInterval);
    }
    
    stopProcessing() {
        if (this.processingTimer) {
            clearInterval(this.processingTimer);
            this.processingTimer = null;
        }
    }
    
    // ========================================================================
    // ENREGISTREMENT DE LISTENERS
    // ========================================================================
    
    /**
     * Enregistre un listener pour un événement
     * @param {string} eventName - Nom de l'événement
     * @param {Function} callback - Fonction à appeler
     * @param {Object} options - Options (priority, once, throttle, debounce)
     * @returns {Function} Fonction pour se désabonner
     */
    on(eventName, callback, options = {}) {
        if (!this.listeners.has(eventName)) {
            this.listeners.set(eventName, []);
        }
        
        const listener = {
            callback,
            priority: options.priority || EventPriority.NORMAL,
            once: options.once || false,
            throttle: options.throttle || 0,
            debounce: options.debounce || 0,
            id: this.generateListenerId()
        };
        
        // Insérer en respectant la priorité
        const listeners = this.listeners.get(eventName);
        const insertIndex = this.findInsertIndex(listeners, listener.priority);
        listeners.splice(insertIndex, 0, listener);
        
        // Retourner fonction de désabonnement
        return () => this.off(eventName, listener.id);
    }
    
    /**
     * Enregistre un listener qui ne s'exécute qu'une fois
     */
    once(eventName, callback, options = {}) {
        return this.on(eventName, callback, { ...options, once: true });
    }
    
    /**
     * Désenregistre un listener
     */
    off(eventName, listenerId) {
        if (!this.listeners.has(eventName)) return;
        
        const listeners = this.listeners.get(eventName);
        const index = listeners.findIndex(l => l.id === listenerId);
        
        if (index !== -1) {
            listeners.splice(index, 1);
        }
        
        // Nettoyer si plus de listeners
        if (listeners.length === 0) {
            this.listeners.delete(eventName);
        }
    }
    
    /**
     * Désenregistre tous les listeners d'un événement
     */
    offAll(eventName) {
        this.listeners.delete(eventName);
    }
    
    // ========================================================================
    // ÉMISSION D'ÉVÉNEMENTS
    // ========================================================================
    
    /**
     * Émet un événement
     * @param {string} eventName - Nom de l'événement
     * @param {*} data - Données de l'événement
     * @param {string} priority - Priorité (HIGH/NORMAL/LOW)
     */
    emit(eventName, data = {}, priority = EventPriority.NORMAL) {
        this.metrics.eventsEmitted++;
        
        const event = {
            name: eventName,
            data,
            priority,
            timestamp: performance.now(),
            id: this.generateEventId()
        };
        
        if (!this.config.enablePriorities || priority === EventPriority.HIGH) {
            // Traiter immédiatement les événements HIGH
            this.processEventNow(event);
        } else {
            // Ajouter à la queue appropriée
            this.enqueueEvent(event);
        }
    }
    
    /**
     * Émet un événement HIGH priority (immédiat)
     */
    emitHigh(eventName, data = {}) {
        this.emit(eventName, data, EventPriority.HIGH);
    }
    
    /**
     * Émet un événement NORMAL priority
     */
    emitNormal(eventName, data = {}) {
        this.emit(eventName, data, EventPriority.NORMAL);
    }
    
    /**
     * Émet un événement LOW priority
     */
    emitLow(eventName, data = {}) {
        this.emit(eventName, data, EventPriority.LOW);
    }
    
    /**
     * Émet avec throttling
     */
    emitThrottled(eventName, data = {}, throttleMs = 100, priority = EventPriority.NORMAL) {
        const key = `${eventName}_${priority}`;
        const now = performance.now();
        const lastEmit = this.throttleCache.get(key) || 0;
        
        if (now - lastEmit >= throttleMs) {
            this.emit(eventName, data, priority);
            this.throttleCache.set(key, now);
        }
    }
    
    /**
     * Émet avec debouncing
     */
    emitDebounced(eventName, data = {}, debounceMs = 300, priority = EventPriority.NORMAL) {
        const key = `${eventName}_${priority}`;
        
        // Annuler le timer précédent
        if (this.debounceTimers.has(key)) {
            clearTimeout(this.debounceTimers.get(key));
        }
        
        // Créer nouveau timer
        const timer = setTimeout(() => {
            this.emit(eventName, data, priority);
            this.debounceTimers.delete(key);
        }, debounceMs);
        
        this.debounceTimers.set(key, timer);
    }
    
    // ========================================================================
    // TRAITEMENT DES ÉVÉNEMENTS
    // ========================================================================
    
    /**
     * Ajoute un événement à la queue
     */
    enqueueEvent(event) {
        const queue = this.queues[event.priority];
        
        if (queue.length >= this.config.maxQueueSize) {
            this.metrics.eventsDropped++;
            console.warn(`EventBus: Queue ${event.priority} full, event dropped:`, event.name);
            return;
        }
        
        queue.push(event);
    }
    
    /**
     * Traite les queues dans l'ordre de priorité
     */
    processQueues() {
        // HIGH (déjà traités en direct)
        
        // NORMAL
        this.processQueue(EventPriority.NORMAL, 10);
        
        // LOW
        this.processQueue(EventPriority.LOW, 5);
        
        // ✅ NOUVEAU: Nettoyage périodique du cache (toutes les 60s)
        const now = Date.now();
        if (!this._lastCacheClean || now - this._lastCacheClean > 60000) {
            this.cleanThrottleCache();
            this._lastCacheClean = now;
        }
    }
    
    /**
     * Traite une queue
     */
    processQueue(priority, maxEvents) {
        const queue = this.queues[priority];
        const toProcess = Math.min(queue.length, maxEvents);
        
        for (let i = 0; i < toProcess; i++) {
            const event = queue.shift();
            if (event) {
                this.processEventNow(event);
            }
        }
    }
    
    /**
     * Traite un événement immédiatement
     */
    processEventNow(event) {
        const startTime = performance.now();
        
        if (!this.listeners.has(event.name)) {
            return;
        }
        
        const listeners = this.listeners.get(event.name);
        const listenersToRemove = [];
        
        for (const listener of listeners) {
            try {
                // Vérifier throttle
                if (listener.throttle > 0) {
                    const key = `${event.name}_${listener.id}`;
                    const lastCall = this.throttleCache.get(key) || 0;
                    const now = performance.now();
                    
                    if (now - lastCall < listener.throttle) {
                        continue;  // Skip ce listener
                    }
                    
                    this.throttleCache.set(key, now);
                }
                
                // Appeler le callback
                listener.callback(event.data, event);
                
                // Marquer pour suppression si "once"
                if (listener.once) {
                    listenersToRemove.push(listener.id);
                }
                
            } catch (error) {
                console.error(`EventBus: Error in listener for ${event.name}:`, error);
            }
        }
        
        // Supprimer les listeners "once"
        listenersToRemove.forEach(id => this.off(event.name, id));
        
        // Mesurer latence
        const latency = performance.now() - startTime;
        this.updateLatencyMetrics(event.priority, latency);
        
        this.metrics.eventsProcessed++;
    }
    
    // ========================================================================
    // MÉTRIQUES
    // ========================================================================
    
    /**
     * Met à jour les métriques de latence
     */
    updateLatencyMetrics(priority, latency) {
        if (!this.config.enableMetrics) return;
        
        const history = this.metrics.latencyHistory[priority];
        history.push(latency);
        
        // Garder seulement les 100 dernières
        if (history.length > 100) {
            history.shift();
        }
        
        // Calculer moyenne
        const sum = history.reduce((a, b) => a + b, 0);
        this.metrics.averageLatency[priority] = sum / history.length;
    }
    
    /**
     * Récupère les métriques
     */
    getMetrics() {
        return {
            ...this.metrics,
            queueSizes: {
                high: this.queues[EventPriority.HIGH].length,
                normal: this.queues[EventPriority.NORMAL].length,
                low: this.queues[EventPriority.LOW].length
            },
            listenerCount: Array.from(this.listeners.values())
                .reduce((sum, arr) => sum + arr.length, 0)
        };
    }
    
    /**
     * Réinitialise les métriques
     */
    resetMetrics() {
        this.metrics = {
            eventsEmitted: 0,
            eventsProcessed: 0,
            eventsDropped: 0,
            averageLatency: { high: 0, normal: 0, low: 0 },
            latencyHistory: { high: [], normal: [], low: [] }
        };
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    cleanThrottleCache() {
        const now = performance.now();
        const maxAge = 60000; // 1 minute
        
        for (const [key, timestamp] of this.throttleCache.entries()) {
            if (now - timestamp > maxAge) {
                this.throttleCache.delete(key);
            }
        }
    }
    
    /**
     * Trouve l'index d'insertion pour respecter la priorité
     */
    findInsertIndex(listeners, priority) {
        const priorityOrder = {
            [EventPriority.HIGH]: 0,
            [EventPriority.NORMAL]: 1,
            [EventPriority.LOW]: 2
        };
        
        for (let i = 0; i < listeners.length; i++) {
            if (priorityOrder[priority] < priorityOrder[listeners[i].priority]) {
                return i;
            }
        }
        
        return listeners.length;
    }
    
    /**
     * Génère un ID unique pour listener
     */
    generateListenerId() {
        return 'listener_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
    }
    
    /**
     * Génère un ID unique pour événement
     */
    generateEventId() {
        return 'event_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
    }
    
    /**
     * Liste tous les événements enregistrés
     */
    listEvents() {
        return Array.from(this.listeners.keys());
    }
    
    /**
     * Liste tous les événements disponibles avec documentation
     */
    listAvailableEvents() {
        return this.eventDocumentation;
    }
    
    /**
     * Compte les listeners pour un événement
     */
    listenerCount(eventName) {
        if (!this.listeners.has(eventName)) return 0;
        return this.listeners.get(eventName).length;
    }
    
    /**
     * Nettoie les ressources
     */
    destroy() {
        this.stopProcessing();
        this.listeners.clear();
        this.throttleCache.clear();
        this.debounceTimers.forEach(timer => clearTimeout(timer));
        this.debounceTimers.clear();
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { EventBus, EventPriority };
}

window.EventBus = EventBus;
window.EventPriority = EventPriority;

// ============================================================================
// FIN DU FICHIER EventBus.js v3.1.0
// ============================================================================