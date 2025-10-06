# 🎯 Guide Simplifié - MidiMind Backend

> Documentation pédagogique pour comprendre facilement le fonctionnement du backend MidiMind

---

## 📋 Table des Matières

1. [Qu'est-ce que MidiMind ?](#quest-ce-que-midimind-)
2. [Architecture en 6 couches](#architecture-en-6-couches)
3. [Communication WebSocket](#communication-websocket)
4. [Exemples de flux simples](#exemples-de-flux-simples)

---

## Qu'est-ce que MidiMind ?

MidiMind est un **système d'orchestration MIDI** qui transforme un Raspberry Pi en hub MIDI professionnel.

### Fonctionnalités principales

- 📱 **Connecter** des instruments MIDI (USB, WiFi, Bluetooth)
- 🔀 **Router** des messages entre appareils avec règles flexibles
- 🎵 **Lire et éditer** des fichiers MIDI en temps réel
- 🎛️ **Appliquer** des effets MIDI (arpégiateur, delay, transpose)
- 🌐 **Contrôler** tout via une API WebSocket JSON

### Principe de fonctionnement

```
Frontend (JavaScript)
        ↓
    WebSocket (JSON)
        ↓
Backend C++ (MidiMind)
        ↓
Appareils MIDI (Hardware)
```

Le frontend envoie des **commandes JSON** via WebSocket.  
Le backend les **exécute** et renvoie les **résultats en temps réel**.

---

## Architecture en 6 couches

Le backend est organisé en **6 couches modulaires** :

```
┌─────────────────────────────────────────────────────┐
│              APPLICATION (Main)                     │
│           Orchestration centrale                    │
└─────────────────────────────────────────────────────┘
                        ↕
┌──────────┬──────────┬──────────┬──────────┬─────────┐
│   Core   │   MIDI   │ Storage  │ Network  │   API   │
└──────────┴──────────┴──────────┴──────────┴─────────┘
                        ↕
┌─────────────────────────────────────────────────────┐
│                  MONITORING                         │
└─────────────────────────────────────────────────────┘
```

### Détail des couches

| Couche | Rôle | Composants principaux |
|--------|------|----------------------|
| **⚙️ Core** | Fondations système | Logger, Config, ThreadPool, MemoryPool |
| **🎹 MIDI** | Logique métier MIDI | Router, Player, Devices, Processors |
| **💾 Storage** | Persistence données | Database (SQLite), Settings, Sessions |
| **🌐 Network** | Connectivité réseau | RTP-MIDI, mDNS, Bluetooth, WiFi |
| **🔌 API** | Interface externe | APIServer (WebSocket), CommandProcessor |
| **📊 Monitoring** | Observabilité | Metrics, Health checks, Latency |

### Exemples de modules

#### 🎹 Couche MIDI
- **MidiRouter** : Route les messages entre devices
- **MidiPlayer** : Lit les fichiers MIDI
- **MidiDeviceManager** : Gère les appareils connectés
- **ProcessorManager** : Applique des effets en temps réel

#### 🔌 Couche API
- **APIServer** : Serveur WebSocket
- **CommandProcessorV2** : Traite les commandes
- **files.cpp** : Handlers pour fichiers MIDI
- **instruments.cpp** : Handlers pour instruments

---

## Communication WebSocket

### Protocole

Le backend utilise **WebSocket** pour communiquer avec le frontend.

- **Port** : 8080 (par défaut)
- **Format** : JSON
- **Bidirectionnel** : Frontend ↔ Backend

### Format des messages

#### 📤 Requête (Frontend → Backend)

```json
{
  "command": "nom.commande",
  "params": {
    "param1": "valeur1",
    "param2": "valeur2"
  }
}
```

**Champs :**
- `command` (obligatoire) : Nom de la commande
- `params` (optionnel) : Paramètres de la commande

#### 📥 Réponse Succès (Backend → Frontend)

```json
{
  "success": true,
  "data": {
    "resultat": "valeur"
  },
  "timestamp": 1696435200000
}
```

#### ❌ Réponse Erreur

```json
{
  "success": false,
  "error": "Message d'erreur explicite",
  "error_code": "DEVICE_NOT_FOUND",
  "timestamp": 1696435200000
}
```

### Types de communication

| Type | Direction | Fréquence | Description |
|------|-----------|-----------|-------------|
| **Commandes** | Frontend → Backend | À la demande | Actions ponctuelles avec réponse |
| **Événements** | Backend → Frontend | Événementiel | Notifications temps réel |
| **Status** | Backend → Frontend | 1x/seconde | État du système |

### Événements automatiques

Le backend envoie **automatiquement** certains événements :

- `status_update` - État général du système (1x/s)
- `device_connected` - Nouveau device connecté
- `device_disconnected` - Device déconnecté
- `playback_update` - Position de lecture (10x/s)
- `metrics_update` - Métriques système (1x/s)

---

## Exemples de flux simples

### Exemple 1 : Lister les fichiers MIDI

#### Flux étape par étape

```
1️⃣ Frontend envoie la commande
   ↓
   {"command": "files.list"}

2️⃣ APIServer reçoit le message WebSocket
   ↓
   Parse le JSON
   Valide la structure

3️⃣ CommandProcessorV2 identifie la commande
   ↓
   Lookup "files.list" dans la factory
   Récupère le handler correspondant

4️⃣ Handler (files.cpp) exécute la logique
   ↓
   Appelle MidiFileManager::scanLibrary()
   Récupère la liste des fichiers

5️⃣ Backend renvoie la réponse
   ↓
   {
     "success": true,
     "data": {
       "files": [
         {
           "id": "1",
           "name": "Song.mid",
           "duration": 180,
           "tracks": 4
         }
       ]
     }
   }
```

#### Diagramme de séquence

```
Frontend          APIServer      CommandProcessor    files.cpp       MidiFileManager
   │                  │                  │               │                  │
   │ files.list       │                  │               │                  │
   ├─────────────────>│                  │               │                  │
   │                  │ processCommand() │               │                  │
   │                  ├─────────────────>│               │                  │
   │                  │                  │ execute       │                  │
   │                  │                  ├──────────────>│                  │
   │                  │                  │               │ scanLibrary()    │
   │                  │                  │               ├─────────────────>│
   │                  │                  │               │    files[]       │
   │                  │                  │               │<─────────────────┤
   │                  │                  │  json result  │                  │
   │                  │                  │<──────────────┤                  │
   │                  │   response       │               │                  │
   │                  │<─────────────────┤               │                  │
   │    {success:true}│                  │               │                  │
   │<─────────────────┤                  │               │                  │
   │                  │                  │               │                  │
```

### Exemple 2 : Connecter un instrument MIDI

#### Flux étape par étape

```
1️⃣ Frontend envoie la commande avec paramètres
   ↓
   {
     "command": "instruments.connect",
     "params": {
       "device_id": "usb_synth_1"
     }
   }

2️⃣ APIServer → CommandProcessorV2
   ↓
   Identifie la commande "instruments.connect"
   Route vers le handler instruments.cpp

3️⃣ Handler (instruments.cpp) traite la requête
   ↓
   Extrait le paramètre "device_id"
   Appelle MidiDeviceManager::connectDevice("usb_synth_1")

4️⃣ MidiDeviceManager exécute
   ↓
   Ouvre le port ALSA correspondant
   Enregistre le device dans MidiRouter
   Configure les callbacks

5️⃣ Backend envoie la réponse + broadcast événement
   ↓
   Réponse : {"success": true, "data": {...}}
   
   Événement broadcast à TOUS les clients :
   {
     "type": "device_connected",
     "data": {
       "device_id": "usb_synth_1",
       "name": "Korg Minilogue",
       "status": "online"
     }
   }
```

#### Diagramme de séquence

```
Frontend    APIServer    CommandProcessor    instruments.cpp    MidiDeviceManager    MidiRouter
   │            │               │                   │                   │               │
   │ connect    │               │                   │                   │               │
   ├───────────>│               │                   │                   │               │
   │            │ process       │                   │                   │               │
   │            ├──────────────>│                   │                   │               │
   │            │               │ execute           │                   │               │
   │            │               ├──────────────────>│                   │               │
   │            │               │                   │ connectDevice()   │               │
   │            │               │                   ├──────────────────>│               │
   │            │               │                   │                   │ Open ALSA     │
   │            │               │                   │                   │ port          │
   │            │               │                   │                   │               │
   │            │               │                   │                   │ registerDevice│
   │            │               │                   │                   ├──────────────>│
   │            │               │                   │   device          │               │
   │            │               │                   │<──────────────────┤               │
   │            │               │    json           │                   │               │
   │            │               │<──────────────────┤                   │               │
   │            │    response   │                   │                   │               │
   │            │<──────────────┤                   │                   │               │
   │  success   │               │                   │                   │               │
   │<───────────┤               │                   │                   │               │
   │            │               │                   │                   │               │
   │            │ broadcast     │                   │                   │               │
   │            │ event         │                   │                   │               │
   │<───────────┤               │                   │                   │               │
   │            │               │                   │                   │               │
```

---

## 🎯 Points clés à retenir

### 1. Architecture modulaire

Le backend est divisé en **6 couches indépendantes** qui communiquent entre elles.

### 2. Communication JSON

Tous les échanges Frontend ↔ Backend utilisent le **format JSON** via WebSocket.

### 3. Pattern Command

Chaque commande suit le même flux :
```
Requête → Validation → Dispatch → Handler → Module métier → Réponse
```

### 4. Temps réel

Le système broadcast automatiquement les **événements importants** à tous les clients connectés.

---

## 📚 Pour aller plus loin

- **[WebSocket API](./WEBSOCKET_API.md)** - Détails de l'API WebSocket
- **[Architecture Avancée](./ARCHITECTURE_ADVANCED.md)** - Détails techniques complets
- **[Référence Commandes](./COMMANDS_REFERENCE.md)** - Liste de toutes les commandes

---

[← Retour à l'index](../README.md)