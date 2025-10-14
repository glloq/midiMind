// ============================================================================
// Fichier: frontend/js/core/EventBus.js
// Version: 3.0.5 - Phase 2 - Avec Priorités
// Date: 2025-10-09
// ============================================================================
// Description:
//   Event Bus centralisé avec système de priorités pour optimisation latence.
//
// Nouveautés Phase 2:
//   ✅ Priorités d'événements (HIGH/NORMAL/LOW)
//   ✅ Files d'attente séparées par priorité
//   ✅ Traitement asynchrone optimisé
//   ✅ Throttling et debouncing intégrés
//   ✅ Métriques de performance
//
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
		//
		this._lastCacheClean = null;
        
        this.init();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    init() {
        console.log('🔄 EventBus v3.0.5 initialized with priorities');
        
        // Démarrer le traitement des queues
        this.startProcessing();
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

// ============================================================================
// EXEMPLES D'UTILISATION
// ============================================================================

/*
// Événement HIGH (MIDI message) - traité immédiatement
eventBus.emitHigh('midi:message', {
    note: 60,
    velocity: 100
});

// Événement NORMAL (UI update)
eventBus.emitNormal('playback:position', {
    position: 1000
});

// Événement LOW (stats)
eventBus.emitLow('stats:update', {
    cpu: 45,
    ram: 60
});

// Listener avec priorité HIGH
eventBus.on('midi:message', (data) => {
    // Traité en premier
}, { priority: EventPriority.HIGH });

// Listener avec throttling (max 1 fois par 100ms)
eventBus.on('playback:position', (data) => {
    updateUI(data);
}, { throttle: 100 });

// Listener avec debouncing (attend 300ms de calme)
eventBus.on('search:query', (data) => {
    performSearch(data);
}, { debounce: 300 });

// Listener "once"
eventBus.once('app:ready', () => {
    console.log('App is ready!');
});
*/

// ============================================================================
// FIN DU FICHIER EventBus.js
// ============================================================================
