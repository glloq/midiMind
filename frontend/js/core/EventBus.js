// ============================================================================
// Fichier: frontend/js/core/EventBus.js
// Chemin réel: frontend/js/core/EventBus.js
// Version: v3.2.0 - FIXED GLOBAL INITIALIZATION
// Date: 2025-10-31
// ============================================================================
// CORRECTIONS v3.2.0:
// ✅ CRITIQUE: Création automatique d'une instance globale
// ✅ Exposition immédiate dans window.eventBus
// ✅ Protection contre double initialisation
// ============================================================================

const EventPriority = {
    HIGH: 'high',
    NORMAL: 'normal',
    LOW: 'low'
};

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
            processingInterval: 10
        };
        
        // Métriques
        this.metrics = {
            eventsEmitted: 0,
            eventsProcessed: 0,
            eventsDropped: 0,
            averageLatency: { high: 0, normal: 0, low: 0 },
            latencyHistory: { high: [], normal: [], low: [] }
        };
        
        // Throttle cache
        this.throttleCache = new Map();
        
        // Debounce timers
        this.debounceTimers = new Map();
        
        // Traitement asynchrone
        this.processingTimer = null;
        this._lastCacheClean = null;
        
        // Documentation des événements
        this.eventDocumentation = this.initEventDocumentation();
        
        this.init();
    }
    
    init() {
        // Démarrer le traitement des queues
        this.startProcessing();
        
        // Nettoyer les caches périodiquement
        setInterval(() => this.cleanCaches(), 60000);
        
        console.log('✓ EventBus initialized');
    }
    
    // ========================================================================
    // MÉTHODES PRINCIPALES
    // ========================================================================
    
    on(event, callback, options = {}) {
        if (!event || typeof callback !== 'function') {
            console.error('EventBus.on: Invalid event or callback');
            return () => {};
        }
        
        if (!this.listeners.has(event)) {
            this.listeners.set(event, []);
        }
        
        const listener = {
            callback,
            priority: options.priority || EventPriority.NORMAL,
            once: options.once || false,
            throttle: options.throttle || 0,
            debounce: options.debounce || 0,
            context: options.context || null,
            filter: options.filter || null,
            id: `${event}_${Date.now()}_${Math.random()}`
        };
        
        this.listeners.get(event).push(listener);
        
        // Retourner fonction de désabonnement
        return () => this.off(event, callback);
    }
    
    once(event, callback, options = {}) {
        return this.on(event, callback, { ...options, once: true });
    }
    
    off(event, callback) {
        if (!this.listeners.has(event)) return;
        
        if (!callback) {
            // Retirer tous les listeners pour cet événement
            this.listeners.delete(event);
            return;
        }
        
        const listeners = this.listeners.get(event);
        const index = listeners.findIndex(l => l.callback === callback);
        
        if (index !== -1) {
            listeners.splice(index, 1);
        }
        
        if (listeners.length === 0) {
            this.listeners.delete(event);
        }
    }
    
    emit(event, data = {}, priority = EventPriority.NORMAL) {
        if (!event) {
            console.error('EventBus.emit: Event name required');
            return;
        }
        
        this.metrics.eventsEmitted++;
        
        const eventData = {
            event,
            data,
            priority,
            timestamp: Date.now(),
            id: `evt_${Date.now()}_${Math.random()}`
        };
        
        if (this.config.enablePriorities && priority === EventPriority.HIGH) {
            // Traiter immédiatement les événements HIGH priority
            this.processEvent(eventData);
        } else {
            // Ajouter à la queue appropriée
            const queue = this.queues[priority] || this.queues[EventPriority.NORMAL];
            
            if (queue.length >= this.config.maxQueueSize) {
                this.metrics.eventsDropped++;
                console.warn(`EventBus: Queue full for priority ${priority}, event dropped`);
                return;
            }
            
            queue.push(eventData);
        }
    }
    
    // ========================================================================
    // TRAITEMENT
    // ========================================================================
    
    processEvent(eventData) {
        const { event, data, timestamp, priority } = eventData;
        
        if (!this.listeners.has(event)) return;
        
        const listeners = this.listeners.get(event);
        const toRemove = [];
        
        for (let i = 0; i < listeners.length; i++) {
            const listener = listeners[i];
            
            try {
                // Filtrage
                if (listener.filter && !listener.filter(data)) {
                    continue;
                }
                
                // Throttling
                if (listener.throttle > 0) {
                    const key = `${event}_${listener.id}`;
                    const lastExec = this.throttleCache.get(key) || 0;
                    const now = Date.now();
                    
                    if (now - lastExec < listener.throttle) {
                        continue;
                    }
                    
                    this.throttleCache.set(key, now);
                }
                
                // Debouncing
                if (listener.debounce > 0) {
                    const key = `${event}_${listener.id}`;
                    
                    if (this.debounceTimers.has(key)) {
                        clearTimeout(this.debounceTimers.get(key));
                    }
                    
                    const timer = setTimeout(() => {
                        this.executeCallback(listener, data);
                        this.debounceTimers.delete(key);
                    }, listener.debounce);
                    
                    this.debounceTimers.set(key, timer);
                    continue;
                }
                
                // Exécution normale
                this.executeCallback(listener, data);
                
                // Marquer pour suppression si once
                if (listener.once) {
                    toRemove.push(i);
                }
                
            } catch (error) {
                console.error(`EventBus: Error in listener for ${event}:`, error);
            }
        }
        
        // Supprimer les listeners "once"
        for (let i = toRemove.length - 1; i >= 0; i--) {
            listeners.splice(toRemove[i], 1);
        }
        
        if (listeners.length === 0) {
            this.listeners.delete(event);
        }
        
        // Métriques
        this.metrics.eventsProcessed++;
        
        if (this.config.enableMetrics && priority) {
            const latency = Date.now() - timestamp;
            this.updateLatencyMetrics(priority, latency);
        }
    }
    
    executeCallback(listener, data) {
        if (listener.context) {
            listener.callback.call(listener.context, data);
        } else {
            listener.callback(data);
        }
    }
    
    startProcessing() {
        if (this.processingTimer) return;
        
        this.processingTimer = setInterval(() => {
            // Traiter HIGH priority en premier
            while (this.queues[EventPriority.HIGH].length > 0) {
                const eventData = this.queues[EventPriority.HIGH].shift();
                this.processEvent(eventData);
            }
            
            // Puis NORMAL
            if (this.queues[EventPriority.NORMAL].length > 0) {
                const eventData = this.queues[EventPriority.NORMAL].shift();
                this.processEvent(eventData);
            }
            
            // Puis LOW
            if (this.queues[EventPriority.LOW].length > 0) {
                const eventData = this.queues[EventPriority.LOW].shift();
                this.processEvent(eventData);
            }
        }, this.config.processingInterval);
    }
    
    stopProcessing() {
        if (this.processingTimer) {
            clearInterval(this.processingTimer);
            this.processingTimer = null;
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    updateLatencyMetrics(priority, latency) {
        const history = this.metrics.latencyHistory[priority];
        history.push(latency);
        
        if (history.length > 100) {
            history.shift();
        }
        
        const avg = history.reduce((a, b) => a + b, 0) / history.length;
        this.metrics.averageLatency[priority] = Math.round(avg * 100) / 100;
    }
    
    cleanCaches() {
        const now = Date.now();
        
        // Nettoyer throttle cache (garder 5 dernières secondes)
        for (const [key, timestamp] of this.throttleCache.entries()) {
            if (now - timestamp > 5000) {
                this.throttleCache.delete(key);
            }
        }
        
        this._lastCacheClean = now;
    }
    
    getMetrics() {
        return { ...this.metrics };
    }
    
    getListenerCount(event = null) {
        if (event) {
            return this.listeners.has(event) ? this.listeners.get(event).length : 0;
        }
        
        let total = 0;
        for (const listeners of this.listeners.values()) {
            total += listeners.length;
        }
        return total;
    }
    
    clear() {
        this.listeners.clear();
        this.queues[EventPriority.HIGH] = [];
        this.queues[EventPriority.NORMAL] = [];
        this.queues[EventPriority.LOW] = [];
        this.throttleCache.clear();
        
        for (const timer of this.debounceTimers.values()) {
            clearTimeout(timer);
        }
        this.debounceTimers.clear();
    }
    
    destroy() {
        this.stopProcessing();
        this.clear();
    }
    
    initEventDocumentation() {
        return {
            // Device events
            'device:connected': 'Device connected',
            'device:disconnected': 'Device disconnected',
            'device:list': 'Device list updated',
            'device:error': 'Device error occurred',
            
            // MIDI events
            'midi:note-on': 'MIDI note on received',
            'midi:note-off': 'MIDI note off received',
            'midi:cc': 'MIDI control change received',
            'midi:message': 'Generic MIDI message received',
            
            // Playback events
            'playback:play': 'Playback started',
            'playback:pause': 'Playback paused',
            'playback:stop': 'Playback stopped',
            'playback:time': 'Playback time updated',
            
            // System events
            'app:ready': 'Application ready',
            'app:error': 'Application error',
            'backend:connected': 'Backend connected',
            'backend:disconnected': 'Backend disconnected'
        };
    }
}

// ============================================================================
// EXPORT & INITIALISATION GLOBALE
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = { EventBus, EventPriority };
}

if (typeof window !== 'undefined') {
    window.EventBus = EventBus;
    window.EventPriority = EventPriority;
    
    // ✅ CRITIQUE: Créer immédiatement l'instance globale
    if (!window.eventBus) {
        window.eventBus = new EventBus();
        console.log('✓ Global EventBus instance created');
    }
}