// ============================================================================
// Fichier: frontend/js/core/EventBus.js
// Version: 3.0.5 - Phase 2 - Avec PrioritÃ©s
// Date: 2025-10-09
// ============================================================================
// Description:
//   Event Bus centralisÃ© avec systÃ¨me de prioritÃ©s pour optimisation latence.
//
// NouveautÃ©s Phase 2:
//   âœ… PrioritÃ©s d'Ã©vÃ©nements (HIGH/NORMAL/LOW)
//   âœ… Files d'attente sÃ©parÃ©es par prioritÃ©
//   âœ… Traitement asynchrone optimisÃ©
//   âœ… Throttling et debouncing intÃ©grÃ©s
//   âœ… MÃ©triques de performance
//
// Auteur: midiMind Team
// ============================================================================

/**
 * @enum EventPriority
 * @description Niveaux de prioritÃ© des Ã©vÃ©nements
 */
const EventPriority = {
    HIGH: 'high',       // TraitÃ© immÃ©diatement (ex: MIDI messages)
    NORMAL: 'normal',   // TraitÃ© normalement (ex: UI updates)
    LOW: 'low'          // TraitÃ© quand le systÃ¨me est libre (ex: stats)
};

/**
 * @class EventBus
 * @description Bus d'Ã©vÃ©nements centralisÃ© avec gestion de prioritÃ©s
 * 
 * Architecture Phase 2:
 * ```
 * emit(event, data, priority) â†’
 *   â†“
 * [Dispatcher avec prioritÃ©s]
 *   â†“
 * â”Œâ”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”
 * â”‚HIGH â”‚ NORMAL â”‚ LOW â”‚ (Files d'attente)
 * â””â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”˜
 *   â†“      â†“       â†“
 * Listeners (triÃ©s par prioritÃ©)
 * ```
 * 
 * Objectifs:
 * - Latence < 5ms pour Ã©vÃ©nements HIGH
 * - Latence < 20ms pour Ã©vÃ©nements NORMAL
 * - Latence < 100ms pour Ã©vÃ©nements LOW
 */
class EventBus {
    constructor() {
        // Listeners organisÃ©s par Ã©vÃ©nement
        this.listeners = new Map();
        
        // Files d'attente par prioritÃ©
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
        
        // MÃ©triques
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
        console.log('ðŸ”„ EventBus v3.0.5 initialized with priorities');
        
        // DÃ©marrer le traitement des queues
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
     * Enregistre un listener pour un Ã©vÃ©nement
     * @param {string} eventName - Nom de l'Ã©vÃ©nement
     * @param {Function} callback - Fonction Ã  appeler
     * @param {Object} options - Options (priority, once, throttle, debounce)
     * @returns {Function} Fonction pour se dÃ©sabonner
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
        
        // InsÃ©rer en respectant la prioritÃ©
        const listeners = this.listeners.get(eventName);
        const insertIndex = this.findInsertIndex(listeners, listener.priority);
        listeners.splice(insertIndex, 0, listener);
        
        // Retourner fonction de dÃ©sabonnement
        return () => this.off(eventName, listener.id);
    }
    
    /**
     * Enregistre un listener qui ne s'exÃ©cute qu'une fois
     */
    once(eventName, callback, options = {}) {
        return this.on(eventName, callback, { ...options, once: true });
    }
    
    /**
     * DÃ©senregistre un listener
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
     * DÃ©senregistre tous les listeners d'un Ã©vÃ©nement
     */
    offAll(eventName) {
        this.listeners.delete(eventName);
    }
    
    // ========================================================================
    // Ã‰MISSION D'Ã‰VÃ‰NEMENTS
    // ========================================================================
    
