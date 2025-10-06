# 🔌 WebSocket API - MidiMind Backend

> Documentation complète de l'API WebSocket pour la communication Frontend/Backend

---

## 📋 Table des Matières

1. [Connexion WebSocket](#connexion-websocket)
2. [Structure des messages](#structure-des-messages)
3. [Types de communication](#types-de-communication)
4. [Événements temps réel](#événements-temps-réel)
5. [Gestion des erreurs](#gestion-des-erreurs)
6. [Exemples pratiques](#exemples-pratiques)

---

## Connexion WebSocket

### URL de connexion

```
ws://localhost:8080
```

Pour accès réseau (depuis autre machine) :
```
ws://192.168.1.100:8080
```

### Code de connexion JavaScript

```javascript
// Créer la connexion
const ws = new WebSocket('ws://localhost:8080');

// Événement : Connexion établie
ws.onopen = () => {
  console.log('✅ Connecté au backend MidiMind');
};

// Événement : Message reçu
ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('📥 Message reçu:', data);
};

// Événement : Erreur
ws.onerror = (error) => {
  console.error('❌ Erreur WebSocket:', error);
};

// Événement : Connexion fermée
ws.onclose = () => {
  console.log('🔌 Connexion fermée');
};
```

### Envoyer une commande

```javascript
ws.send(JSON.stringify({
  command: 'instruments.list'
}));
```

---

## Structure des messages

### 📤 Format de Requête

**Structure minimale :**
```json
{
  "command": "nom.commande"
}
```

**Avec paramètres :**
```json
{
  "command": "nom.commande",
  "params": {
    "param1": "valeur1",
    "param2": "valeur2"
  }
}
```

**Avec ID de suivi :**
```json
{
  "command": "nom.commande",
  "params": { ... },
  "request_id": "unique-id-12345"
}
```

### Champs de requête

| Champ | Type | Obligatoire | Description |
|-------|------|-------------|-------------|
| `command` | string | ✅ Oui | Nom de la commande (ex: "files.list") |
| `params` | object | ❌ Non | Paramètres de la commande |
| `request_id` | string | ❌ Non | ID unique pour tracer la requête |

### 📥 Format de Réponse

#### Réponse Succès

```json
{
  "success": true,
  "data": {
    "resultat": "valeur",
    "autre_info": 123
  },
  "timestamp": 1696435200000
}
```

#### Réponse Erreur

```json
{
  "success": false,
  "error": "Message d'erreur explicite",
  "error_code": "DEVICE_NOT_FOUND",
  "timestamp": 1696435200000
}
```

### Champs de réponse

| Champ | Type | Présence | Description |
|-------|------|----------|-------------|
| `success` | boolean | Toujours | `true` si succès, `false` si erreur |
| `data` | object | Si succès | Données de la réponse |
| `error` | string | Si erreur | Message d'erreur |
| `error_code` | string | Si erreur | Code d'erreur (ex: "DEVICE_NOT_FOUND") |
| `timestamp` | number | Toujours | Timestamp Unix (ms) |
| `request_id` | string | Si fourni | Echo du request_id de la requête |

---

## Types de communication

### 1. Commandes (Request/Response)

**Direction :** Frontend → Backend → Frontend

**Utilisation :** Actions ponctuelles avec réponse attendue

**Exemple :**
```javascript
// Envoyer commande
ws.send(JSON.stringify({
  command: 'playback.play'
}));

// Recevoir réponse
ws.onmessage = (event) => {
  const response = JSON.parse(event.data);
  if (response.success) {
    console.log('Lecture démarrée');
  }
};
```

### 2. Événements (Push)

**Direction :** Backend → Frontend (sans requête)

**Utilisation :** Notifications automatiques

**Exemple :**
```javascript
ws.onmessage = (event) => {
  const message = JSON.parse(event.data);
  
  // Distinguer événement d'une réponse
  if (message.type) {
    // C'est un événement
    console.log('📡 Événement:', message.type);
  } else if (message.success !== undefined) {
    // C'est une réponse à une commande
    console.log('📥 Réponse:', message);
  }
};
```

### 3. Status périodique

**Direction :** Backend → Frontend

**Fréquence :** Automatique (1x/seconde)

**Exemple :**
```javascript
ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  
  if (data.type === 'status_update') {
    console.log('CPU:', data.cpu_usage + '%');
    console.log('RAM:', data.ram_usage + 'MB');
    console.log('Temp:', data.temperature + '°C');
  }
};
```

---

## Événements temps réel

### Liste des événements

| Événement | Fréquence | Description |
|-----------|-----------|-------------|
| `status_update` | 1x/seconde | État général du système |
| `device_connected` | Événementiel | Nouveau device MIDI connecté |
| `device_disconnected` | Événementiel | Device MIDI déconnecté |
| `playback_update` | 10x/seconde | Position de lecture MIDI |
| `metrics_update` | 1x/seconde | Métriques système détaillées |
| `route_changed` | Événementiel | Modification du routage |
| `file_added` | Événementiel | Nouveau fichier MIDI détecté |

### Format des événements

#### status_update

```json
{
  "type": "status_update",
  "data": {
    "uptime": 3600,
    "cpu_usage": 25.5,
    "ram_usage": 512,
    "temperature": 45.2,
    "devices_connected": 3,
    "player_state": "playing"
  },
  "timestamp": 1696435200000
}
```

#### device_connected

```json
{
  "type": "device_connected",
  "data": {
    "device_id": "usb_synth_1",
    "name": "Korg Minilogue",
    "type": "USB",
    "ports": {
      "input": true,
      "output": true
    }
  },
  "timestamp": 1696435200000
}
```

#### device_disconnected

```json
{
  "type": "device_disconnected",
  "data": {
    "device_id": "usb_synth_1",
    "reason": "Cable unplugged"
  },
  "timestamp": 1696435200000
}
```

#### playback_update

```json
{
  "type": "playback_update",
  "data": {
    "state": "playing",
    "position": 5000,
    "duration": 180000,
    "tempo": 120,
    "bar": 12,
    "beat": 3
  },
  "timestamp": 1696435200000
}
```

#### metrics_update

```json
{
  "type": "metrics_update",
  "data": {
    "cpu_usage": 25.5,
    "ram_usage": 512,
    "temperature": 45.2,
    "disk_usage": 60.5,
    "midi_latency": 2,
    "messages_per_sec": 1500
  },
  "timestamp": 1696435200000
}
```

---

## Gestion des erreurs

### Codes d'erreur

| Code | Description | Action suggérée |
|------|-------------|-----------------|
| `INVALID_COMMAND` | Commande inconnue | Vérifier la documentation |
| `INVALID_PARAMETERS` | Paramètres invalides | Vérifier les types/valeurs |
| `DEVICE_NOT_FOUND` | Device introuvable | Scanner les devices |
| `DEVICE_BUSY` | Device occupé | Réessayer plus tard |
| `FILE_NOT_FOUND` | Fichier introuvable | Vérifier le chemin |
| `PLAYBACK_ERROR` | Erreur de lecture | Vérifier le fichier MIDI |
| `DATABASE_ERROR` | Erreur base de données | Contacter support |
| `NETWORK_ERROR` | Erreur réseau | Vérifier la connexion |

### Gestion côté Frontend

```javascript
ws.onmessage = (event) => {
  const response = JSON.parse(event.data);
  
  if (!response.success) {
    switch (response.error_code) {
      case 'DEVICE_NOT_FOUND':
        console.error('Device introuvable, scan en cours...');
        ws.send(JSON.stringify({
          command: 'instruments.scan'
        }));
        break;
        
      case 'INVALID_PARAMETERS':
        console.error('Paramètres invalides:', response.error);
        break;
        
      case 'DATABASE_ERROR':
        console.error('Erreur système:', response.error);
        alert('Erreur critique, redémarrage nécessaire');
        break;
        
      default:
        console.error('Erreur:', response.error);
    }
  }
};
```

### Reconnexion automatique

```javascript
class MidiMindClient {
  constructor(url) {
    this.url = url;
    this.ws = null;
    this.reconnectDelay = 1000;
    this.maxReconnectDelay = 30000;
    this.connect();
  }
  
  connect() {
    this.ws = new WebSocket(this.url);
    
    this.ws.onopen = () => {
      console.log('✅ Connecté');
      this.reconnectDelay = 1000;
    };
    
    this.ws.onclose = () => {
      console.log('🔌 Déconnecté, reconnexion dans', this.reconnectDelay + 'ms');
      
      setTimeout(() => {
        this.reconnectDelay = Math.min(
          this.reconnectDelay * 1.5,
          this.maxReconnectDelay
        );
        this.connect();
      }, this.reconnectDelay);
    };
    
    this.ws.onmessage = (event) => {
      this.handleMessage(JSON.parse(event.data));
    };
  }
  
  send(command, params = {}) {
    if (this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify({ command, params }));
    } else {
      console.error('WebSocket non connecté');
    }
  }
  
  handleMessage(data) {
    // Traiter le message
    console.log('Message:', data);
  }
}

// Utilisation
const client = new MidiMindClient('ws://localhost:8080');
client.send('system.status');
```

---

## Exemples pratiques

### Exemple 1 : Connexion et status

```javascript
const ws = new WebSocket('ws://localhost:8080');

ws.onopen = () => {
  // Demander le status système
  ws.send(JSON.stringify({
    command: 'system.status'
  }));
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  
  if (data.success) {
    console.log('Uptime:', data.data.uptime, 'secondes');
    console.log('CPU:', data.data.cpu_usage, '%');
    console.log('Devices:', data.data.devices_connected);
  }
};
```

### Exemple 2 : Gérer plusieurs types de messages

```javascript
ws.onmessage = (event) => {
  const message = JSON.parse(event.data);
  
  // Événements
  if (message.type === 'device_connected') {
    console.log('✅ Device connecté:', message.data.name);
    updateDeviceList();
  }
  
  if (message.type === 'playback_update') {
    updateProgressBar(message.data.position, message.data.duration);
  }
  
  if (message.type === 'status_update') {
    updateSystemMetrics(message.data);
  }
  
  // Réponses à des commandes
  if (message.success !== undefined) {
    if (message.success) {
      console.log('✅ Commande réussie:', message.data);
    } else {
      console.error('❌ Erreur:', message.error);
    }
  }
};
```

### Exemple 3 : Charger et jouer un fichier

```javascript
async function loadAndPlay(filePath) {
  // 1. Charger le fichier
  ws.send(JSON.stringify({
    command: 'playback.load',
    params: { file_path: filePath },
    request_id: 'load-123'
  }));
  
  // 2. Attendre la réponse
  ws.onmessage = (event) => {
    const response = JSON.parse(event.data);
    
    if (response.request_id === 'load-123') {
      if (response.success) {
        console.log('✅ Fichier chargé');
        
        // 3. Démarrer la lecture
        ws.send(JSON.stringify({
          command: 'playback.play',
          request_id: 'play-123'
        }));
      } else {
        console.error('❌ Erreur chargement:', response.error);
      }
    }
    
    if (response.request_id === 'play-123') {
      if (response.success) {
        console.log('▶️ Lecture démarrée');
      }
    }
  };
}

loadAndPlay('/home/pi/midi-files/song.mid');
```

### Exemple 4 : Éditer un fichier MIDI

```javascript
function editMidiFile(filePath) {
  // 1. Charger dans l'éditeur
  ws.send(JSON.stringify({
    command: 'editor.load',
    params: { file_path: filePath }
  }));
  
  // 2. Attendre le chargement
  ws.onmessage = (event) => {
    const response = JSON.parse(event.data);
    
    if (response.success && response.data.jsonmidi) {
      console.log('✅ Fichier chargé dans éditeur');
      
      // 3. Ajouter une note
      ws.send(JSON.stringify({
        command: 'editor.addNote',
        params: {
          track: 0,
          time: 0,
          note: 60,      // C4
          velocity: 100,
          duration: 480  // 1 beat
        }
      }));
    }
  };
}
```

---

## 📚 Voir aussi

- **[Guide Simplifié](./GUIDE_SIMPLE.md)** - Concepts de base
- **[Référence Commandes](./COMMANDS_REFERENCE.md)** - Liste complète des commandes
- **[Architecture Avancée](./ARCHITECTURE_ADVANCED.md)** - Détails techniques

---

[← Retour à l'index](../README.md)