# ============================================================================
# Fichier: backend/CMakeLists.txt
# Version: 4.0.4 - VÉRIFICATION COMPLÈTE ET OPTIMISATIONS
# Date: 2025-10-15
# Projet: MidiMind - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
#
# CORRECTIONS v4.0.4 - VÉRIFICATION COMPLÈTE:
#   ✅ Tous les 32 fichiers sources requis confirmés
#   ✅ Fichiers optionnels correctement gérés
#   ✅ NetworkManager avec tous ses composants
#   ✅ Headers-only exclus de la compilation
#   ✅ Dépendances système optimisées
#   ✅ Messages de status améliorés
#   ✅ Gestion des erreurs renforcée
#
# TOTAL FICHIERS REQUIS: 32
# TOTAL FICHIERS OPTIONNELS: Variables selon présence
#
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(MidiMind 
    VERSION 4.0.4
    LANGUAGES CXX
    DESCRIPTION "MIDI Orchestration System for Raspberry Pi"
)

# ============================================================================
# OPTIONS
# ============================================================================

option(BUILD_TESTS "Build test suite" OFF)
option(BUILD_DOCS "Build documentation" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers (Debug only)" ON)
option(ENABLE_NETWORK "Enable network features (WiFi/BT/RTP-MIDI)" ON)

# ============================================================================
# CONFIGURATION
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "╔════════════════════════════════════════╗")
message(STATUS "║     MidiMind v${PROJECT_VERSION}                   ║")
message(STATUS "╚════════════════════════════════════════╝")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "Sanitizers:      ${ENABLE_SANITIZERS}")
message(STATUS "Network Module:  ${ENABLE_NETWORK}")
message(STATUS "========================================")

# ============================================================================
# FLAGS
# ============================================================================

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")

if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")
    message(STATUS "✓ Sanitizers enabled (Debug mode)")
endif()

# ============================================================================
# DÉPENDANCES SYSTÈME
# ============================================================================

message(STATUS "")
message(STATUS "Checking system dependencies...")
message(STATUS "────────────────────────────────────────")

# Threads (pthread)
find_package(Threads REQUIRED)
message(STATUS "✓ Threads: found")

# SQLite3
find_package(SQLite3 REQUIRED)
if(SQLite3_FOUND)
    message(STATUS "✓ SQLite3: ${SQLite3_VERSION}")
else()
    message(FATAL_ERROR "❌ SQLite3 required. Install: sudo apt install libsqlite3-dev")
endif()

# ALSA (pour MIDI USB/Virtual)
find_library(ALSA_LIBRARY asound)
find_path(ALSA_INCLUDE_DIR alsa/asoundlib.h)
if(ALSA_LIBRARY AND ALSA_INCLUDE_DIR)
    message(STATUS "✓ ALSA: ${ALSA_LIBRARY}")
else()
    message(FATAL_ERROR "❌ ALSA required. Install: sudo apt install libasound2-dev")
endif()

# nlohmann/json (JSON parsing)
find_package(nlohmann_json 3.11.0 QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "✓ nlohmann/json: ${nlohmann_json_VERSION}")
else()
    message(WARNING "⚠ nlohmann/json not found. Install: sudo apt install nlohmann-json3-dev")
endif()

# Boost (optionnel, pour utilitaires)
find_package(Boost QUIET)
if(Boost_FOUND)
    message(STATUS "✓ Boost: ${Boost_VERSION} (optional)")
endif()

message(STATUS "────────────────────────────────────────")
message(STATUS "")

# ============================================================================
# FICHIERS SOURCE - CORE (2 REQUIS)
# ============================================================================
# NOTE: Config.h, Logger.h, ErrorManager.h sont HEADERS-ONLY (pas de .cpp)

set(SOURCES_CORE
    src/main.cpp
    src/core/Application.cpp
)

message(STATUS "📁 Core sources (required): ${CMAKE_CURRENT_LIST_LINE}")
message(STATUS "   ├─ main.cpp")
message(STATUS "   └─ Application.cpp")

