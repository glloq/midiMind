// ============================================================================
// Fichier: frontend/js/core/EventBus.js
// Chemin réel: frontend/js/core/EventBus.js
// Version: v3.3.1 - DETECTION BOUCLES D'EVENEMENTS
// Date: 2025-11-13
// ============================================================================
// CORRECTIONS v3.3.1:
// ✅ CRITIQUE: Détection d'événements émis en boucle (max 100/sec par type)
// ✅ Protection contre bombardement d'événements identiques
// ✅ Logs détaillés pour identifier les sources de boucles
//
// CORRECTIONS v3.3.0:
// ✅ CRITIQUE: Fix boucle infinie dans le traitement des événements HIGH priority
// ✅ Limitation du nombre d'événements traités par cycle (maxEventsPerCycle)
// ✅ Capture de la longueur de queue AVANT traitement pour éviter boucles infinies
// ✅ Protection contre saturation mémoire
//
// CORRECTIONS v3.2.0:
// ✓ CRITIQUE: Création automatique d'une instance globale
// ✓ Exposition immédiate dans window.eventBus
// ✓ Protection contre double initialisation
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
            processingInterval: 10,
            // ✅ NOUVEAU v3.3.0: Limiter le nombre d'événements traités par cycle
            // pour éviter les boucles infinies
            maxEventsPerCycle: 50,
            // Protection contre récursion infinie
            maxRecursionDepth: 10,
            // ✅ NOUVEAU v3.3.1: Protection contre bombardement d'événements
            maxEventsPerTypePerSecond: 100 // Max 100 événements du même type par seconde
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

        // ✅ NOUVEAU v3.3.0: Compteur de profondeur de récursion
        this._processingDepth = 0;
        this._eventCounter = 0;

        // ✅ NOUVEAU v3.3.1: Protection contre boucles d'événements
        this._eventCountMap = new Map(); // Compte combien de fois un événement est émis par seconde
        this._eventCountResetInterval = null; // Sera initialisé dans init()

        // Documentation des événements
        this.eventDocumentation = this.initEventDocumentation();

        this.init();
    }
    
    init() {
        // Démarrer le traitement des queues
        this.startProcessing();

        // Nettoyer les caches périodiquement
        setInterval(() => this.cleanCaches(), 60000);

        // ✅ NOUVEAU v3.3.1: Reset du compteur d'événements toutes les secondes
        this._eventCountResetInterval = setInterval(() => {
            this._eventCountMap.clear();
        }, 1000);

        console.log('✓ EventBus v3.3.1 initialized');
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

        // ✅ NOUVEAU v3.3.1: Détecter les boucles d'événements
        // Si un événement est émis plus de maxEventsPerTypePerSecond fois par seconde,
        // c'est probablement une boucle infinie
        const currentCount = this._eventCountMap.get(event) || 0;
        const maxAllowed = this.config.maxEventsPerTypePerSecond;

        if (currentCount >= maxAllowed) {
            this.metrics.eventsDropped++;
            console.error(
                `❌ EventBus: LOOP DETECTED! Event "${event}" was emitted ${currentCount} times in 1 second.` +
                ` This event is now blocked to prevent infinite loop and memory saturation.` +
                ` Check listeners for this event that might be emitting it recursively.`
            );
            // Log la stack trace pour aider au debug
            console.trace(`Stack trace for blocked event "${event}"`);
            return;
        }

        this._eventCountMap.set(event, currentCount + 1);
        this.metrics.eventsEmitted++;

        const eventData = {
            event,
            data,
            priority,
            timestamp: Date.now(),
            id: `evt_${Date.now()}_${Math.random()}`
        };

        // ✅ FIX v3.3.0: Ne JAMAIS traiter immédiatement les événements HIGH priority
        // pour éviter les boucles de récursion infinies.
        // Au lieu de cela, toujours les ajouter à la queue et laisser
        // le processeur de queue les gérer de manière contrôlée.
        const queue = this.queues[priority] || this.queues[EventPriority.NORMAL];

        if (queue.length >= this.config.maxQueueSize) {
            this.metrics.eventsDropped++;
            console.warn(`EventBus: Queue full for priority ${priority}, event dropped`);
            return;
        }

        queue.push(eventData);
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
            // ✅ FIX v3.3.0: Capturer la longueur de la queue AVANT le traitement
            // pour éviter les boucles infinies si de nouveaux événements sont ajoutés
            // pendant le traitement

            let eventsProcessedThisCycle = 0;
            const maxPerCycle = this.config.maxEventsPerCycle;

            // Traiter HIGH priority en premier
            // IMPORTANT: Capturer la longueur AVANT la boucle pour éviter boucle infinie
            const highQueueLength = Math.min(this.queues[EventPriority.HIGH].length, maxPerCycle);
            for (let i = 0; i < highQueueLength; i++) {
                if (eventsProcessedThisCycle >= maxPerCycle) break;
                if (this.queues[EventPriority.HIGH].length === 0) break;

                const eventData = this.queues[EventPriority.HIGH].shift();
                this.processEvent(eventData);
                eventsProcessedThisCycle++;
            }

            // Puis NORMAL (seulement si on n'a pas atteint la limite)
            if (eventsProcessedThisCycle < maxPerCycle && this.queues[EventPriority.NORMAL].length > 0) {
                const eventData = this.queues[EventPriority.NORMAL].shift();
                this.processEvent(eventData);
                eventsProcessedThisCycle++;
            }

            // Puis LOW (seulement si on n'a pas atteint la limite)
            if (eventsProcessedThisCycle < maxPerCycle && this.queues[EventPriority.LOW].length > 0) {
                const eventData = this.queues[EventPriority.LOW].shift();
                this.processEvent(eventData);
                eventsProcessedThisCycle++;
            }

            // ✅ NOUVEAU: Avertir si les queues sont saturées
            const totalQueueSize = this.queues[EventPriority.HIGH].length +
                                   this.queues[EventPriority.NORMAL].length +
                                   this.queues[EventPriority.LOW].length;

            if (totalQueueSize > this.config.maxQueueSize * 0.8) {
                console.warn(`EventBus: Queues are ${Math.round((totalQueueSize / this.config.maxQueueSize) * 100)}% full (${totalQueueSize} events)`);
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

        // ✅ NOUVEAU v3.3.1: Nettoyer l'interval de reset des compteurs
        if (this._eventCountResetInterval) {
            clearInterval(this._eventCountResetInterval);
            this._eventCountResetInterval = null;
        }

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
    
    // ✓ CRITIQUE: Créer immédiatement l'instance globale
    if (!window.eventBus) {
        window.eventBus = new EventBus();
        console.log('✓ Global EventBus instance created');
    }
}