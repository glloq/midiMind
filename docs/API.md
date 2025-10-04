# 🌐 MidiMind API Documentation

Version: 3.0.0  
Last Updated: 2025-10-04

---

## Table of Contents

- [Overview](#overview)
- [Authentication](#authentication)
- [Base URL](#base-url)
- [Response Format](#response-format)
- [Error Handling](#error-handling)
- [REST API](#rest-api)
  - [System](#system-endpoints)
  - [Devices](#devices-endpoints)
  - [Routes](#routes-endpoints)
  - [Processing](#processing-endpoints)
  - [Clock](#clock-endpoints)
  - [Player](#player-endpoints)
  - [Files](#files-endpoints)
  - [Network](#network-endpoints)
  - [Monitoring](#monitoring-endpoints)
  - [Sessions](#sessions-endpoints)
- [WebSocket API](#websocket-api)
- [Examples](#examples)

---

## Overview

MidiMind provides two APIs for control and monitoring:

1. **REST API** - HTTP/JSON for commands and queries
2. **WebSocket API** - Real-time bidirectional communication

Both APIs use JSON for data exchange and provide the same functionality.

---

## Authentication

### Optional Token Authentication

Authentication is **disabled by default** for local usage. To enable:

```json
// config/config.json
{
  "api": {
    "authentication": {
      "enabled": true,
      "api_key": "your-secret-token-here"
    }
  }
}
```

### Using Authentication

Include the token in the `Authorization` header:

```bash
curl -H "Authorization: Bearer your-secret-token-here" \
  http://localhost:8080/api/status
```

---

## Base URL

```
http://localhost:8080/api
```

Or by IP:
```
http://192.168.1.100:8080/api
```

---

## Response Format

### Success Response

```json
{
  "success": true,
  "data": {
    // Response data
  },
  "timestamp": 1696435200000
}
```

### Error Response

```json
{
  "success": false,
  "error": {
    "code": "DEVICE_NOT_FOUND",
    "message": "Device with ID 'synth1' not found"
  },
  "timestamp": 1696435200000
}
```

---

## Error Handling

### HTTP Status Codes

| Code | Meaning | Description |
|------|---------|-------------|
| `200` | OK | Request successful |
| `201` | Created | Resource created |
| `400` | Bad Request | Invalid parameters |
| `401` | Unauthorized | Authentication required |
| `404` | Not Found | Resource not found |
| `409` | Conflict | Resource already exists |
| `500` | Internal Error | Server error |

### Error Codes

| Code | Description |
|------|-------------|
| `INVALID_PARAMETERS` | Invalid request parameters |
| `DEVICE_NOT_FOUND` | MIDI device not found |
| `ROUTE_NOT_FOUND` | Route not found |
| `FILE_NOT_FOUND` | MIDI file not found |
| `DEVICE_BUSY` | Device is busy |
| `DEVICE_ERROR` | Device operation failed |
| `DATABASE_ERROR` | Database operation failed |

---

## REST API

---

## System Endpoints

### GET /api/status

Get system status.

**Response:**
```json
{
  "success": true,
  "data": {
    "version": "3.0.0",
    "state": "RUNNING",
    "uptime_ms": 123456789,
    "uptime_str": "1d 10h 17m"
  }
}
```

---

### GET /api/version

Get version information.

**Response:**
```json
{
  "success": true,
  "data": {
    "version": "3.0.0",
    "build_date": "Oct  4 2025",
    "build_time": "14:30:00"
  }
}
```

---

### POST /api/restart

Restart the application.

**Response:**
```json
{
  "success": true,
  "data": {
    "message": "Application restarting..."
  }
}
```

---

### POST /api/shutdown

Shutdown the application.

**Response:**
```json
{
  "success": true,
  "data": {
    "message": "Application shutting down..."
  }
}
```

---

## Devices Endpoints

### GET /api/devices

List all MIDI devices.

**Response:**
```json
{
  "success": true,
  "data": {
    "devices": [
      {
        "id": "hw:1,0",
        "name": "USB MIDI Keyboard",
        "type": "USB",
        "direction": "input",
        "status": "open",
        "port": "hw:1,0",
        "vendor": "Akai",
        "product": "MPK Mini"
      },
      {
        "id": "hw:2,0",
        "name": "USB MIDI Synth",
        "type": "USB",
        "direction": "output",
        "status": "open",
        "port": "hw:2,0"
      }
    ]
  }
}
```

---

### GET /api/devices/:id

Get specific device information.

**Parameters:**
- `id` (path) - Device ID

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "hw:1,0",
    "name": "USB MIDI Keyboard",
    "type": "USB",
    "direction": "input",
    "status": "open",
    "capabilities": {
      "channels": 16,
      "sysex": true,
      "clock": false
    }
  }
}
```

---

### POST /api/devices/scan

Scan for new MIDI devices.

**Response:**
```json
{
  "success": true,
  "data": {
    "devices_found": 2,
    "new_devices": 1
  }
}
```

---

### POST /api/devices/:id/open

Open a MIDI device.

**Parameters:**
- `id` (path) - Device ID

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "hw:1,0",
    "status": "open"
  }
}
```

---

### POST /api/devices/:id/close

Close a MIDI device.

**Parameters:**
- `id` (path) - Device ID

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "hw:1,0",
    "status": "closed"
  }
}
```

---

## Routes Endpoints

### GET /api/routes

List all MIDI routes.

**Response:**
```json
{
  "success": true,
  "data": {
    "routes": [
      {
        "id": "route-123",
        "name": "Keyboard to Synth",
        "enabled": true,
        "source": "USB MIDI Keyboard",
        "destination": "USB MIDI Synth",
        "channel_filter": [],
        "message_type_filter": [],
        "processors": [],
        "priority": 100
      }
    ]
  }
}
```

---

### GET /api/routes/:id

Get specific route information.

**Parameters:**
- `id` (path) - Route ID

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "route-123",
    "name": "Keyboard to Synth",
    "enabled": true,
    "source": "USB MIDI Keyboard",
    "destination": "USB MIDI Synth",
    "statistics": {
      "messages_routed": 15234,
      "last_message_at": 1696435200000
    }
  }
}
```

---

### POST /api/routes

Create a new MIDI route.

**Request Body:**
```json
{
  "name": "Keyboard to Synth",
  "source": "USB MIDI Keyboard",
  "destination": "USB MIDI Synth",
  "enabled": true,
  "channel_filter": [1, 2, 3],
  "message_type_filter": ["note_on", "note_off"],
  "priority": 100
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "route-456",
    "name": "Keyboard to Synth",
    "created_at": 1696435200000
  }
}
```

---

### PUT /api/routes/:id

Update an existing route.

**Parameters:**
- `id` (path) - Route ID

**Request Body:**
```json
{
  "enabled": false,
  "channel_filter": [1]
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "route-123",
    "updated": true
  }
}
```

---

### DELETE /api/routes/:id

Delete a route.

**Parameters:**
- `id` (path) - Route ID

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "route-123",
    "deleted": true
  }
}
```

---

## Processing Endpoints

### GET /api/routes/:id/processors

List processors for a route.

**Parameters:**
- `id` (path) - Route ID

**Response:**
```json
{
  "success": true,
  "data": {
    "processors": [
      {
        "id": "proc-123",
        "type": "arpeggiator",
        "enabled": true,
        "params": {
          "pattern": "up",
          "rate": "1/16",
          "octaves": 2
        }
      }
    ]
  }
}
```

---

### POST /api/routes/:id/processors

Add a processor to a route.

**Parameters:**
- `id` (path) - Route ID

**Request Body:**
```json
{
  "type": "arpeggiator",
  "enabled": true,
  "params": {
    "pattern": "up",
    "rate": "1/16",
    "octaves": 2,
    "gate": 0.8
  }
}
```

**Available Processor Types:**
- `transpose` - Transpose notes
- `velocity` - Scale velocity
- `channel_filter` - Filter by channel
- `note_filter` - Filter by note range
- `arpeggiator` - Arpeggiate chords
- `delay` - Echo/delay effect
- `chord` - Add harmonies
- `quantize` - Quantize timing

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "proc-456",
    "type": "arpeggiator",
    "created_at": 1696435200000
  }
}
```

---

### DELETE /api/routes/:routeId/processors/:processorId

Remove a processor from a route.

**Parameters:**
- `routeId` (path) - Route ID
- `processorId` (path) - Processor ID

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "proc-123",
    "deleted": true
  }
}
```

---

## Clock Endpoints

### GET /api/clock

Get MIDI clock status.

**Response:**
```json
{
  "success": true,
  "data": {
    "running": true,
    "tempo": 120.0,
    "ppqn": 24,
    "position": 384,
    "bar": 2,
    "beat": 1
  }
}
```

---

### POST /api/clock/start

Start MIDI clock.

**Request Body (optional):**
```json
{
  "tempo": 120.0
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "running": true,
    "tempo": 120.0
  }
}
```

---

### POST /api/clock/stop

Stop MIDI clock.

**Response:**
```json
{
  "success": true,
  "data": {
    "running": false
  }
}
```

---

### POST /api/clock/continue

Continue MIDI clock from current position.

**Response:**
```json
{
  "success": true,
  "data": {
    "running": true,
    "position": 384
  }
}
```

---

### PUT /api/clock/tempo

Set MIDI clock tempo.

**Request Body:**
```json
{
  "tempo": 140.0
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "tempo": 140.0
  }
}
```

---

## Player Endpoints

### GET /api/player/status

Get player status.

**Response:**
```json
{
  "success": true,
  "data": {
    "state": "playing",
    "file": "song.mid",
    "position_ms": 45000,
    "duration_ms": 180000,
    "loop": false,
    "tempo": 120.0
  }
}
```

---

### POST /api/player/load

Load a MIDI file.

**Request Body:**
```json
{
  "file": "song.mid"
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "file": "song.mid",
    "duration_ms": 180000,
    "tracks": 4,
    "loaded": true
  }
}
```

---

### POST /api/player/play

Start playback.

**Response:**
```json
{
  "success": true,
  "data": {
    "state": "playing"
  }
}
```

---

### POST /api/player/pause

Pause playback.

**Response:**
```json
{
  "success": true,
  "data": {
    "state": "paused",
    "position_ms": 45000
  }
}
```

---

### POST /api/player/stop

Stop playback.

**Response:**
```json
{
  "success": true,
  "data": {
    "state": "stopped"
  }
}
```

---

### POST /api/player/seek

Seek to position.

**Request Body:**
```json
{
  "position_ms": 60000
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "position_ms": 60000
  }
}
```

---

## Files Endpoints

### GET /api/files

List MIDI files.

**Query Parameters:**
- `path` (optional) - Subdirectory path

**Response:**
```json
{
  "success": true,
  "data": {
    "files": [
      {
        "name": "song.mid",
        "path": "/home/pi/MidiMind/midi/files/song.mid",
        "size": 12345,
        "modified": 1696435200000,
        "info": {
          "format": 1,
          "tracks": 4,
          "ppqn": 480,
          "duration_ms": 180000
        }
      }
    ]
  }
}
```

---

### GET /api/files/:filename

Get MIDI file information.

**Parameters:**
- `filename` (path) - File name

**Response:**
```json
{
  "success": true,
  "data": {
    "name": "song.mid",
    "format": 1,
    "tracks": 4,
    "ppqn": 480,
    "duration_ms": 180000,
    "tempo": 120.0,
    "time_signature": "4/4"
  }
}
```

---

### DELETE /api/files/:filename

Delete a MIDI file.

**Parameters:**
- `filename` (path) - File name

**Response:**
```json
{
  "success": true,
  "data": {
    "deleted": true
  }
}
```

---

## Network Endpoints

### GET /api/network/status

Get network status.

**Response:**
```json
{
  "success": true,
  "data": {
    "rtpmidi": {
      "enabled": true,
      "running": true,
      "port": 5004,
      "sessions": 1
    },
    "mdns": {
      "enabled": true,
      "running": true,
      "name": "MidiMind"
    },
    "bluetooth": {
      "enabled": false
    }
  }
}
```

---

### POST /api/network/rtpmidi/enable

Enable RTP-MIDI.

**Response:**
```json
{
  "success": true,
  "data": {
    "enabled": true,
    "port": 5004
  }
}
```

---

### POST /api/network/rtpmidi/disable

Disable RTP-MIDI.

**Response:**
```json
{
  "success": true,
  "data": {
    "enabled": false
  }
}
```

---

## Monitoring Endpoints

### GET /api/metrics

Get current metrics.

**Response:**
```json
{
  "success": true,
  "data": {
    "system": {
      "cpu_usage": 25.5,
      "cpu_temperature": 52.3,
      "ram_usage": 45.2,
      "ram_free_mb": 1024,
      "disk_usage": 35.8
    },
    "latency": {
      "current": 0.85,
      "average": 0.92,
      "min": 0.45,
      "max": 2.15,
      "jitter": 0.35
    },
    "midi": {
      "messages_received": 15234,
      "messages_sent": 15234,
      "messages_dropped": 0,
      "throughput": 150.5
    }
  }
}
```

---

### GET /api/health

Get health status.

**Response:**
```json
{
  "success": true,
  "data": {
    "status": "healthy",
    "score": 95,
    "checks": {
      "cpu": {
        "status": "ok",
        "value": 25.5,
        "threshold": 90
      },
      "memory": {
        "status": "ok",
        "value": 45.2,
        "threshold": 90
      },
      "temperature": {
        "status": "ok",
        "value": 52.3,
        "threshold": 80
      },
      "midi": {
        "status": "ok",
        "latency": 0.92
      }
    }
  }
}
```

---

## Sessions Endpoints

### GET /api/sessions

List all sessions.

**Response:**
```json
{
  "success": true,
  "data": {
    "sessions": [
      {
        "id": 1,
        "name": "Live Performance",
        "description": "Main live setup",
        "created_at": 1696435200000,
        "modified_at": 1696435200000,
        "active": true
      }
    ]
  }
}
```

---

### POST /api/sessions

Create a new session.

**Request Body:**
```json
{
  "name": "Studio Setup",
  "description": "Recording session configuration"
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": 2,
    "name": "Studio Setup",
    "created_at": 1696435200000
  }
}
```

---

### POST /api/sessions/:id/load

Load a session.

**Parameters:**
- `id` (path) - Session ID

**Response:**
```json
{
  "success": true,
  "data": {
    "id": 1,
    "name": "Live Performance",
    "loaded": true
  }
}
```

---

### DELETE /api/sessions/:id

Delete a session.

**Parameters:**
- `id` (path) - Session ID

**Response:**
```json
{
  "success": true,
  "data": {
    "id": 1,
    "deleted": true
  }
}
```

---

## WebSocket API

### Connection

```javascript
const ws = new WebSocket('ws://localhost:8080/ws');

ws.onopen = () => {
  console.log('Connected');
};

ws.onmessage = (event) => {
  const message = JSON.parse(event.data);
  console.log('Received:', message);
};

ws.onerror = (error) => {
  console.error('Error:', error);
};

ws.onclose = () => {
  console.log('Disconnected');
};
```

---

### Sending Commands

```javascript
// Send a command
ws.send(JSON.stringify({
  command: 'get_devices'
}));

// With parameters
ws.send(JSON.stringify({
  command: 'add_route',
  params: {
    name: 'New Route',
    source: 'USB MIDI Keyboard',
    destination: 'USB MIDI Synth'
  }
}));
```

---

### Subscribing to Events

```javascript
// Subscribe to events
ws.send(JSON.stringify({
  command: 'subscribe',
  events: ['midi', 'metrics', 'health']
}));
```

**Available Events:**
- `midi` - MIDI message received
- `metrics_update` - System metrics updated (every 1s)
- `health_alert` - Health check alert
- `device_connected` - Device connected
- `device_disconnected` - Device disconnected
- `route_added` - Route added
- `route_removed` - Route removed

---

### Event Messages

#### MIDI Message Event

```json
{
  "type": "midi",
  "data": {
    "message": {
      "type": "note_on",
      "channel": 1,
      "note": 60,
      "velocity": 100
    },
    "device": "USB MIDI Keyboard",
    "timestamp": 1696435200000
  }
}
```

#### Metrics Update Event

```json
{
  "type": "metrics_update",
  "data": {
    "system": {
      "cpu_usage": 25.5,
      "cpu_temperature": 52.3
    },
    "latency": {
      "current": 0.85
    }
  },
  "timestamp": 1696435200000
}
```

#### Health Alert Event

```json
{
  "type": "health_alert",
  "data": {
    "status": "warning",
    "message": "CPU usage high: 92%",
    "component": "cpu"
  },
  "timestamp": 1696435200000
}
```

---

## Examples

### Example 1: Create a Simple Route

```bash
curl -X POST http://localhost:8080/api/routes \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Keyboard to Synth",
    "source": "USB MIDI Keyboard",
    "destination": "USB MIDI Synth",
    "enabled": true
  }'
```

### Example 2: Add an Arpeggiator

```bash
curl -X POST http://localhost:8080/api/routes/route-123/processors \
  -H "Content-Type: application/json" \
  -d '{
    "type": "arpeggiator",
    "params": {
      "pattern": "up",
      "rate": "1/16",
      "octaves": 2,
      "gate": 0.8
    }
  }'
```

### Example 3: Start MIDI Clock

```bash
curl -X POST http://localhost:8080/api/clock/start \
  -H "Content-Type: application/json" \
  -d '{
    "tempo": 120.0
  }'
```

### Example 4: Get Metrics

```bash
curl http://localhost:8080/api/metrics
```

### Example 5: WebSocket Real-Time Monitoring

```javascript
const ws = new WebSocket('ws://localhost:8080/ws');

ws.onopen = () => {
  // Subscribe to metrics updates
  ws.send(JSON.stringify({
    command: 'subscribe',
    events: ['metrics_update']
  }));
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  
  if (data.type === 'metrics_update') {
    console.log('CPU:', data.data.system.cpu_usage + '%');
    console.log('Latency:', data.data.latency.current + 'ms');
  }
};
```

---

## Rate Limiting

Rate limiting is **disabled by default**. To enable:

```json
{
  "api": {
    "rest": {
      "rate_limit": {
        "enabled": true,
        "requests_per_minute": 100
      }
    }
  }
}
```

---

## CORS

CORS is **enabled by default** for all origins. To restrict:

```json
{
  "api": {
    "rest": {
      "cors_enabled": true,
      "cors_origins": ["http://localhost:3000", "http://192.168.1.100"]
    }
  }
}
```

