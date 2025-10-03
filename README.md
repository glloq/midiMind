# 🎹 MidiMind

<div align="center">

![MidiMind Logo](docs/images/logo.png)

**MIDI Orchestration System for Raspberry Pi**

[![Version](https://img.shields.io/badge/version-3.0.0-blue.svg)](https://github.com/midimind/midimind)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Raspberry%20Pi-red.svg)](https://www.raspberrypi.org/)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/midimind/midimind)

[Features](#-features) • [Installation](#-installation) • [Quick Start](#-quick-start) • [Documentation](#-documentation) • [API](#-api-reference)

</div>


## 🎯 Overview

**MidiMind** is a powerful, professional-grade MIDI orchestration system designed specifically for Raspberry Pi. It transforms your Raspberry Pi into a versatile MIDI hub with advanced routing, processing, and monitoring capabilities.

### What can MidiMind do?

- 🎼 **Route MIDI** between multiple devices with flexible rules
- 🎛️ **Process MIDI** with arpeggiators, delays, harmonizers, and more
- 🌐 **Network MIDI** via RTP-MIDI (AppleMIDI), mDNS discovery
- 📡 **Wireless** support via Bluetooth LE MIDI and WiFi hotspot
- 📊 **Monitor** system health, latency, and performance in real-time
- 🎹 **Play** MIDI files with tempo sync and looping
- 💾 **Record** MIDI performances to standard MIDI files
- 🔌 **Hot-plug** automatic device detection and reconnection
- 🌐 **REST API** and WebSocket for remote control
- ⚡ **High Performance** with lock-free queues and thread pools

---

## ✨ Features

### 🎵 MIDI Features

- **Multi-Device Support**
  - USB MIDI devices (keyboards, controllers, synthesizers)
  - Virtual MIDI ports for DAW integration
  - Network MIDI (RTP-MIDI/AppleMIDI)
  - Bluetooth LE MIDI
  - Hot-plug detection and auto-reconnection

- **Advanced Routing**
  - Flexible routing matrix
  - Channel filtering and remapping
  - Message type filtering
  - Priority-based routing
  - Multiple simultaneous routes

- **MIDI Processing**
  - **Basic Processors**: Transpose, Velocity scaling, Channel filter, Note filter
  - **Creative Processors**: Arpeggiator, Delay, Chord generator, Harmonizer
  - **Processor Chains**: Combine multiple processors
  - **Real-time**: Sub-millisecond latency

- **MIDI Clock**
  - Accurate tempo generation (20-300 BPM)
  - 24 PPQN (Pulses Per Quarter Note)
  - Sync to external clock
  - Send to multiple outputs

- **File Management**
  - Play standard MIDI files (SMF 0/1/2)
  - Record performances to MIDI files
  - Analyze MIDI file structure
  - Batch operations

### 🌐 Network Features

- **RTP-MIDI (AppleMIDI)**
  - Industry-standard network MIDI protocol
  - Compatible with macOS, iOS, Windows, Linux
  - Low latency (< 5ms typical)
  - Automatic session management

- **mDNS Discovery**
  - Zero-configuration networking
  - Automatic service discovery
  - Bonjour/Avahi compatible

- **Bluetooth LE MIDI**
  - Wireless MIDI over Bluetooth
  - iOS and Android compatible
  - Auto-reconnect

- **WiFi Hotspot**
  - Turn Raspberry Pi into a WiFi access point
  - Mobile device connectivity
  - Captive portal support

### 📊 Monitoring & Management

- **System Monitoring**
  - CPU usage, temperature, frequency
  - RAM usage and free memory
  - Disk usage and I/O
  - Network traffic

- **Performance Monitoring**
  - MIDI message latency (µs precision)
  - Message throughput
  - Dropped messages
  - Jitter analysis

- **Health Checks**
  - Automatic anomaly detection
  - Configurable alert thresholds
  - Real-time notifications
  - System health dashboard

### 🚀 Optimization

- **High Performance**
  - Lock-free message queues
  - Thread pool for parallel processing
  - Memory pool for reduced allocations
  - Object pools for MIDI messages
  - Zero-copy message passing

- **Low Latency**
  - Typical latency: < 1ms
  - Real-time priority threads
  - Optimized for Raspberry Pi hardware

### 🔌 API & Integration

- **REST API**
  - Full control via HTTP
  - JSON request/response
  - CORS support
  - Rate limiting

- **WebSocket API**
  - Real-time events
  - Bidirectional communication
  - Automatic reconnection
  - Message broadcasting

- **Command System**
  - 50+ commands
  - Extensible architecture
  - Parameter validation
  - Error handling

---

## 🏗️ Architecture
