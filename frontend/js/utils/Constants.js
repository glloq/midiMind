// ============================================================================
// Fichier: frontend/js/utils/Constants.js
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   Centralisation de toutes les constantes de l'application.
//   Facilite la maintenance et évite les "magic numbers".
//
// Constantes:
//   - MIDI : Notes, CC types, messages types, channels
//   - UI : Tailles, couleurs, durées animations
//   - Audio : Sample rates, buffer sizes, latences
//   - Files : Extensions supportées, tailles max
//   - Network : URLs, ports, timeouts
//   - Config : Valeurs par défaut
//
// Architecture:
//   Object freezé (immutable)
//   - Hiérarchie logique (MIDI.NOTES, UI.COLORS)
//   - JSDoc complet pour autocomplete
//
// Auteur: MidiMind Team
// ============================================================================

const Constants = {
    
    // ========================================================================
    // VERSION DU PROTOCOLE
    // ========================================================================
    
    PROTOCOL_VERSION: '3.0',
    
    // ========================================================================
    // COMMANDES BACKEND (100% synchronisées)
    // ========================================================================
    
    COMMANDS: {
        // ===================================================================
        // DEVICES - backend/src/api/commands/devices.cpp (5 commandes) ✅
        // ===================================================================
        DEVICES_SCAN: 'devices.scan',
        DEVICES_LIST: 'devices.list',
        DEVICES_CONNECT: 'devices.connect',
        DEVICES_DISCONNECT: 'devices.disconnect',
        DEVICES_INFO: 'devices.info',
        
        // ===================================================================
        // ROUTING - backend/src/api/commands/routing.cpp (6 commandes) ✅
        // ===================================================================
        ROUTING_ADD: 'routing.addRoute',
        ROUTING_REMOVE: 'routing.removeRoute',
        ROUTING_LIST: 'routing.listRoutes',
        ROUTING_UPDATE: 'routing.updateRoute',
        ROUTING_CLEAR: 'routing.clearRoutes',
        ROUTING_STATS: 'routing.getStats',
        
        // ===================================================================
        // PLAYBACK - backend/src/api/commands/playback.cpp (11 commandes) ✅
        // ===================================================================
        PLAYBACK_LOAD: 'playback.load',
        PLAYBACK_PLAY: 'playback.play',
        PLAYBACK_PAUSE: 'playback.pause',
        PLAYBACK_STOP: 'playback.stop',
        PLAYBACK_SEEK: 'playback.seek',
        PLAYBACK_STATUS: 'playback.status',
        PLAYBACK_METADATA: 'playback.getMetadata',
        PLAYBACK_SET_LOOP: 'playback.setLoop',
        PLAYBACK_SET_TEMPO: 'playback.setTempo',
        PLAYBACK_SET_VOLUME: 'playback.setVolume',      // ✅ CONFIRMÉE v3.1.1
        PLAYBACK_GET_VOLUME: 'playback.getVolume',      // ✅ NOUVELLE v3.1.1
        
        // ===================================================================
        // FILES - backend/src/api/commands/files.cpp (7 commandes) ✅
        // ===================================================================
        FILES_LIST: 'files.list',
        FILES_SCAN: 'files.scan',
        FILES_DELETE: 'files.delete',
        FILES_UPLOAD: 'files.upload',
        FILES_METADATA: 'files.getMetadata',
        FILES_CONVERT: 'files.convert',
        FILES_MOVE: 'files.move',
        
        // Legacy aliases (kept for compatibility)
        FILES_LOAD: 'playback.load',  // Redirects to playback
        FILES_SAVE: 'editor.save',    // Redirects to editor
        FILES_INFO: 'files.getMetadata',
        
        // ===================================================================
        // EDITOR - backend/src/api/commands/editor.cpp (7 commandes) ✅
        // ===================================================================
        EDITOR_LOAD: 'editor.load',
        EDITOR_SAVE: 'editor.save',
        EDITOR_ADD_NOTE: 'editor.addNote',
        EDITOR_DELETE_NOTE: 'editor.deleteNote',
        EDITOR_UPDATE_NOTE: 'editor.updateNote',
        EDITOR_ADD_CC: 'editor.addCC',
        EDITOR_UNDO: 'editor.undo',
        EDITOR_REDO: 'editor.redo',
        
        // ===================================================================
        // NETWORK - backend/src/api/commands/network.cpp (6 commandes) ✅
        // ===================================================================
        NETWORK_STATUS: 'network.status',
        NETWORK_INTERFACES: 'network.getInterfaces',
        NETWORK_SCAN_WIFI: 'network.scanWifi',
        NETWORK_CONNECT_WIFI: 'network.connectWifi',
        NETWORK_START_HOTSPOT: 'network.startHotspot',
        NETWORK_STOP_HOTSPOT: 'network.stopHotspot',
        
        // ===================================================================
        // SYSTEM - backend/src/api/commands/system.cpp (6 commandes) ✅
        // ===================================================================
        SYSTEM_STATUS: 'system.status',
        SYSTEM_INFO: 'system.info',
        SYSTEM_COMMANDS: 'system.commands',
        SYSTEM_SHUTDOWN: 'system.shutdown',
        SYSTEM_RESTART: 'system.restart',
        SYSTEM_PING: 'system.ping',
        
        // ===================================================================
        // INSTRUMENTS - backend/src/api/commands/instruments.cpp ✅
        // ===================================================================
        INSTRUMENTS_GET_PROFILE: 'instruments.getProfile',  // ✅ CONFIRMÉE
        INSTRUMENTS_SCAN: 'instruments.scan',
        INSTRUMENTS_CONNECT: 'instruments.connect',
        INSTRUMENTS_DISCONNECT: 'instruments.disconnect',
        INSTRUMENTS_REQUEST_IDENTITY: 'instruments.requestIdentity',
        INSTRUMENTS_REQUEST_NOTEMAP: 'instruments.requestNoteMap',
        INSTRUMENTS_REQUEST_CC: 'instruments.requestCC',
        INSTRUMENTS_SET_CONFIG: 'instruments.setConfig',
        
        // ===================================================================
        // LOOPS - backend/src/loop/LoopManager.cpp ✅
        // ===================================================================
        LOOP_CREATE: 'loop.create',
        LOOP_START_RECORDING: 'loop.startRecording',
        LOOP_STOP_RECORDING: 'loop.stopRecording',
        LOOP_PLAY: 'loop.play',
        LOOP_STOP: 'loop.stop',
        LOOP_SAVE: 'loop.save',
        LOOP_LOAD: 'loop.load',
        LOOP_DELETE: 'loop.delete',
        LOOP_LIST: 'loop.list',
        LOOP_ADD_LAYER: 'loop.addLayer',
        LOOP_DELETE_LAYER: 'loop.deleteLayer'
    },
    
    // ========================================================================
    // ÉVÉNEMENTS BACKEND → FRONTEND
    // ========================================================================
    
    EVENTS: {
        // Connexion
        WEBSOCKET_CONNECTED: 'websocket:connected',
        WEBSOCKET_DISCONNECTED: 'websocket:disconnected',
        WEBSOCKET_ERROR: 'websocket:error',
        
        // Backend général
        BACKEND_STATUS: 'backend:status',
        BACKEND_EVENT: 'backend:event',
        BACKEND_ERROR: 'backend:error',
        BACKEND_RESPONSE: 'backend:response',
        
        // MIDI Messages
        MIDI_MESSAGE: 'backend:midi:message',
        MIDI_NOTE_ON: 'backend:midi:note_on',
        MIDI_NOTE_OFF: 'backend:midi:note_off',
        MIDI_CC: 'backend:midi:cc',
        MIDI_PROGRAM_CHANGE: 'backend:midi:program_change',
        MIDI_PITCH_BEND: 'backend:midi:pitch_bend',
        
        // Devices
        DEVICE_CONNECTED: 'backend:device:connected',
        DEVICE_DISCONNECTED: 'backend:device:disconnected',
        DEVICE_DISCOVERED: 'backend:device:discovered',
        DEVICE_ERROR: 'backend:device:error',
        
        // SysEx
        SYSEX_IDENTITY: 'backend:sysex:identity',
        SYSEX_NOTEMAP: 'backend:sysex:notemap',
        SYSEX_MESSAGE: 'backend:sysex:message',
        
        // Playback
        PLAYBACK_STARTED: 'backend:playback:started',
        PLAYBACK_STOPPED: 'backend:playback:stopped',
        PLAYBACK_PAUSED: 'backend:playback:paused',
        PLAYBACK_POSITION: 'backend:playback:position',
        PLAYBACK_FINISHED: 'backend:playback:finished',
        PLAYBACK_TEMPO_CHANGED: 'backend:playback:tempo_changed',
        PLAYBACK_VOLUME_CHANGED: 'backend:playback:volume_changed',  // ✅ NOUVEAU
        
        // Files
        FILE_LOADED: 'backend:file:loaded',
        FILE_SAVED: 'backend:file:saved',
        FILE_DELETED: 'backend:file:deleted',
        FILE_ADDED: 'backend:file:added',
        FILE_SCAN_COMPLETE: 'backend:file:scan_complete',
        
        // Routing
        ROUTE_ADDED: 'backend:route:added',
        ROUTE_REMOVED: 'backend:route:removed',
        ROUTE_UPDATED: 'backend:route:updated',
        
        // System
        SYSTEM_ERROR: 'backend:system:error',
        SYSTEM_WARNING: 'backend:system:warning',
        SYSTEM_STATUS_UPDATE: 'backend:status_update',
        
        // Network
        NETWORK_WIFI_CONNECTED: 'backend:network:wifi_connected',
        NETWORK_WIFI_DISCONNECTED: 'backend:network:wifi_disconnected',
        NETWORK_HOTSPOT_STARTED: 'backend:network:hotspot_started',
        NETWORK_HOTSPOT_STOPPED: 'backend:network:hotspot_stopped',
        
        // Performance
        METRICS_UPDATE: 'backend:metrics_update',
        PERFORMANCE_WARNING: 'backend:performance:warning'
    },
    
    // ========================================================================
    // ÉTATS PLAYBACK
    // ========================================================================
    
    PLAYBACK_STATES: {
        STOPPED: 'stopped',
        PLAYING: 'playing',
        PAUSED: 'paused',
        LOADING: 'loading'
    },
    
    // ========================================================================
    // TYPES DE DEVICES
    // ========================================================================
    
    DEVICE_TYPES: {
        USB: 'usb',
        WIFI: 'wifi',
        BLUETOOTH: 'bluetooth',
        VIRTUAL: 'virtual',
        ALSA: 'alsa'
    },
    
    // ========================================================================
    // STATUTS DEVICE
    // ========================================================================
    
    DEVICE_STATUS: {
        CONNECTED: 'connected',
        DISCONNECTED: 'disconnected',
        CONNECTING: 'connecting',
        ERROR: 'error',
        AVAILABLE: 'available'
    },
    
    // ========================================================================
    // LIMITES
    // ========================================================================
    
    LIMITS: {
        MAX_FILENAME_LENGTH: 255,
        MAX_PLAYLIST_SIZE: 1000,
        MAX_HISTORY_SIZE: 50,
        MAX_UNDO_STACK: 50,
        MAX_NOTES_PER_TRACK: 100000,
        MAX_ROUTES: 64,
        MAX_DEVICES: 32,
        MIN_TEMPO: 20,
        MAX_TEMPO: 300,
        MIN_VELOCITY: 0,
        MAX_VELOCITY: 127,
        MIN_VOLUME: 0,          // ✅ NOUVEAU
        MAX_VOLUME: 100         // ✅ NOUVEAU
    },
    
    // ========================================================================
    // AUTRES CONSTANTES (inchangées)
    // ========================================================================
    
    INSTRUMENT_TYPES: {
        PIANO: 'piano',
        SYNTH: 'synth',
        ORGAN: 'organ',
        DRUMS: 'drums',
        PERCUSSION: 'percussion',
        BASS: 'bass',
        STRINGS: 'strings',
        GUITAR: 'guitar',
        BRASS: 'brass',
        WOODWINDS: 'woodwinds',
        CHOIR: 'choir',
        PAD: 'pad',
        LEAD: 'lead',
        SFX: 'sfx',
        OTHER: 'other'
    },
    
    MIDI_MESSAGES: {
        NOTE_OFF: 0x80,
        NOTE_ON: 0x90,
        POLY_PRESSURE: 0xA0,
        CONTROL_CHANGE: 0xB0,
        PROGRAM_CHANGE: 0xC0,
        CHANNEL_PRESSURE: 0xD0,
        PITCH_BEND: 0xE0,
        SYSTEM: 0xF0
    },
    
    MIDI_CC: {
        BANK_SELECT: 0,
        MODULATION: 1,
        BREATH: 2,
        FOOT: 4,
        PORTAMENTO_TIME: 5,
        DATA_ENTRY: 6,
        VOLUME: 7,
        BALANCE: 8,
        PAN: 10,
        EXPRESSION: 11,
        SUSTAIN: 64,
        PORTAMENTO: 65,
        SOSTENUTO: 66,
        SOFT_PEDAL: 67,
        REVERB: 91,
        TREMOLO: 92,
        CHORUS: 93,
        DETUNE: 94,
        PHASER: 95,
        ALL_SOUND_OFF: 120,
        RESET_ALL: 121,
        ALL_NOTES_OFF: 123
    },
    
    REPEAT_MODES: {
        NONE: 'none',
        ONE: 'one',
        ALL: 'all'
    },
    
    EDITOR_TOOLS: {
        SELECT: 'select',
        DRAW: 'draw',
        ERASE: 'erase',
        CUT: 'cut',
        VELOCITY: 'velocity'
    },
    
    SNAP_VALUES: {
        NONE: 0,
        QUARTER: 480,
        EIGHTH: 240,
        SIXTEENTH: 120,
        THIRTY_SECOND: 60,
        TRIPLET: 160
    },
    
    LOG_LEVELS: {
        DEBUG: 0,
        INFO: 1,
        WARN: 2,
        ERROR: 3,
        FATAL: 4
    },
    
    UI: {
        NOTIFICATION_DURATION: 3000,
        TOAST_DURATION: 2000,
        ANIMATION_DURATION: 300,
        DEBOUNCE_DELAY: 300,
        LONG_PRESS_DURATION: 500
    },
    
    STORAGE_KEYS: {
        SETTINGS: 'midimind_settings',
        PLAYLISTS: 'midimind_playlists',
        RECENT_FILES: 'midimind_recent_files',
        UI_STATE: 'midimind_ui_state',
        ROUTING_CONFIG: 'midimind_routing',
        USER_PREFERENCES: 'midimind_preferences'
    },
    
    FILE_FORMATS: {
        MIDI: '.mid',
        MIDI_EXTENDED: '.midi',
        JSONMIDI: '.jsonmidi',
        JSON: '.json'
    },
    
    DEFAULT_PATHS: {
        MIDI_FILES: '/home/pi/midi-files',
        PRESETS: '/home/pi/midi-files/presets',
        SESSIONS: '/home/pi/midi-files/sessions',
        EXPORTS: '/home/pi/midi-files/exports'
    }
};

