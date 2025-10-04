# ============================================================================
# Fichier: src/CMakeLists.txt
# ============================================================================

# ============================================================================
# LIBRAIRIES
# ============================================================================

# Core
add_library(midimind_core
    core/Logger.cpp
    core/Config.cpp
    core/optimization/ThreadPool.cpp
    core/optimization/MemoryPool.cpp
    core/optimization/PerformanceOptimizer.cpp
)

target_link_libraries(midimind_core
    PUBLIC
        nlohmann_json::nlohmann_json
        Threads::Threads
)

# MIDI
add_library(midimind_midi
    midi/MidiMessage.cpp
    midi/MidiRouter.cpp
    midi/MidiPlayer.cpp
    midi/MidiClock.cpp
    midi/MidiFileManager.cpp
    midi/MidiFileAnalyzer.cpp
    midi/MidiFileReader.cpp
    midi/MidiFileWriter.cpp
    midi/devices/MidiDevice.cpp
    midi/devices/MidiDeviceManager.cpp
    midi/devices/UsbMidiDevice.cpp
    midi/devices/VirtualMidiDevice.cpp
    midi/sysex/SysExHandler.cpp
    midi/processing/ProcessorManager.cpp
    midi/processing/creative/ArpeggiatorProcessor.cpp
    midi/processing/creative/DelayProcessor.cpp
)

target_link_libraries(midimind_midi
    PUBLIC
        midimind_core
        ${ALSA_LIBRARY}
)

# Network
add_library(midimind_network
    network/NetworkManager.cpp
    network/rtpmidi/RtpMidiSession.cpp
    network/rtpmidi/RtpMidiServer.cpp
    network/discovery/MdnsDiscovery.cpp
    network/bluetooth/BleMidiDevice.cpp
    network/wifi/WiFiHotspot.cpp
)

target_link_libraries(midimind_network
    PUBLIC
        midimind_core
        midimind_midi
)

# Storage
add_library(midimind_storage
    storage/Database.cpp
    storage/Settings.cpp
    storage/SessionManager.cpp
    storage/PathManager.cpp
)

target_link_libraries(midimind_storage
    PUBLIC
        midimind_core
        ${SQLite3_LIBRARIES}
)

# API
add_library(midimind_api
    api/APIServer.cpp
    api/WebSocketHandler.cpp
    api/CommandProcessor.cpp
    api/JsonValidator.cpp
)

target_link_libraries(midimind_api
    PUBLIC
        midimind_core
        midimind_midi
)

# Monitoring
add_library(midimind_monitoring
    monitoring/SystemMonitor.cpp
    monitoring/LatencyMonitor.cpp
    monitoring/MetricsCollector.cpp
    monitoring/HealthCheck.cpp
)

target_link_libraries(midimind_monitoring
    PUBLIC
        midimind_core
)

# ============================================================================
# EXÉCUTABLE PRINCIPAL
# ============================================================================

add_executable(midimind
    main.cpp
    Application.cpp
)

target_link_libraries(midimind
    PRIVATE
        midimind_core
        midimind_midi
        midimind_network
        midimind_storage
        midimind_api
        midimind_monitoring
)

# ============================================================================
# FIN DU FICHIER src/CMakeLists.txt
# ============================================================================