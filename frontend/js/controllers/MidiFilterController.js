// ============================================================================
// Fichier: frontend/js/controllers/MidiFilterController.js
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   Contrôleur de filtrage des messages MIDI en temps réel.
//   Permet de filtrer par type de message, canal, vélocité, etc.
//
// Fonctionnalités:
//   - Filtrage par type message (Note On/Off, CC, Program Change)
//   - Filtrage par canal MIDI (1-16)
//   - Filtrage par plage de vélocité
//   - Filtrage par plage de pitch
//   - Filtres personnalisés (regex, callback)
//   - Presets de filtres sauvegardables
//   - Statistiques de filtrage (messages bloqués/passés)
//
// Architecture:
//   MidiFilterController extends BaseController
//   - Pipeline de filtres chainés
//   - Cache de décisions de filtrage
//   - Validation des règles de filtrage
//
// Auteur: MidiMind Team
// ============================================================================

class MidiFilterController {
    constructor(eventBus) {
        this.eventBus = eventBus;
        
        // Filtres actifs
        this.filters = {
            noteOn: true,
            noteOff: true,
            cc: true,
            programChange: true,
            pitchBend: true,
            sysex: false,
            aftertouch: false,
            channelPressure: false
        };

        // Filtres par canal
        this.channelFilters = new Set(); // Canaux désactivés

        // Filtres par plage de notes
        this.noteRangeFilter = {
            enabled: false,
            min: 0,
            max: 127
        };

        // Filtres par vélocité
        this.velocityFilter = {
            enabled: false,
            min: 1,
            max: 127
        };

        // Filtres par CC spécifiques
        this.ccFilters = new Set(); // CC à filtrer
    }

    /**
     * Active/désactive un type de message
     */
    setMessageTypeFilter(type, enabled) {
        if (type in this.filters) {
            this.filters[type] = enabled;
            this.emitFilterChanged();
            return true;
        }
        return false;
    }

    /**
     * Active/désactive un canal
     */
    setChannelFilter(channel, enabled) {
        if (enabled) {
            this.channelFilters.delete(channel);
        } else {
            this.channelFilters.add(channel);
        }
        this.emitFilterChanged();
    }

    /**
     * Définit le filtre de plage de notes
     */
    setNoteRangeFilter(enabled, min = 0, max = 127) {
        this.noteRangeFilter = {
            enabled: enabled,
            min: Math.max(0, Math.min(127, min)),
            max: Math.max(0, Math.min(127, max))
        };
        this.emitFilterChanged();
    }

    /**
     * Définit le filtre de vélocité
     */
    setVelocityFilter(enabled, min = 1, max = 127) {
        this.velocityFilter = {
            enabled: enabled,
            min: Math.max(1, Math.min(127, min)),
            max: Math.max(1, Math.min(127, max))
        };
        this.emitFilterChanged();
    }

    /**
     * Active/désactive le filtrage d'un CC spécifique
     */
    setCCFilter(ccNumber, enabled) {
        if (enabled) {
            this.ccFilters.delete(ccNumber);
        } else {
            this.ccFilters.add(ccNumber);
        }
        this.emitFilterChanged();
    }

    /**
     * Filtre un événement MIDI
     */
    filterEvent(event) {
        // Filtre par type
        if (!this.filters[event.type]) {
            return false;
        }

        // Filtre par canal
        if (event.channel !== undefined && this.channelFilters.has(event.channel)) {
            return false;
        }

        // Filtre par note
        if (event.type === 'noteOn' || event.type === 'noteOff') {
            if (this.noteRangeFilter.enabled) {
                if (event.note < this.noteRangeFilter.min || 
                    event.note > this.noteRangeFilter.max) {
                    return false;
                }
            }

            // Filtre par vélocité
            if (event.type === 'noteOn' && this.velocityFilter.enabled) {
                if (event.velocity < this.velocityFilter.min || 
                    event.velocity > this.velocityFilter.max) {
                    return false;
                }
            }
        }

        // Filtre par CC spécifique
        if (event.type === 'cc' && this.ccFilters.has(event.controller)) {
            return false;
        }

        return true;
    }

    /**
     * Filtre une timeline complète
     */
    filterTimeline(timeline) {
        return timeline.filter(event => this.filterEvent(event));
    }

    /**
     * Réinitialise tous les filtres
     */
    resetFilters() {
        this.filters = {
            noteOn: true,
            noteOff: true,
            cc: true,
            programChange: true,
            pitchBend: true,
            sysex: false,
            aftertouch: false,
            channelPressure: false
        };

        this.channelFilters.clear();
        this.ccFilters.clear();
        
        this.noteRangeFilter.enabled = false;
        this.velocityFilter.enabled = false;

        this.emitFilterChanged();
    }

    /**
     * Obtient les statistiques de filtrage
     */
    getFilterStats(timeline) {
        const stats = {
            total: timeline.length,
            filtered: 0,
            visible: 0,
            byType: {}
        };

        timeline.forEach(event => {
            const type = event.type;
            
            if (!stats.byType[type]) {
                stats.byType[type] = { total: 0, visible: 0 };
            }
            
            stats.byType[type].total++;

            if (this.filterEvent(event)) {
                stats.visible++;
                stats.byType[type].visible++;
            } else {
                stats.filtered++;
            }
        });

        return stats;
    }

    /**
     * Émet un événement de changement de filtre
     */
    emitFilterChanged() {
        this.eventBus.emit('filter:changed', {
            filters: this.filters,
            channelFilters: Array.from(this.channelFilters),
            noteRangeFilter: this.noteRangeFilter,
            velocityFilter: this.velocityFilter,
            ccFilters: Array.from(this.ccFilters)
        });
    }

    /**
     * Sauvegarde la configuration des filtres
     */
    saveConfiguration(name) {
        const config = {
            name: name,
            filters: { ...this.filters },
            channelFilters: Array.from(this.channelFilters),
            noteRangeFilter: { ...this.noteRangeFilter },
            velocityFilter: { ...this.velocityFilter },
            ccFilters: Array.from(this.ccFilters),
            timestamp: Date.now()
        };

        // Sauvegarder dans localStorage
        const saved = JSON.parse(localStorage.getItem('midiFilterConfigs') || '[]');
        saved.push(config);
        localStorage.setItem('midiFilterConfigs', JSON.stringify(saved));

        return config;
    }

    /**
     * Charge une configuration de filtres
     */
    loadConfiguration(config) {
        this.filters = { ...config.filters };
        this.channelFilters = new Set(config.channelFilters);
        this.noteRangeFilter = { ...config.noteRangeFilter };
        this.velocityFilter = { ...config.velocityFilter };
        this.ccFilters = new Set(config.ccFilters);

        this.emitFilterChanged();
    }

    /**
     * Obtient toutes les configurations sauvegardées
     */
    getSavedConfigurations() {
        return JSON.parse(localStorage.getItem('midiFilterConfigs') || '[]');
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = MidiFilterController;
}
window.MidiFilterController = MidiFilterController;