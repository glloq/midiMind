# 🔬 Architecture Avancée - MidiMind Backend

> Documentation technique détaillée pour les développeurs

---

## 📋 Table des Matières

1. [Flux de traitement complet](#flux-de-traitement-complet)
2. [Couche API](#couche-api)
3. [Couche MIDI](#couche-midi)
4. [Couche Storage](#couche-storage)
5. [Couche Network](#couche-network)
6. [Couche Monitoring](#couche-monitoring)
7. [Couche Core](#couche-core)

---

## Flux de traitement complet

### Pipeline de traitement d'une commande

```
┌─────────────────────────────────────────────────────┐
│  1. RÉCEPTION                                       │
│  APIServer::onMessage()                             │
│  • Reçoit message WebSocket brut                    │
│  • Parse JSON avec nlohmann::json                   │
│  • Extraction de 'command' et 'params'              │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│  2. VALIDATION                                      │
│  JsonValidator::validate()                          │
│  • Vérifie présence de "command" (string)           │
│  • Valide structure "params" (object)               │
│  • Retourne erreur si invalide                      │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│  3. DISPATCH                                        │
│  CommandProcessorV2::processCommand()               │
│  • Lookup dans CommandFactory                       │
│  • Récupère lambda handler                          │
│  • Vérifie existence de la commande                 │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│  4. EXÉCUTION HANDLER                               │
│  Handler Lambda (files/instruments/editor/playback) │
│  • Extrait paramètres du JSON                       │
│  • Valide paramètres spécifiques                    │
│  • Appelle modules métier                           │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│  5. LOGIQUE MÉTIER                                  │
│  Modules MIDI/Storage/Network                       │
│  • MidiFileManager::scanLibrary()                   │
│  • MidiDeviceManager::connectDevice()               │
│  • MidiPlayer::play()                               │
│  • Database::query()                                │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│  6. RÉPONSE                                         │
│  APIServer::send()                                  │
│  • Construit JSON réponse                           │
│  • Ajoute timestamp                                 │
│  • Envoie via WebSocket                             │
└─────────────────────────────────────────────────────┘
```

### Exemple de code complet

```cpp
// 1. RÉCEPTION (APIServer.cpp)
void APIServer::onMessage(connection_hdl hdl, message_ptr msg) {
    json request = json::parse(msg->get_payload());
    
    // 2. VALIDATION
    if (!JsonValidator::validate(request)) {
        sendError(hdl, "Invalid request format");
        return;
    }
    
    // 3. DISPATCH
    json response = commandProcessor_->processCommand(request);
    
    // 6. RÉPONSE
    send(hdl, response.dump());
}

// 3. DISPATCH (CommandProcessorV2.cpp)
json CommandProcessorV2::processCommand(const json& request) {
    std::string commandName = request["command"];
    json params = request.value("params", json::object());
    
    // Lookup handler
    if (!factory_.hasCommand(commandName)) {
        return createErrorResponse("Unknown command", "INVALID_COMMAND");
    }
    
    // 4. EXÉCUTION
    return factory_.execute(commandName, params);
}

// 4. HANDLER (files.cpp)
factory.registerCommand("files.list",
    [fileManager](const json& params) -> json {
        // 5. LOGIQUE MÉTIER
        auto files = fileManager->scanLibrary();
        
        return {
            {"success", true},
            {"data", files}
        };
    }
);
```

---

## Couche API

### Fichiers et responsabilités

| Fichier | Rôle | Dépendances | Thread-Safe |
|---------|------|-------------|-------------|
| `APIServer.cpp` | Serveur WebSocket | WebSocketpp | ✅ |
| `CommandProcessorV2.cpp` | Dispatch commandes | CommandFactory | ✅ |
| `JsonValidator.cpp` | Validation JSON | nlohmann/json | ✅ |
| `files.cpp` | Handlers fichiers (5) | MidiFileManager | ❌ |
| `instruments.cpp` | Handlers instruments (5) | MidiDeviceManager | ❌ |
| `editor.cpp` | Handlers éditeur (6) | MidiFileManager | ❌ |
| `playback.cpp` | Handlers lecture (8) | MidiPlayer | ❌ |

### APIServer - Serveur WebSocket

**Fichier :** `src/api/APIServer.cpp`

**Fonctions clés :**

```cpp
class APIServer {
public:
    // Démarrer le serveur
    void start(uint16_t port = 8080);
    
    // Arrêter le serveur
    void stop();
    
    // Broadcast à tous les clients
    void broadcast(const std::string& message);
    
    // Envoyer à un client spécifique
    void send(connection_hdl hdl, const std::string& message);
    
private:
    // Handlers WebSocket
    void onOpen(connection_hdl hdl);
    void onClose(connection_hdl hdl);
    void onMessage(connection_hdl hdl, message_ptr msg);
    
    WebSocketServer server_;
    std::set<connection_hdl> connections_;
    std::shared_ptr<CommandProcessorV2> commandProcessor_;
};
```

### CommandProcessorV2 - Dispatch

**Fichier :** `src/api/CommandProcessorV2.cpp`

**Architecture Lambda (Pattern):**

```cpp
class CommandProcessorV2 {
public:
    CommandProcessorV2(
        std::shared_ptr<MidiDeviceManager> deviceManager,
        std::shared_ptr<MidiRouter> router,
        std::shared_ptr<MidiPlayer> player,
        std::shared_ptr<MidiFileManager> fileManager
    );
    
    // Traiter une commande
    json processCommand(const json& request);
    
private:
    // Enregistrer toutes les commandes
    void registerAllCommands();
    
    CommandFactory factory_;
    
    // Modules métier
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    std::shared_ptr<MidiRouter> router_;
    std::shared_ptr<MidiPlayer> player_;
    std::shared_ptr<MidiFileManager> fileManager_;
};
```

**Enregistrement des handlers :**

```cpp
void CommandProcessorV2::registerAllCommands() {
    // Commandes système
    factory_.registerCommand("system.status", ...);
    factory_.registerCommand("system.commands", ...);
    
    // Commandes par domaine
    registerFileCommands(factory_, fileManager_);        // 5 commandes
    registerInstrumentCommands(factory_, deviceManager_); // 5 commandes
    registerEditorCommands(factory_, fileManager_);       // 6 commandes
    registerPlaybackCommands(factory_, player_);          // 8 commandes
}
```

### Handlers - Pattern Lambda

**Fichier :** `src/api/files.cpp`

```cpp
void registerFileCommands(CommandFactory& factory, 
                         std::shared_ptr<MidiFileManager> fileManager) {
    
    // files.list
    factory.registerCommand("files.list",
        [fileManager](const json& params) -> json {
            auto files = fileManager->scanLibrary();
            return {
                {"success", true},
                {"data", {"files", files}}
            };
        }
    );
    
    // files.scan
    factory.registerCommand("files.scan",
        [fileManager](const json& params) -> json {
            std::string directory = params.value("directory", "/home/pi/midi-files");
            auto files = fileManager->scanDirectory(directory);
            return {
                {"success", true},
                {"data", {"files", files, "count", files.size()}}
            };
        }
    );
    
    // ... autres commandes
}
```

---

## Couche MIDI

### Architecture MIDI

| Fichier | Rôle | Thread-Safe | Latence |
|---------|------|-------------|---------|
| `MidiRouter.cpp` | Routage messages | ✅ shared_mutex | < 1ms |
| `MidiPlayer.cpp` | Lecture fichiers | ✅ atomic | < 2ms |
| `MidiClock.cpp` | Horloge sync | ✅ high-res timer | < 0.5ms |
| `MidiFileManager.cpp` | Gestion fichiers | ❌ | N/A |
| `MidiDeviceManager.cpp` | Gestion devices | ✅ mutex | N/A |
| `ProcessorManager.cpp` | Effets temps réel | ✅ lock-free | < 1ms |

### MidiRouter - Routage

**Fichier :** `src/midi/MidiRouter.cpp`

```cpp
class MidiRouter {
public:
    // Router un message
    void route(const MidiMessage& message);
    
    // Ajouter une route
    void addRoute(std::shared_ptr<MidiRoute> route);
    
    // Enregistrer un device
    void registerDevice(std::shared_ptr<MidiDevice> device);
    
private:
    std::shared_mutex routesMutex_;
    std::vector<std::shared_ptr<MidiRoute>> routes_;
    std::map<std::string, std::shared_ptr<MidiDevice>> devices_;
};
```

**Algorithme de routage :**

```cpp
void MidiRouter::route(const MidiMessage& message) {
    std::shared_lock lock(routesMutex_);
    
    for (const auto& route : routes_) {
        // Filtrer par canal
        if (route->channelFilter != -1 && 
            message.channel != route->channelFilter) {
            continue;
        }
        
        // Filtrer par type
        if (!route->messageTypeFilter.empty() &&
            !route->messageTypeFilter.contains(message.type)) {
            continue;
        }
        
        // Envoyer au device de destination
        auto device = devices_[route->destinationId];
        if (device && device->isConnected()) {
            device->sendMessage(message);
        }
    }
}
```

### MidiPlayer - Lecture

**Fichier :** `src/midi/MidiPlayer.cpp`

```cpp
class MidiPlayer {
public:
    void loadFile(const std::string& filePath);
    void play();
    void pause();
    void stop();
    void seek(uint32_t positionMs);
    
    PlayerStatus getStatus() const;
    void setTempo(float bpm);
    void setTranspose(int semitones);
    
private:
    void playbackThread();
    
    std::atomic<PlayerState> state_;
    std::atomic<uint32_t> position_;
    std::vector<MidiEvent> events_;
    std::thread playbackThread_;
};
```

### MidiDeviceManager - Devices

**Fichier :** `src/midi/devices/MidiDeviceManager.cpp`

```cpp
class MidiDeviceManager {
public:
    // Scanner les devices disponibles
    void scanDevices();
    
    // Connecter un device
    void connectDevice(const std::string& deviceId);
    
    // Déconnecter un device
    void disconnectDevice(const std::string& deviceId);
    
    // Obtenir tous les devices
    std::vector<std::shared_ptr<MidiDevice>> getAllDevices();
    
private:
    std::mutex devicesMutex_;
    std::map<std::string, std::shared_ptr<MidiDevice>> devices_;
};
```

**Types de devices supportés :**

```cpp
// USB MIDI (ALSA)
class UsbMidiDevice : public MidiDevice {
    snd_rawmidi_t* handle_;
};

// Virtual MIDI
class VirtualMidiDevice : public MidiDevice {
    snd_seq_t* seq_;
    int port_;
};

// RTP-MIDI (réseau)
class RtpMidiDevice : public MidiDevice {
    std::unique_ptr<RtpMidiSession> session_;
};

// Bluetooth LE
class BleMidiDevice : public MidiDevice {
    int gattHandle_;
};
```

---

## Couche Storage

### Architecture Storage

| Fichier | Technologie | Format | Thread-Safe |
|---------|-------------|--------|-------------|
| `Database.cpp` | SQLite3 | SQL | ✅ (transactions) |
| `Settings.cpp` | SQLite + JSON | Key-Value | ✅ |
| `SessionManager.cpp` | SQLite + JSON | Snapshot | ✅ |
| `PathManager.cpp` | Filesystem | Paths | ✅ (const) |

### Database - SQLite Wrapper

**Fichier :** `src/storage/Database.cpp`

```cpp
class Database {
public:
    bool open(const std::string& path);
    void close();
    
    // Exécuter une requête SQL
    void execute(const std::string& sql);
    
    // Requête avec résultat
    QueryResult query(const std::string& sql);
    
    // Transaction
    void beginTransaction();
    void commit();
    void rollback();
    
private:
    sqlite3* db_;
    std::mutex mutex_;
};
```

### Schéma de base de données

```sql
-- Fichiers MIDI
CREATE TABLE midi_files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    path TEXT UNIQUE NOT NULL,
    name TEXT NOT NULL,
    duration INTEGER,
    tracks INTEGER,
    tempo REAL,
    time_signature TEXT,
    jsonmidi TEXT,  -- Cache JSON
    created_at INTEGER,
    updated_at INTEGER
);

-- Routes MIDI
CREATE TABLE routes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    source_id TEXT NOT NULL,
    dest_id TEXT NOT NULL,
    channel_filter INTEGER,  -- -1 = tous
    config TEXT,  -- JSON
    enabled INTEGER DEFAULT 1,
    created_at INTEGER
);

-- Sessions
CREATE TABLE sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    config TEXT NOT NULL,  -- JSON complet
    created_at INTEGER,
    updated_at INTEGER
);

-- Processors
CREATE TABLE processors (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    route_id INTEGER,
    type TEXT NOT NULL,
    position INTEGER,
    config TEXT,  -- JSON
    enabled INTEGER DEFAULT 1,
    FOREIGN KEY(route_id) REFERENCES routes(id)
);

-- Settings
CREATE TABLE settings (
    key TEXT PRIMARY KEY,
    value TEXT,
    type TEXT,  -- string, int, float, bool, json
    updated_at INTEGER
);
```

### SessionManager - Save/Load

**Fichier :** `src/storage/SessionManager.cpp`

```cpp
class SessionManager {
public:
    // Sauvegarder session
    void saveSession(const std::string& name);
    
    // Charger session
    void loadSession(const std::string& sessionId);
    
    // Lister sessions
    std::vector<SessionInfo> listSessions();
    
private:
    json captureCurrentState();
    void restoreState(const json& state);
    
    std::shared_ptr<Database> db_;
};
```

**Format de session (JSON) :**

```json
{
  "version": "3.0.0",
  "timestamp": 1696435200000,
  "devices": [
    {
      "id": "usb_synth_1",
      "type": "USB",
      "connected": true
    }
  ],
  "routes": [
    {
      "source": "keyboard_1",
      "destination": "usb_synth_1",
      "channel_filter": 1,
      "processors": [
        {
          "type": "transpose",
          "config": {"semitones": 12}
        }
      ]
    }
  ],
  "player": {
    "file": "/path/to/song.mid",
    "position": 5000,
    "tempo": 120
  }
}
```

---

## Couche Network

### Architecture Network

| Fichier | Protocole | Port | Latence |
|---------|-----------|------|---------|
| `RtpMidiServer.cpp` | RTP-MIDI (Apple) | 5004 UDP | < 5ms |
| `MdnsDiscovery.cpp` | mDNS/Bonjour | 5353 UDP | N/A |
| `BleMidiDevice.cpp` | BLE MIDI | - | < 10ms |
| `WiFiHotspot.cpp` | hostapd | - | N/A |

### RtpMidiServer - MIDI over Network

**Fichier :** `src/network/rtpmidi/RtpMidiServer.cpp`

```cpp
class RtpMidiServer {
public:
    void start(uint16_t port = 5004);
    void stop();
    
    // Envoyer message MIDI
    void sendMessage(const MidiMessage& message);
    
private:
    void handleRtpPacket(const uint8_t* data, size_t len);
    
    int socket_;
    std::thread receiveThread_;
    std::vector<RtpMidiSession> sessions_;
};
```

### MdnsDiscovery - Découverte réseau

**Fichier :** `src/network/discovery/MdnsDiscovery.cpp`

```cpp
class MdnsDiscovery {
public:
    void start(const std::string& name = "MidiMind");
    void stop();
    
    // Publier service
    void publishService(const std::string& type, uint16_t port);
    
    // Scanner services
    std::vector<ServiceInfo> scanServices(const std::string& type);
    
private:
    avahi_client* client_;
    avahi_entry_group* group_;
};
```

---

## Couche Monitoring

### Architecture Monitoring

| Fichier | Métriques | Fréquence | Export |
|---------|-----------|-----------|--------|
| `SystemMonitor.cpp` | CPU, RAM, Temp | 1s | JSON |
| `LatencyMonitor.cpp` | MIDI latency | Temps réel | JSON |
| `MetricsCollector.cpp` | Agrégation | 1s | JSON |
| `HealthCheck.cpp` | Status services | 5s | JSON |

### SystemMonitor - Métriques système

**Fichier :** `src/monitoring/SystemMonitor.cpp`

```cpp
class SystemMonitor {
public:
    void start();
    void stop();
    
    SystemMetrics getMetrics() const;
    
private:
    void collectMetrics();
    
    std::atomic<float> cpuUsage_;
    std::atomic<float> ramUsage_;
    std::atomic<float> temperature_;
    std::thread monitorThread_;
};

struct SystemMetrics {
    float cpuUsage;       // %
    float ramUsage;       // MB
    float temperature;    // °C
    float diskUsage;      // %
    uint32_t uptime;      // secondes
};
```

### LatencyMonitor - Latence MIDI

**Fichier :** `src/monitoring/LatencyMonitor.cpp`

```cpp
class LatencyMonitor {
public:
    // Enregistrer timestamp input
    void recordInput(uint64_t messageId);
    
    // Enregistrer timestamp output
    void recordOutput(uint64_t messageId);
    
    // Obtenir statistiques
    LatencyStats getStats() const;
    
private:
    std::map<uint64_t, uint64_t> inputTimestamps_;
    std::deque<uint32_t> latencies_;  // µs
};

struct LatencyStats {
    uint32_t currentLatency;  // µs
    uint32_t averageLatency;  // µs
    uint32_t minLatency;      // µs
    uint32_t maxLatency;      // µs
    uint32_t jitter;          // µs
};
```

---

## Couche Core

### Architecture Core

| Fichier | Pattern | Thread-Safe | Usage |
|---------|---------|-------------|-------|
| `Logger.cpp` | Singleton | ✅ | Logging |
| `Config.cpp` | Singleton | ✅ | Configuration |
| `ThreadPool.cpp` | Object Pool | ✅ | Tasks async |
| `MemoryPool.cpp` | Object Pool | ✅ | Allocation RT |
| `PerformanceOptimizer.cpp` | Strategy | ✅ | Optimisations |

### Logger - Logging structuré

**Fichier :** `src/core/Logger.cpp`

```cpp
class Logger {
public:
    static Logger& instance();
    
    static void debug(const std::string& module, const std::string& message);
    static void info(const std::string& module, const std::string& message);
    static void warn(const std::string& module, const std::string& message);
    static void error(const std::string& module, const std::string& message);
    
    void setLevel(LogLevel level);
    void setOutput(const std::string& type, const std::string& path);
    
private:
    std::mutex mutex_;
    LogLevel level_;
    std::ofstream fileOutput_;
};
```

### ThreadPool - Pool de threads

**Fichier :** `src/core/optimization/ThreadPool.cpp`

```cpp
class ThreadPool {
public:
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    template<typename F>
    void enqueue(F&& task) {
        {
            std::unique_lock lock(queueMutex_);
            tasks_.emplace(std::forward<F>(task));
        }
        condition_.notify_one();
    }
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
};
```

---

## 📊 Récapitulatif

### Statistiques du code

| Catégorie | Nombre |
|-----------|--------|
| Couches | 6 |
| Fichiers source | ~50 |
| Classes principales | ~30 |
| Commandes API | 26 |
| Threads | 5-10 |

### Performance

| Métrique | Valeur |
|----------|--------|
| Latence MIDI | < 2ms |
| Throughput | 15,000 msg/s |
| CPU (idle) | < 5% |
| RAM | ~50-100 MB |

---

## 📚 Voir aussi

- **[Guide Simplifié](./GUIDE_SIMPLE.md)** - Concepts de base
- **[WebSocket API](./WEBSOCKET_API.md)** - API WebSocket
- **[Référence Commandes](./COMMANDS_REFERENCE.md)** - Toutes les commandes

---

[← Retour à l'index](../README.md)