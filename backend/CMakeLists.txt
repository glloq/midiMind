# ============================================================================
# Fichier: backend/CMakeLists.txt
# Version: 4.0.1 - CORRIGÉ avec vrais noms de fichiers
# Date: 2025-10-14
# ============================================================================
# Corrections v4.0.1:
#   ✅ DeviceIdentity.cpp (était SysExIdentity.cpp)
#   ✅ NoteMap.cpp (était SysExNotemap.cpp)
#   ✅ UsbMidiDevice.cpp (était AlsaDevice.cpp)
#   ✅ BleMidiDevice.cpp (était BluetoothDevice.cpp)
#   ✅ VirtualMidiDevice.cpp (était VirtualDevice.cpp)
#   ✅ MessageEnvelope.cpp confirmé existant
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(MidiMind VERSION 4.0.1 LANGUAGES CXX)

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
option(BUILD_DOCS "Build documentation" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers (Debug only)" ON)

message(STATUS "========================================")
message(STATUS "MidiMind v${PROJECT_VERSION} Configuration")
message(STATUS "========================================")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler:        ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build tests:     ${BUILD_TESTS}")
message(STATUS "Build docs:      ${BUILD_DOCS}")
message(STATUS "Sanitizers:      ${ENABLE_SANITIZERS}")
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

# Flags Debug
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -DDEBUG)
    
    if(ENABLE_SANITIZERS)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()

# Flags Release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -DNDEBUG -march=native -flto)
    add_link_options(-flto)
endif()

# ============================================================================
# DÉPENDANCES
# ============================================================================

# Threads (requis)
find_package(Threads REQUIRED)

# SQLite3 (requis)
find_package(SQLite3 REQUIRED)
if(NOT SQLite3_FOUND)
    message(FATAL_ERROR "SQLite3 not found. Install: sudo apt install libsqlite3-dev")
endif()

# ALSA (requis pour MIDI sur Linux)
find_library(ALSA_LIBRARY asound)
if(NOT ALSA_LIBRARY)
    message(FATAL_ERROR "ALSA not found. Install: sudo apt install libasound2-dev")
endif()

