// ============================================================================
// Fichier: frontend/scripts/utils/RoutingManager.js
// Projet: midiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// Description:
//   Gestion complète du routage MIDI : canaux → instruments
//   Détection automatique, suggestions, presets, validation
// ============================================================================

class RoutingManager {
    constructor(eventBus, debugConsole) {
        this.eventBus = eventBus;
        this.debugConsole = debugConsole;
        
        // Configuration du routage
        this.routing = {
            assignments: new Map(),  // channel -> instrument
            mode: 'manual',          // 'manual', 'auto', 'preset'
            currentPreset: null
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
            compatibilityScore: 0
        };
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    /**
     * Initialise le routage pour un fichier MIDI
     */
    initialize(midiData, instruments) {
        console.log('[RoutingManager] Initializing routing...');
        
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
            instruments: this.instruments.length
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
    // ASSIGNATION
    // ========================================================================

    /**
     * Assigne un canal à un instrument
     */
    assign(channelNumber, instrumentId) {
        // Validation
        const channel = this.midiChannels.find(c => c.number === channelNumber);
        if (!channel) {
            console.warn('[RoutingManager] Invalid channel:', channelNumber);
            return false;
        }
        
        const instrument = this.instruments.find(i => i.id === instrumentId);
        if (!instrument) {
            console.warn('[RoutingManager] Invalid instrument:', instrumentId);
            return false;
        }
        
        // Vérifier compatibilité
        const compatibility = this.checkCompatibility(channel, instrument);
        if (compatibility.score < 0.3) {
            console.warn('[RoutingManager] Low compatibility:', compatibility);
        }
        
        // Assigner
        this.routing.assignments.set(channelNumber, {
            instrumentId: instrumentId,
            instrument: instrument,
            channel: channel,
            compatibility: compatibility,
            timestamp: Date.now()
        });
        
        console.log(`[RoutingManager] Assigned CH${channelNumber + 1} → ${instrument.name}`);
        
        // Mettre à jour
        this.validate();
        this.updateStats();
        
        this.eventBus.emit('routing:assigned', {
            channel: channelNumber,
            instrument: instrumentId,
            compatibility: compatibility.score
        });
        
        return true;
    }

    /**
     * Retire l'assignation d'un canal
     */
    unassign(channelNumber) {
        if (!this.routing.assignments.has(channelNumber)) {
            return false;
        }
        
        this.routing.assignments.delete(channelNumber);
        
        this.validate();
        this.updateStats();
        
        this.eventBus.emit('routing:unassigned', { channel: channelNumber });
        
        return true;
    }

    /**
     * Efface toutes les assignations
     */
    clearAll() {
        this.routing.assignments.clear();
        
        this.validate();
        this.updateStats();
        
        this.eventBus.emit('routing:cleared');
        
        console.log('[RoutingManager] All assignments cleared');
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
        
        this.routing.mode = 'auto';
        
        console.log('[RoutingManager] Auto-routing completed');
        
        this.eventBus.emit('routing:auto-routed', {
            assignments: assignments.length
        });
    }

    /**
     * Calcule le meilleur routage possible
     */
    calculateBestRouting() {
        const assignments = [];
        const usedInstruments = new Set();
        
        // Trier les canaux par nombre de notes (priorité)
        const sortedChannels = [...this.midiChannels].sort(
            (a, b) => b.noteCount - a.noteCount
        );
        
        sortedChannels.forEach(channel => {
            // Trouver le meilleur instrument disponible
            let bestInstrument = null;
            let bestScore = 0;
            
            this.instruments.forEach(instrument => {
                if (usedInstruments.has(instrument.id)) return;
                
                const compatibility = this.checkCompatibility(channel, instrument);
                
                if (compatibility.score > bestScore) {
                    bestScore = compatibility.score;
                    bestInstrument = instrument;
                }
            });
            
            if (bestInstrument && bestScore > 0.3) {
                assignments.push({
                    channel: channel.number,
                    instrument: bestInstrument
                });
                usedInstruments.add(bestInstrument.id);
            }
        });
        
        return assignments;
    }

    /**
     * Vérifie la compatibilité canal/instrument
     */
    checkCompatibility(channel, instrument) {
        let score = 0;
        const reasons = [];
        
        // 1. Correspondance de type d'instrument (40%)
        if (this.matchInstrumentType(channel.instrument, instrument.type)) {
            score += 0.4;
            reasons.push('Type matches');
        }
        
        // 2. Plage de notes supportée (30%)
        const noteRangeScore = this.calculateNoteRangeScore(
            channel.noteRange,
            instrument.noteRange
        );
        score += noteRangeScore * 0.3;
        if (noteRangeScore > 0.8) {
            reasons.push('Note range compatible');
        }
        
        // 3. Vélocité supportée (20%)
        if (instrument.supportsVelocity) {
            score += 0.2;
            reasons.push('Velocity supported');
        }
        
        // 4. Disponibilité (10%)
        if (instrument.state === 'ready') {
            score += 0.1;
            reasons.push('Instrument ready');
        }
        
        return {
            score: score,
            reasons: reasons,
            details: {
                typeMatch: this.matchInstrumentType(channel.instrument, instrument.type),
                noteRangeScore: noteRangeScore,
                velocitySupport: instrument.supportsVelocity,
                availability: instrument.state === 'ready'
            }
        };
    }

    /**
     * Vérifie la correspondance de type d'instrument
     */
    matchInstrumentType(midiInstrument, deviceType) {
        // Mapping MIDI GM → Types d'instruments
        const typeMapping = {
            'Piano': ['keyboard', 'piano', 'synth'],
            'Organ': ['organ', 'keyboard'],
            'Guitar': ['guitar', 'string'],
            'Bass': ['bass', 'string'],
            'Strings': ['string', 'orchestral'],
            'Ensemble': ['orchestral', 'synth'],
            'Brass': ['brass', 'wind'],
            'Reed': ['reed', 'wind'],
            'Pipe': ['wind', 'orchestral'],
            'Lead': ['synth', 'lead'],
            'Pad': ['synth', 'pad'],
            'Synth': ['synth'],
            'Drum': ['percussion', 'drum'],
            'Percussion': ['percussion']
        };
        
        const expectedTypes = typeMapping[midiInstrument] || [];
        return expectedTypes.includes(deviceType?.toLowerCase());
    }

    /**
     * Calcule le score de compatibilité de plage de notes
     */
    calculateNoteRangeScore(channelRange, instrumentRange) {
        if (!instrumentRange) return 0.5; // Valeur par défaut si inconnu
        
        const channelSpan = channelRange.max - channelRange.min;
        const instrumentSpan = instrumentRange.max - instrumentRange.min;
        
        // Vérifier si les notes du canal sont dans la plage de l'instrument
        const notesInRange = 
            channelRange.min >= instrumentRange.min &&
            channelRange.max <= instrumentRange.max;
        
        if (notesInRange) {
            return 1.0; // Parfait
        }
        
        // Calculer le pourcentage de recouvrement
        const overlapMin = Math.max(channelRange.min, instrumentRange.min);
        const overlapMax = Math.min(channelRange.max, instrumentRange.max);
        const overlap = Math.max(0, overlapMax - overlapMin);
        
        return overlap / channelSpan;
    }

    // ========================================================================
    // PRESETS
    // ========================================================================

    /**
     * Charge les presets depuis le localStorage
     */
    loadPresets() {
        try {
            const saved = localStorage.getItem('midiMind_routingPresets');
            if (saved) {
                this.presets = JSON.parse(saved);
                console.log('[RoutingManager] Loaded', this.presets.length, 'presets');
            }
        } catch (error) {
            console.error('[RoutingManager] Failed to load presets:', error);
        }
    }

    /**
     * Sauvegarde les presets dans le localStorage
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
            assignments: Array.from(this.routing.assignments.entries()).map(
                ([channel, assignment]) => ({
                    channel: channel,
                    instrumentId: assignment.instrumentId,
                    instrumentName: assignment.instrument.name
                })
            ),
            metadata: {
                created: Date.now(),
                channelCount: this.midiChannels.length,
                assignmentCount: this.routing.assignments.size
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
        
        preset.assignments.forEach(({ channel, instrumentId }) => {
            // Vérifier que l'instrument existe toujours
            const instrument = this.instruments.find(i => i.id === instrumentId);
            if (instrument) {
                this.assign(channel, instrumentId);
            } else {
                console.warn('[RoutingManager] Instrument not found:', instrumentId);
            }
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
        
        // 1. Vérifier que tous les canaux sont assignés
        this.midiChannels.forEach(channel => {
            if (!this.routing.assignments.has(channel.number)) {
                this.conflicts.push({
                    type: 'unassigned',
                    channel: channel.number,
                    message: `Channel ${channel.number + 1} is not assigned`
                });
                this.isValid = false;
            }
        });
        
        // 2. Vérifier les assignations multiples (même instrument utilisé 2x)
        const usedInstruments = new Map();
        this.routing.assignments.forEach((assignment, channel) => {
            const instId = assignment.instrumentId;
            if (usedInstruments.has(instId)) {
                this.conflicts.push({
                    type: 'duplicate',
                    channel: channel,
                    otherChannel: usedInstruments.get(instId),
                    instrumentId: instId,
                    message: `Instrument ${assignment.instrument.name} assigned multiple times`
                });
            } else {
                usedInstruments.set(instId, channel);
            }
        });
        
        // 3. Vérifier la compatibilité
        this.routing.assignments.forEach((assignment, channel) => {
            if (assignment.compatibility.score < 0.3) {
                this.conflicts.push({
                    type: 'low-compatibility',
                    channel: channel,
                    instrumentId: assignment.instrumentId,
                    score: assignment.compatibility.score,
                    message: `Low compatibility (${Math.round(assignment.compatibility.score * 100)}%)`
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
        this.stats.assignedChannels = this.routing.assignments.size;
        this.stats.unassignedChannels = this.stats.totalChannels - this.stats.assignedChannels;
        
        // Score de compatibilité moyen
        let totalScore = 0;
        let count = 0;
        
        this.routing.assignments.forEach(assignment => {
            totalScore += assignment.compatibility.score;
            count++;
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
            mode: this.routing.mode,
            currentPreset: this.routing.currentPreset,
            assignments: Array.from(this.routing.assignments.entries()).map(
                ([channel, assignment]) => ({
                    channel: channel,
                    instrumentId: assignment.instrumentId,
                    instrumentName: assignment.instrument.name,
                    compatibility: assignment.compatibility.score
                })
            ),
            stats: this.stats,
            timestamp: Date.now()
        };
    }

    /**
     * Importe une configuration de routage
     */
    import(config) {
        if (!config || !config.assignments) return false;
        
        this.clearAll();
        
        config.assignments.forEach(({ channel, instrumentId }) => {
            const instrument = this.instruments.find(i => i.id === instrumentId);
            if (instrument) {
                this.assign(channel, instrumentId);
            }
        });
        
        this.routing.mode = config.mode || 'manual';
        this.routing.currentPreset = config.currentPreset || null;
        
        console.log('[RoutingManager] Configuration imported');
        
        return true;
    }

    // ========================================================================
    // GETTERS
    // ========================================================================

    getAssignment(channelNumber) {
        return this.routing.assignments.get(channelNumber);
    }

    getAssignments() {
        return Array.from(this.routing.assignments.entries());
    }

    getUnassignedChannels() {
        return this.midiChannels.filter(
            channel => !this.routing.assignments.has(channel.number)
        );
    }

    getConflicts() {
        return [...this.conflicts];
    }

    isChannelAssigned(channelNumber) {
        return this.routing.assignments.has(channelNumber);
    }
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
/**
 * ✅ NOUVEAU: Routing automatique basé sur le type d'instrument
 * Analyse le contenu MIDI et assigne intelligemment
 * @returns {number} Nombre de canaux routés
 */
autoRouteByInstrumentType() {
    console.log('[RoutingManager] 🤖 Auto-routing by instrument type...');
    
    if (this.midiChannels.length === 0) {
        console.warn('[RoutingManager] No MIDI channels to route');
        return 0;
    }
    
    if (this.instruments.length === 0) {
        console.warn('[RoutingManager] No instruments available');
        return 0;
    }
    
    let routedCount = 0;
    const results = [];
    
    this.midiChannels.forEach(channel => {
        // Analyser le contenu du canal
        const analysis = this.analyzeChannelContent(channel.number);
        
        // Trouver le meilleur instrument
        const bestMatch = this.findBestInstrument(analysis);
        
        if (bestMatch) {
            // Assigner
            const success = this.assign(channel.number, bestMatch.instrument.id);
            
            if (success) {
                routedCount++;
                results.push({
                    channel: channel.number,
                    instrument: bestMatch.instrument.name,
                    type: analysis.type,
                    confidence: bestMatch.score
                });
                
                console.log(
                    `[RoutingManager] ✅ CH${channel.number + 1} → ${bestMatch.instrument.name} ` +
                    `(${analysis.type}, ${Math.round(bestMatch.score * 100)}%)`
                );
            }
        } else {
            console.warn(`[RoutingManager] ⚠️  No suitable instrument for CH${channel.number + 1}`);
        }
    });
    
    // Émettre événement
    this.eventBus.emit('routing:auto-routed', { 
        count: routedCount,
        total: this.midiChannels.length,
        results
    });
    
    console.log(`[RoutingManager] 🎉 Auto-routing complete: ${routedCount}/${this.midiChannels.length} channels`);
    
    return routedCount;
}

/**
 * ✅ NOUVEAU: Analyse le contenu d'un canal MIDI
 * Détecte le type d'instrument (drums, bass, piano, strings, etc.)
 * @param {number} channelNumber - Numéro du canal
 * @returns {Object} Analyse détaillée
 */
analyzeChannelContent(channelNumber) {
    const channel = this.midiChannels.find(c => c.number === channelNumber);
    
    if (!channel) {
        return { type: 'unknown', confidence: 0 };
    }
    
    // Récupérer toutes les notes du canal
    const notes = this.getNotesForChannel(channelNumber);
    
    if (notes.length === 0) {
        return { type: 'unknown', confidence: 0 };
    }
    
    // ========== STATISTIQUES ==========
    
    const pitches = notes.map(n => n.pitch);
    const minPitch = Math.min(...pitches);
    const maxPitch = Math.max(...pitches);
    const avgPitch = pitches.reduce((a, b) => a + b, 0) / pitches.length;
    const range = maxPitch - minPitch;
    
    // Durée moyenne des notes
    const durations = notes.map(n => n.duration || 0);
    const avgDuration = durations.reduce((a, b) => a + b, 0) / durations.length;
    
    // Vélocité moyenne
    const velocities = notes.map(n => n.velocity);
    const avgVelocity = velocities.reduce((a, b) => a + b, 0) / velocities.length;
    
    // Densité (notes par seconde)
    const duration = this.getMIDIDuration();
    const density = notes.length / (duration / 1000);
    
    // Variété de pitch (nombre de notes différentes)
    const uniquePitches = new Set(pitches);
    const pitchVariety = uniquePitches.size;
    
    // ========== DÉTECTION DU TYPE ==========
    
    let type = 'melodic';
    let confidence = 0.5;
    let reasoning = [];
    
    // 1. PERCUSSION (Drums)
    if (channelNumber === 9) {
        // Canal 10 en MIDI (9 en 0-indexed) = toujours drums
        type = 'percussion';
        confidence = 1.0;
        reasoning.push('MIDI channel 10 (drums)');
    } else if (minPitch >= 35 && maxPitch <= 81 && range < 20) {
        // Range drums typique (35-81) et petit range
        type = 'percussion';
        confidence = 0.9;
        reasoning.push('Drum pitch range (35-81)');
    } else if (avgDuration < 100 && density > 4) {
        // Notes très courtes et haute densité
        type = 'percussion';
        confidence = 0.7;
        reasoning.push('Short notes + high density');
    }
    
    // 2. BASS
    else if (avgPitch < 48) {
        // Pitch moyen très bas (< C2)
        type = 'bass';
        confidence = 0.8;
        reasoning.push('Low average pitch (<48)');
        
        if (avgDuration > 200 && density < 3) {
            confidence = 0.9;
            reasoning.push('Long sustained notes');
        }
    }
    
    // 3. PIANO
    else if (range > 48) {
        // Large range (> 4 octaves)
        type = 'piano';
        confidence = 0.8;
        reasoning.push('Wide pitch range (>48 semitones)');
        
        if (pitchVariety > 20) {
            confidence = 0.9;
            reasoning.push('High pitch variety');
        }
    }
    
    // 4. STRINGS
    else if (avgDuration > 500 && range > 24 && range < 48) {
        // Notes longues, medium range
        type = 'strings';
        confidence = 0.75;
        reasoning.push('Long notes + medium range');
        
        if (avgVelocity < 80) {
            confidence = 0.8;
            reasoning.push('Moderate velocity (legato style)');
        }
    }
    
    // 5. LEAD (Synth, Lead guitar, etc.)
    else if (avgPitch >= 60 && avgPitch <= 84 && pitchVariety < 15) {
        // Medium-high pitch, peu de variété (mélodie)
        type = 'lead';
        confidence = 0.7;
        reasoning.push('Melodic range + limited pitch variety');
    }
    
    // 6. PAD (Synth pad, choir)
    else if (avgDuration > 1000 && density < 2) {
        // Notes très longues, faible densité
        type = 'pad';
        confidence = 0.75;
        reasoning.push('Very long notes + low density');
    }
    
    // 7. ARPEGGIO
    else if (density > 6 && pitchVariety > 10 && avgDuration < 300) {
        // Haute densité, variété, notes courtes
        type = 'arpeggio';
        confidence = 0.7;
        reasoning.push('High density + variety + short notes');
    }
    
    return {
        type,
        confidence,
        reasoning,
        stats: {
            noteCount: notes.length,
            pitchRange: { min: minPitch, max: maxPitch, avg: Math.round(avgPitch) },
            range,
            avgDuration: Math.round(avgDuration),
            avgVelocity: Math.round(avgVelocity),
            density: Math.round(density * 10) / 10,
            pitchVariety,
            uniquePitches: Array.from(uniquePitches).sort((a, b) => a - b)
        }
    };
}

/**
 * ✅ NOUVEAU: Trouve le meilleur instrument pour une analyse donnée
 * @param {Object} analysis - Résultat de analyzeChannelContent()
 * @returns {Object|null} { instrument, score, reasons }
 */
findBestInstrument(analysis) {
    if (!analysis || analysis.type === 'unknown') {
        return null;
    }
    
    // Filtrer instruments par type
    const candidates = this.instruments.filter(inst => {
        // Type match
        if (inst.type !== analysis.type && 
            inst.category !== analysis.type &&
            !inst.tags?.includes(analysis.type)) {
            return false;
        }
        
        // Vérifier compatibilité de range si disponible
        if (inst.noteRange && analysis.stats.pitchRange) {
            const instMin = inst.noteRange.min || 0;
            const instMax = inst.noteRange.max || 127;
            const noteMin = analysis.stats.pitchRange.min;
            const noteMax = analysis.stats.pitchRange.max;
            
            // Notes en dehors du range de l'instrument
            if (noteMin < instMin || noteMax > instMax) {
                return false;
            }
        }
        
        return true;
    });
    
    if (candidates.length === 0) {
        // Fallback: chercher par type plus général
        const fallbackCandidates = this.instruments.filter(inst => 
            inst.type === 'general' || inst.category === 'multi-purpose'
        );
        
        if (fallbackCandidates.length > 0) {
            return {
                instrument: fallbackCandidates[0],
                score: 0.5,
                reasons: ['Fallback to general instrument']
            };
        }
        
        return null;
    }
    
    // Scorer chaque candidat
    const scored = candidates.map(inst => ({
        instrument: inst,
        score: this.calculateCompatibilityScore(inst, analysis),
        reasons: this.getCompatibilityReasons(inst, analysis)
    }));
    
    // Trier par score
    scored.sort((a, b) => b.score - a.score);
    
    return scored[0];
}

/**
 * ✅ NOUVEAU: Calcule score de compatibilité instrument/analyse
 * @param {Object} instrument - Instrument
 * @param {Object} analysis - Analyse canal
 * @returns {number} Score 0-1
 */
calculateCompatibilityScore(instrument, analysis) {
    let score = analysis.confidence; // Base score
    
    // Bonus si type exact match
    if (instrument.type === analysis.type) {
        score += 0.2;
    }
    
    // Bonus si capabilities match
    if (instrument.capabilities) {
        if (analysis.type === 'percussion' && instrument.capabilities.includes('drum-sounds')) {
            score += 0.15;
        }
        if (analysis.stats.avgDuration > 500 && instrument.capabilities.includes('sustain')) {
            score += 0.1;
        }
        if (analysis.stats.range > 48 && instrument.capabilities.includes('wide-range')) {
            score += 0.1;
        }
    }
    
    // Bonus si quality match
    if (instrument.quality === 'premium' || instrument.quality === 'high') {
        score += 0.05;
    }
    
    // Malus si range incompatible
    if (instrument.noteRange && analysis.stats.pitchRange) {
        const instMin = instrument.noteRange.min || 0;
        const instMax = instrument.noteRange.max || 127;
        const noteMin = analysis.stats.pitchRange.min;
        const noteMax = analysis.stats.pitchRange.max;
        
        const outOfRange = (noteMin < instMin ? instMin - noteMin : 0) +
                          (noteMax > instMax ? noteMax - instMax : 0);
        
        if (outOfRange > 0) {
            score -= outOfRange * 0.01; // -1% par semitone hors range
        }
    }
    
    return Math.max(0, Math.min(1, score));
}

/**
 * Helper: Raisons compatibilité
 */
getCompatibilityReasons(instrument, analysis) {
    const reasons = [];
    
    if (instrument.type === analysis.type) {
        reasons.push(`Matching type (${analysis.type})`);
    }
    
    if (instrument.noteRange && analysis.stats.pitchRange) {
        reasons.push('Compatible note range');
    }
    
    if (instrument.capabilities) {
        if (analysis.stats.avgDuration > 500 && instrument.capabilities.includes('sustain')) {
            reasons.push('Supports long notes');
        }
    }
    
    return reasons;
}

/**
 * Helper: Récupère notes d'un canal (depuis EditorModel)
 */
getNotesForChannel(channelNumber) {
    // Doit être implémenté ou récupéré depuis EditorModel
    // Pour l'instant, utiliser les infos du channel
    const channel = this.midiChannels.find(c => c.number === channelNumber);
    return channel?.notes || [];
}

/**
 * Helper: Durée totale MIDI
 */
getMIDIDuration() {
    // Récupérer depuis EditorModel ou calculer
    if (this.editorModel) {
        return this.editorModel.getDuration();
    }
    return 10000; // Fallback 10s
}

// ========================================================================
// ✅ PHASE 3.2 - SUGGESTIONS & VALIDATION
// ========================================================================

/**
 * ✅ NOUVEAU: Suggère des instruments pour un canal
 * @param {number} channelNumber - Numéro du canal
 * @param {number} topN - Nombre de suggestions (défaut 5)
 * @returns {Array} Top N suggestions
 */
suggestRouting(channelNumber) {
    console.log(`[RoutingManager] 💡 Generating routing suggestions for CH${channelNumber + 1}...`);
    
    const analysis = this.analyzeChannelContent(channelNumber);
    
    if (analysis.type === 'unknown') {
        return [];
    }
    
    const suggestions = [];
    
    this.instruments.forEach(instrument => {
        const score = this.calculateCompatibilityScore(instrument, analysis);
        
        if (score > 0.3) {
            suggestions.push({
                instrument,
                compatibility: score,
                reasons: this.getCompatibilityReasons(instrument, analysis),
                analysis
            });
        }
    });
    
    // Trier par compatibilité
    suggestions.sort((a, b) => b.compatibility - a.compatibility);
    
    console.log(`[RoutingManager] Found ${suggestions.length} suggestions`);
    
    return suggestions.slice(0, 5); // Top 5
}

/**
 * ✅ NOUVEAU: Valide le routing complet
 * Détecte erreurs, warnings et fait des recommendations
 * @returns {Object} Résultat validation
 */
validateRouting() {
    console.log('[RoutingManager] 🔍 Validating routing...');
    
    const issues = [];
    const warnings = [];
    const recommendations = [];
    
    // 1. Vérifier chaque route
    this.routing.routes.forEach((route, routeId) => {
        route.sources.forEach(channelNum => {
            const channel = this.midiChannels.find(c => c.number === channelNum);
            
            route.destinations.forEach(instrumentId => {
                const instrument = this.instruments.find(i => i.id === instrumentId);
                
                if (!instrument) {
                    issues.push({
                        severity: 'error',
                        type: 'missing-instrument',
                        channel: channelNum,
                        message: `Instrument not found (ID: ${instrumentId})`,
                        routeId
                    });
                    return;
                }
                
                // Analyser compatibilité
                const analysis = this.analyzeChannelContent(channelNum);
                const score = this.calculateCompatibilityScore(instrument, analysis);
                
                if (score < 0.3) {
                    warnings.push({
                        severity: 'warning',
                        type: 'low-compatibility',
                        channel: channelNum,
                        instrument: instrument.name,
                        compatibility: Math.round(score * 100),
                        message: `Low compatibility (${Math.round(score * 100)}%)`,
                        routeId
                    });
                    
                    // Suggérer alternative
                    const suggestions = this.suggestRouting(channelNum);
                    if (suggestions.length > 0 && suggestions[0].instrument.id !== instrumentId) {
                        recommendations.push({
                            type: 'better-match',
                            channel: channelNum,
                            currentInstrument: instrument.name,
                            suggestedInstrument: suggestions[0].instrument.name,
                            improvement: `+${Math.round((suggestions[0].compatibility - score) * 100)}%`,
                            message: `Consider using ${suggestions[0].instrument.name} instead`
                        });
                    }
                }
                
                // Vérifier range de notes
                if (instrument.noteRange && channel) {
                    const notes = this.getNotesForChannel(channelNum);
                    const outOfRange = notes.filter(n => {
                        const range = instrument.noteRange;
                        return n.pitch < (range.min || 0) || n.pitch > (range.max || 127);
                    });
                    
                    if (outOfRange.length > 0) {
                        issues.push({
                            severity: 'error',
                            type: 'notes-out-of-range',
                            channel: channelNum,
                            instrument: instrument.name,
                            count: outOfRange.length,
                            message: `${outOfRange.length} notes out of instrument range`,
                            detail: `Instrument range: ${instrument.noteRange.min}-${instrument.noteRange.max}`,
                            routeId
                        });
                    }
                }
            });
        });
    });
    
    // 2. Canaux non routés
    const allChannels = this.midiChannels;
    const routedChannels = new Set();
    this.routing.routes.forEach(route => {
        route.sources.forEach(ch => routedChannels.add(ch));
    });
    
    allChannels.forEach(ch => {
        if (!routedChannels.has(ch.number)) {
            warnings.push({
                severity: 'warning',
                type: 'unrouted-channel',
                channel: ch.number,
                message: `Channel ${ch.number + 1} not routed`,
                suggestion: 'Use auto-route or assign manually'
            });
            
            // Suggestion
            const suggestions = this.suggestRouting(ch.number);
            if (suggestions.length > 0) {
                recommendations.push({
                    type: 'route-unassigned',
                    channel: ch.number,
                    suggestedInstrument: suggestions[0].instrument.name,
                    compatibility: Math.round(suggestions[0].compatibility * 100),
                    message: `Route CH${ch.number + 1} to ${suggestions[0].instrument.name}`
                });
            }
        }
    });
    
    const isValid = issues.length === 0;
    
    // Émettre événement
    this.eventBus.emit('routing:validated', { 
        isValid,
        issues,
        warnings,
        recommendations
    });
    
    console.log(
        `[RoutingManager] Validation complete: ` +
        `${issues.length} errors, ${warnings.length} warnings, ${recommendations.length} recommendations`
    );
    
    return {
        isValid,
        issues,
        warnings,
        recommendations,
        summary: {
            totalRoutes: this.routing.routes.size,
            routedChannels: routedChannels.size,
            totalChannels: allChannels.length,
            unroutedChannels: allChannels.length - routedChannels.size
        }
    };
}

// ========================================================================
// ✅ PHASE 3.3 - PRESETS SYSTEM
// ========================================================================

/**
 * ✅ NOUVEAU: Sauvegarde preset de routing
 * @param {string} name - Nom du preset
 * @param {Object} options - Options (description, tags, etc.)
 * @returns {Object} Preset sauvegardé
 */
saveRoutingPreset(name, options = {}) {
    console.log(`[RoutingManager] 💾 Saving routing preset "${name}"...`);
    
    const preset = {
        name,
        description: options.description || '',
        tags: options.tags || [],
        timestamp: Date.now(),
        
        // Routes
        routes: Array.from(this.routing.routes.values()).map(route => ({
            sources: route.sources,
            destinations: route.destinations,
            type: route.type,
            enabled: route.enabled,
            // Sauvegarder noms pour affichage
            sourceNames: route.sources.map(ch => `CH${ch + 1}`),
            destinationNames: route.destinations.map(id => {
                const inst = this.instruments.find(i => i.id === id);
                return inst ? inst.name : id;
            })
        })),
        
        // Métadonnées
        metadata: {
            totalChannels: this.midiChannels.length,
            totalRoutes: this.routing.routes.size,
            instrumentTypes: this.getInstrumentTypesUsed(),
            routeTypes: this.getRouteTypesCount()
        },
        
        // Validation
        validation: {
            isValid: true,
            lastValidated: Date.now()
        }
    };
    
    // Valider avant sauvegarde
    const validation = this.validateRouting();
    preset.validation.isValid = validation.isValid;
    
    // Sauvegarder dans localStorage
    const presets = this.loadPresetsFromStorage();
    
    // Remplacer si existe déjà
    const existingIndex = presets.findIndex(p => p.name === name);
    if (existingIndex >= 0) {
        presets[existingIndex] = preset;
        console.log(`[RoutingManager] Updated existing preset "${name}"`);
    } else {
        presets.push(preset);
        console.log(`[RoutingManager] Created new preset "${name}"`);
    }
    
    localStorage.setItem('routingPresets', JSON.stringify(presets));
    
    // Émettre événement
    this.eventBus.emit('routing:preset-saved', { name, preset });
    
    console.log(`[RoutingManager] ✅ Preset "${name}" saved successfully`);
    
    return preset;
}

/**
 * ✅ NOUVEAU: Charge preset de routing
 * @param {string} name - Nom du preset
 * @returns {boolean} Succès
 */
loadRoutingPreset(name) {
    console.log(`[RoutingManager] 📂 Loading routing preset "${name}"...`);
    
    const presets = this.loadPresetsFromStorage();
    const preset = presets.find(p => p.name === name);
    
    if (!preset) {
        console.error(`[RoutingManager] ❌ Preset "${name}" not found`);
        return false;
    }
    
    // Effacer routing actuel
    this.clearAll();
    
    // Appliquer routes du preset
    let appliedCount = 0;
    let skippedCount = 0;
    
    preset.routes.forEach(routeData => {
        // Vérifier que tous les instruments existent
        const allInstrumentsExist = routeData.destinations.every(instId => 
            this.instruments.find(i => i.id === instId)
        );
        
        if (!allInstrumentsExist) {
            console.warn(
                `[RoutingManager] ⚠️  Skipping route ${routeData.sourceNames.join(',')} ` +
                `→ ${routeData.destinationNames.join(',')} (instruments not available)`
            );
            skippedCount++;
            return;
        }
        
        // Créer la route
        const success = this.createRoute(
            routeData.sources,
            routeData.destinations,
            {
                enabled: routeData.enabled,
                name: `${routeData.sourceNames.join(',')} → ${routeData.destinationNames.join(',')}`
            }
        );
        
        if (success) {
            appliedCount++;
        } else {
            skippedCount++;
        }
    });
    
    // Émettre événement
    this.eventBus.emit('routing:preset-loaded', { 
        name, 
        preset, 
        appliedCount,
        skippedCount
    });
    
    console.log(
        `[RoutingManager] 📂 Preset "${name}" loaded: ` +
        `${appliedCount} routes applied, ${skippedCount} skipped`
    );
    
    return appliedCount > 0;
}

/**
 * ✅ NOUVEAU: Liste tous les presets
 * @returns {Array} Liste des presets
 */
listRoutingPresets() {
    return this.loadPresetsFromStorage();
}

/**
 * ✅ NOUVEAU: Supprime un preset
 * @param {string} name - Nom du preset
 * @returns {boolean} Succès
 */
deleteRoutingPreset(name) {
    console.log(`[RoutingManager] 🗑️  Deleting routing preset "${name}"...`);
    
    const presets = this.loadPresetsFromStorage();
    const filtered = presets.filter(p => p.name !== name);
    
    if (filtered.length === presets.length) {
        console.warn(`[RoutingManager] ⚠️  Preset "${name}" not found`);
        return false;
    }
    
    localStorage.setItem('routingPresets', JSON.stringify(filtered));
    
    // Émettre événement
    this.eventBus.emit('routing:preset-deleted', { name });
    
    console.log(`[RoutingManager] ✅ Preset "${name}" deleted`);
    
    return true;
}

/**
 * ✅ NOUVEAU: Exporte preset en JSON
 * @param {string} name - Nom du preset
 * @returns {string|null} JSON du preset
 */
exportPresetAsJSON(name) {
    const presets = this.loadPresetsFromStorage();
    const preset = presets.find(p => p.name === name);
    
    if (!preset) {
        console.error(`[RoutingManager] Preset "${name}" not found`);
        return null;
    }
    
    return JSON.stringify(preset, null, 2);
}

/**
 * ✅ NOUVEAU: Importe preset depuis JSON
 * @param {string} jsonString - JSON du preset
 * @returns {boolean} Succès
 */
importPresetFromJSON(jsonString) {
    try {
        const preset = JSON.parse(jsonString);
        
        // Validation basique
        if (!preset.name || !preset.routes) {
            throw new Error('Invalid preset format');
        }
        
        // Sauvegarder
        const presets = this.loadPresetsFromStorage();
        presets.push(preset);
        localStorage.setItem('routingPresets', JSON.stringify(presets));
        
        console.log(`[RoutingManager] ✅ Preset "${preset.name}" imported`);
        
        this.eventBus.emit('routing:preset-imported', { preset });
        
        return true;
    } catch (error) {
        console.error(`[RoutingManager] ❌ Import failed:`, error);
        return false;
    }
}

// ========================================================================
// HELPERS PRESETS
// ========================================================================

/**
 * Helper: Charge presets depuis localStorage
 */
loadPresetsFromStorage() {
    try {
        const stored = localStorage.getItem('routingPresets');
        return stored ? JSON.parse(stored) : [];
    } catch (error) {
        console.error('[RoutingManager] Error loading presets:', error);
        return [];
    }
}

/**
 * Helper: Types d'instruments utilisés
 */
getInstrumentTypesUsed() {
    const types = new Set();
    this.routing.routes.forEach(route => {
        route.destinations.forEach(instId => {
            const inst = this.instruments.find(i => i.id === instId);
            if (inst) types.add(inst.type || 'unknown');
        });
    });
    return Array.from(types);
}

/**
 * Helper: Compte des types de routes
 */
getRouteTypesCount() {
    const counts = {
        '1→1': 0,
        '1→N': 0,
        'N→1': 0,
        'N→M': 0
    };
    
    this.routing.routes.forEach(route => {
        counts[route.type] = (counts[route.type] || 0) + 1;
    });
    
    return counts;
}

/**
 * Helper: Efface tout le routing
 */
clearAll() {
    this.routing.routes.clear();
    this.routing.assignments.clear();
    this.routing.channelToRoutes.clear();
    this.routing.instrumentToRoutes.clear();
    
    this.updateStats();
    
    this.eventBus.emit('routing:cleared');
    
    console.log('[RoutingManager] All routes cleared');
}

// ============================================================================
// EXEMPLE D'UTILISATION DES NOUVELLES MÉTHODES PHASE 3
// ============================================================================

/*
// 3.1 - Auto-Route
const routedCount = routingManager.autoRouteByInstrumentType();
console.log(`Auto-routed ${routedCount} channels`);

// Analyser un canal spécifique
const analysis = routingManager.analyzeChannelContent(0);
console.log('Analysis:', analysis);
// → { type: 'bass', confidence: 0.8, stats: {...}, reasoning: [...] }

// Trouver meilleur instrument
const bestMatch = routingManager.findBestInstrument(analysis);
console.log('Best match:', bestMatch);
// → { instrument: {...}, score: 0.9, reasons: [...] }

// 3.2 - Suggestions & Validation
const suggestions = routingManager.suggestRouting(0);
console.log('Top 5 suggestions:', suggestions);

const validation = routingManager.validateRouting();
console.log('Validation:', validation);
// → { isValid: true, issues: [], warnings: [], recommendations: [] }

// 3.3 - Presets
// Sauvegarder configuration actuelle
routingManager.saveRoutingPreset('My Song Setup', {
    description: 'Routing for my rock song',
    tags: ['rock', 'band']
});

// Charger preset
routingManager.loadRoutingPreset('My Song Setup');

// Lister presets
const presets = routingManager.listRoutingPresets();
console.log('Available presets:', presets);

// Supprimer preset
routingManager.deleteRoutingPreset('Old Setup');

// Export/Import JSON
const json = routingManager.exportPresetAsJSON('My Song Setup');
routingManager.importPresetFromJSON(json);
*/

// ============================================================================
// ÉVÉNEMENTS ÉMIS (PHASE 3)
// ============================================================================

/*
ÉVÉNEMENTS AUTO-ROUTE:
- 'routing:auto-routed' → { count, total, results }

ÉVÉNEMENTS VALIDATION:
- 'routing:validated' → { isValid, issues, warnings, recommendations }

ÉVÉNEMENTS PRESETS:
- 'routing:preset-saved' → { name, preset }
- 'routing:preset-loaded' → { name, preset, appliedCount, skippedCount }
- 'routing:preset-deleted' → { name }
- 'routing:preset-imported' → { preset }
- 'routing:cleared'
*/
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = RoutingManager;
}