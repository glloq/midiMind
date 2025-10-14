# Protocole SysEx Custom MidiMind v3.0
## Documentation Complète - Architecture par BLOCS

---

## 📋 Table des Matières

1. [Vue d'ensemble](#vue-densemble)
2. [Structure générale des messages](#structure-générale)
3. [Bloc 1 - Identification](#bloc-1---identification)
4. [Bloc 2 - Note Map](#bloc-2---note-map)
5. [Blocs 3-8 - Spécifications futures](#blocs-futurs)
6. [Gestion dans midiMind](#gestion-dans-midimind)
7. [Remontée vers l'interface utilisateur](#remontée-interface)
8. [Flux de communication](#flux-de-communication)
9. [Exemples pratiques](#exemples-pratiques)

---

## 🎯 Vue d'ensemble

### Objectif du Protocole

Le protocole SysEx personnalisé de midiMind permet de **récupérer les informations** des instruments DIY connectés. Il s'agit d'un protocole **unidirectionnel** :

- ✅ **Instrument → Backend** : Envoi d'informations de configuration
- ❌ **Backend → Instrument** : Pas de commandes de contrôle (géré via MIDI CC)

### Philosophie de Conception

1. **Extensible** : Architecture modulaire par blocs (1-8)
2. **Versionné** : Chaque bloc possède son propre numéro de version
3. **Optionnel** : Les blocs peuvent être implémentés progressivement
4. **Logging complet** : Toutes les données sont loggées, même si non utilisées

---

## 📦 Structure Générale des Messages

### Format de Base

```
F0 7D <DeviceID> <BlockID> <BlockVersion> [...données...] F7
```

### Décomposition

| Byte | Valeur | Description |
|------|--------|-------------|
| 0 | `F0` | Start of Exclusive (SOX) |
| 1 | `7D` | Manufacturer ID (Educational Use) |
| 2 | `0x00-0x7F` | Device ID (0x7F = broadcast) |
| 3 | `0x01-0x08` | Block ID |
| 4 | `0x01-0xFF` | Block Version |
| 5-n | ... | Données du bloc |
| n+1 | `F7` | End of Exclusive (EOX) |

### IDs de Blocs Définis

| Block ID | Nom | Version | Statut | Taille |
|----------|-----|---------|--------|--------|
| `0x01` | Identification | v1.0 | ✅ Complet | ~34 bytes |
| `0x02` | Note Map | v1.0 | ✅ Complet | ~20 bytes |
| `0x03` | CC Supportés | v2.0 | 🔮 Futur | ~25 bytes |
| `0x04` | Capacités Air | v2.0 | 🔮 Futur | ~15 bytes |
| `0x05` | Capacités Lumières | v2.0 | 🔮 Futur | ~20 bytes |
| `0x06` | *Réservé* | - | - | - |
| `0x07` | Capteurs/Feedback | v3.0 | 🔮 Futur | ~30 bytes |
| `0x08` | Sync & Clock | v3.0 | 🔮 Futur | ~12 bytes |

---

## 🎫 Bloc 1 - Identification

### Objectif

Fournir l'**identité complète** de l'instrument : nom, type, capacités, plage de notes, version firmware.

### Format du Message

```
F0 7D <DeviceID> 01 01 
<UniqueID[4]>           // 28-bit unique ID (4 bytes encodés 7-bit)
<Name...> 00            // Nom de l'instrument (C-string null-terminated)
<Type>                  // Type d'instrument (1 byte)
<FirstNote>             // Première note MIDI jouable (0-127)
<NoteCount>             // Nombre de notes jouables (1-128)
<MaxPoly>               // Polyphonie max (0 = mono, 1-16)
<TuningMode>            // Mode d'accordage (1 byte)
<DelayLSB> <DelayMSB>   // Délai de réponse (ms, 14-bit)
<FwV1> <FwV2> <FwV3> <FwV4>  // Version firmware (4 bytes)
<Flags>                 // Capacités (bitfield)
<Programs>              // Nombre de programmes/presets (0-127)
F7
```

### Détail des Champs

#### 1. Unique ID (4 bytes)

**Format** : 28-bit encodé en 7-bit (4 bytes)

```
Byte 0: bits 0-6   (LSB)
Byte 1: bits 7-13
Byte 2: bits 14-20
Byte 3: bits 21-27 (MSB)
```

**Utilisation** :
- Identifiant **persistant** unique par instrument
- Permet de reconnaître un instrument même si son nom change
- Généré aléatoirement ou basé sur un serial number

**Exemple** :
```
ID = 0x1234567 (hex) = 19088743 (dec)
Encodé: [0x67, 0x4C, 0x11, 0x09]
```

#### 2. Name (variable, max 16 chars)

**Format** : ASCII null-terminated

**Règles** :
- Maximum 16 caractères (+ null byte)
- Caractères autorisés : `A-Z`, `a-z`, `0-9`, `_`, `-`
- Pas d'espaces (utiliser `_`)

**Exemples** :
- `"MaFlute_DIY"`
- `"BagPipe_V2"`
- `"Saxophone_Alto"`

#### 3. Type (1 byte)

**Valeurs** : Compatible General MIDI étendu

| Valeur | Type | Catégorie |
|--------|------|-----------|
| `0x00-0x07` | Piano | Keyboard |
| `0x08-0x0F` | Chromatic Percussion | Keyboard |
| `0x10-0x17` | Organ | Keyboard |
| `0x18-0x1F` | Guitar | Plucked |
| `0x20-0x27` | Bass | Plucked |
| `0x28-0x2F` | Strings | Bowed |
| `0x30-0x37` | Ensemble | Orchestra |
| `0x38-0x3F` | Brass | Wind |
| `0x40-0x47` | Reed | Wind |
| `0x48-0x4F` | Pipe | Wind |
| `0x50-0x57` | Synth Lead | Synth |
| `0x58-0x5F` | Synth Pad | Synth |
| `0x60-0x67` | Synth Effects | Synth |
| `0x68-0x6F` | Ethnic | World |
| `0x70-0x77` | Percussive | Percussion |
| `0x78-0x7F` | Sound Effects | FX |
| **`0x80-0xFF`** | **DIY Custom** | **DIY** |

**Valeurs DIY recommandées** :
- `0x80` : Wind DIY (flûtes, trompettes...)
- `0x81` : String DIY (guitares, harpes...)
- `0x82` : Percussion DIY (pads, tambours...)
- `0x83` : Keyboard DIY (claviers custom...)
- `0x84` : Controller DIY (contrôleurs gestuels...)
- `0x85-0xFF` : Libre pour usage custom

#### 4. First Note (1 byte)

**Format** : MIDI Note Number (0-127)

**Utilisation** :
- Définit la **première note** jouable de l'instrument
- Permet au backend de savoir où commence la plage

**Exemples** :
- `60` (C4) : Flûte à bec alto
- `48` (C3) : Guitare basse
- `40` (E2) : Contrebasse
- `36` (C2) : Tuba

#### 5. Note Count (1 byte)

**Format** : Nombre de notes (1-128)

**Calcul** : `LastNote = FirstNote + NoteCount - 1`

**Exemples** :
- `FirstNote=60, NoteCount=24` → Plage C4 à B5
- `FirstNote=48, NoteCount=36` → Plage C3 à B5
- `FirstNote=0, NoteCount=128` → Plage complète MIDI

**Cas spéciaux** :
- `NoteCount=0` : Invalide (min 1)
- `NoteCount=128` : Toutes les notes MIDI

#### 6. Max Polyphony (1 byte)

**Format** : Nombre de voix simultanées (0-16)

**Valeurs** :
- `0` : Monophonique strict
- `1` : Monophonique avec legato
- `2-16` : Polyphonie (nombre de voix)

**Exemples** :
- `0` : Flûte, clarinette
- `1` : Synthé mono avec glide
- `4` : Guitare (4 cordes)
- `6` : Piano simple
- `8` : Orgue

#### 7. Tuning Mode (1 byte)

**Format** : Bitfield

```
Bit 0-3: Mode de base
Bit 4-7: Réservé (0)
```

**Modes de base** :

| Valeur | Mode | Description |
|--------|------|-------------|
| `0x00` | CHROMATIC | 12 demi-tons par octave |
| `0x01` | DIATONIC | 7 notes par octave (gamme majeure) |
| `0x02` | PENTATONIC | 5 notes par octave |
| `0x03` | BLUES | Pentatonique + blue notes |
| `0x04` | WHOLE_TONE | Gamme par tons |
| `0x05` | OCTATONIC | 8 notes alternées |
| `0x08` | MONO | Monophonique avec priorité |
| `0x09` | POLY | Polyphonique standard |
| `0x0A` | DRONE | Note pédale permanente |
| `0x0B` | CLUSTER | Notes groupées |

**Usage** :
- Indique au backend comment interpréter les notes
- Permet des optimisations (ex: éviter d'envoyer des notes non jouables)

#### 8. Delay (2 bytes)

**Format** : 14-bit (LSB, MSB) en millisecondes

**Calcul** : `Delay_ms = (MSB << 7) | LSB`

**Plage** : 0-16383 ms

**Utilisation** :
- Temps de réponse typique de l'instrument
- Permet au backend d'anticiper la latence
- Utilisé pour la compensation de timing

**Exemples** :
- `0 ms` : Électronique pure (synthé)
- `5-10 ms` : Capteurs piézo rapides
- `20-50 ms` : Capteurs de souffle
- `50-100 ms` : Systèmes mécaniques
- `100+ ms` : Instruments acoustiques numérisés

#### 9. Firmware Version (4 bytes)

**Format** : `Major.Minor.Patch.Build`

**Exemple** :
```
Version 2.3.1.45 → [0x02, 0x03, 0x01, 0x2D]
```

**Usage** :
- Permet de détecter les bugs connus
- Affichage dans l'interface utilisateur
- Gestion de compatibilité

#### 10. Flags (1 byte)

**Format** : Bitfield des capacités

```
Bit 0: Velocity Support       (0=Non, 1=Oui)
Bit 1: Aftertouch Support     (0=Non, 1=Oui)
Bit 2: Breath Controller      (0=Non, 1=Oui)
Bit 3: Pitch Bend             (0=Non, 1=Oui)
Bit 4: Modulation Wheel       (0=Non, 1=Oui)
Bit 5: Expression Pedal       (0=Non, 1=Oui)
Bit 6: Sustain Pedal          (0=Non, 1=Oui)
Bit 7: Program Change Support (0=Non, 1=Oui)
```

**Lecture** :
```
bool hasVelocity = (flags & 0x01) != 0;
bool hasAftertouch = (flags & 0x02) != 0;
// etc.
```

#### 11. Programs (1 byte)

**Format** : Nombre de presets/programmes (0-127)

**Valeurs** :
- `0` : Pas de presets
- `1-127` : Nombre de presets disponibles

**Usage** :
- Affichage dans l'interface de sélection
- Validation des Program Change messages

### Exemple Complet - Bloc 1

**Configuration instrument** :
- Unique ID: `0x1234567`
- Nom: `"MaFlute_DIY"`
- Type: Wind DIY (`0x80`)
- Plage: C4 (60) à B5 (83) → 24 notes
- Monophonique (`0`)
- Mode chromatique (`0x00`)
- Délai: 30 ms
- Firmware: 1.2.0.10
- Flags: Velocity + Breath (`0x05`)
- Programmes: 4

**Message SysEx** :
```
F0 7D 00 01 01
67 4C 11 09          // Unique ID
4D 61 46 6C 75 74 65 5F 44 49 59 00  // "MaFlute_DIY\0"
80                   // Type: Wind DIY
3C                   // First Note: 60 (C4)
18                   // Note Count: 24
00                   // Max Poly: 0 (mono)
00                   // Tuning: Chromatic
1E 00                // Delay: 30 ms
01 02 00 0A          // FW: 1.2.0.10
05                   // Flags: 0b00000101
04                   // Programs: 4
F7

Taille totale: 34 bytes
```

---

## 🎹 Bloc 2 - Note Map

### Objectif

Fournir la **liste précise** des notes jouables sous forme de **bitmap 128-bit**.

### Format du Message

```
F0 7D <DeviceID> 02 01 
<Bitmap[16]>         // 128 bits = 16 bytes (notes 0-127)
<Reserved[2]>        // 2 bytes réservés (0x00)
F7

Taille: 22 bytes
```

### Structure du Bitmap

**Organisation** : 1 bit par note MIDI (0-127)

```
Byte  0: Notes   0-6   (bit 0=note 0,  bit 6=note 6)
Byte  1: Notes   7-13
Byte  2: Notes  14-20
...
Byte 15: Notes 105-111
Byte 16: Notes 112-118
Byte 17: Notes 119-125
Byte 18: Bit 0=note 126, bit 1=note 127, bits 2-6 unused
```

**Encodage** :
- Bit à `1` : Note **jouable**
- Bit à `0` : Note **non jouable**

### Pourquoi un Bitmap ?

**Avantages** :
1. **Compact** : 128 notes = 16 bytes seulement
2. **Rapide** : Test de jouabilité en O(1)
3. **Flexible** : Supporte n'importe quelle configuration
4. **Précis** : Définit exactement quelles notes sont jouables

**Cas d'usage** :
- Instruments avec notes non-chromatiques (pentatonique, gammes exotiques)
- Touches cassées ou désactivées
- Configurations personnalisées
- Modes de jeu alternatifs

### Relation avec Bloc 1

**Bloc 1** définit :
- `FirstNote = 60`
- `NoteCount = 24`
- **Plage théorique** : 60-83

**Bloc 2** précise :
- Quelles notes de cette plage sont **réellement jouables**
- Exemple : notes 61, 63, 68 désactivées

**Priorité** : Bloc 2 > Bloc 1

### Génération du Bitmap

#### Méthode 1 : Plage Continue

**Configuration** : C4 (60) à B5 (83), chromatique

```cpp
// Initialiser à 0
uint8_t bitmap[19] = {0};

// Activer les notes 60-83
for (uint8_t note = 60; note <= 83; note++) {
    uint8_t byteIndex = note / 7;
    uint8_t bitIndex = note % 7;
    bitmap[byteIndex] |= (1 << bitIndex);
}
```

**Résultat** :
```
Bitmap: [00 00 00 00 00 00 00 00 7F 7F 7F 7E 00 00 00 00 00 00 00]
         └─────notes 0-55──────┘ └─60-83─┘ └────84-127────┘
```

#### Méthode 2 : Pentatonique

**Configuration** : C4 pentatonique (60, 62, 64, 67, 69)

```cpp
uint8_t bitmap[19] = {0};
uint8_t notes[] = {60, 62, 64, 67, 69};

for (uint8_t note : notes) {
    uint8_t byteIndex = note / 7;
    uint8_t bitIndex = note % 7;
    bitmap[byteIndex] |= (1 << bitIndex);
}
```

#### Méthode 3 : Configuration Custom

**Exemple** : Toutes les notes blanches du piano

```cpp
uint8_t bitmap[19] = {0};

for (uint8_t note = 0; note <= 127; note++) {
    uint8_t chromaticPos = note % 12;
    // Notes blanches: C, D, E, F, G, A, B (0, 2, 4, 5, 7, 9, 11)
    if (chromaticPos == 0 || chromaticPos == 2 || 
        chromaticPos == 4 || chromaticPos == 5 || 
        chromaticPos == 7 || chromaticPos == 9 || 
        chromaticPos == 11) {
        uint8_t byteIndex = note / 7;
        uint8_t bitIndex = note % 7;
        bitmap[byteIndex] |= (1 << bitIndex);
    }
}
```

### Lecture du Bitmap (Backend)

```cpp
bool isNotePlayable(const uint8_t* bitmap, uint8_t note) {
    if (note > 127) return false;
    
    uint8_t byteIndex = note / 7;
    uint8_t bitIndex = note % 7;
    
    return (bitmap[byteIndex] & (1 << bitIndex)) != 0;
}
```

### Exemple Complet - Bloc 2

**Configuration** : Flûte C4 à B5 (60-83), toutes les notes actives

**Message SysEx** :
```
F0 7D 00 02 01
00 00 00 00 00 00 00 00  // Notes 0-55: inactives
7F 7F 7F 7E              // Notes 56-83: 60-83 actives
00 00 00 00 00 00 00     // Notes 84-127: inactives
00 00                    // Reserved
F7

Taille: 22 bytes
```

**Détail du bitmap pour notes 60-83** :
```
Byte 8:  0x7F = 0b01111111 → notes 56-62 (60,61,62 actives)
Byte 9:  0x7F = 0b01111111 → notes 63-69 (63-69 actives)
Byte 10: 0x7F = 0b01111111 → notes 70-76 (70-76 actives)
Byte 11: 0x7E = 0b01111110 → notes 77-83 (77-83 actives, 84 inactive)
```

---

## 🔮 Blocs 3-8 - Spécifications Futures

### Bloc 3 - CC Supportés (v2.0)

**Objectif** : Lister les Control Change messages supportés

**Format prévu** :
```
F0 7D <DeviceID> 03 02
<CCCount>            // Nombre de CC supportés (1-128)
<CC1> <CC2> ... <CCn>  // Liste des numéros CC (0-127)
F7
```

**Exemples de CC** :
- `1` : Modulation Wheel
- `2` : Breath Controller
- `7` : Volume
- `10` : Pan
- `11` : Expression
- `64` : Sustain Pedal
- `74` : Brightness

**Usage** :
- Permet au backend de savoir quels CC ont un effet
- Interface utilisateur peut afficher les contrôles disponibles
- Optimisation : ne pas envoyer de CC non supportés

---

### Bloc 4 - Capacités Air (v2.0)

**Objectif** : Détailler les capacités de contrôle par souffle/air

**Format prévu** :
```
F0 7D <DeviceID> 04 02
<BreathType>         // Type de capteur (0=None, 1=Pressure, 2=Flow, 3=Both)
<BreathCC>           // CC utilisé pour le souffle (2 ou autre)
<MinValue>           // Valeur min du capteur (0-127)
<MaxValue>           // Valeur max du capteur (0-127)
<Sensitivity>        // Sensibilité (0-127, 64=normal)
<ResponseCurve>      // Courbe de réponse (0=Linear, 1=Exp, 2=Log)
<Reserved[8]>        // Réservé
F7
```

**Usage** :
- Calibration automatique
- Adaptation des courbes de réponse
- Interface de configuration

---

### Bloc 5 - Capacités Lumières (v2.0)

**Objectif** : Détailler les capacités LED/lumière

**Format prévu** :
```
F0 7D <DeviceID> 05 02
<LedCount>           // Nombre de LEDs (0-255)
<LedType>            // Type (0=None, 1=Single, 2=RGB, 3=RGBW)
<Protocol>           // Protocole (0=None, 1=WS2812, 2=APA102, 3=DMX)
<Brightness>         // Luminosité par défaut (0-127)
<AnimationSupport>   // Animations supportées (bitfield)
<Reserved[12]>       // Réservé
F7
```

**Usage** :
- Configuration visuelle
- Synchronisation LED avec notes
- Effets visuels

---

### Bloc 7 - Capteurs/Feedback (v3.0)

**Objectif** : Monitoring temps réel des capteurs

**Format prévu** :
```
F0 7D <DeviceID> 07 03
<SensorCount>        // Nombre de capteurs (1-16)
[Pour chaque capteur:]
  <SensorID>         // ID du capteur
  <SensorType>       // Type (Pressure, Flex, Distance, etc.)
  <CurrentValue>     // Valeur actuelle (0-127)
  <MinValue>         // Min calibré
  <MaxValue>         // Max calibré
F7
```

**Usage** :
- Debugging
- Calibration
- Monitoring de santé de l'instrument

---

### Bloc 8 - Sync & Clock (v3.0)

**Objectif** : Capacités de synchronisation

**Format prévu** :
```
F0 7D <DeviceID> 08 03
<ClockSupport>       // Support MIDI Clock (0=No, 1=Yes)
<MTCSupport>         // Support MTC (0=No, 1=Yes)
<InternalTempo>      // Tempo interne (0=No, 1-250 BPM)
<Reserved[8]>        // Réservé
F7
```

**Usage** :
- Synchronisation avec DAW
- Séquenceurs internes
- Effets rythmiques

---

## 🔧 Gestion dans midiMind

### Architecture du Traitement SysEx

```
┌─────────────────┐
│  Instrument DIY │
│    (Arduino)    │
└────────┬────────┘
         │ SysEx Message
         ▼
┌─────────────────┐
│  MidiDevice     │◄─── Réception du message brut
│  (ALSA/Driver)  │
└────────┬────────┘
         │ vector<uint8_t>
         ▼
┌─────────────────┐
│  SysExHandler   │◄─── Distribution par type
└────────┬────────┘
         │
         ├─→ Standard SysEx (Identity, GM, etc.)
         │   └─→ SysExParser::parseIdentityReply()
         │
         └─→ Custom SysEx (0x7D)
             └─→ CustomSysExParser::parseBlockMessage()
                  │
                  ├─→ Bloc 1: parseIdentification()
                  │   └─→ CustomDeviceIdentity
                  │
                  ├─→ Bloc 2: parseNoteMap()
                  │   └─→ NoteMap (128-bit bitmap)
                  │
                  └─→ Blocs 3-8: parseFutureBlock()
                      └─→ Logging + Storage
```

### Flux de Données

#### 1. Réception

```cpp
// Dans MidiDevice::handleSysExMessage()
void handleSysExMessage(const std::vector<uint8_t>& data) {
    // Créer le message
    SysExMessage msg(data);
    
    // Valider
    if (!msg.isValid()) {
        Logger::error("Invalid SysEx message");
        return;
    }
    
    // Transférer au handler
    sysexHandler->handleSysExMessage(msg, deviceId);
}
```

#### 2. Dispatch

```cpp
// Dans SysExHandler::handleSysExMessage()
void handleSysExMessage(const SysExMessage& msg, const std::string& deviceId) {
    uint8_t manufacturerId = msg.getManufacturerId();
    
    if (manufacturerId == 0x7D) {
        // Custom SysEx
        handleCustomSysEx(msg, deviceId);
    } else if (manufacturerId == 0x7E) {
        // Universal Non-Realtime
        handleUniversalSysEx(msg, deviceId);
    }
    // etc.
}
```

#### 3. Parsing Custom

```cpp
// Dans SysExHandler::handleCustomSysEx()
void handleCustomSysEx(const SysExMessage& msg, const std::string& deviceId) {
    uint8_t blockId = msg.getRawData()[3];  // Byte 3
    uint8_t blockVersion = msg.getRawData()[4];  // Byte 4
    
    switch (blockId) {
        case 0x01:  // Identification
            {
                auto identity = CustomSysExParser::parseIdentification(msg);
                if (identity) {
                    storeDeviceIdentity(deviceId, *identity);
                    notifyUI("device_identified", identity->toJSON());
                }
            }
            break;
            
        case 0x02:  // Note Map
            {
                auto noteMap = CustomSysExParser::parseNoteMap(msg);
                if (noteMap) {
                    storeNoteMap(deviceId, *noteMap);
                    notifyUI("notemap_received", noteMap->toJSON());
                }
            }
            break;
            
        default:
            // Bloc non implémenté → Logging
            logUnknownBlock(blockId, blockVersion, msg);
            break;
    }
}
```

### Stockage des Données

#### Structure CustomDeviceIdentity

```cpp
struct CustomDeviceIdentity {
    // Bloc 1 - Identification
    uint32_t uniqueId;           // ID unique 28-bit
    std::string name;            // Nom de l'instrument
    uint8_t type;                // Type GM étendu
    uint8_t firstNote;           // Première note
    uint8_t noteCount;           // Nombre de notes
    uint8_t maxPolyphony;        // Polyphonie max
    uint8_t tuningMode;          // Mode d'accordage
    uint16_t responseDelay;      // Délai de réponse (ms)
    std::array<uint8_t, 4> firmwareVersion;  // Version [Major, Minor, Patch, Build]
    uint8_t flags;               // Capacités (bitfield)
    uint8_t programCount;        // Nombre de presets
    
    // Méthodes utiles
    uint8_t getLastNote() const { return firstNote + noteCount - 1; }
    bool hasVelocity() const { return (flags & 0x01) != 0; }
    bool hasAftertouch() const { return (flags & 0x02) != 0; }
    bool hasBreath() const { return (flags & 0x04) != 0; }
    std::string getFirmwareString() const {
        return std::to_string(firmwareVersion[0]) + "." +
               std::to_string(firmwareVersion[1]) + "." +
               std::to_string(firmwareVersion[2]) + "." +
               std::to_string(firmwareVersion[3]);
    }
};
```

#### Structure NoteMap

```cpp
struct NoteMap {
    std::array<uint8_t, 19> bitmap;  // 128 bits + padding
    
    // Test de jouabilité
    bool isNotePlayable(uint8_t note) const {
        if (note > 127) return false;
        uint8_t byteIndex = note / 7;
        uint8_t bitIndex = note % 7;
        return (bitmap[byteIndex] & (1 << bitIndex)) != 0;
    }
    
    // Liste toutes les notes jouables
    std::vector<uint8_t> getPlayableNotes() const {
        std::vector<uint8_t> notes;
        for (uint8_t note = 0; note <= 127; note++) {
            if (isNotePlayable(note)) {
                notes.push_back(note);
            }
        }
        return notes;
    }
    
    // Compte le nombre de notes
    uint8_t countPlayableNotes() const {
        uint8_t count = 0;
        for (uint8_t byte : bitmap) {
            // Count bits set (population count)
            uint8_t n = byte;
            while (n) {
                count += n & 1;
                n >>= 1;
            }
        }
        return count;
    }
};
```

#### Cache des Identités

```cpp
// Dans SysExHandler
class SysExHandler {
private:
    // Cache des identités custom
    std::map<std::string, CustomDeviceIdentity> customIdentities_;
    
    // Cache des note maps
    std::map<std::string, NoteMap> noteMaps_;
    
    // Mutex pour thread-safety
    mutable std::mutex customMutex_;
    
public:
    // Stockage
    void storeDeviceIdentity(const std::string& deviceId,
                            const CustomDeviceIdentity& identity) {
        std::lock_guard<std::mutex> lock(customMutex_);
        customIdentities_[deviceId] = identity;
        Logger::info("CustomSysEx", "Stored identity for " + deviceId);
    }
    
    void storeNoteMap(const std::string& deviceId,
                     const NoteMap& noteMap) {
        std::lock_guard<std::mutex> lock(customMutex_);
        noteMaps_[deviceId] = noteMap;
        Logger::info("CustomSysEx", "Stored note map for " + deviceId);
    }
    
    // Récupération
    std::optional<CustomDeviceIdentity> getCustomIdentity(
        const std::string& deviceId) const {
        std::lock_guard<std::mutex> lock(customMutex_);
        auto it = customIdentities_.find(deviceId);
        if (it != customIdentities_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::optional<NoteMap> getNoteMap(
        const std::string& deviceId) const {
        std::lock_guard<std::mutex> lock(customMutex_);
        auto it = noteMaps_.find(deviceId);
        if (it != noteMaps_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};
```

---

## 📡 Remontée vers l'Interface Utilisateur

### Architecture des Notifications

```
Backend (C++)               WebSocket              Frontend (JS)
┌─────────────┐            ┌─────────┐           ┌──────────────┐
│ SysExHandler├───JSON────►│ WSServer├──JSON────►│  UI Manager  │
└─────────────┘            └─────────┘           └──────┬───────┘
                                                         │
                                                         ├─→ Device List
                                                         ├─→ Note Map View
                                                         └─→ Instrument Config
```

### Messages JSON

#### 1. Device Identified

**Événement** : Bloc 1 reçu et parsé

**Message JSON** :
```json
{
    "type": "device_identified",
    "timestamp": 1633024800000,
    "device": {
        "id": "device_usb_0",
        "uniqueId": "0x1234567",
        "name": "MaFlute_DIY",
        "type": {
            "code": 128,
            "category": "Wind DIY"
        },
        "range": {
            "firstNote": 60,
            "lastNote": 83,
            "noteCount": 24,
            "firstNoteName": "C4",
            "lastNoteName": "B5"
        },
        "polyphony": {
            "maxVoices": 0,
            "type": "monophonic"
        },
        "tuning": {
            "mode": "chromatic",
            "code": 0
        },
        "latency": {
            "responseDelay": 30,
            "unit": "ms"
        },
        "firmware": {
            "version": "1.2.0.10",
            "major": 1,
            "minor": 2,
            "patch": 0,
            "build": 10
        },
        "capabilities": {
            "velocity": true,
            "aftertouch": false,
            "breath": true,
            "pitchBend": true,
            "modulation": false,
            "expression": false,
            "sustain": false,
            "programChange": true
        },
        "programs": {
            "count": 4,
            "list": [0, 1, 2, 3]
        }
    }
}
```

#### 2. Note Map Received

**Événement** : Bloc 2 reçu et parsé

**Message JSON** :
```json
{
    "type": "notemap_received",
    "timestamp": 1633024801000,
    "device": {
        "id": "device_usb_0",
        "name": "MaFlute_DIY"
    },
    "noteMap": {
        "totalNotes": 24,
        "playableNotes": [
            60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
            72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83
        ],
        "noteNames": [
            "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4",
            "G#4", "A4", "A#4", "B4", "C5", "C#5", "D5", "D#5",
            "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5"
        ],
        "ranges": [
            {
                "start": 60,
                "end": 83,
                "startName": "C4",
                "endName": "B5",
                "octaves": 2
            }
        ],
        "bitmap": "00000000007F7F7F7E0000000000000"  // Hex string
    }
}
```

#### 3. Unknown Block Received

**Événement** : Bloc non implémenté reçu (3-8)

**Message JSON** :
```json
{
    "type": "unknown_block_received",
    "timestamp": 1633024802000,
    "device": {
        "id": "device_usb_0",
        "name": "MaFlute_DIY"
    },
    "block": {
        "id": 3,
        "version": 2,
        "size": 25,
        "data": "F07D0003020F010207...",  // Hex string
        "note": "Bloc non encore implémenté dans cette version"
    }
}
```

### Interface Utilisateur

#### Affichage de l'Instrument

**Vue Device List** :
```
┌──────────────────────────────────────────────┐
│ 🎵 MaFlute_DIY                        [v1.2] │
│                                               │
│ 🆔 ID: 0x1234567                             │
│ 🎹 Type: Wind DIY                            │
│ 🎼 Range: C4 - B5 (24 notes)                │
│ 🎛️  Polyphony: Mono                          │
│ ⚡ Latency: 30ms                             │
│                                               │
│ Capabilities:                                 │
│ ✅ Velocity    ✅ Breath     ✅ Pitch Bend   │
│ ❌ Aftertouch  ❌ Modulation ❌ Expression   │
│                                               │
│ Programs: 4 presets available                 │
│                                               │
│ [View Note Map] [Configure] [Test]           │
└──────────────────────────────────────────────┘
```

#### Note Map Visualisation

**Vue Interactive** :
```
┌──────────────────────────────────────────────────────────┐
│ Note Map - MaFlute_DIY                                    │
├──────────────────────────────────────────────────────────┤
│                                                           │
│  Octave 2         Octave 3         Octave 4              │
│  ░░░░░░░░░░░░     ░░░░░░░░░░░░     ▓▓▓▓▓▓▓▓▓▓▓▓         │
│                                     ↑ C4-B4              │
│                                                           │
│  Octave 5         Octave 6         Octave 7              │
│  ▓▓▓▓▓▓▓▓▓▓▓▓     ░░░░░░░░░░░░     ░░░░░░░░░░░░         │
│  ↑ C5-B5                                                 │
│                                                           │
│  ▓ = Playable (24 notes)    ░ = Not playable             │
│                                                           │
│  [Export] [Test Range] [Calibrate]                       │
└──────────────────────────────────────────────────────────┘
```

---

## 🔄 Flux de Communication

### Séquence de Connexion Complète

```
Arduino                Backend               UI
   │                      │                   │
   │ ─────USB Connect────►│                   │
   │                      │                   │
   │                      │◄──Auto-Identify───┤ (si activé)
   │                      │  Identity Request  │
   │◄─────────────────────┤                   │
   │  F0 7E 7F 06 01 F7   │                   │
   │                      │                   │
   │──────Bloc 1─────────►│                   │
   │  F0 7D 00 01 01 ... │                   │
   │                      │                   │
   │                      ├──device_identified─►
   │                      │  (JSON)           │
   │                      │                   │
   │──────Bloc 2─────────►│                   │
   │  F0 7D 00 02 01 ... │                   │
   │                      │                   │
   │                      ├─notemap_received──►
   │                      │  (JSON)           │
   │                      │                   │
   │◄─────Ready───────────┤                   │
   │                      │                   │
   │──MIDI Notes────────►│                   │
   │  90 3C 64 (C4)      │                   │
   │                      │                   │
```

### Gestion des Erreurs

#### Message SysEx Invalide

```cpp
if (!msg.isValid()) {
    Logger::error("CustomSysEx", "Invalid message from " + deviceId);
    
    // Log pour debugging
    Logger::debug("CustomSysEx", "Raw data: " + msg.toHexString());
    
    // Notifier l'UI
    notifyUI("sysex_error", {
        {"device", deviceId},
        {"error", "Invalid SysEx message format"},
        {"data", msg.toHexString()}
    });
    
    return;
}
```

#### Bloc Incomplet

```cpp
if (msg.getSize() < expectedSize) {
    Logger::warn("CustomSysEx", "Incomplete Block " + 
                std::to_string(blockId) + " from " + deviceId);
    
    // Log les données partielles
    Logger::debug("CustomSysEx", "Received: " + 
                 std::to_string(msg.getSize()) + " bytes, expected: " + 
                 std::to_string(expectedSize) + " bytes");
    
    return;
}
```

#### Version Non Supportée

```cpp
if (blockVersion > SUPPORTED_VERSION) {
    Logger::warn("CustomSysEx", "Block " + std::to_string(blockId) + 
                " version " + std::to_string(blockVersion) + 
                " not supported (max: " + 
                std::to_string(SUPPORTED_VERSION) + ")");
    
    // Log mais ne rejette pas → compatibilité future
    // Essayer de parser quand même
}
```

---

## 📊 Exemples Pratiques

### Exemple 1 : Flûte à Bec Alto

**Configuration** :
- Nom: `"RecorderAlto"`
- Type: Wind DIY (0x80)
- Plage: F4 (65) à G6 (91) = 27 notes
- Monophonique
- Mode chromatique
- Latency: 25ms
- Velocity + Breath
- 2 presets (baroque, moderne)

**Bloc 1** :
```
F0 7D 00 01 01
12 34 56 78              // Unique ID (exemple)
52 65 63 6F 72 64 65 72 41 6C 74 6F 00  // "RecorderAlto\0"
80                       // Wind DIY
41                       // First: 65 (F4)
1B                       // Count: 27
00                       // Mono
00                       // Chromatic
19 00                    // Delay: 25ms
01 00 00 05              // FW: 1.0.0.5
05                       // Flags: Velocity+Breath
02                       // 2 presets
F7
```

**Bloc 2** (notes 65-91 actives) :
```
F0 7D 00 02 01
00 00 00 00 00 00 00 00 00  // Notes 0-62: off
7E 7F 7F 7F 0F              // Notes 63-97: 65-91 on
00 00 00 00 00 00           // Notes 98-127: off
00 00                        // Reserved
F7
```

---

### Exemple 2 : Pad Pentatonique DIY

**Configuration** :
- Nom: `"PentaPad_16"`
- Type: Percussion DIY (0x82)
- Notes pentatoniques: C3, D3, E3, G3, A3 + octave (10 notes)
- Polyphonie 4 voix
- Mode pentatonique
- Latency: 5ms (capteurs piézo)
- Velocity + Program Change
- 8 gammes préprogrammées

**Bloc 1** :
```
F0 7D 00 01 01
AA BB CC DD              // Unique ID
50 65 6E 74 61 50 61 64 5F 31 36 00  // "PentaPad_16\0"
82                       // Percussion DIY
24                       // First: 36 (C2)
0A                       // Count: 10
04                       // Poly: 4 voices
02                       // Pentatonic
05 00                    // Delay: 5ms
01 01 00 01              // FW: 1.1.0.1
81                       // Flags: Velocity+PgmChange
08                       // 8 presets
F7
```

**Bloc 2** (notes pentatoniques seulement) :
```
F0 7D 00 02 01
00 00 00 00 00           // Notes 0-34: off
15 00 15                 // Notes 35-55: pattern penta
00 00 00 00 00 00 00 00 00 00 00  // Notes 56-127: off
00 00                    // Reserved
F7

Détail bitmap:
0x15 = 0b00010101 → bits 0,2,4 → notes C, D, E (pattern)
```

---

### Exemple 3 : Contrôleur Gestuel

**Configuration** :
- Nom: `"GestureCtrl_X"`
- Type: Controller DIY (0x84)
- Pas de notes (contrôle via CC uniquement)
- Type: 0 notes (instrument de contrôle pur)
- Latency: 10ms
- Toutes les capacités CC
- Pas de presets

**Bloc 1** :
```
F0 7D 00 01 01
11 22 33 44              // Unique ID
47 65 73 74 75 72 65 43 74 72 6C 5F 58 00  // "GestureCtrl_X\0"
84                       // Controller DIY
00                       // First: 0 (N/A)
00                       // Count: 0 (pas de notes)
00                       // Mono (N/A)
08                       // Mode: Mono control
0A 00                    // Delay: 10ms
02 00 00 01              // FW: 2.0.0.1
7F                       // Flags: All (7 bits)
00                       // No presets
F7
```

**Bloc 2** (aucune note) :
```
F0 7D 00 02 01
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  // Tout à 0
00 00                    // Reserved
F7
```

**Bloc 3** (CC supportés) - Futur :
```
F0 7D 00 03 02
08                       // 8 CC supportés
01 02 07 0A 0B 40 42 43  // Mod, Breath, Vol, Pan, Expr, Sustain, etc.
F7
```

---

## 📝 Logging et Debugging

### Niveaux de Log

#### DEBUG - Détails complets

```
[DEBUG] [CustomSysEx] Received Bloc 1 from device_usb_0
[DEBUG] [CustomSysEx]   Unique ID: 0x1234567
[DEBUG] [CustomSysEx]   Name: MaFlute_DIY
[DEBUG] [CustomSysEx]   Type: 0x80 (Wind DIY)
[DEBUG] [CustomSysEx]   Range: 60-83 (24 notes)
[DEBUG] [CustomSysEx]   Polyphony: 0 (mono)
[DEBUG] [CustomSysEx]   Tuning: 0x00 (chromatic)
[DEBUG] [CustomSysEx]   Delay: 30ms
[DEBUG] [CustomSysEx]   Firmware: 1.2.0.10
[DEBUG] [CustomSysEx]   Flags: 0x05 (Velocity, Breath)
[DEBUG] [CustomSysEx]   Programs: 4
```

#### INFO - Événements importants

```
[INFO] [CustomSysEx] Device identified: MaFlute_DIY (0x1234567)
[INFO] [CustomSysEx] Note map received: 24 playable notes (60-83)
[INFO] [CustomSysEx] Unknown block received: Block 3 v2.0 (25 bytes)
```

#### WARN - Problèmes non bloquants

```
[WARN] [CustomSysEx] Incomplete Bloc 1 from device_usb_0 (30/34 bytes)
[WARN] [CustomSysEx] Block 3 version 2.0 not supported (max: 1.0)
[WARN] [CustomSysEx] Note map conflicts with identity: 20 vs 24 notes
```

#### ERROR - Erreurs critiques

```
[ERROR] [CustomSysEx] Invalid SysEx message from device_usb_0
[ERROR] [CustomSysEx] Failed to parse Bloc 1: Invalid name string
[ERROR] [CustomSysEx] Memory allocation failed for note map
```

### Logs des Blocs Non Implémentés

**Objectif** : Capturer toutes les données des blocs futurs pour analyse

```cpp
void logUnknownBlock(uint8_t blockId, uint8_t version, 
                     const SysExMessage& msg) {
    Logger::info("CustomSysEx", "Unknown block received:");
    Logger::info("CustomSysEx", "  Block ID: " + std::to_string(blockId));
    Logger::info("CustomSysEx", "  Version: " + std::to_string(version));
    Logger::info("CustomSysEx", "  Size: " + std::to_string(msg.getSize()));
    
    // Log les données en hex
    std::string hexData = msg.toHexString();
    Logger::info("CustomSysEx", "  Data: " + hexData);
    
    // Sauvegarder dans un fichier pour analyse future
    std::ofstream logFile("unknown_blocks.log", std::ios::app);
    logFile << "Timestamp: " << getCurrentTimestamp() << "\n";
    logFile << "Block ID: " << (int)blockId << "\n";
    logFile << "Version: " << (int)version << "\n";
    logFile << "Data: " << hexData << "\n";
    logFile << "---\n";
    logFile.close();
    
    // Notifier l'UI pour affichage
    notifyUI("unknown_block_received", {
        {"blockId", blockId},
        {"version", version},
        {"size", msg.getSize()},
        {"data", hexData},
        {"note", "Block not yet implemented"}
    });
}
```

---

## 🎯 Recommandations d'Implémentation

### Pour Arduino (Instrument)

1. **Envoi au démarrage** :
   - Envoyer Bloc 1 immédiatement après connexion USB
   - Attendre 100ms puis envoyer Bloc 2
   - Réenvoyer si Identity Request reçu

2. **Optimisations** :
   - Pré-calculer les messages SysEx en constantes
   - Utiliser PROGMEM pour économiser la RAM
   - Envoyer en une seule fois (pas byte par byte)

3. **Validation** :
   - Vérifier la taille finale du message
   - Tester avec MIDI-OX ou autre analyseur
   - Logger les envois pour debugging

### Pour Backend (C++)

1. **Parser robuste** :
   - Valider chaque champ
   - Gérer les messages tronqués
   - Accepter les versions supérieures (avec warning)

2. **Cache intelligent** :
   - Mémoriser les identités
   - Invalider si reconnexion détectée
   - Expiration après déconnexion

3. **Logging complet** :
   - Toujours logger les blocs inconnus
   - Sauvegarder les données brutes
   - Faciliter le debugging à distance

### Pour Interface (JavaScript)

1. **Affichage temps réel** :
   - Notification toast lors de la découverte
   - Mise à jour immédiate de la liste
   - Animation de la note map

2. **Validation visuelle** :
   - Colorier les notes jouables
   - Afficher les conflits
   - Indicateurs de santé (latence, etc.)

3. **Export/Import** :
   - Sauvegarder les configurations
   - Partager entre utilisateurs
   - Backup automatique

---

## ✅ Checklist de Validation

### Pour un Instrument DIY

- [ ] Bloc 1 envoyé au démarrage
- [ ] Bloc 2 envoyé après Bloc 1
- [ ] Unique ID persistant (stocké en EEPROM)
- [ ] Nom unique et descriptif
- [ ] Plage de notes cohérente (FirstNote + NoteCount)
- [ ] Note map correspond à la réalité
- [ ] Flags reflètent les vraies capacités
- [ ] Version firmware correcte
- [ ] Messages testés avec MIDI-OX

### Pour le Backend

- [ ] Parsing Bloc 1 fonctionnel
- [ ] Parsing Bloc 2 fonctionnel
- [ ] Cache des identités actif
- [ ] Notification UI fonctionnelle
- [ ] Logging des blocs inconnus
- [ ] Gestion des erreurs complète
- [ ] Thread-safety assurée
- [ ] Tests unitaires passés

### Pour l'Interface

- [ ] Affichage de l'identité
- [ ] Visualisation note map
- [ ] Indicateurs de capacités
- [ ] Notifications temps réel
- [ ] Export/Import configs
- [ ] Documentation utilisateur
- [ ] Tests d'intégration OK

---

## 📚 Ressources

### Spécifications MIDI

- MIDI 1.0 Specification
- Universal System Exclusive Messages
- General MIDI Level 1
- Manufacturer IDs List

### Outils de Test

- **MIDI-OX** : Moniteur/Analyseur MIDI (Windows)
- **MIDI Monitor** : Analyseur (macOS)
- **amidi** : CLI Linux
- **QMidiNet** : Test réseau MIDI

### Documentation Connexe

- `README.md` : Vue d'ensemble midiMind
- `ARCHITECTURE.md` : Architecture système
- `SysExHandler.h` : API C++ du handler
- `CustomSysExProtocol.h` : Définitions des constantes

---

## 🔄 Historique des Versions

### v1.0 - Octobre 2025

- ✅ Bloc 1 : Identification complète
- ✅ Bloc 2 : Note Map 128-bit
- ✅ Documentation protocole
- ✅ Implémentation backend
- ✅ Interface UI

### v2.0 - Planifié Q1 2026

- 🔮 Bloc 3 : CC Supportés
- 🔮 Bloc 4 : Capacités Air
- 🔮 Bloc 5 : Capacités Lumières

### v3.0 - Planifié Q2 2026

- 🔮 Bloc 7 : Monitoring capteurs
- 🔮 Bloc 8 : Sync & Clock

---

**Fin de la documentation**

*MidiMind v3.0 - Custom SysEx Protocol*
*© 2025 MidiMind Team*