# ============================================================================
# FICHIERS SOURCE - API (3 REQUIS)
# ============================================================================
# NOTE: Les fichiers commands/*.cpp sont INCLUS dans CommandProcessorV2.cpp
#       via #include, ils ne se compilent PAS séparément !

set(SOURCES_API
    src/api/ApiServer.cpp
    src/api/CommandProcessorV2.cpp
    src/api/MessageEnvelope.cpp
)

message(STATUS "📁 API sources (required): 3")
message(STATUS "   ├─ ApiServer.cpp")
message(STATUS "   ├─ CommandProcessorV2.cpp")
message(STATUS "   └─ MessageEnvelope.cpp")
message(STATUS "   Note: commands/*.cpp included in CommandProcessorV2.cpp")

# ============================================================================
# FICHIERS SOURCE - MIDI CORE (5 REQUIS)
# ============================================================================

set(SOURCES_MIDI_CORE
    src/midi/MidiMessage.cpp
    src/midi/MidiRouter.cpp
    src/midi/MidiFileManager.cpp
    src/midi/MidiValidator.cpp
    src/midi/JsonMidiConverter.cpp
)

message(STATUS "📁 MIDI Core sources (required): 5")
message(STATUS "   ├─ MidiMessage.cpp")
message(STATUS "   ├─ MidiRouter.cpp")
message(STATUS "   ├─ MidiFileManager.cpp")
message(STATUS "   ├─ MidiValidator.cpp")
message(STATUS "   └─ JsonMidiConverter.cpp")

# ============================================================================
# FICHIERS SOURCE - MIDI DEVICES (3 REQUIS)
# ============================================================================
# NOTE: WifiDevice.h est HEADER-ONLY (plugin)
# NOTE: BleMidiDevice.cpp est dans SOURCES_NETWORK, pas ici

set(SOURCES_MIDI_DEVICES
    src/midi/devices/MidiDeviceManager.cpp
    src/midi/devices/UsbMidiDevice.cpp
    src/midi/devices/VirtualMidiDevice.cpp
)

message(STATUS "📁 MIDI Devices sources (required): 3")
message(STATUS "   ├─ MidiDeviceManager.cpp")
message(STATUS "   ├─ UsbMidiDevice.cpp")
message(STATUS "   └─ VirtualMidiDevice.cpp")
message(STATUS "   Note: WifiDevice.h is header-only")
message(STATUS "   Note: BleMidiDevice.cpp in network/ module")

# ============================================================================
# FICHIERS SOURCE - MIDI PLAYER (1 REQUIS)
# ============================================================================

set(SOURCES_MIDI_PLAYER
    src/midi/player/MidiPlayer.cpp
)

message(STATUS "📁 MIDI Player sources (required): 1")
message(STATUS "   └─ MidiPlayer.cpp")

# ============================================================================
# FICHIERS SOURCE - MIDI FILE (2 REQUIS)
# ============================================================================

set(SOURCES_MIDI_FILE
    src/midi/file/MidiFileReader.cpp
    src/midi/file/MidiFileWriter.cpp
)

message(STATUS "📁 MIDI File sources (required): 2")
message(STATUS "   ├─ MidiFileReader.cpp")
message(STATUS "   └─ MidiFileWriter.cpp")

# ============================================================================
# FICHIERS SOURCE - MIDI PROCESSING (1 REQUIS)
# ============================================================================

set(SOURCES_MIDI_PROCESSING
    src/midi/processing/ProcessorManager.cpp
)

message(STATUS "📁 MIDI Processing sources (required): 1")
message(STATUS "   └─ ProcessorManager.cpp")

# ============================================================================
# FICHIERS SOURCE - SYSEX (2 REQUIS)
# ============================================================================
# NOTE: DeviceIdentity.h, NoteMap.h sont HEADERS-ONLY

set(SOURCES_SYSEX
    src/midi/sysex/SysExHandler.cpp
    src/midi/sysex/SysExParser.cpp
)

message(STATUS "📁 SysEx sources (required): 2")
message(STATUS "   ├─ SysExHandler.cpp")
message(STATUS "   └─ SysExParser.cpp")
message(STATUS "   Note: DeviceIdentity.h, NoteMap.h header-only")

