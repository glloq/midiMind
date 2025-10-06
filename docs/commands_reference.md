# ⚡ Référence Commandes - MidiMind Backend

> Guide complet de toutes les commandes API WebSocket

---

## 📋 Table des Matières

1. [Vue d'ensemble](#vue-densemble)
2. [Commandes Files (5)](#-commandes-files)
3. [Commandes Instruments (5)](#-commandes-instruments)
4. [Commandes Editor (6)](#-commandes-editor)
5. [Commandes Playback (8)](#-commandes-playback)
6. [Commandes System (2)](#-commandes-system)
7. [Exemples d'intégration](#-exemples-dintégration)

---

## Vue d'ensemble

### Total : 26 Commandes

| Catégorie | Nombre | Module principal |
|-----------|--------|-----------------|
| **Files** | 5 | MidiFileManager |
| **Instruments** | 5 | MidiDeviceManager |
| **Editor** | 6 | MidiFileManager |
| **Playback** | 8 | MidiPlayer |
| **System** | 2 | Application |

### Mapping Commande → Code

| Commande | Fichier Handler | Module | Fonction |
|----------|----------------|--------|----------|
| `files.*` | `api/files.cpp` | MidiFileManager | Gestion fichiers |
| `instruments.*` | `api/instruments.cpp` | MidiDeviceManager | Gestion devices |
| `editor.*` | `api/editor.cpp` | MidiFileManager | Édition MIDI |
| `playback.*` | `api/playback.cpp` | MidiPlayer | Lecture MIDI |
| `system.*` | `api/CommandProcessorV2.cpp` | Application | Info système |

---

## 📁 Commandes Files

### files.list

Liste tous les fichiers MIDI de la bibliothèque.

**Handler :** `api/files.cpp`  
**Module :** `MidiFileManager::scanLibrary()`

#### Requête

```json
{
  "command": "files.list"
}
```

#### Réponse

```json
{
  "success": true,
  "data": {
    "files": [
      {
        "id": "1",
        "path": "/home/pi/midi-files/song.mid",
        "name": "song.mid",
        "duration": 180000,
        "tracks": 4,
        "tempo": 120,
        "time_signature": "4/4"
      }
    ],
    "count": 1
  },
  "timestamp": 1696435200000
}
```

---

### files.scan

Scanne un répertoire pour trouver des fichiers MIDI.

**Handler :** `api/files.cpp`  
**Module :** `MidiFileManager::scanDirectory()`

#### Requête

```json
{
  "command": "files.scan",
  "params": {
    "directory": "/home/pi/midi-files"
  }
}
```

#### Paramètres

| Paramètre | Type | Obligatoire | Description |
|-----------|------|-------------|-------------|
| `directory` | string | ✅ | Chemin du répertoire à scanner |

#### Réponse

```json
{
  "success": true,
  "data": {
    "files": [...],
    "count": 10,
    "new_files": 3
  }
}
```

---

### files.analyze

Analyse détaillée d'un fichier MIDI.

**Handler :** `api/files.cpp`  
**Module :** `MidiFileAnalyzer::analyzeFile()`

#### Requête

```json
{
  "command": "files.analyze",
  "params": {
    "file_id": "12345"
  }
}
```

#### Paramètres

| Paramètre | Type | Obligatoire | Description |
|-----------|------|-------------|-------------|
| `file_id` | string | ✅ | ID du fichier à analyser |

#### Réponse

```json
{
  "success": true,
  "data": {
    "tracks": 4,
    "duration": 180000,
    "tempo": 120,
    "time_signature": "4/4",
    "notes_count": 1523,
    "key_signature": "C major",
    "tracks_info": [
      {
        "id": 0,
        "name": "Piano",
        "notes": 456,
        "program": 0
      }
    ]
  }
}
```

---

### files.load

Charge un fichier MIDI en mémoire.

**Handler :** `api/files.cpp`  
**Module :** `MidiFileManager::loadFile()`

#### Requête

```json
{
  "command": "files.load",
  "params": {
    "file_path": "/home/pi/midi-files/song.mid"
  }
}
```

#### Paramètres

| Paramètre | Type | Obligatoire | Description |
|-----------|------|-------------|-------------|
| `file_path` | string | ✅ | Chemin complet du fichier |

#### Réponse

```json
{
  "success": true,
  "data": {
    "file_id": "12345",
    "loaded": true,
    "tracks": 4,
    "duration": 180000
  }
}
```

---

### files.export

Exporte un fichier au format MIDI.

**Handler :** `api/files.cpp`  
**Module :** `MidiFileWriter::exportFile()`

#### Requête

```json
{
  "command": "files.export",
  "params": {
    "file_id": "12345",
    "output_path": "/home/pi/exports/song_export.mid",
    "format": "midi_1"
  }
}
```

#### Paramètres

| Paramètre | Type | Obligatoire | Description |
|-----------|------|-------------|-------------|
| `file_id` | string | ✅ | ID du fichier à exporter |
| `output_path` | string | ✅ | Chemin de sortie |
| `format` | string | ❌ | Format (midi_0, midi_1) |

#### Réponse

```json
{
  "success": true,
  "data": {
    "exported": true,
    "path": "/home/pi/exports/song_export.mid",
    "size": 15234
  }
}
```

---

## 🎹 Commandes Instruments

### instruments.list

Liste tous les instruments MIDI disponibles.

**Handler :** `api/instruments.cpp`  
**Module :** `MidiDeviceManager::getAllDevices()`

#### Requête

```json
{
  "command": "instruments.list"
}
```

#### Réponse

```json
{
  "success": true,
  "data": {
    "devices": [
      {
        "id": "usb_synth_1",
        "name": "Korg Minilogue",
        "type": "USB",
        "connected": true,
        "ports": {
          "input": true,
          "output": true
        }
      },
      {
        "id": "rtpmidi_1",
        "name": "iPad MIDI",
        "type": "RTP-MIDI",
        "connected": false
      }
    ],
    "count": 2
  }
}
```

---

### instruments.connect

Connecte un instrument MIDI.

**Handler :** `api/instruments.cpp`  
**Module :** `MidiDeviceManager::connectDevice()`

#### Requête

```json
{
  "command": "instruments.connect",
  "params": {
    "device_id": "usb_synth_1"
  }
}
```

#### Paramètres

| Paramètre | Type | Obligatoire | Description |
|-----------|------|-------------|-------------|
| `device_id` | string | ✅ | ID du device à connecter |

#### Réponse

```json
{
  "success": true,
  "data": {
    "device_id": "usb_synth_1",
    "name": "Korg Minilogue",
    "status": "connected",
    "ports": {
      "input": true,
      "output": true
    }
  }
}
```

#### Événement broadcast

```json
{
  "type": "device_connected",
  "data": {
    "device_id": "usb_synth_1",
    "name": "Korg Minilogue"
  }
}
```

---

### instruments.disconnect

Déconnecte un instrument MIDI.

**Handler :** `api/instruments.cpp`  
**Module :** `MidiDeviceManager::disconnectDevice()`

#### Requête

```json
{
  "command": "instruments.disconnect",
  "params": {
    "device_id": "usb_synth_1"
  }
}
```

#### Paramètres

| Paramètre | Type | Obligatoire | Description |
|-----------|------|-------------|-------------|
| `device_id` | string | ✅ | ID du device à déconnecter |

#### Réponse

```json
{
  "success": true,
  "data": {
    "device_id": "usb_synth_1",
    "status": "disconnected"
  }
}
```

---

### instruments.test

Envoie une note de test à un instrument.

**Handler :** `api/instruments.cpp`  
**Module :** `MidiDevice::sendTestNote()`

#### Requête

```json
{
  "command": "instruments.test",
  "params": {
    "device_id": "usb_synth_1",
    "note": 60,
    "velocity": 100,
    "duration": 500
  }
}
```

#### Paramètres

| Paramètre | Type | Obligatoire | Défaut | Description |
|-----------|------|-------------|--------|-------------|
| `device_id` | string | ✅ | - | ID du device |
| `note` | number | ❌ | 60 | Note MIDI (0-127) |
| `velocity` | number | ❌ | 100 | Vélocité (0-127) |
| `duration` | number | ❌ | 500 | Durée en ms |

#### Réponse

```json
{
  "success": true,
  "data": {
    "note_sent": true,
    "note": 60,
    "device_id": "usb_synth_1"
  }
}
```

---

### instruments.scan

Scanne les instruments MIDI disponibles.

**Handler :** `api/instruments.cpp`  
**Module :** `MidiDeviceManager::scanDevices()`

#### Requête

```json
{
  "command": "instruments.scan"
}
```

#### Réponse

```json
{
  "success": true,
  "data": {
    "devices": [...],
    "count": 5,
    "new_devices": 2
  }
}
```

---

## ✏️ Commandes Editor

### editor.load

Charge un fichier MIDI dans l'éditeur.

**Handler :** `api/editor.cpp`  
**Module :** `MidiFileManager::loadAsJsonMidi()`

#### Requête

```json
{
  "command": "editor.load",
  "params": {
    "file_path": "/home/pi/midi-files/song.mid"
  }
}
```

#### Réponse

```json
{
  "success": true,
  "data": {
    "file_id": "12345",
    "jsonmidi": {
      "header": {
        "format": 1,
        "tracks": 4,
        "ppq": 480
      },
      "tracks": [...]
    }
  }
}
```

---

### editor.save

Sauvegarde les modifications de l'éditeur.

**Handler :** `api/editor.cpp`  
**Module :** `MidiFileWriter::saveFromJsonMidi()`

#### Requête

```json
{
  "command": "editor.save",
  "params": {
    "file_path": "/home/pi/midi-files/song.mid",
    "jsonmidi": {...}
  }
}
```

#### Paramètres

| Paramètre | Type | Obligatoire | Description |
|-----------|------|-------------|-------------|
| `file_path` | string | ✅ | Chemin de sauvegarde |
| `jsonmidi` | object | ✅ | Données MIDI en JSON |

#### Réponse

```json
{
  "success": true,
  "data": {
    "saved": true,
    "path": "/home/pi/midi-files/song.mid"
  }
}
```

---

### editor.addNote

Ajoute une note MIDI.

**Handler :** `api/editor.cpp`  
**Module :** `MidiEditor::addNote()`

#### Requête

```json
{
  "command": "editor.addNote",
  "params": {
    "track": 0,
    "time": 0,
    "note": 60,
    "velocity": 100,
    "duration": 480
  }
}
```

#### Paramètres

| Paramètre | Type | Obligatoire | Description |
|-----------|------|-------------|-------------|
| `track` | number | ✅ | Index de la piste |
| `time` | number | ✅ | Position en ticks |
| `note` | number | ✅ | Note MIDI (0-127) |
| `velocity` | number | ✅ | Vélocité (0-127) |
| `duration` | number | ✅ | Durée en ticks |

#### Réponse

```json
{
  "success": true,
  "data": {
    "note_id": "note_12345",
    "track": 0,
    "time": 0
  }
}
```

---

### editor.deleteNote

Supprime une note MIDI.

**Handler :** `api/editor.cpp`  
**Module :** `MidiEditor::deleteNote()`

#### Requête

```json
{
  "command": "editor.deleteNote",
  "params": {
    "note_id": "note_12345"
  }
}
```

#### Réponse

```json
{
  "success": true,
  "data": {
    "deleted": true,
    "note_id": "note_12345"
  }
}
```

---

### editor.addCC

Ajoute un Control Change.

**Handler :** `api/editor.cpp`  
**Module :** `MidiEditor::addControlChange()`

#### Requête

```json
{
  "command": "editor.addCC",
  "params": {
    "track": 0,
    "time": 0,
    "controller": 7,
    "value": 127
  }
}
```

#### Paramètres

| Paramètre | Type | Obligatoire | Description |
|-----------|------|-------------|-------------|
| `track` | number | ✅ | Index de la piste |
| `time` | number | ✅ | Position en ticks |
| `controller` | number | ✅ | CC number (0-127) |
| `value` | number | ✅ | Valeur (0-127) |

#### Réponse

```json
{
  "success": true,
  "data": {
    "cc_id": "cc_12345",
    "track": 0,
    "controller": 7
  }
}
```

---

### editor.undo

Annule la dernière action.

**Handler :** `api/editor.cpp`  
**Module :** `MidiEditor::undo()`

#### Requête

```json
{
  "command": "editor.undo"
}
```

#### Réponse

```json
{
  "success": true,
  "data": {
    "undone": true,
    "action": "addNote"
  }
}
```

---

## ▶️ Commandes Playback

### playback.load

Charge un fichier dans le player.

**Handler :** `api/playback.cpp`  
**Module :** `MidiPlayer::loadFile()`

#### Requête

```json
{
  "command": "playback.load",
  "params": {
    "file_path": "/home/pi/midi-files/song.mid"
  }
}
```

#### Réponse

```json
{
  "success": true,
  "data": {
    "loaded": true,
    "duration": 180000,
    "tracks": 4,
    "tempo": 120
  }
}
```

---

### playback.play

Démarre la lecture.

**Handler :** `api/playback.cpp`  
**Module :** `MidiPlayer::play()`

#### Requête

```json
{
  "command": "playback.play"
}
```

#### Réponse

```json
{
  "success": true,
  "data": {
    "state": "playing",
    "position": 0,
    "tempo": 120
  }
}
```

---

### playback.pause

Met en pause la lecture.

**Handler :** `api/playback.cpp`  
**Module :** `MidiPlayer::pause()`

#### Requête

```json
{
  "command": "playback.pause"
}
```

#### Réponse

```json
{
  "success": true,
  "data": {
    "state": "paused",
    "position": 5000
  }
}
```

---

### playback.stop

Arrête la lecture.

**Handler :** `api/playback.cpp`  
**Module :** `MidiPlayer::stop()`

#### Requête

```json
{
  "command": "playback.stop"
}
```

#### Réponse

```json
{
  "success": true,
  "data": {
    "state": "stopped",
    "position": 0
  }
}
```

---

### playback.seek

Déplace la position de lecture.

**Handler :** `api/playback.cpp`  
**Module :** `MidiPlayer::seek()`

#### Requête

```json
{
  "command": "playback.seek",
  "params": {
    "position": 10000
  }
}
```

#### Paramètres

| Paramètre | Type | Obligatoire | Description |
|-----------|------|-------------|-------------|
| `position` | number | ✅ | Position en ms |

#### Réponse

```json
{
  "success": true,
  "data": {
    "position": 10000,
    "bar": 20,
    "beat": 1
  }
}
```

---

### playback.status

Obtient l'état de la lecture.

**Handler :** `api/playback.cpp`  
**Module :** `MidiPlayer::getStatus()`

#### Requête

```json
{
  "command": "playback.status"
}
```

#### Réponse

```json
{
  "success": true,
  "data": {
    "state": "playing",
    "position": 5000,
    "duration": 180000,
    "tempo": 120,
    "transpose": 0,
    "bar": 10,
    "beat": 3
  }
}
```

---

### playback.tempo

Change le tempo.

**Handler :** `api/playback.cpp`  
**Module :** `MidiPlayer::setTempo()`

#### Requête

```json
{
  "command": "playback.tempo",
  "params": {
    "bpm": 140
  }
}
```

#### Paramètres

| Paramètre | Type | Obligatoire | Description |
|-----------|------|-------------|-------------|
| `bpm` | number | ✅ | Tempo en BPM |

#### Réponse

```json
{
  "success": true,
  "data": {
    "tempo": 140
  }
}
```

---

### playback.transpose

Transpose les notes.

**Handler :** `api/playback.cpp`  
**Module :** `MidiPlayer::setTranspose()`

#### Requête

```json
{
  "command": "playback.transpose",
  "params": {
    "semitones": 12
  }
}
```

#### Paramètres

| Paramètre | Type | Obligatoire | Description |
|-----------|------|-------------|-------------|
| `semitones` | number | ✅ | Demi-tons (-12 à +12) |

#### Réponse

```json
{
  "success": true,
  "data": {
    "transpose": 12
  }
}
```

---

## ⚙️ Commandes System

### system.status

Obtient l'état complet du système.

**Handler :** `api/CommandProcessorV2.cpp`  
**Module :** `Application::getSystemStatus()`

#### Requête

```json
{
  "command": "system.status"
}
```

#### Réponse

```json
{
  "success": true,
  "data": {
    "uptime": 3600,
    "cpu_usage": 25.5,
    "ram_usage": 512,
    "temperature": 45.2,
    "disk_usage": 60.5,
    "devices_connected": 3,
    "player_state": "playing",
    "network": {
      "wifi": true,
      "rtpmidi": true
    }
  }
}
```

---

### system.commands

Liste toutes les commandes disponibles.

**Handler :** `api/CommandProcessorV2.cpp`  
**Module :** `CommandFactory::listCommands()`

#### Requête

```json
{
  "command": "system.commands"
}
```

#### Réponse

```json
{
  "success": true,
  "data": {
    "total": 26,
    "commands": [
      "files.list",
      "files.scan",
      "instruments.connect",
      ...
    ],
    "by_category": {
      "files": 5,
      "instruments": 5,
      "editor": 6,
      "playback": 8,
      "system": 2
    }
  }
}
```

---

## 🚀 Exemples d'intégration

### Exemple 1 : Session complète

```javascript
const ws = new WebSocket('ws://localhost:8080');

async function startSession() {
  // 1. Status système
  ws.send(JSON.stringify({
    command: 'system.status'
  }));
  
  // 2. Scanner devices
  ws.send(JSON.stringify({
    command: 'instruments.scan'
  }));
  
  // 3. Connecter synthé
  ws.send(JSON.stringify({
    command: 'instruments.connect',
    params: { device_id: 'usb_synth_1' }
  }));
  
  // 4. Lister fichiers
  ws.send(JSON.stringify({
    command: 'files.list'
  }));
  
  // 5. Charger et jouer
  ws.send(JSON.stringify({
    command: 'playback.load',
    params: { file_path: '/home/pi/song.mid' }
  }));
  
  setTimeout(() => {
    ws.send(JSON.stringify({
      command: 'playback.play'
    }));
  }, 1000);
}
```

### Exemple 2 : Édition MIDI

```javascript
// Éditer un fichier
async function editFile(filePath) {
  // 1. Charger
  ws.send(JSON.stringify({
    command: 'editor.load',
    params: { file_path: filePath }
  }));
  
  // 2. Ajouter notes
  const notes = [
    { note: 60, time: 0, duration: 480 },
    { note: 64, time: 480, duration: 480 },
    { note: 67, time: 960, duration: 480 }
  ];
  
  notes.forEach(note => {
    ws.send(JSON.stringify({
      command: 'editor.addNote',
      params: { track: 0, velocity: 100, ...note }
    }));
  });
  
  // 3. Sauvegarder
  ws.send(JSON.stringify({
    command: 'editor.save',
    params: { file_path: filePath }
  }));
}
```

---

## 📚 Voir aussi

- **[Guide Simplifié](./GUIDE_SIMPLE.md)** - Concepts de base
- **[WebSocket API](./WEBSOCKET_API.md)** - Détails API
- **[Architecture Avancée](./ARCHITECTURE_ADVANCED.md)** - Architecture technique

---

[← Retour à l'index](../README.md)