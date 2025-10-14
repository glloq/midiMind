# ============================================================================
# Fichier: backend/CMakeLists.txt
# Version: 3.0.8
# Date: 2025-10-14
# ============================================================================
# CORRECTIONS v3.0.8:
# ✅ Alignement COMPLET avec structure réelle du projet
# ✅ Retrait Config.cpp (header-only)
# ✅ Chemins corrigés selon structure fournie
# ✅ Fichiers optionnels gérés correctement
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(MidiMind VERSION 3.0.8 LANGUAGES CXX)

# ============================================================================
# OPTIONS DE COMPILATION
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(BUILD_TESTS "Build test suite" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers (Debug only)" ON)

message(STATUS "========================================")
message(STATUS "MidiMind v${PROJECT_VERSION} Configuration")
message(STATUS "========================================")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler:        ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "========================================")

# ============================================================================
# FLAGS DE COMPILATION
# ============================================================================

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -pthread
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -DDEBUG)
    if(ENABLE_SANITIZERS)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -DNDEBUG -march=native -flto)
    add_link_options(-flto)
endif()

# ============================================================================
# DÉPENDANCES
# ============================================================================

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

find_library(ALSA_LIBRARY asound)
if(NOT ALSA_LIBRARY)
    message(FATAL_ERROR "ALSA not found. Install: sudo apt install libasound2-dev")
endif()