# ============================================================================
# FICHIERS SOURCE - STORAGE (6 REQUIS)
# ============================================================================

set(SOURCES_STORAGE
    src/storage/Database.cpp
    src/storage/FileManager.cpp
    src/storage/PathManager.cpp
    src/storage/PresetManager.cpp
    src/storage/SessionManager.cpp
    src/storage/Settings.cpp
)

message(STATUS "📁 Storage sources (required): 6")
message(STATUS "   ├─ Database.cpp")
message(STATUS "   ├─ FileManager.cpp")
message(STATUS "   ├─ PathManager.cpp")
message(STATUS "   ├─ PresetManager.cpp")
message(STATUS "   ├─ SessionManager.cpp")
message(STATUS "   └─ Settings.cpp")

# ============================================================================
# FICHIERS SOURCE - NETWORK (7 REQUIS si ENABLE_NETWORK=ON)
# ============================================================================

set(SOURCES_NETWORK)

if(ENABLE_NETWORK)
    set(SOURCES_NETWORK
        src/network/NetworkManager.cpp
        src/network/WifiManager.cpp
        src/network/BluetoothManager.cpp
        src/network/bluetooth/BleMidiDevice.cpp
        src/network/wifi/WiFiHotspot.cpp
        src/network/discovery/MdnsDiscovery.cpp
        src/network/rtpmidi/RtpMidiServer.cpp
    )
    
    message(STATUS "📁 Network sources (enabled): 7")
    message(STATUS "   ├─ NetworkManager.cpp")
    message(STATUS "   ├─ WifiManager.cpp")
    message(STATUS "   ├─ BluetoothManager.cpp")
    message(STATUS "   ├─ bluetooth/BleMidiDevice.cpp")
    message(STATUS "   ├─ wifi/WiFiHotspot.cpp")
    message(STATUS "   ├─ discovery/MdnsDiscovery.cpp")
    message(STATUS "   └─ rtpmidi/RtpMidiServer.cpp")
else()
    message(STATUS "📁 Network sources: DISABLED")
endif()

# ============================================================================
# FICHIERS OPTIONNELS
# ============================================================================

set(SOURCES_OPTIONAL)
set(OPTIONAL_COUNT 0)

message(STATUS "")
message(STATUS "📦 Checking optional sources...")
message(STATUS "────────────────────────────────────────")

# EventBus
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/EventBus.cpp")
    list(APPEND SOURCES_OPTIONAL src/core/EventBus.cpp)
    math(EXPR OPTIONAL_COUNT "${OPTIONAL_COUNT} + 1")
    message(STATUS "   ✓ EventBus.cpp")
endif()

# JsonValidator  
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/JsonValidator.cpp")
    list(APPEND SOURCES_OPTIONAL src/core/JsonValidator.cpp)
    math(EXPR OPTIONAL_COUNT "${OPTIONAL_COUNT} + 1")
    message(STATUS "   ✓ JsonValidator.cpp")
endif()

# EditorState
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/editor/EditorState.cpp")
    list(APPEND SOURCES_OPTIONAL src/api/editor/EditorState.cpp)
    math(EXPR OPTIONAL_COUNT "${OPTIONAL_COUNT} + 1")
    message(STATUS "   ✓ EditorState.cpp")
endif()

# MidiClock
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/MidiClock.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/MidiClock.cpp)
    math(EXPR OPTIONAL_COUNT "${OPTIONAL_COUNT} + 1")
    message(STATUS "   ✓ MidiClock.cpp")
endif()

# MidiTransforms
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/MidiTransforms.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/MidiTransforms.cpp)
    math(EXPR OPTIONAL_COUNT "${OPTIONAL_COUNT} + 1")
    message(STATUS "   ✓ MidiTransforms.cpp")
endif()

# MidiFileAnalyzer
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/file/MidiFileAnalyzer.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/file/MidiFileAnalyzer.cpp)
    math(EXPR OPTIONAL_COUNT "${OPTIONAL_COUNT} + 1")
    message(STATUS "   ✓ MidiFileAnalyzer.cpp")
endif()

# TransportController
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/player/TransportController.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/player/TransportController.cpp)
    math(EXPR OPTIONAL_COUNT "${OPTIONAL_COUNT} + 1")
    message(STATUS "   ✓ TransportController.cpp")
