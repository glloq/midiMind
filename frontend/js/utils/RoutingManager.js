// ============================================================================
// Fichier: frontend/scripts/utils/RoutingManager.js
// Version: v3.2 Advanced
// Projet: midiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// Description:
//   Gestion avancée du routage MIDI avec support de topologies complexes
//   - 1→1 : Un canal vers un instrument (standard)
//   - 1→N : Un canal vers plusieurs instruments (split/layer)
//   - N→1 : Plusieurs canaux vers un instrument (merge)
//   - N→M : Plusieurs canaux vers plusieurs instruments (complexe)
// ============================================================================

class RoutingManager {
    constructor(eventBus, debugConsole) {
        this.eventBus = eventBus;
        this.debugConsole = debugConsole;
        
        // Configuration du routage - STRUCTURE AMÉLIORÉE
        this.routing = {
            // Mode de routage
            mode: 'manual',          // 'manual', 'auto', 'preset'
            currentPreset: null,
            
            // Routes individuelles (structure flexible)
            // Format: Map<routeId, Route>
            routes: new Map(),
            
            // Index rapides pour recherche
            channelToRoutes: new Map(),    // channel → Set<routeId>
            instrumentToRoutes: new Map()  // instrumentId → Set<routeId>
        };
        
        // Données
        this.midiChannels = [];      // Canaux du fichier MIDI
        this.instruments = [];        // Instruments disponibles
        this.presets = [];           // Presets de routage
        
        // État
        this.isValid = false;
        this.conflicts = [];
        
        // Statistiques
        this.stats = {
            totalChannels: 0,
            assignedChannels: 0,
            unassignedChannels: 0,
            totalRoutes: 0,
            oneToOne: 0,      // Routes 1→1
            oneToMany: 0,     // Routes 1→N
            manyToOne: 0,     // Routes N→1
            manyToMany: 0,    // Routes N→M
            compatibilityScore: 0
        };
        
        // Types de routage
        this.ROUTE_TYPES = {
            ONE_TO_ONE: '1→1',
            ONE_TO_MANY: '1→N',
            MANY_TO_ONE: 'N→1',
            MANY_TO_MANY: 'N→M'
        };
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    /**
     * Initialise le routage pour un fichier MIDI
     */
    initialize(midiData, instruments) {
        console.log('[RoutingManager] Initializing advanced routing...');
        
        this.midiChannels = this.extractChannels(midiData);
        this.instruments = instruments || [];
        
        // Charger les presets
        this.loadPresets();
        
        // Tentative de routage automatique
        if (this.routing.mode === 'auto') {
            this.autoRoute();
        }
        
        // Valider
        this.validate();
        
        // Calculer les stats
        this.updateStats();
        
        console.log('[RoutingManager] Routing initialized:', this.stats);
        
        this.eventBus.emit('routing:initialized', {
            channels: this.midiChannels.length,
            instruments: this.instruments.length,
            stats: this.stats
        });
    }

    /**
     * Extrait les canaux d'un fichier MIDI
     */
    extractChannels(midiData) {
        if (!midiData || !midiData.timeline) return [];
        
        const channelMap = new Map();
        
        midiData.timeline.forEach(event => {
            if (event.type === 'noteOn') {
                const channel = event.channel;
                
                if (!channelMap.has(channel)) {
                    channelMap.set(channel, {
                        number: channel,
                        name: `Channel ${channel + 1}`,
                        instrument: event.instrument || 'Unknown',
                        program: event.program || 0,
                        noteCount: 0,
                        noteRange: { min: 127, max: 0 },
                        velocity: { min: 127, max: 0, avg: 0 }
                    });
                }
                
                const info = channelMap.get(channel);
                info.noteCount++;
                info.noteRange.min = Math.min(info.noteRange.min, event.note);
                info.noteRange.max = Math.max(info.noteRange.max, event.note);
                info.velocity.min = Math.min(info.velocity.min, event.velocity);
                info.velocity.max = Math.max(info.velocity.max, event.velocity);
            }
        });
        
        // Calculer moyennes
        channelMap.forEach(info => {
            info.velocity.avg = Math.round((info.velocity.min + info.velocity.max) / 2);
        });
        
        return Array.from(channelMap.values()).sort((a, b) => a.number - b.number);
    }

    // ========================================================================
    // GESTION DES ROUTES - API AVANCÉE
    // ========================================================================

    /**
     * Crée une route de routage
     * @param {Array|number} sources - Canal(aux) source(s)
     * @param {Array|string} destinations - Instrument(s) destination(s)
     * @param {Object} options - Options de la route
     */
    createRoute(sources, destinations, options = {}) {
        // Normaliser en tableaux
        const sourceChannels = Array.isArray(sources) ? sources : [sources];
        const destInstruments = Array.isArray(destinations) ? destinations : [destinations];
        
        // Validation
        if (sourceChannels.length === 0 || destInstruments.length === 0) {
            console.warn('[RoutingManager] Invalid route: empty sources or destinations');
            return null;
        }
        
        // Vérifier que les canaux existent
        for (const channel of sourceChannels) {
            if (!this.midiChannels.find(c => c.number === channel)) {
                console.warn(`[RoutingManager] Invalid channel: ${channel}`);
                return null;
            }
        }
        
        // Vérifier que les instruments existent
        for (const instId of destInstruments) {
            if (!this.instruments.find(i => i.id === instId)) {
                console.warn(`[RoutingManager] Invalid instrument: ${instId}`);
                return null;
            }
        }
        
        // Créer la route
        const routeId = `route_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
        
        const route = {
            id: routeId,
            sources: sourceChannels,
            destinations: destInstruments,
            type: this.determineRouteType(sourceChannels.length, destInstruments.length),
            enabled: options.enabled !== false,
            priority: options.priority || 0,
            
            // Filtres optionnels
            filters: {
                noteRange: options.noteRange || null,      // { min, max }
                velocityRange: options.velocityRange || null,
                messageTypes: options.messageTypes || null  // ['noteOn', 'noteOff', 'cc']
            },
            
            // Transformations optionnelles
            transforms: {
                transpose: options.transpose || 0,
                velocityScale: options.velocityScale || 1.0,
                channelRemap: options.channelRemap || null
            },
            
            // Métadonnées
            name: options.name || this.generateRouteName(sourceChannels, destInstruments),
            compatibility: this.calculateRouteCompatibility(sourceChannels, destInstruments),
            created: Date.now(),
            lastModified: Date.now()
        };
        
        // Ajouter aux maps
        this.routing.routes.set(routeId, route);
        
        // Mettre à jour les index
        sourceChannels.forEach(channel => {
            if (!this.routing.channelToRoutes.has(channel)) {
                this.routing.channelToRoutes.set(channel, new Set());
            }
            this.routing.channelToRoutes.get(channel).add(routeId);
        });
        
        destInstruments.forEach(instId => {
            if (!this.routing.instrumentToRoutes.has(instId)) {
                this.routing.instrumentToRoutes.set(instId, new Set());
            }
            this.routing.instrumentToRoutes.get(instId).add(routeId);
        });
        
        // Mettre à jour
        this.validate();
        this.updateStats();
        
        console.log(`[RoutingManager] Created route ${route.type}: ${route.name}`);
        
        this.eventBus.emit('routing:route-created', { route });
        
        return route;
    }

    /**
     * Détermine le type de route
     */
    determineRouteType(sourceCount, destCount) {
        if (sourceCount === 1 && destCount === 1) {
            return this.ROUTE_TYPES.ONE_TO_ONE;
        } else if (sourceCount === 1 && destCount > 1) {
            return this.ROUTE_TYPES.ONE_TO_MANY;
        } else if (sourceCount > 1 && destCount === 1) {
            return this.ROUTE_TYPES.MANY_TO_ONE;
        } else {
            return this.ROUTE_TYPES.MANY_TO_MANY;
        }
    }

    /**
     * Génère un nom de route
     */
    generateRouteName(sources, destinations) {
        const srcStr = sources.length === 1 
            ? `CH${sources[0] + 1}` 
            : `CH[${sources.map(c => c + 1).join(',')}]`;
        
        const destStr = destinations.length === 1
            ? this.instruments.find(i => i.id === destinations[0])?.name || destinations[0]
            : `${destinations.length} instruments`;
        
        return `${srcStr} → ${destStr}`;
    }

    /**
     * Calcule la compatibilité d'une route
     */
    calculateRouteCompatibility(sources, destinations) {
        let totalScore = 0;
        let count = 0;
        
        // Calculer compatibilité pour chaque paire canal-instrument
        sources.forEach(channel => {
            const channelInfo = this.midiChannels.find(c => c.number === channel);
            if (!channelInfo) return;
            
            destinations.forEach(instId => {
                const instrument = this.instruments.find(i => i.id === instId);
                if (!instrument) return;
                
                const compat = this.checkCompatibility(channelInfo, instrument);
                totalScore += compat.score;
                count++;
            });
        });
        
        return count > 0 ? totalScore / count : 0;
    }

    // ========================================================================
    // API SIMPLE (RÉTROCOMPATIBILITÉ)
    // ========================================================================

    /**
     * Assigne un canal à un instrument (API simple 1→1)
     */
    assign(channelNumber, instrumentId) {
        // Vérifier si une route existe déjà pour ce canal
        const existingRoutes = this.getRoutesForChannel(channelNumber);
        
        // Supprimer les anciennes routes 1→1 pour ce canal
        existingRoutes.forEach(route => {
            if (route.type === this.ROUTE_TYPES.ONE_TO_ONE) {
                this.removeRoute(route.id);
            }
        });
        
        // Créer une nouvelle route 1→1
        const route = this.createRoute(channelNumber, instrumentId);
        
        return route !== null;
    }

    /**
     * Retire l'assignation d'un canal
     */
    unassign(channelNumber) {
        const routes = this.getRoutesForChannel(channelNumber);
        
        let removed = 0;
        routes.forEach(route => {
            if (this.removeRoute(route.id)) {
                removed++;
            }
        });
        
        if (removed > 0) {
            this.eventBus.emit('routing:unassigned', { channel: channelNumber });
        }
        
        return removed > 0;
    }

    /**
     * Obtient l'assignation d'un canal (compatible avec ancien code)
     */
    getAssignment(channelNumber) {
        const routes = this.getRoutesForChannel(channelNumber);
        
        // Retourner la première route 1→1 trouvée (compatibilité)
        const route = routes.find(r => r.type === this.ROUTE_TYPES.ONE_TO_ONE);
        
        if (!route) return null;
        
        const instrument = this.instruments.find(i => i.id === route.destinations[0]);
        
        return {
            instrumentId: route.destinations[0],
            instrument: instrument,
            channel: this.midiChannels.find(c => c.number === channelNumber),
            compatibility: { score: route.compatibility },
            timestamp: route.created
        };
    }

    // ========================================================================
    // API AVANCÉE - ROUTES MULTIPLES
    // ========================================================================

    /**
     * Ajoute un instrument de destination à un canal existant (1→N)
     */
    addDestination(channelNumber, instrumentId) {
        const routes = this.getRoutesForChannel(channelNumber);
        
        // Si route 1→1 existe, la convertir en 1→N
        const simpleRoute = routes.find(r => r.type === this.ROUTE_TYPES.ONE_TO_ONE);
        
        if (simpleRoute) {
            // Ajouter la destination
            simpleRoute.destinations.push(instrumentId);
            simpleRoute.type = this.determineRouteType(
                simpleRoute.sources.length,
                simpleRoute.destinations.length
            );
            simpleRoute.name = this.generateRouteName(
                simpleRoute.sources,
                simpleRoute.destinations
            );
            simpleRoute.lastModified = Date.now();
            
            // Mettre à jour l'index
            if (!this.routing.instrumentToRoutes.has(instrumentId)) {
                this.routing.instrumentToRoutes.set(instrumentId, new Set());
            }
            this.routing.instrumentToRoutes.get(instrumentId).add(simpleRoute.id);
            
            this.updateStats();
            this.eventBus.emit('routing:destination-added', { 
                channel: channelNumber, 
                instrument: instrumentId 
            });
            
            return true;
        } else {
            // Créer une nouvelle route 1→1
            return this.assign(channelNumber, instrumentId);
        }
    }

    /**
     * Retire un instrument de destination d'un canal (1→N → 1→1 ou suppression)
     */
    removeDestination(channelNumber, instrumentId) {
        const routes = this.getRoutesForChannel(channelNumber);
        
        for (const route of routes) {
            const index = route.destinations.indexOf(instrumentId);
            if (index !== -1) {
                route.destinations.splice(index, 1);
                
                // Si plus de destinations, supprimer la route
                if (route.destinations.length === 0) {
                    this.removeRoute(route.id);
                } else {
                    // Mettre à jour le type
                    route.type = this.determineRouteType(
                        route.sources.length,
                        route.destinations.length
                    );
                    route.name = this.generateRouteName(
                        route.sources,
                        route.destinations
                    );
                    route.lastModified = Date.now();
                }
                
                // Mettre à jour l'index
                const instRoutes = this.routing.instrumentToRoutes.get(instrumentId);
                if (instRoutes) {
                    instRoutes.delete(route.id);
                }
                
                this.updateStats();
                this.eventBus.emit('routing:destination-removed', { 
                    channel: channelNumber, 
                    instrument: instrumentId 
                });
                
                return true;
            }
        }
        
        return false;
    }

    /**
     * Merge plusieurs canaux vers un instrument (N→1)
     */
    mergeChannels(channels, instrumentId) {
        // Supprimer les routes existantes pour ces canaux
        channels.forEach(channel => {
            this.unassign(channel);
        });
        
        // Créer une route N→1
        return this.createRoute(channels, instrumentId, {
            name: `Merge CH[${channels.map(c => c + 1).join(',')}] → instrument`
        });
    }

    /**
     * Split un canal vers plusieurs instruments (1→N)
     */
    splitChannel(channel, instrumentIds, options = {}) {
        // Supprimer les routes existantes pour ce canal
        this.unassign(channel);
        
        // Créer une route 1→N
        return this.createRoute(channel, instrumentIds, {
            name: `Split CH${channel + 1} → ${instrumentIds.length} instruments`,
            ...options
        });
    }

    /**
     * Crée un routage complexe N→M
     */
    createComplexRoute(channels, instruments, options = {}) {
        return this.createRoute(channels, instruments, {
            name: `Complex route: ${channels.length} → ${instruments.length}`,
            ...options
        });
    }

    // ========================================================================
    // GESTION DES ROUTES
    // ========================================================================

    /**
     * Supprime une route
     */
    removeRoute(routeId) {
        const route = this.routing.routes.get(routeId);
        if (!route) return false;
        
        // Supprimer des index
        route.sources.forEach(channel => {
            const routes = this.routing.channelToRoutes.get(channel);
            if (routes) {
                routes.delete(routeId);
            }
        });
        
        route.destinations.forEach(instId => {
            const routes = this.routing.instrumentToRoutes.get(instId);
            if (routes) {
                routes.delete(routeId);
            }
        });
        
        // Supprimer la route
        this.routing.routes.delete(routeId);
        
        this.updateStats();
        this.eventBus.emit('routing:route-removed', { routeId });
        
        return true;
    }

    /**
     * Obtient toutes les routes pour un canal
     */
    getRoutesForChannel(channelNumber) {
        const routeIds = this.routing.channelToRoutes.get(channelNumber);
        if (!routeIds) return [];
        
        return Array.from(routeIds)
            .map(id => this.routing.routes.get(id))
            .filter(r => r !== undefined);
    }

    /**
     * Obtient toutes les routes pour un instrument
     */
    getRoutesForInstrument(instrumentId) {
        const routeIds = this.routing.instrumentToRoutes.get(instrumentId);
        if (!routeIds) return [];
        
        return Array.from(routeIds)
            .map(id => this.routing.routes.get(id))
            .filter(r => r !== undefined);
    }

    /**
     * Obtient toutes les routes
     */
    getAllRoutes() {
        return Array.from(this.routing.routes.values());
    }

    /**
     * Active/désactive une route
     */
    toggleRoute(routeId, enabled) {
        const route = this.routing.routes.get(routeId);
        if (!route) return false;
        
        route.enabled = enabled;
        route.lastModified = Date.now();
        
        this.eventBus.emit('routing:route-toggled', { routeId, enabled });
        
        return true;
    }

    /**
     * Efface toutes les routes
     */
    clearAll() {
        this.routing.routes.clear();
        this.routing.channelToRoutes.clear();
        this.routing.instrumentToRoutes.clear();
        
        this.validate();
        this.updateStats();
        
        this.eventBus.emit('routing:cleared');
        
        console.log('[RoutingManager] All routes cleared');
    }

    // ========================================================================
    // ROUTAGE AUTOMATIQUE
    // ========================================================================

    /**
     * Routage automatique intelligent
     */
    autoRoute() {
        console.log('[RoutingManager] Auto-routing...');
        
        this.clearAll();
        
        // Algorithme de routage
        const assignments = this.calculateBestRouting();
        
        assignments.forEach(({ channel, instrument }) => {
            this.assign(channel, instrument.id);
        });
        
        console.log('[RoutingManager] Auto-routing complete:', assignments.length);
        
        this.eventBus.emit('routing:auto-route-complete', { 
            assignments: assignments.length 
        });
    }

    /**
     * Calcule le meilleur routage automatique
     */
    calculateBestRouting() {
        const assignments = [];
        const usedInstruments = new Set();
        
        // Trier canaux par nombre de notes (priorité aux canaux actifs)
        const sortedChannels = [...this.midiChannels]
            .sort((a, b) => b.noteCount - a.noteCount);
        
        sortedChannels.forEach(channel => {
            // Trouver meilleur instrument non utilisé
            let bestInstrument = null;
            let bestScore = 0;
            
            this.instruments.forEach(instrument => {
                if (usedInstruments.has(instrument.id)) return;
                
                const compat = this.checkCompatibility(channel, instrument);
                if (compat.score > bestScore) {
                    bestScore = compat.score;
                    bestInstrument = instrument;
                }
            });
            
            if (bestInstrument) {
                assignments.push({ channel: channel.number, instrument: bestInstrument });
                usedInstruments.add(bestInstrument.id);
            }
        });
        
        return assignments;
    }

    /**
     * Vérifie la compatibilité canal-instrument
     */
    checkCompatibility(channel, instrument) {
        const compatibility = {
            score: 0,
            reasons: []
        };
        
        // Vérifier plage de notes
        if (instrument.noteRange) {
            const overlap = this.calculateRangeOverlap(
                channel.noteRange,
                instrument.noteRange
            );
            compatibility.score += overlap * 0.5;
            
            if (overlap > 0.8) {
                compatibility.reasons.push('Excellent note range match');
            }
        }
        
        // Vérifier type d'instrument
        if (instrument.type && channel.instrument) {
            const typeMatch = this.matchInstrumentTypes(
                channel.instrument,
                instrument.type
            );
            compatibility.score += typeMatch * 0.3;
        }
        
        // Vérifier vélocité
        if (instrument.velocityRange) {
            const velOverlap = this.calculateRangeOverlap(
                channel.velocity,
                instrument.velocityRange
            );
            compatibility.score += velOverlap * 0.2;
        }
        
        // Normaliser le score (0-1)
        compatibility.score = Math.min(1, Math.max(0, compatibility.score));
        
        return compatibility;
    }

    /**
     * Calcule le chevauchement de deux plages
     */
    calculateRangeOverlap(range1, range2) {
        const overlapMin = Math.max(range1.min, range2.min);
        const overlapMax = Math.min(range1.max, range2.max);
        
        if (overlapMax < overlapMin) return 0;
        
        const overlapSize = overlapMax - overlapMin;
        const range1Size = range1.max - range1.min;
        const range2Size = range2.max - range2.min;
        
        return overlapSize / Math.max(range1Size, range2Size);
    }

    /**
     * Compare types d'instruments
     */
    matchInstrumentTypes(channelType, instrumentType) {
        const typeMap = {
            'Piano': ['piano', 'keyboard'],
            'Drums': ['drums', 'percussion'],
            'Bass': ['bass'],
            'Guitar': ['guitar'],
            'Synth': ['synth', 'synthesizer'],
            'Strings': ['strings', 'violin', 'cello']
        };
        
        const channelLower = channelType.toLowerCase();
        const instrumentLower = instrumentType.toLowerCase();
        
        if (channelLower === instrumentLower) return 1.0;
        
        for (const [key, aliases] of Object.entries(typeMap)) {
            if (aliases.includes(channelLower) && aliases.includes(instrumentLower)) {
                return 0.8;
            }
        }
        
        return 0.3;
    }

    // ========================================================================
    // PRESETS
    // ========================================================================

    /**
     * Charge les presets depuis localStorage
     */
    loadPresets() {
        try {
            const stored = localStorage.getItem('midiMind_routingPresets');
            if (stored) {
                this.presets = JSON.parse(stored);
                console.log(`[RoutingManager] ${this.presets.length} presets loaded`);
            }
        } catch (error) {
            console.error('[RoutingManager] Failed to load presets:', error);
        }
    }

    /**
     * Sauvegarde les presets
     */
    savePresets() {
        try {
            localStorage.setItem('midiMind_routingPresets', 
                JSON.stringify(this.presets));
            console.log('[RoutingManager] Presets saved');
        } catch (error) {
            console.error('[RoutingManager] Failed to save presets:', error);
        }
    }

    /**
     * Crée un preset depuis le routage actuel
     */
    createPreset(name) {
        const preset = {
            id: `preset_${Date.now()}`,
            name: name,
            routes: this.getAllRoutes().map(route => ({
                sources: route.sources,
                destinations: route.destinations,
                type: route.type,
                filters: route.filters,
                transforms: route.transforms,
                enabled: route.enabled
            })),
            metadata: {
                created: Date.now(),
                channelCount: this.midiChannels.length,
                routeCount: this.routing.routes.size,
                stats: { ...this.stats }
            }
        };
        
        this.presets.push(preset);
        this.savePresets();
        
        console.log('[RoutingManager] Preset created:', name);
        
        this.eventBus.emit('routing:preset-created', { preset });
        
        return preset;
    }

    /**
     * Applique un preset
     */
    applyPreset(presetId) {
        const preset = this.presets.find(p => p.id === presetId);
        if (!preset) {
            console.warn('[RoutingManager] Preset not found:', presetId);
            return false;
        }
        
        this.clearAll();
        
        // Recréer toutes les routes
        preset.routes.forEach(routeData => {
            this.createRoute(
                routeData.sources,
                routeData.destinations,
                {
                    filters: routeData.filters,
                    transforms: routeData.transforms,
                    enabled: routeData.enabled
                }
            );
        });
        
        this.routing.mode = 'preset';
        this.routing.currentPreset = presetId;
        
        console.log('[RoutingManager] Preset applied:', preset.name);
        
        this.eventBus.emit('routing:preset-applied', { preset });
        
        return true;
    }

    /**
     * Supprime un preset
     */
    deletePreset(presetId) {
        const index = this.presets.findIndex(p => p.id === presetId);
        if (index === -1) return false;
        
        this.presets.splice(index, 1);
        this.savePresets();
        
        if (this.routing.currentPreset === presetId) {
            this.routing.currentPreset = null;
        }
        
        console.log('[RoutingManager] Preset deleted');
        
        this.eventBus.emit('routing:preset-deleted', { presetId });
        
        return true;
    }

    // ========================================================================
    // VALIDATION
    // ========================================================================

    /**
     * Valide la configuration de routage
     */
    validate() {
        this.conflicts = [];
        this.isValid = true;
        
        // 1. Vérifier les canaux non assignés
        this.midiChannels.forEach(channel => {
            const routes = this.getRoutesForChannel(channel.number);
            if (routes.length === 0) {
                this.conflicts.push({
                    type: 'unassigned',
                    channel: channel.number,
                    message: `Channel ${channel.number + 1} has no routes`
                });
                this.isValid = false;
            }
        });
        
        // 2. Vérifier les routes désactivées
        this.routing.routes.forEach(route => {
            if (!route.enabled) {
                this.conflicts.push({
                    type: 'disabled',
                    route: route.id,
                    message: `Route "${route.name}" is disabled`
                });
            }
        });
        
        // 3. Vérifier la compatibilité basse
        this.routing.routes.forEach(route => {
            if (route.compatibility < 0.3) {
                this.conflicts.push({
                    type: 'low-compatibility',
                    route: route.id,
                    score: route.compatibility,
                    message: `Route "${route.name}" has low compatibility (${Math.round(route.compatibility * 100)}%)`
                });
            }
        });
        
        return this.isValid;
    }

    // ========================================================================
    // STATISTIQUES
    // ========================================================================

    /**
     * Met à jour les statistiques
     */
    updateStats() {
        this.stats.totalChannels = this.midiChannels.length;
        this.stats.totalRoutes = this.routing.routes.size;
        
        // Compter les canaux assignés
        const assignedChannels = new Set();
        this.routing.routes.forEach(route => {
            route.sources.forEach(ch => assignedChannels.add(ch));
        });
        this.stats.assignedChannels = assignedChannels.size;
        this.stats.unassignedChannels = this.stats.totalChannels - this.stats.assignedChannels;
        
        // Compter les types de routes
        this.stats.oneToOne = 0;
        this.stats.oneToMany = 0;
        this.stats.manyToOne = 0;
        this.stats.manyToMany = 0;
        
        this.routing.routes.forEach(route => {
            switch (route.type) {
                case this.ROUTE_TYPES.ONE_TO_ONE:
                    this.stats.oneToOne++;
                    break;
                case this.ROUTE_TYPES.ONE_TO_MANY:
                    this.stats.oneToMany++;
                    break;
                case this.ROUTE_TYPES.MANY_TO_ONE:
                    this.stats.manyToOne++;
                    break;
                case this.ROUTE_TYPES.MANY_TO_MANY:
                    this.stats.manyToMany++;
                    break;
            }
        });
        
        // Score de compatibilité moyen
        let totalScore = 0;
        let count = 0;
        
        this.routing.routes.forEach(route => {
            if (route.enabled) {
                totalScore += route.compatibility;
                count++;
            }
        });
        
        this.stats.compatibilityScore = count > 0 ? totalScore / count : 0;
    }

    /**
     * Obtient les statistiques
     */
    getStats() {
        return { ...this.stats };
    }

    // ========================================================================
    // EXPORT / IMPORT
    // ========================================================================

    /**
     * Exporte la configuration de routage
     */
    export() {
        return {
            version: '3.2',
            mode: this.routing.mode,
            currentPreset: this.routing.currentPreset,
            routes: this.getAllRoutes().map(route => ({
                sources: route.sources,
                destinations: route.destinations,
                type: route.type,
                enabled: route.enabled,
                priority: route.priority,
                filters: route.filters,
                transforms: route.transforms,
                name: route.name,
                compatibility: route.compatibility
            })),
            stats: this.stats,
            timestamp: Date.now()
        };
    }

    /**
     * Importe une configuration de routage
     */
    import(config) {
        if (!config || !config.routes) return false;
        
        this.clearAll();
        
        config.routes.forEach(routeData => {
            this.createRoute(
                routeData.sources,
                routeData.destinations,
                {
                    enabled: routeData.enabled,
                    priority: routeData.priority,
                    filters: routeData.filters,
                    transforms: routeData.transforms,
                    name: routeData.name
                }
            );
        });
        
        this.routing.mode = config.mode || 'manual';
        this.routing.currentPreset = config.currentPreset || null;
        
        console.log('[RoutingManager] Configuration imported');
        
        return true;
    }

    // ========================================================================
    // GETTERS
    // ========================================================================

    getAssignments() {
        // Pour compatibilité - retourne format ancien
        const assignments = [];
        
        this.routing.routes.forEach(route => {
            if (route.type === this.ROUTE_TYPES.ONE_TO_ONE) {
                assignments.push([
                    route.sources[0],
                    {
                        instrumentId: route.destinations[0],
                        instrument: this.instruments.find(i => i.id === route.destinations[0]),
                        compatibility: { score: route.compatibility }
                    }
                ]);
            }
        });
        
        return assignments;
    }

    getUnassignedChannels() {
        return this.midiChannels.filter(
            channel => this.getRoutesForChannel(channel.number).length === 0
        );
    }

    getConflicts() {
        return [...this.conflicts];
    }

    isChannelAssigned(channelNumber) {
        return this.getRoutesForChannel(channelNumber).length > 0;
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = RoutingManager;
}