    /**
     * Ã‰met un Ã©vÃ©nement
     * @param {string} eventName - Nom de l'Ã©vÃ©nement
     * @param {*} data - DonnÃ©es de l'Ã©vÃ©nement
     * @param {string} priority - PrioritÃ© (HIGH/NORMAL/LOW)
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
            // Traiter immÃ©diatement les Ã©vÃ©nements HIGH
            this.processEventNow(event);
        } else {
            // Ajouter Ã  la queue appropriÃ©e
            this.enqueueEvent(event);
        }
    }
    
    /**
     * Ã‰met un Ã©vÃ©nement HIGH priority (immÃ©diat)
     */
    emitHigh(eventName, data = {}) {
        this.emit(eventName, data, EventPriority.HIGH);
    }
    
    /**
     * Ã‰met un Ã©vÃ©nement NORMAL priority
     */
    emitNormal(eventName, data = {}) {
        this.emit(eventName, data, EventPriority.NORMAL);
    }
    
    /**
     * Ã‰met un Ã©vÃ©nement LOW priority
     */
    emitLow(eventName, data = {}) {
        this.emit(eventName, data, EventPriority.LOW);
    }
    
    /**
     * Ã‰met avec throttling
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
     * Ã‰met avec debouncing
     */
    emitDebounced(eventName, data = {}, debounceMs = 300, priority = EventPriority.NORMAL) {
        const key = `${eventName}_${priority}`;
        
        // Annuler le timer prÃ©cÃ©dent
        if (this.debounceTimers.has(key)) {
            clearTimeout(this.debounceTimers.get(key));
        }
        
        // CrÃ©er nouveau timer
        const timer = setTimeout(() => {
            this.emit(eventName, data, priority);
            this.debounceTimers.delete(key);
        }, debounceMs);
        
        this.debounceTimers.set(key, timer);
    }
    
    // ========================================================================
    // TRAITEMENT DES Ã‰VÃ‰NEMENTS
    // ========================================================================
    
    /**
     * Ajoute un Ã©vÃ©nement Ã  la queue
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
     * Traite les queues dans l'ordre de prioritÃ©
     */
    processQueues() {
        // HIGH (dÃ©jÃ  traitÃ©s en direct)
        
        // NORMAL
		this.processQueue(EventPriority.NORMAL, 10);
		
		// LOW
		this.processQueue(EventPriority.LOW, 5);
		
		// âœ… NOUVEAU: Nettoyage pÃ©riodique du cache (toutes les 60s)
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
     * Traite un Ã©vÃ©nement immÃ©diatement
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
                // VÃ©rifier throttle
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
    // MÃ‰TRIQUES
    // ========================================================================
    
    /**
     * Met Ã  jour les mÃ©triques de latence
     */
    updateLatencyMetrics(priority, latency) {
        if (!this.config.enableMetrics) return;
        
        const history = this.metrics.latencyHistory[priority];
        history.push(latency);
        
        // Garder seulement les 100 derniÃ¨res
        if (history.length > 100) {
            history.shift();
        }
        
        // Calculer moyenne
        const sum = history.reduce((a, b) => a + b, 0);
        this.metrics.averageLatency[priority] = sum / history.length;
    }
    
    /**
     * RÃ©cupÃ¨re les mÃ©triques
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
     * RÃ©initialise les mÃ©triques
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
     * Trouve l'index d'insertion pour respecter la prioritÃ©
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
     * GÃ©nÃ¨re un ID unique pour listener
     */
    generateListenerId() {
        return 'listener_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
    }
    
    /**
     * GÃ©nÃ¨re un ID unique pour Ã©vÃ©nement
     */
    generateEventId() {
        return 'event_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
    }
    
    /**
     * Liste tous les Ã©vÃ©nements enregistrÃ©s
     */
    listEvents() {
        return Array.from(this.listeners.keys());
    }
    
    /**
     * Compte les listeners pour un Ã©vÃ©nement
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
	
	/**
     * Efface tous les événements
     */
    clearEvents() {
        this.listeners = {};
        this.wildcardListeners = [];
        this.maxListeners = 100;
        console.log('✅ All events cleared');
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { EventBus, EventPriority };
}

window.EventBus = EventBus;