endif()

# Processors créatifs
set(CREATIVE_PROCESSORS
    "ArpeggiatorProcessor"
    "DelayProcessor"
    "HarmonizerProcessor"
    "TransposeProcessor"
)

foreach(PROCESSOR ${CREATIVE_PROCESSORS})
    set(PROCESSOR_PATH "src/midi/processing/creative/${PROCESSOR}.cpp")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PROCESSOR_PATH}")
        list(APPEND SOURCES_OPTIONAL ${PROCESSOR_PATH})
        math(EXPR OPTIONAL_COUNT "${OPTIONAL_COUNT} + 1")
        message(STATUS "   ✓ ${PROCESSOR}.cpp")
    endif()
endforeach()

# Processors effects
set(EFFECT_PROCESSORS
    "ChorusProcessor"
    "CompressorProcessor"
    "FilterProcessor"
    "ReverbProcessor"
)

foreach(PROCESSOR ${EFFECT_PROCESSORS})
    set(PROCESSOR_PATH "src/midi/processing/effects/${PROCESSOR}.cpp")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PROCESSOR_PATH}")
        list(APPEND SOURCES_OPTIONAL ${PROCESSOR_PATH})
        math(EXPR OPTIONAL_COUNT "${OPTIONAL_COUNT} + 1")
        message(STATUS "   ✓ ${PROCESSOR}.cpp")
    endif()
endforeach()

# PriorityQueueScheduler
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/routing/PriorityQueueScheduler.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/routing/PriorityQueueScheduler.cpp)
    math(EXPR OPTIONAL_COUNT "${OPTIONAL_COUNT} + 1")
    message(STATUS "   ✓ PriorityQueueScheduler.cpp")
endif()

# CustomSysExParser
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/sysex/CustomSysExParser.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/sysex/CustomSysExParser.cpp)
    math(EXPR OPTIONAL_COUNT "${OPTIONAL_COUNT} + 1")
    message(STATUS "   ✓ CustomSysExParser.cpp")
endif()

if(OPTIONAL_COUNT EQUAL 0)
    message(STATUS "   (none found)")
endif()

message(STATUS "────────────────────────────────────────")
message(STATUS "Total optional sources: ${OPTIONAL_COUNT}")
message(STATUS "")

# ============================================================================
# COMBINAISON DES SOURCES
# ============================================================================

set(ALL_SOURCES
    ${SOURCES_CORE}
    ${SOURCES_API}
    ${SOURCES_MIDI_CORE}
    ${SOURCES_MIDI_DEVICES}
    ${SOURCES_MIDI_PLAYER}
    ${SOURCES_MIDI_FILE}
    ${SOURCES_MIDI_PROCESSING}
    ${SOURCES_SYSEX}
    ${SOURCES_STORAGE}
    ${SOURCES_NETWORK}
    ${SOURCES_OPTIONAL}
)

# Compter les sources
list(LENGTH SOURCES_CORE COUNT_CORE)
list(LENGTH SOURCES_API COUNT_API)
list(LENGTH SOURCES_MIDI_CORE COUNT_MIDI_CORE)
list(LENGTH SOURCES_MIDI_DEVICES COUNT_MIDI_DEVICES)
list(LENGTH SOURCES_MIDI_PLAYER COUNT_MIDI_PLAYER)
list(LENGTH SOURCES_MIDI_FILE COUNT_MIDI_FILE)
list(LENGTH SOURCES_MIDI_PROCESSING COUNT_MIDI_PROCESSING)
list(LENGTH SOURCES_SYSEX COUNT_SYSEX)
list(LENGTH SOURCES_STORAGE COUNT_STORAGE)
list(LENGTH SOURCES_NETWORK COUNT_NETWORK)
list(LENGTH ALL_SOURCES COUNT_TOTAL)

math(EXPR COUNT_REQUIRED "${COUNT_CORE} + ${COUNT_API} + ${COUNT_MIDI_CORE} + ${COUNT_MIDI_DEVICES} + ${COUNT_MIDI_PLAYER} + ${COUNT_MIDI_FILE} + ${COUNT_MIDI_PROCESSING} + ${COUNT_SYSEX} + ${COUNT_STORAGE} + ${COUNT_NETWORK}")