find_package(nlohmann_json 3.11.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json will be used from include/")
endif()

# ============================================================================
# FICHIERS SOURCE - MAIN
# ============================================================================

set(SOURCES_MAIN
    src/main.cpp
)

# ============================================================================
# FICHIERS SOURCE - CORE
# ============================================================================

set(SOURCES_CORE
    src/core/Application.cpp
    # Config.cpp removed - Config.h is header-only
    src/core/Logger.cpp
    src/core/ErrorManager.cpp
    src/core/EventBus.cpp
)

# Core optimisation
set(SOURCES_CORE_OPTIMISATION
    src/core/optimisation/MemoryPool.cpp
    src/core/optimisation/PerformanceOptimizer.cpp
    src/core/optimisation/ThreadPool.cpp
)

# ============================================================================
# FICHIERS SOURCE - MIDI CORE
# ============================================================================

set(SOURCES_MIDI_CORE
    src/midi/MidiMessage.cpp
    src/midi/MidiRouter.cpp
    src/midi/MidiClock.cpp
    src/midi/JsonMidiConverter.cpp
    src/midi/MidiFileManager.cpp
    src/midi/MidiTransforms.cpp
    src/midi/MidiValidator.cpp
)

# ============================================================================
# FICHIERS SOURCE - MIDI DEVICES
# ============================================================================

set(SOURCES_MIDI_DEVICES
    src/midi/devices/MidiDeviceManager.cpp
    src/midi/devices/VirtualMidiDevice.cpp
)

# ============================================================================
# FICHIERS SOURCE - MIDI PLUGIN
# ============================================================================

set(SOURCES_MIDI_PLUGIN)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/plugin/BleMidiPlugin.cpp")
    list(APPEND SOURCES_MIDI_PLUGIN src/midi/plugin/BleMidiPlugin.cpp)
    message(STATUS "✓ BleMidiPlugin.cpp found")
endif()

# ============================================================================
# FICHIERS SOURCE - MIDI FILE
# ============================================================================

set(SOURCES_MIDI_FILE
    src/midi/file/MidiFileReader.cpp
    src/midi/file/MidiFileWriter.cpp
)

# ============================================================================
# FICHIERS SOURCE - MIDI PLAYER
# ============================================================================

set(SOURCES_MIDI_PLAYER
    src/midi/player/MidiPlayer.cpp
)

# ============================================================================
# FICHIERS SOURCE - MIDI PROCESSING
# ============================================================================

set(SOURCES_MIDI_PROCESSING
    src/midi/processing/ProcessorManager.cpp
)

# Processing - Creative
set(SOURCES_MIDI_PROCESSING_CREATIVE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/processing/creative/ArpeggiatorProcessor.cpp")
    list(APPEND SOURCES_MIDI_PROCESSING_CREATIVE 
        src/midi/processing/creative/ArpeggiatorProcessor.cpp
        src/midi/processing/creative/DelayProcessor.cpp
    )
    message(STATUS "✓ Creative processors found")
endif()

# ============================================================================
# FICHIERS SOURCE - MIDI SYSEX
# ============================================================================

set(SOURCES_MIDI_SYSEX
    src/midi/sysex/SysExHandler.cpp
    src/midi/sysex/SysExParser.cpp
    src/midi/sysex/CustomSysExParser.cpp
)

# ============================================================================
# FICHIERS SOURCE - NETWORK
# ============================================================================

set(SOURCES_NETWORK)

# Network - Bluetooth
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/network/bluetooth/BleMidiDevice.cpp")
    list(APPEND SOURCES_NETWORK src/network/bluetooth/BleMidiDevice.cpp)
    message(STATUS "✓ BleMidiDevice.cpp found")
endif()

# Network - Discovery
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/network/discovery/MdnsDiscovery.cpp")
    list(APPEND SOURCES_NETWORK src/network/discovery/MdnsDiscovery.cpp)
    message(STATUS "✓ MdnsDiscovery.cpp found")
endif()

# Network - RTP MIDI
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/network/rtpmidi/RtpMidiServer.cpp")
    list(APPEND SOURCES_NETWORK 
        src/network/rtpmidi/RtpMidiServer.cpp
        src/network/rtpmidi/RtpMidiSession.cpp
    )
    message(STATUS "✓ RTP-MIDI found")
endif()

# Network - WiFi
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/network/wifi/WiFiHotspot.cpp")
    list(APPEND SOURCES_NETWORK src/network/wifi/WiFiHotspot.cpp)
    message(STATUS "✓ WiFiHotspot.cpp found")
endif()

# ============================================================================
# FICHIERS SOURCE - STORAGE
# ============================================================================

set(SOURCES_STORAGE
    src/storage/Database.cpp
    src/storage/PathManager.cpp
    src/storage/SessionManager.cpp
    src/storage/PresetManager.cpp
    src/storage/Settings.cpp
)

# ============================================================================
# FICHIERS SOURCE - TIMING
# ============================================================================

set(SOURCES_TIMING)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/timing/LatencyCompensator.cpp")
    list(APPEND SOURCES_TIMING
        src/timing/LatencyCompensator.cpp
        src/timing/TimestampManager.cpp
    )
    message(STATUS "✓ Timing modules found")
else()
    message(STATUS "⚠ Timing modules not found (optional)")
endif()

# ============================================================================
# FICHIERS SOURCE - UTILS
# ============================================================================

set(SOURCES_UTILS)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/JsonValidator.cpp")
    list(APPEND SOURCES_UTILS src/utils/JsonValidator.cpp)
    message(STATUS "✓ JsonValidator.cpp found")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/ResourceMonitor.cpp")
    list(APPEND SOURCES_UTILS src/utils/ResourceMonitor.cpp)
    message(STATUS "✓ ResourceMonitor.cpp found")
endif()

# ============================================================================
# FICHIERS SOURCE - LOOP
# ============================================================================

set(SOURCES_LOOP)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/loop/loopManager.cpp")
    list(APPEND SOURCES_LOOP src/loop/loopManager.cpp)
    message(STATUS "✓ loopManager.cpp found")
else()
    message(STATUS "⚠ loopManager.cpp not found (optional)")
endif()

# ============================================================================
# FICHIERS SOURCE - MONITORING
# ============================================================================

set(SOURCES_MONITORING
    src/monitoring/HealthCheck.cpp
    src/monitoring/LatencyMonitor.cpp
    src/monitoring/MetricsCollector.cpp
    src/monitoring/SystemMonitor.cpp
)

# ============================================================================
# FICHIERS SOURCE - API
# ============================================================================

set(SOURCES_API
    src/api/ApiServer.cpp
    src/api/CommandProcessorV2.cpp
    src/api/MessageEnvelope.cpp
)

# API - Commands (selon structure fournie)
set(SOURCES_API_COMMANDS
    src/api/commands/editor.cpp
    src/api/commands/files.cpp
    src/api/commands/logger.cpp
    src/api/commands/network.cpp
    src/api/commands/playback.cpp
    src/api/commands/processing.cpp
    src/api/commands/system.cpp
)

# API - Commands additionnels (si existent)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/commands/loops.cpp")
    list(APPEND SOURCES_API_COMMANDS src/api/commands/loops.cpp)
    message(STATUS "✓ loops.cpp found")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/devices.cpp")
    list(APPEND SOURCES_API src/api/devices.cpp)
    message(STATUS "✓ devices.cpp found")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/instruments.cpp")
    list(APPEND SOURCES_API src/api/instruments.cpp)
    message(STATUS "✓ instruments.cpp found")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/routing.cpp")
    list(APPEND SOURCES_API src/api/routing.cpp)
    message(STATUS "✓ routing.cpp found")
endif()

# API - Editor
set(SOURCES_API_EDITOR)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/editor/editorState.cpp")
    list(APPEND SOURCES_API_EDITOR src/api/editor/editorState.cpp)
    message(STATUS "✓ editorState.cpp found")
endif()

# ============================================================================
# AGRÉGATION DE TOUS LES SOURCES
# ============================================================================

set(ALL_SOURCES
    ${SOURCES_MAIN}
    ${SOURCES_CORE}
    ${SOURCES_CORE_OPTIMISATION}
    ${SOURCES_MIDI_CORE}
    ${SOURCES_MIDI_DEVICES}
    ${SOURCES_MIDI_PLUGIN}
    ${SOURCES_MIDI_FILE}
    ${SOURCES_MIDI_PLAYER}
    ${SOURCES_MIDI_PROCESSING}
    ${SOURCES_MIDI_PROCESSING_CREATIVE}
    ${SOURCES_MIDI_SYSEX}
    ${SOURCES_NETWORK}
    ${SOURCES_STORAGE}
    ${SOURCES_TIMING}
    ${SOURCES_UTILS}
    ${SOURCES_LOOP}
    ${SOURCES_MONITORING}
    ${SOURCES_API}
    ${SOURCES_API_COMMANDS}
    ${SOURCES_API_EDITOR}
)

# Afficher nombre de fichiers sources
list(LENGTH ALL_SOURCES SOURCE_COUNT)
message(STATUS "========================================")
message(STATUS "Total source files: ${SOURCE_COUNT}")
message(STATUS "========================================")

# ============================================================================
# EXÉCUTABLE PRINCIPAL
# ============================================================================

add_executable(midimind ${ALL_SOURCES})

# ============================================================================
# INCLUDE DIRECTORIES
# ============================================================================

target_include_directories(midimind PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SQLite3_INCLUDE_DIRS}
)

# ============================================================================
# LINK LIBRARIES
# ============================================================================

target_link_libraries(midimind PRIVATE
    Threads::Threads
    SQLite::SQLite3
    ${ALSA_LIBRARY}
    stdc++fs
)

if(nlohmann_json_FOUND)
    target_link_libraries(midimind PRIVATE nlohmann_json::nlohmann_json)
endif()

# ============================================================================
# PROPRIÉTÉS
# ============================================================================

set_target_properties(midimind PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME midimind
)

# ============================================================================
# TESTS (optionnel)
# ============================================================================

if(BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
        message(STATUS "Tests enabled")
    else()
        message(STATUS "⚠ tests/ directory not found")
    endif()
endif()

# ============================================================================
# EXAMPLES (optionnel)
# ============================================================================

if(BUILD_EXAMPLES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
        add_subdirectory(examples)
        message(STATUS "Examples enabled")
    else()
        message(STATUS "⚠ examples/ directory not found")
    endif()
endif()

# ============================================================================
# INSTALLATION
# ============================================================================

install(TARGETS midimind
    RUNTIME DESTINATION bin
)

# Configuration (si existe)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config")
    install(DIRECTORY config/
        DESTINATION /etc/midimind
        OPTIONAL
    )
endif()

# Documentation (si existe)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    install(FILES README.md
        DESTINATION /usr/share/doc/midimind
        OPTIONAL
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    install(FILES LICENSE
        DESTINATION /usr/share/doc/midimind
        OPTIONAL
    )
endif()

# ============================================================================
# PACKAGE
# ============================================================================

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "midimind")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MIDI Orchestration System for Raspberry Pi")
set(CPACK_PACKAGE_CONTACT "MidiMind Team <contact@midimind.io>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libasound2, libsqlite3-0")

include(CPack)

# ============================================================================
# RÉSUMÉ FINAL
# ============================================================================

message(STATUS "========================================")
message(STATUS "Configuration complete!")
message(STATUS "Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "Executable: ${CMAKE_BINARY_DIR}/bin/midimind")
message(STATUS "Source files: ${SOURCE_COUNT}")
message(STATUS "========================================")

# ============================================================================
# FIN DU FICHIER CMakeLists.txt v3.0.8
# ============================================================================