# nlohmann/json (requis)
find_package(nlohmann_json 3.11.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(WARNING "nlohmann/json not found via CMake, assuming header-only in include/")
endif()

# Boost (optionnel)
find_package(Boost QUIET)
if(Boost_FOUND)
    message(STATUS "Boost found: ${Boost_VERSION}")
else()
    message(STATUS "Boost not found (optional)")
endif()

# ============================================================================
# FICHIERS SOURCE - CORE
# ============================================================================

set(SOURCES_CORE
    src/main.cpp
    src/core/Application.cpp
    src/core/Config.cpp
    src/core/Logger.cpp
    src/core/ErrorManager.cpp
)

# EventBus (optionnel)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/EventBus.cpp")
    list(APPEND SOURCES_CORE src/core/EventBus.cpp)
    message(STATUS "✓ EventBus.cpp found")
endif()

# JsonValidator (optionnel)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/JsonValidator.cpp")
    list(APPEND SOURCES_CORE src/core/JsonValidator.cpp)
    message(STATUS "✓ JsonValidator.cpp found")
endif()

# ============================================================================
# FICHIERS SOURCE - API
# ============================================================================

set(SOURCES_API
    # API Server
    src/api/ApiServer.cpp
    src/api/CommandProcessorV2.cpp
    src/api/MessageEnvelope.cpp       # ✅ Confirmé existant
    
    # Commands handlers
    src/api/commands/devices.cpp
    src/api/commands/routing.cpp
    src/api/commands/playback.cpp
    src/api/commands/files.cpp
    src/api/commands/system.cpp
    src/api/commands/editor.cpp
    src/api/commands/network.cpp
    src/api/commands/logger.cpp
)

# loops.cpp (optionnel)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/commands/loops.cpp")
    list(APPEND SOURCES_API src/api/commands/loops.cpp)
    message(STATUS "✓ loops.cpp found")
endif()

# EditorState (optionnel)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/editor/EditorState.cpp")
    list(APPEND SOURCES_API src/api/editor/EditorState.cpp)
    message(STATUS "✓ EditorState.cpp found")
endif()

message(STATUS "API sources: ${SOURCES_API}")

# ============================================================================
# FICHIERS SOURCE - MIDI CORE
# ============================================================================

set(SOURCES_MIDI_CORE
    src/midi/MidiMessage.cpp
    src/midi/MidiRouter.cpp
    src/midi/MidiFileManager.cpp
    src/midi/MidiValidator.cpp
    src/midi/JsonMidiConverter.cpp
)

# MidiClock (optionnel)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/MidiClock.cpp")
    list(APPEND SOURCES_MIDI_CORE src/midi/MidiClock.cpp)
    message(STATUS "✓ MidiClock.cpp found")
endif()

# MidiTransforms (optionnel)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/MidiTransforms.cpp")
    list(APPEND SOURCES_MIDI_CORE src/midi/MidiTransforms.cpp)
    message(STATUS "✓ MidiTransforms.cpp found")
endif()

message(STATUS "MIDI Core sources: ${SOURCES_MIDI_CORE}")

# ============================================================================
# FICHIERS SOURCE - MIDI DEVICES
# ============================================================================

set(SOURCES_MIDI_DEVICES
    src/midi/devices/MidiDeviceManager.cpp
    src/midi/devices/UsbMidiDevice.cpp        # ✅ Corrigé (était AlsaDevice.cpp)
    src/midi/devices/WifiDevice.cpp
    src/midi/devices/BleMidiDevice.cpp        # ✅ Corrigé (était BluetoothDevice.cpp)
    src/midi/devices/VirtualMidiDevice.cpp    # ✅ Corrigé (était VirtualDevice.cpp)
)

message(STATUS "MIDI Devices sources: ${SOURCES_MIDI_DEVICES}")

# ============================================================================
# FICHIERS SOURCE - MIDI PLAYER
# ============================================================================

set(SOURCES_MIDI_PLAYER
    src/midi/player/MidiPlayer.cpp    # ✅ Chemin corrigé (était dans file/)
)

# TransportController (optionnel)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/player/TransportController.cpp")
    list(APPEND SOURCES_MIDI_PLAYER src/midi/player/TransportController.cpp)
    message(STATUS "✓ TransportController.cpp found")
endif()

# PlaybackEngine (optionnel)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/player/PlaybackEngine.cpp")
    list(APPEND SOURCES_MIDI_PLAYER src/midi/player/PlaybackEngine.cpp)
    message(STATUS "✓ PlaybackEngine.cpp found")
endif()

message(STATUS "MIDI Player sources: ${SOURCES_MIDI_PLAYER}")

# ============================================================================
# FICHIERS SOURCE - MIDI FILE
# ============================================================================

set(SOURCES_MIDI_FILE
    src/midi/file/MidiFileReader.cpp
    src/midi/file/MidiFileWriter.cpp
)

# MidiFileAnalyzer (optionnel)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/file/MidiFileAnalyzer.cpp")
    list(APPEND SOURCES_MIDI_FILE src/midi/file/MidiFileAnalyzer.cpp)
    message(STATUS "✓ MidiFileAnalyzer.cpp found")
endif()

message(STATUS "MIDI File sources: ${SOURCES_MIDI_FILE}")

# ============================================================================
# FICHIERS SOURCE - MIDI PROCESSING
# ============================================================================

set(SOURCES_MIDI_PROCESSING
    src/midi/processing/ProcessorManager.cpp
)

# Processeurs créatifs (optionnels)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/processing/creative/ArpeggiatorProcessor.cpp")
    list(APPEND SOURCES_MIDI_PROCESSING src/midi/processing/creative/ArpeggiatorProcessor.cpp)
    message(STATUS "✓ ArpeggiatorProcessor.cpp found")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/processing/creative/DelayProcessor.cpp")
    list(APPEND SOURCES_MIDI_PROCESSING src/midi/processing/creative/DelayProcessor.cpp)
    message(STATUS "✓ DelayProcessor.cpp found")
endif()

message(STATUS "MIDI Processing sources: ${SOURCES_MIDI_PROCESSING}")

# ============================================================================
# FICHIERS SOURCE - MIDI ROUTING
# ============================================================================

set(SOURCES_MIDI_ROUTING)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/routing/PriorityQueueScheduler.cpp")
    list(APPEND SOURCES_MIDI_ROUTING src/midi/routing/PriorityQueueScheduler.cpp)
    message(STATUS "✓ PriorityQueueScheduler.cpp found")
endif()

# ============================================================================
# FICHIERS SOURCE - SYSEX
# ============================================================================

set(SOURCES_SYSEX
    src/midi/sysex/SysExHandler.cpp
    src/midi/sysex/SysExValidator.cpp
    src/midi/sysex/SysExRegistry.cpp
    src/midi/sysex/SysExParser.cpp
    src/midi/sysex/DeviceIdentity.cpp        # ✅ Corrigé (était SysExIdentity.cpp)
    src/midi/sysex/NoteMap.cpp               # ✅ Corrigé (était SysExNotemap.cpp)
)

# CustomSysExParser (optionnel)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/sysex/CustomSysExParser.cpp")
    list(APPEND SOURCES_SYSEX src/midi/sysex/CustomSysExParser.cpp)
    message(STATUS "✓ CustomSysExParser.cpp found")
endif()

message(STATUS "SysEx sources: ${SOURCES_SYSEX}")

# ============================================================================
# FICHIERS SOURCE - STORAGE
# ============================================================================

set(SOURCES_STORAGE
    src/storage/Database.cpp
    src/storage/FileManager.cpp
    src/storage/PathManager.cpp
    src/storage/PresetManager.cpp
    src/storage/SessionManager.cpp
    src/storage/Settings.cpp
)

message(STATUS "Storage sources: ${SOURCES_STORAGE}")

# ============================================================================
# FICHIERS SOURCE - NETWORK
# ============================================================================

set(SOURCES_NETWORK
    src/network/NetworkManager.cpp
    src/network/WifiManager.cpp
    src/network/BluetoothManager.cpp
)

# MdnsDiscovery (optionnel, avec stubs)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/network/MdnsDiscovery.cpp")
    list(APPEND SOURCES_NETWORK src/network/MdnsDiscovery.cpp)
    message(STATUS "✓ MdnsDiscovery.cpp found")
else()
    message(STATUS "⚠ MdnsDiscovery.cpp not found (optional with stubs)")
endif()

message(STATUS "Network sources: ${SOURCES_NETWORK}")

# ============================================================================
# FICHIERS SOURCE - MONITORING
# ============================================================================

set(SOURCES_MONITORING)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/monitoring/PerformanceMonitor.cpp")
    list(APPEND SOURCES_MONITORING src/monitoring/PerformanceMonitor.cpp)
    message(STATUS "✓ PerformanceMonitor.cpp found")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/monitoring/HealthMonitor.cpp")
    list(APPEND SOURCES_MONITORING src/monitoring/HealthMonitor.cpp)
    message(STATUS "✓ HealthMonitor.cpp found")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/monitoring/MetricsCollector.cpp")
    list(APPEND SOURCES_MONITORING src/monitoring/MetricsCollector.cpp)
    message(STATUS "✓ MetricsCollector.cpp found")
endif()

# ============================================================================
# FICHIERS SOURCE - TIMING
# ============================================================================

set(SOURCES_TIMING)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/timing/LatencyCompensator.cpp")
    list(APPEND SOURCES_TIMING src/timing/LatencyCompensator.cpp)
    message(STATUS "✓ LatencyCompensator.cpp found")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/timing/TimestampManager.cpp")
    list(APPEND SOURCES_TIMING src/timing/TimestampManager.cpp)
    message(STATUS "✓ TimestampManager.cpp found")
endif()

# ============================================================================
# FICHIERS SOURCE - LOOP
# ============================================================================

set(SOURCES_LOOP)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/loop/LoopManager.cpp")
    list(APPEND SOURCES_LOOP src/loop/LoopManager.cpp)
    message(STATUS "✓ LoopManager.cpp found")
else()
    message(STATUS "⚠ LoopManager.cpp not found (optional)")
endif()

# ============================================================================
# AGRÉGATION DE TOUS LES SOURCES
# ============================================================================

set(ALL_SOURCES
    ${SOURCES_CORE}
    ${SOURCES_API}
    ${SOURCES_MIDI_CORE}
    ${SOURCES_MIDI_DEVICES}
    ${SOURCES_MIDI_PLAYER}
    ${SOURCES_MIDI_FILE}
    ${SOURCES_MIDI_PROCESSING}
    ${SOURCES_MIDI_ROUTING}
    ${SOURCES_SYSEX}
    ${SOURCES_STORAGE}
    ${SOURCES_NETWORK}
    ${SOURCES_MONITORING}
    ${SOURCES_TIMING}
    ${SOURCES_LOOP}
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

# Ajouter Boost si trouvé
if(Boost_FOUND)
    target_include_directories(midimind PRIVATE ${Boost_INCLUDE_DIRS})
endif()

# ============================================================================
# LINK LIBRARIES
# ============================================================================

target_link_libraries(midimind PRIVATE
    Threads::Threads
    SQLite::SQLite3
    ${ALSA_LIBRARY}
    stdc++fs  # Pour std::filesystem
)

# Ajouter nlohmann_json si trouvé via CMake
if(nlohmann_json_FOUND)
    target_link_libraries(midimind PRIVATE nlohmann_json::nlohmann_json)
endif()

# Ajouter Boost si trouvé
if(Boost_FOUND)
    target_link_libraries(midimind PRIVATE ${Boost_LIBRARIES})
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
    add_subdirectory(tests)
    message(STATUS "Tests enabled")
endif()

# ============================================================================
# EXAMPLES (optionnel)
# ============================================================================

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
    message(STATUS "Examples enabled")
endif()

# ============================================================================
# DOCUMENTATION (optionnel)
# ============================================================================

if(BUILD_DOCS)
    find_package(Doxygen QUIET)
    
    if(Doxygen_FOUND)
        set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs)
        set(DOXYGEN_GENERATE_HTML YES)
        set(DOXYGEN_GENERATE_MAN NO)
        set(DOXYGEN_EXTRACT_ALL YES)
        set(DOXYGEN_EXTRACT_PRIVATE YES)
        set(DOXYGEN_RECURSIVE YES)
        
        doxygen_add_docs(docs
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            COMMENT "Generating API documentation with Doxygen"
        )
        
        message(STATUS "Documentation target 'docs' created")
    else()
        message(WARNING "Doxygen not found. Documentation disabled.")
    endif()
endif()

# ============================================================================
# INSTALLATION
# ============================================================================

install(TARGETS midimind
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION /etc/midimind
    OPTIONAL
)

install(FILES README.md LICENSE
    DESTINATION /usr/share/doc/midimind
    OPTIONAL
)

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
message(STATUS "========================================")

# ============================================================================
# FIN DU FICHIER CMakeLists.txt v4.0.1
# ============================================================================