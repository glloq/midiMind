/**
 * MidiUtils.js
 * Fonctions utilitaires pour la manipulation MIDI
 */

const MidiUtils = {
    /**
     * Convertit un numéro de note MIDI en nom
     * @param {number} noteNumber - Numéro MIDI (0-127)
     * @returns {string} Nom de la note (ex: "C4")
     */
    noteNumberToName(noteNumber) {
        const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const octave = Math.floor(noteNumber / 12) - 1;
        const noteName = noteNames[noteNumber % 12];
        return `${noteName}${octave}`;
    },

    /**
     * Convertit un nom de note en numéro MIDI
     * @param {string} noteName - Nom de la note (ex: "C4")
     * @returns {number|null} Numéro MIDI ou null si invalide
     */
    noteNameToNumber(noteName) {
        const match = noteName.match(/^([A-G]#?)(-?\d+)$/);
        if (!match) return null;
        
        const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const [, note, octave] = match;
        const noteIndex = noteNames.indexOf(note);
        
        if (noteIndex === -1) return null;
        
        return (parseInt(octave) + 1) * 12 + noteIndex;
    },

    /**
     * Convertit une vélocité MIDI (0-127) en dB
     * @param {number} velocity - Vélocité MIDI
     * @returns {number} Niveau en dB
     */
    velocityToDb(velocity) {
        if (velocity === 0) return -Infinity;
        return 20 * Math.log10(velocity / 127);
    },

    /**
     * Convertit des dB en vélocité MIDI
     * @param {number} db - Niveau en dB
     * @returns {number} Vélocité MIDI (0-127)
     */
    dbToVelocity(db) {
        if (db === -Infinity) return 0;
        return Math.round(127 * Math.pow(10, db / 20));
    },

    /**
     * Convertit un numéro de note MIDI en fréquence (Hz)
     * @param {number} noteNumber - Numéro MIDI
     * @returns {number} Fréquence en Hz
     */
    noteToFrequency(noteNumber) {
        return 440 * Math.pow(2, (noteNumber - 69) / 12);
    },

    /**
     * Convertit une fréquence en numéro de note MIDI
     * @param {number} frequency - Fréquence en Hz
     * @returns {number} Numéro MIDI
     */
    frequencyToNote(frequency) {
        return Math.round(69 + 12 * Math.log2(frequency / 440));
    },

    /**
     * Convertit BPM et ticks en millisecondes
     * @param {number} ticks - Nombre de ticks
     * @param {number} bpm - Tempo en BPM
     * @param {number} ticksPerQuarter - Ticks par noire
     * @returns {number} Durée en millisecondes
     */
    ticksToMs(ticks, bpm = 120, ticksPerQuarter = 480) {
        const msPerQuarter = (60000 / bpm);
        return (ticks / ticksPerQuarter) * msPerQuarter;
    },

    /**
     * Convertit millisecondes en ticks
     * @param {number} ms - Durée en millisecondes
     * @param {number} bpm - Tempo en BPM
     * @param {number} ticksPerQuarter - Ticks par noire
     * @returns {number} Nombre de ticks
     */
    msToTicks(ms, bpm = 120, ticksPerQuarter = 480) {
        const msPerQuarter = (60000 / bpm);
        return Math.round((ms / msPerQuarter) * ticksPerQuarter);
    },

    /**
     * Formate une durée en secondes au format MM:SS
     * @param {number} seconds - Durée en secondes
     * @returns {string} Durée formatée
     */
    formatDuration(seconds) {
        const mins = Math.floor(seconds / 60);
        const secs = Math.floor(seconds % 60);
        return `${mins}:${secs.toString().padStart(2, '0')}`;
    },

    /**
     * Obtient le nom d'un programme General MIDI
     * @param {number} program - Numéro de programme (0-127)
     * @returns {string} Nom de l'instrument
     */
    getProgramName(program) {
        const gmInstruments = [
            // Piano (0-7)
            'Acoustic Grand Piano', 'Bright Acoustic Piano', 'Electric Grand Piano',
            'Honky-tonk Piano', 'Electric Piano 1', 'Electric Piano 2', 'Harpsichord', 'Clavi',
            // Chromatic Percussion (8-15)
            'Celesta', 'Glockenspiel', 'Music Box', 'Vibraphone', 'Marimba', 'Xylophone',
            'Tubular Bells', 'Dulcimer',
            // Organ (16-23)
            'Drawbar Organ', 'Percussive Organ', 'Rock Organ', 'Church Organ', 'Reed Organ',
            'Accordion', 'Harmonica', 'Tango Accordion',
            // Guitar (24-31)
            'Acoustic Guitar (nylon)', 'Acoustic Guitar (steel)', 'Electric Guitar (jazz)',
            'Electric Guitar (clean)', 'Electric Guitar (muted)', 'Overdriven Guitar',
            'Distortion Guitar', 'Guitar harmonics',
            // Bass (32-39)
            'Acoustic Bass', 'Electric Bass (finger)', 'Electric Bass (pick)', 'Fretless Bass',
            'Slap Bass 1', 'Slap Bass 2', 'Synth Bass 1', 'Synth Bass 2',
            // Strings (40-47)
            'Violin', 'Viola', 'Cello', 'Contrabass', 'Tremolo Strings', 'Pizzicato Strings',
            'Orchestral Harp', 'Timpani',
            // Ensemble (48-55)
            'String Ensemble 1', 'String Ensemble 2', 'SynthStrings 1', 'SynthStrings 2',
            'Choir Aahs', 'Voice Oohs', 'Synth Voice', 'Orchestra Hit',
            // Brass (56-63)
            'Trumpet', 'Trombone', 'Tuba', 'Muted Trumpet', 'French Horn', 'Brass Section',
            'SynthBrass 1', 'SynthBrass 2',
            // Reed (64-71)
            'Soprano Sax', 'Alto Sax', 'Tenor Sax', 'Baritone Sax', 'Oboe', 'English Horn',
            'Bassoon', 'Clarinet',
            // Pipe (72-79)
            'Piccolo', 'Flute', 'Recorder', 'Pan Flute', 'Blown Bottle', 'Shakuhachi',
            'Whistle', 'Ocarina',
            // Synth Lead (80-87)
            'Lead 1 (square)', 'Lead 2 (sawtooth)', 'Lead 3 (calliope)', 'Lead 4 (chiff)',
            'Lead 5 (charang)', 'Lead 6 (voice)', 'Lead 7 (fifths)', 'Lead 8 (bass + lead)',
            // Synth Pad (88-95)
            'Pad 1 (new age)', 'Pad 2 (warm)', 'Pad 3 (polysynth)', 'Pad 4 (choir)',
            'Pad 5 (bowed)', 'Pad 6 (metallic)', 'Pad 7 (halo)', 'Pad 8 (sweep)',
            // Synth Effects (96-103)
            'FX 1 (rain)', 'FX 2 (soundtrack)', 'FX 3 (crystal)', 'FX 4 (atmosphere)',
            'FX 5 (brightness)', 'FX 6 (goblins)', 'FX 7 (echoes)', 'FX 8 (sci-fi)',
            // Ethnic (104-111)
            'Sitar', 'Banjo', 'Shamisen', 'Koto', 'Kalimba', 'Bag pipe', 'Fiddle', 'Shanai',
            // Percussive (112-119)
            'Tinkle Bell', 'Agogo', 'Steel Drums', 'Woodblock', 'Taiko Drum', 'Melodic Tom',
            'Synth Drum', 'Reverse Cymbal',
            // Sound effects (120-127)
            'Guitar Fret Noise', 'Breath Noise', 'Seashore', 'Bird Tweet', 'Telephone Ring',
            'Helicopter', 'Applause', 'Gunshot'
        ];
        
        return gmInstruments[program] || 'Unknown';
    },

    /**
     * Vérifie si une note est une touche noire de piano
     * @param {number} noteNumber - Numéro MIDI
     * @returns {boolean} True si touche noire
     */
    isBlackKey(noteNumber) {
        const noteInOctave = noteNumber % 12;
        return [1, 3, 6, 8, 10].includes(noteInOctave);
    },

    /**
     * Crée un message MIDI
     * @param {number} status - Status byte
     * @param {number} data1 - Premier data byte
     * @param {number} data2 - Deuxième data byte
     * @returns {Uint8Array} Message MIDI
     */
    createMessage(status, data1, data2 = 0) {
        return new Uint8Array([status, data1, data2]);
    },

    /**
     * Parse un message MIDI
     * @param {Uint8Array} message - Message MIDI
     * @returns {Object} Message parsé
     */
    parseMessage(message) {
        const status = message[0];
        const type = status & 0xF0;
        const channel = status & 0x0F;
        
        return {
            type: this.getMessageTypeName(type),
            channel,
            data1: message[1],
            data2: message[2]
        };
    },

    /**
     * Obtient le nom d'un type de message
     * @param {number} type - Type de message
     * @returns {string} Nom du type
     */
    getMessageTypeName(type) {
        const types = {
            0x80: 'noteOff',
            0x90: 'noteOn',
            0xA0: 'polyAftertouch',
            0xB0: 'controlChange',
            0xC0: 'programChange',
            0xD0: 'channelAftertouch',
            0xE0: 'pitchBend'
        };
        return types[type] || 'unknown';
    },

    /**
     * Transpose une note
     * @param {number} noteNumber - Numéro MIDI
     * @param {number} semitones - Nombre de demi-tons
     * @returns {number} Note transposée
     */
    transpose(noteNumber, semitones) {
        return Math.max(0, Math.min(127, noteNumber + semitones));
    },

    /**
     * Calcule l'intervalle entre deux notes
     * @param {number} note1 - Première note
     * @param {number} note2 - Deuxième note
     * @returns {number} Intervalle en demi-tons
     */
    interval(note1, note2) {
        return Math.abs(note2 - note1);
    },

    /**
     * Quantize une valeur de ticks
     * @param {number} ticks - Ticks à quantizer
     * @param {number} gridSize - Taille de la grille
     * @returns {number} Ticks quantizés
     */
    quantize(ticks, gridSize) {
        return Math.round(ticks / gridSize) * gridSize;
    }
	
    // ========================================================================
    // CONVERSION NOTE ↔ NOM (CENTRALISÉ)
    // ========================================================================
    
    static NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
    
    /**
     * Obtenir le nom d'une note MIDI
     * @param {number} midiNote - Numéro MIDI (0-127)
     * @returns {string} Nom de la note (ex: "C#")
     */
    static getNoteName(midiNote) {
        return this.NOTE_NAMES[midiNote % 12];
    }
    
    /**
     * Obtenir l'octave d'une note MIDI
     * @param {number} midiNote - Numéro MIDI (0-127)
     * @returns {number} Octave (-1 à 9)
     */
    static getOctave(midiNote) {
        return Math.floor(midiNote / 12) - 1;
    }
    
    /**
     * Obtenir le nom complet (note + octave)
     * @param {number} midiNote - Numéro MIDI (0-127)
     * @returns {string} Ex: "C#4"
     */
    static getFullNoteName(midiNote) {
        return this.getNoteName(midiNote) + this.getOctave(midiNote);
    }
    
    /**
     * Convertir nom de note en numéro MIDI
     * @param {string} noteName - Ex: "C#4"
     * @returns {number} Numéro MIDI
     */
    static noteNameToMidi(noteName) {
        const match = noteName.match(/^([A-G]#?)(-?\d+)$/);
        if (!match) return null;
        
        const [, note, octave] = match;
        const noteIndex = this.NOTE_NAMES.indexOf(note);
        if (noteIndex === -1) return null;
        
        return (parseInt(octave) + 1) * 12 + noteIndex;
    }
    
    /**
     * Vérifier si une note est une touche noire
     * @param {number} midiNote - Numéro MIDI
     * @returns {boolean}
     */
    static isBlackKey(midiNote) {
        const noteName = this.getNoteName(midiNote);
        return noteName.includes('#');
    }
}

// Export si module ES6
if (typeof module !== 'undefined' && module.exports) {
    module.exports = MidiUtils;
}
};