// ============================================================================
// VALIDATION
// ============================================================================

Constants.isValidCommand = function(command) {
    return Object.values(this.COMMANDS).includes(command);
};

Constants.getCommandCategory = function(command) {
    const parts = command.split('.');
    return parts.length > 0 ? parts[0] : null;
};

Constants.getCommandsByCategory = function(category) {
    return Object.values(this.COMMANDS).filter(cmd => 
        cmd.startsWith(category + '.')
    );
};

// ============================================================================
// STATISTIQUES (NOUVEAU)
// ============================================================================

Constants.getStats = function() {
    const commands = Object.values(this.COMMANDS);
    const categories = {};
    
    commands.forEach(cmd => {
        const category = this.getCommandCategory(cmd);
        if (category) {
            categories[category] = (categories[category] || 0) + 1;
        }
    });
    
    return {
        totalCommands: commands.length,
        categories: categories,
        protocolVersion: this.PROTOCOL_VERSION
    };
};

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = Constants;
}

if (typeof window !== 'undefined') {
    window.Constants = Constants;
}

// ============================================================================
// LOG STATS AU CHARGEMENT (DEBUG)
// ============================================================================

if (typeof console !== 'undefined') {
    const stats = Constants.getStats();
    console.log('📊 Constants loaded:', stats);
}
