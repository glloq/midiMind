
# 🏗️ MidiMind Architecture Documentation

Version: 3.0.0  
Last Updated: 2025-10-04

---

## Table of Contents

- [Overview](#overview)
- [System Architecture](#system-architecture)
- [Core Components](#core-components)
- [Design Patterns](#design-patterns)
- [Thread Safety](#thread-safety)
- [Data Flow](#data-flow)
- [Memory Management](#memory-management)
- [Performance Optimization](#performance-optimization)
- [Error Handling](#error-handling)
- [Extensibility](#extensibility)
- [Dependencies](#dependencies)

---

## Overview

MidiMind is a **professional MIDI orchestration system** designed for Raspberry Pi, built with C++17. The architecture follows modern software engineering principles with emphasis on:

- **Modularity** - Independent, reusable components
- **Performance** - Low-latency, high-throughput MIDI processing
- **Thread-Safety** - Concurrent access without race conditions
- **Extensibility** - Easy to add new features
- **Maintainability** - Clean code, clear separation of concerns

### Key Characteristics

| Aspect | Implementation |
|--------|----------------|
| **Language** | C++17 |
| **Architecture** | Layered, event-driven |
| **Threading** | Multi-threaded with thread pools |
| **Memory** | Pool allocation, minimal allocations |
| **Latency** | < 1ms typical |
| **Throughput** | 15,000+ messages/sec |

---

## System Architecture

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         MidiMind Application                         │
│                                                                       │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │                      Presentation Layer                         │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │ │
│  │  │  REST API    │  │  WebSocket   │  │  Web UI      │        │ │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘        │ │
│  └─────────┼──────────────────┼──────────────────┼────────────────┘ │
│            │                  │                  │                    │
│  ┌─────────▼──────────────────▼──────────────────▼────────────────┐ │
│  │                      Business Logic Layer                       │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │ │
│  │  │ Command      │  │ MIDI Router  │  │ Session      │        │ │
│  │  │ Processor    │  │              │  │ Manager      │        │ │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘        │ │
│  └─────────┼──────────────────┼──────────────────┼────────────────┘ │
│            │                  │                  │                    │
│  ┌─────────▼──────────────────▼──────────────────▼────────────────┐ │
│  │                      Service Layer                              │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │ │
│  │  │ Device       │  │ Processor    │  │ Network      │        │ │
│  │  │ Manager      │  │ Manager      │  │ Manager      │        │ │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘        │ │
│  └─────────┼──────────────────┼──────────────────┼────────────────┘ │
│            │                  │                  │                    │
│  ┌─────────▼──────────────────▼──────────────────▼────────────────┐ │
│  │                      Infrastructure Layer                       │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │ │
│  │  │ Database     │  │ File System  │  │ Monitoring   │        │ │
│  │  │ (SQLite)     │  │              │  │              │        │ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘        │ │
│  │                                                                 │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │ │
│  │  │ Thread Pool  │  │ Memory Pool  │  │ Logger       │        │ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘        │ │
│  └─────────────────────────────────────────────────────────────────┘ │
│                                                                       │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │                      Hardware Abstraction Layer                  │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │ │
│  │  │ ALSA         │  │ Socket       │  │ Bluetooth    │        │ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘        │ │
│  └─────────────────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────────────┘
```

---

## Core Components

### 1. Application (Core Orchestrator)

**File:** `src/Application.h/cpp`

**Responsibility:** Central orchestrator, manages lifecycle of all components.

**Key Features:**
- Singleton pattern
- Initialization sequencing
- Graceful shutdown
- Signal handling
- Component coordination

```cpp
class Application {
public:
    static Application& instance();
    
    bool initialize(const std::string& configPath = "");
    bool start();
    void stop();
    
    // Component accessors
    std::shared_ptr<MidiRouter> getMidiRouter();
    std::shared_ptr<APIServer> getAPIServer();
    // ... other components
};
```

**Thread Safety:** ✅ All methods thread-safe with mutex protection

---

### 2. MIDI Router (Message Routing Engine)

**File:** `src/midi/MidiRouter.h/cpp`

**Responsibility:** Route MIDI messages between devices based on rules.

**Key Features:**
- Flexible routing matrix
- Channel/message filtering
- Priority-based routing
- Lock-free message passing (with shared_mutex)

```cpp
class MidiRouter {
public:
    void route(const MidiMessage& message);
    void addRoute(std::shared_ptr<MidiRoute> route);
    void registerDevice(std::shared_ptr<MidiDevice> device);
    
private:
    std::shared_mutex routesMutex_;  // Read/write separated
    std::vector<std::shared_ptr<MidiRoute>> routes_;
    std::map<std::string, std::shared_ptr<MidiDevice>> devices_;
};
```

**Thread Safety:** ✅ Concurrent routing with shared_lock, exclusive modifications

**Performance:** 
- Latency: < 1ms
- Throughput: 15,000+ msg/sec

---

### 3. Device Manager (Hardware Abstraction)

**File:** `src/midi/devices/MidiDeviceManager.h/cpp`

**Responsibility:** Manage MIDI hardware devices (USB, virtual, network).

**Key Features:**
- Hot-plug detection
- Auto-reconnection
- Device enumeration
- Status monitoring

```cpp
class MidiDeviceManager {
public:
    void scanDevices();
    std::vector<std::shared_ptr<MidiDevice>> getDevices();
    std::shared_ptr<MidiDevice> getDevice(const std::string& id);
    
private:
    void onDeviceConnected(std::shared_ptr<MidiDevice> device);
    void onDeviceDisconnected(const std::string& deviceId);
};
```

**Device Types:**
- USB MIDI (via ALSA)
- Virtual MIDI ports
- RTP-MIDI (network)
- Bluetooth LE MIDI

---

### 4. Processor Manager (Effects Chain)

**File:** `src/midi/processing/ProcessorManager.h/cpp`

**Responsibility:** Manage MIDI processing effects.

**Key Features:**
- Processor chains
- Real-time processing
- Parameter automation
- Preset management

```cpp
class ProcessorManager {
public:
    void addProcessor(std::shared_ptr<MidiProcessor> processor);
    MidiMessage process(const MidiMessage& input);
    
private:
    std::vector<std::shared_ptr<MidiProcessor>> processors_;
};
```

**Available Processors:**
- Transpose
- Velocity scaling
- Arpeggiator
- Delay
- Chord generator
- Harmonizer
- Quantizer

---

### 5. Network Manager (Network MIDI)

**File:** `src/network/NetworkManager.h/cpp`

**Responsibility:** Manage network MIDI protocols.

**Key Features:**
- RTP-MIDI (AppleMIDI)
- mDNS discovery
- Bluetooth LE MIDI
- WiFi hotspot

```cpp
class NetworkManager {
public:
    void start();
    void stop();
    
    void enableRtpMidi(int port);
    void enableMdns(const std::string& name);
    void enableBluetooth(const std::string& name);
    
private:
    std::unique_ptr<RtpMidiServer> rtpMidiServer_;
    std::unique_ptr<MdnsDiscovery> mdnsDiscovery_;
    std::unique_ptr<BleMidiDevice> bleDevice_;
};
```

---

### 6. API Server (External Interface)

**File:** `src/api/APIServer.h/cpp`

**Responsibility:** REST API and WebSocket server.

**Key Features:**
- HTTP server
- WebSocket support
- JSON request/response
- Event broadcasting

```cpp
class APIServer {
public:
    void start(int port);
    void stop();
    
    void broadcast(const std::string& message);
    
    void setCommandHandler(
        std::function<json(const json&)> handler
    );
    
private:
    void handleRequest(const Request& req, Response& res);
    void handleWebSocket(WebSocketConnection* conn);
};
```

**Thread Safety:** ✅ Concurrent request handling with thread pool

---

### 7. Storage Layer (Persistence)

**Files:**
- `src/storage/Database.h/cpp` - SQLite wrapper
- `src/storage/Settings.h/cpp` - Settings management
- `src/storage/SessionManager.h/cpp` - Session persistence

**Responsibility:** Data persistence and configuration.

**Key Features:**
- SQLite database
- Settings key/value store
- Session save/load
- Automatic backups

```cpp
class Database {
public:
    bool open(const std::string& path);
    void execute(const std::string& sql);
    QueryResult query(const std::string& sql);
    
private:
    sqlite3* db_;
};
```

**Thread Safety:** ✅ Connection per thread, transactions

---

### 8. Monitoring System (Observability)

**Files:**
- `src/monitoring/MetricsCollector.h/cpp`
- `src/monitoring/SystemMonitor.h/cpp`
- `src/monitoring/LatencyMonitor.h/cpp`
- `src/monitoring/HealthCheck.h/cpp`

**Responsibility:** Monitor system health and performance.

**Key Features:**
- Real-time metrics
- Latency tracking
- Health checks
- Alerting

```cpp
class MetricsCollector {
public:
    void start();
    void collect();
    
    AggregatedMetrics getMetrics() const;
    
private:
    std::shared_ptr<SystemMonitor> systemMonitor_;
    std::shared_ptr<LatencyMonitor> latencyMonitor_;
};
```

---

## Design Patterns

### 1. Singleton Pattern

**Used in:** Application, PerformanceOptimizer, PathManager

**Why:** Single global instance needed for coordination.

```cpp
class Application {
public:
    static Application& instance() {
        static Application instance;
        return instance;
    }
    
private:
    Application();  // Private constructor
};
```

---

### 2. Observer Pattern

**Used in:** Event system, device notifications

**Why:** Decouple components, enable event-driven architecture.

```cpp
template<typename EventType>
class IObserver {
public:
    virtual void onNotify(const EventType& event) = 0;
};

template<typename EventType>
class ISubject {
public:
    void attach(std::shared_ptr<IObserver<EventType>> observer);
    void notify(const EventType& event);
};
```

---

### 3. Factory Pattern

**Used in:** Device creation, processor creation

**Why:** Abstract object creation, enable extensibility.

```cpp
class MidiProcessorFactory {
public:
    static std::shared_ptr<MidiProcessor> create(
        const std::string& type,
        const json& params
    );
};
```

---

### 4. Strategy Pattern

**Used in:** MIDI processors

**Why:** Interchangeable algorithms.

```cpp
class MidiProcessor {
public:
    virtual MidiMessage process(const MidiMessage& input) = 0;
};

class TransposeProcessor : public MidiProcessor {
    MidiMessage process(const MidiMessage& input) override;
};
```

---

### 5. Object Pool Pattern

**Used in:** MidiMessage allocation, memory management

**Why:** Reduce allocations, improve performance.

```cpp
template<typename T>
class ObjectPool {
public:
    std::shared_ptr<T> acquire();
    void release(std::shared_ptr<T> obj);
    
private:
    std::vector<std::shared_ptr<T>> pool_;
};
```

---

### 6. Command Pattern

**Used in:** API commands

**Why:** Encapsulate requests as objects.

```cpp
class Command {
public:
    virtual json execute(const json& params) = 0;
};

class GetDevicesCommand : public Command {
    json execute(const json& params) override;
};
```

---

## Thread Safety

### Thread Model

MidiMind uses a **multi-threaded architecture** with the following threads:

| Thread | Purpose | Priority |
|--------|---------|----------|
| **Main Thread** | Application lifecycle | Normal |
| **MIDI Input Threads** | Per-device input (ALSA callbacks) | Real-time |
| **MIDI Router Thread** | Message routing | Real-time |
| **Worker Threads** | Processing (ThreadPool) | Normal |
| **API Server Thread** | HTTP/WebSocket | Normal |
| **Monitoring Thread** | Metrics collection | Low |
| **Network Threads** | RTP-MIDI, mDNS | Normal |

---

### Thread Safety Mechanisms

#### 1. Mutex Protection

```cpp
class MidiRouter {
private:
    mutable std::mutex mutex_;
    std::vector<MidiRoute> routes_;
    
public:
    void addRoute(const MidiRoute& route) {
        std::lock_guard<std::mutex> lock(mutex_);
        routes_.push_back(route);
    }
};
```

**Issue:** Blocks all operations (read and write).

---

#### 2. Shared Mutex (Read/Write Lock) ✅ RECOMMENDED

```cpp
class MidiRouter {
private:
    mutable std::shared_mutex routesMutex_;
    std::vector<MidiRoute> routes_;
    
public:
    // Multiple readers (shared lock)
    void route(const MidiMessage& msg) {
        std::shared_lock<std::shared_mutex> lock(routesMutex_);
        // Read routes_
    }
    
    // Single writer (unique lock)
    void addRoute(const MidiRoute& route) {
        std::unique_lock<std::shared_mutex> lock(routesMutex_);
        routes_.push_back(route);
    }
};
```

**Benefits:**
- ✅ Multiple concurrent readers
- ✅ Exclusive writer access
- ✅ Better performance for read-heavy workloads

---

#### 3. Lock-Free Queues

```cpp
template<typename T>
class LockFreeQueue {
public:
    bool push(const T& item);
    bool pop(T& item);
    
private:
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
    std::vector<T> buffer_;
};
```

**Used for:** High-frequency message passing (MIDI routing).

**Benefits:**
- ✅ No blocking
- ✅ Wait-free operations
- ✅ Cache-friendly

---

#### 4. Atomic Variables

```cpp
class MidiRouter {
private:
    std::atomic<uint64_t> messagesRouted_;
    std::atomic<uint64_t> messagesDropped_;
    
public:
    void route(const MidiMessage& msg) {
        // ... routing logic
        messagesRouted_++;  // Atomic increment
    }
};
```

**Used for:** Counters, flags, simple state.

---

### Thread Safety Rules

1. ✅ **Always use locks** when modifying shared data
2. ✅ **Prefer shared_mutex** for read-heavy operations
3. ✅ **Use lock-free queues** for high-frequency data passing
4. ✅ **Minimize lock scope** (RAII with lock_guard)
5. ✅ **Avoid nested locks** (deadlock risk)
6. ✅ **Copy shared_ptr** before using without lock (Application::stop)

---

## Data Flow

### MIDI Message Flow

```
┌─────────────┐
│ USB Device  │
│ (Keyboard)  │
└──────┬──────┘
       │ ALSA Callback
       ▼
┌─────────────────┐
│ UsbMidiDevice   │
│ ::onReceive()   │
└──────┬──────────┘
       │ MidiMessage
       ▼
┌─────────────────┐
│ MidiRouter      │
│ ::route()       │
└──────┬──────────┘
       │
       ├─────────────────┐
       │                 │
       ▼                 ▼
┌──────────────┐   ┌──────────────┐
│ Route Match? │   │ Processor    │
│ (filters)    │   │ Chain        │
└──────┬───────┘   └──────┬───────┘
       │ YES              │ Transform
       ▼                  ▼
┌─────────────────────────────┐
│ MidiRouter::sendToDevice()  │
└──────┬──────────────────────┘
       │
       ▼
┌─────────────────┐
│ UsbMidiDevice   │
│ ::send()        │
└──────┬──────────┘
       │ ALSA Write
       ▼
┌─────────────┐
│ USB Device  │
│ (Synth)     │
└─────────────┘
```

**Latency:** < 1ms (typical)

---

### API Request Flow

```
┌─────────────┐
│ HTTP Client │
│ (Browser)   │
└──────┬──────┘
       │ HTTP POST /api/routes
       ▼
┌─────────────────┐
│ APIServer       │
│ ::handleRequest │
└──────┬──────────┘
       │ Parse JSON
       ▼
┌─────────────────┐
│ JsonValidator   │
│ ::validate()    │
└──────┬──────────┘
       │ Valid
       ▼
┌─────────────────┐
│ CommandProcessor│
│ ::process()     │
└──────┬──────────┘
       │ Execute
       ▼
┌─────────────────┐
│ AddRouteCommand │
│ ::execute()     │
└──────┬──────────┘
       │
       ▼
┌─────────────────┐
│ MidiRouter      │
│ ::addRoute()    │
└──────┬──────────┘
       │ Success
       ▼
┌─────────────────┐
│ Response JSON   │
└──────┬──────────┘
       │ HTTP 201
       ▼
┌─────────────┐
│ HTTP Client │
└─────────────┘
```

---

### Event Broadcasting Flow

```
┌─────────────────┐
│ MetricsCollector│ (Every 1 second)
│ ::collect()     │
└──────┬──────────┘
       │ Metrics ready
       ▼
┌─────────────────┐
│ Callback        │
│ metricsCallback_│
└──────┬──────────┘
       │
       ▼
┌─────────────────┐
│ APIServer       │
│ ::broadcast()   │
└──────┬──────────┘
       │ For each client
       ▼
┌─────────────────┐
│ WebSocket       │
│ ::send()        │
└──────┬──────────┘
       │
       ├─────────┬─────────┬─────────┐
       ▼         ▼         ▼         ▼
   ┌───────┐ ┌───────┐ ┌───────┐ ┌───────┐
   │Client1│ │Client2│ │Client3│ │Client4│
   └───────┘ └───────┘ └───────┘ └───────┘
```

---

## Memory Management

### Memory Pools

**Purpose:** Reduce allocations for frequently created objects.

```cpp
class MemoryPool {
public:
    void* allocate(size_t size);
    void deallocate(void* ptr);
    
private:
    std::vector<uint8_t*> blocks_;
    std::vector<uint8_t*> freeList_;
};
```

**Used for:**
- MIDI messages
- Network packets
- Temporary buffers

**Benefits:**
- ✅ Predictable allocation time
- ✅ Reduced memory fragmentation
- ✅ Cache-friendly

---

### Object Pools

```cpp
template<typename T>
class ObjectPool {
public:
    std::shared_ptr<T> acquire() {
        if (!pool_.empty()) {
            auto obj = pool_.back();
            pool_.pop_back();
            return obj;
        }
        return std::make_shared<T>();
    }
    
    void release(std::shared_ptr<T> obj) {
        pool_.push_back(obj);
    }
};
```

**Used for:** MidiMessage objects (2000 pre-allocated).

---

### Smart Pointers

**Guidelines:**

1. **std::shared_ptr** - Shared ownership
   ```cpp
   std::shared_ptr<MidiDevice> device;
   ```

2. **std::unique_ptr** - Exclusive ownership
   ```cpp
   std::unique_ptr<Database> database_;
   ```

3. **std::weak_ptr** - Observer pattern (avoid cycles)
   ```cpp
   std::vector<std::weak_ptr<IObserver>> observers_;
   ```

4. **Raw pointers** - Non-owning (rarely)
   ```cpp
   MidiDevice* getDeviceRaw();  // Avoid if possible
   ```

---

## Performance Optimization

### 1. Lock-Free Message Passing

**Technique:** Lock-free circular queue

**Impact:** 
- Before: ~2ms latency (with mutex)
- After: ~0.5ms latency (lock-free)

---

### 2. Thread Pool

**Technique:** Pre-created worker threads

**Configuration:**
```cpp
OptimizationConfig config;
config.threadPoolSize = 4;  // Number of cores
```

**Benefits:**
- ✅ No thread creation overhead
- ✅ Load balancing
- ✅ CPU cache utilization

---

### 3. Memory Pool

**Technique:** Pre-allocated memory blocks

**Configuration:**
```cpp
config.memoryBlockSize = 1024;
config.memoryInitialBlocks = 500;
```

**Benefits:**
- ✅ Constant-time allocation
- ✅ No fragmentation
- ✅ Predictable memory usage

---

### 4. Zero-Copy Message Passing

**Technique:** Pass by const reference, shared_ptr

```cpp
// ❌ BAD: Copy
void route(MidiMessage message);

// ✅ GOOD: Reference
void route(const MidiMessage& message);

// ✅ GOOD: Shared pointer
void route(std::shared_ptr<MidiMessage> message);
```

---

### 5. SIMD Optimization (Future)

**Potential:** Process multiple messages in parallel.

```cpp
// Process 4 messages at once with AVX
__m128i notes = _mm_load_si128(...);
__m128i transposed = _mm_add_epi32(notes, transpose);
```

---

## Error Handling

### Error Strategy

1. **Exceptions** - For unexpected errors
2. **Return codes** - For expected errors
3. **Logging** - For debugging

---

### Exception Hierarchy

```cpp
class MidiMindException : public std::runtime_error {
public:
    MidiMindException(ErrorCode code, const std::string& msg);
    ErrorCode getCode() const;
};

enum class ErrorCode {
    // Device errors
    DEVICE_NOT_FOUND,
    DEVICE_OPEN_FAILED,
    DEVICE_BUSY,
    
    // Routing errors
    ROUTE_NOT_FOUND,
    ROUTE_ALREADY_EXISTS,
    
    // API errors
    API_INVALID_PARAMETERS,
    API_UNAUTHORIZED,
    
    // Database errors
    DATABASE_OPEN_FAILED,
    DATABASE_QUERY_FAILED,
    
    // Network errors
    NETWORK_CONNECTION_FAILED,
    NETWORK_TIMEOUT
};
```

---

### Error Handling Example

```cpp
try {
    auto device = deviceManager->getDevice("synth1");
    if (!device) {
        THROW_ERROR(ErrorCode::DEVICE_NOT_FOUND, "Device 'synth1' not found");
    }
    
    device->send(message);
    
} catch (const MidiMindException& e) {
    Logger::error("Router", "Routing failed: " + std::string(e.what()));
    
    // Notify API clients
    json error;
    error["error"] = e.what();
    error["code"] = static_cast<int>(e.getCode());
    apiServer->broadcast(error.dump());
    
} catch (const std::exception& e) {
    Logger::error("Router", "Unexpected error: " + std::string(e.what()));
}
```

---

## Extensibility

### Adding a New MIDI Processor

1. **Create processor class:**

```cpp
// src/midi/processing/creative/MyProcessor.h
class MyProcessor : public MidiProcessor {
public:
    MyProcessor(const json& params);
    MidiMessage process(const MidiMessage& input) override;
    
private:
    // Parameters
    float myParam_;
};
```

2. **Register in factory:**

```cpp
// src/midi/processing/ProcessorFactory.cpp
std::shared_ptr<MidiProcessor> ProcessorFactory::create(
    const std::string& type,
    const json& params
) {
    if (type == "my_processor") {
        return std::make_shared<MyProcessor>(params);
    }
    // ... other processors
}
```

3. **Use in API:**

```bash
curl -X POST http://localhost:8080/api/routes/1/processors \
  -d '{"type": "my_processor", "params": {"myParam": 0.5}}'
```

---

### Adding a New Device Type

1. **Inherit from MidiDevice:**

```cpp
// src/midi/devices/MyDevice.h
class MyDevice : public MidiDevice {
public:
    bool open() override;
    void close() override;
    void send(const MidiMessage& msg) override;
    
private:
    // Device-specific members
};
```

2. **Register in DeviceManager:**

```cpp
// src/midi/devices/MidiDeviceManager.cpp
void MidiDeviceManager::scanDevices() {
    // ... existing code
    
    // Scan for MyDevice
    auto myDevices = scanMyDevices();
    for (auto& device : myDevices) {
        devices_.push_back(device);
    }
}
```

---

### Adding a New API Command

1. **Create command class:**

```cpp
// src/api/commands/MyCommand.h
class MyCommand : public Command {
public:
    json execute(const json& params) override {
        // Implementation
        return result;
    }
};
```

2. **Register in CommandProcessor:**

```cpp
// src/api/CommandProcessor.cpp
CommandProcessor::CommandProcessor() {
    registerCommand("my_command", std::make_shared<MyCommand>());
}
```

3. **Use via API:**

```bash
curl -X POST http://localhost:8080/api/command \
  -d '{"command": "my_command", "params": {}}'
```

---

## Dependencies

### External Libraries

| Library | Version | Purpose |
|---------|---------|---------|
| **ALSA** | 1.2+ | MIDI device access (Linux) |
| **SQLite3** | 3.35+ | Database |
| **nlohmann/json** | 3.11+ | JSON parsing |
| **pthread** | - | Threading |

### Build Dependencies

| Tool | Version | Purpose |
|------|---------|---------|
| **CMake** | 3.16+ | Build system |
| **GCC/Clang** | 9+ | C++ compiler |
| **Make** | - | Build automation |

---

## Performance Metrics

### Benchmarks (Raspberry Pi 4, 4GB)

| Metric | Value | Notes |
|--------|-------|-------|
| **Latency (avg)** | 0.92 ms | MIDI in → out |
| **Latency (min)** | 0.45 ms | Best case |
| **Latency (max)** | 2.15 ms | Worst case |
| **Jitter** | < 0.5 ms | Latency variance |
| **Throughput** | 15,000+ msg/sec | Sustained |
| **CPU Usage** | 15-25% | Typical load |
| **RAM Usage** | 150-200 MB | Resident |
| **Startup Time** | < 2 sec | Cold start |

---

## Future Improvements

### Planned Optimizations

1. **SIMD Processing** - Vectorized message processing
2. **GPU Acceleration** - Complex effects on GPU
3. **MIDI 2.0** - Support for new protocol
4. **Distributed Processing** - Multi-Pi clustering
5. **Hardware Acceleration** - FPGA for ultra-low latency

---