message(STATUS "════════════════════════════════════════")
message(STATUS "📊 Source Files Summary")
message(STATUS "────────────────────────────────────────")
message(STATUS "  Required sources:  ${COUNT_REQUIRED}")
message(STATUS "  Optional sources:  ${OPTIONAL_COUNT}")
message(STATUS "  ────────────────────────────")
message(STATUS "  TOTAL:             ${COUNT_TOTAL}")
message(STATUS "════════════════════════════════════════")
message(STATUS "")

# ============================================================================
# CIBLE EXÉCUTABLE
# ============================================================================

add_executable(midimind ${ALL_SOURCES})

# ============================================================================
# INCLUDES
# ============================================================================

target_include_directories(midimind PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SQLite3_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIR}
)

if(Boost_FOUND)
    target_include_directories(midimind PRIVATE ${Boost_INCLUDE_DIRS})
endif()

if(nlohmann_json_FOUND)
    # nlohmann_json est header-only, pas besoin d'include_directories
endif()

# ============================================================================
# LIBRARIES
# ============================================================================

target_link_libraries(midimind PRIVATE
    Threads::Threads
    ${SQLite3_LIBRARIES}
    ${ALSA_LIBRARY}
)

if(Boost_FOUND)
    target_link_libraries(midimind PRIVATE ${Boost_LIBRARIES})
endif()

if(nlohmann_json_FOUND)
    target_link_libraries(midimind PRIVATE nlohmann_json::nlohmann_json)
endif()

# ============================================================================
# PROPRIÉTÉS
# ============================================================================

set_target_properties(midimind PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "midimind"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# ============================================================================
# INSTALLATION
# ============================================================================

install(TARGETS midimind 
    RUNTIME DESTINATION bin
    COMPONENT runtime
)

# Configuration files
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config/")
    install(DIRECTORY config/ 
        DESTINATION /etc/midimind 
        OPTIONAL
        COMPONENT config
    )
endif()

# Documentation
install(FILES 
    README.md 
    LICENSE 
    DESTINATION /usr/share/doc/midimind 
    OPTIONAL
    COMPONENT documentation
)

# ============================================================================
# PACKAGE (DEB)
# ============================================================================

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "midimind")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "MidiMind Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MIDI Orchestration System for Raspberry Pi")
set(CPACK_PACKAGE_DESCRIPTION "Complete MIDI orchestration and routing system with WebSocket API, multi-device support, and advanced processing capabilities.")
set(CPACK_PACKAGE_CONTACT "contact@midimind.io")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/midimind/midimind")

# Dépendances Debian
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libasound2 (>= 1.2.0), libsqlite3-0 (>= 3.36.0)")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# Composants
set(CPACK_COMPONENTS_ALL runtime config documentation)
set(CPACK_COMPONENT_RUNTIME_REQUIRED ON)

include(CPack)

# ============================================================================
# RÉSUMÉ FINAL
# ============================================================================

message(STATUS "")
message(STATUS "╔════════════════════════════════════════╗")
message(STATUS "║   Configuration Complete! ✓            ║")
message(STATUS "╚════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "  📂 Build directory:  ${CMAKE_BINARY_DIR}")
message(STATUS "  🎯 Executable:       ${CMAKE_BINARY_DIR}/bin/midimind")
message(STATUS "  🔧 Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  📊 Source files:     ${COUNT_TOTAL}")
message(STATUS "")
message(STATUS "════════════════════════════════════════")
message(STATUS "🚀 Build Commands:")
message(STATUS "────────────────────────────────────────")
message(STATUS "  make                 - Compile")
message(STATUS "  make -j$(nproc)      - Parallel compile")
message(STATUS "  make install         - Install system-wide")
message(STATUS "  make package         - Create DEB package")
message(STATUS "  make clean           - Clean build")
message(STATUS "════════════════════════════════════════")
message(STATUS "")

# ============================================================================
# FIN DU FICHIER CMakeLists.txt v4.0.4
# ============================================================================