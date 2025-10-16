# ============================================================================
# Fichier: CMakeLists.txt
# Version: 4.0.4
# Date: 2025-10-16
# ============================================================================
# CORRECTIONS v4.0.4:
#   ✅ Ajout configuration ASIO standalone
#   ✅ find_package pour ASIO avec fallback
#   ✅ Support ASIO_INCLUDE_DIR manuel si nécessaire
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(MidiMind 
    VERSION 3.0.0
    DESCRIPTION "MIDI Orchestration System for Raspberry Pi"
    LANGUAGES CXX
)

option(ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)
option(BUILD_TESTS "Build test suite" OFF)

message(STATUS "========================================")
message(STATUS "MidiMind v${PROJECT_VERSION}")
message(STATUS "  Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler:     ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  CXX Standard: 17")
message(STATUS "========================================")

# ============================================================================
# FLAGS
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")

if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")
endif()

# ============================================================================
# DÉPENDANCES
# ============================================================================

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# ALSA
find_library(ALSA_LIBRARY asound)
find_path(ALSA_INCLUDE_DIR alsa/asoundlib.h)
if(NOT ALSA_LIBRARY OR NOT ALSA_INCLUDE_DIR)
    message(FATAL_ERROR "ALSA required. Install: sudo apt install libasound2-dev")
endif()

# nlohmann/json
find_package(nlohmann_json 3.11.0 QUIET)

# Boost (optionnel)
find_package(Boost QUIET)

# ============================================================================
# ASIO (NOUVEAU)
# ============================================================================
# ASIO peut être installé via:
#   sudo apt install libasio-dev
# Ou téléchargé depuis: https://think-async.com/Asio/
#
# Si installé via apt, il sera dans /usr/include/asio.hpp
# Sinon, définir: cmake -DASIO_INCLUDE_DIR=/path/to/asio/include

find_path(ASIO_INCLUDE_DIR 
    NAMES asio.hpp
    PATHS
        /usr/include
        /usr/local/include
        ${CMAKE_SOURCE_DIR}/third_party/asio/include
        ${CMAKE_SOURCE_DIR}/external/asio/include
)

if(ASIO_INCLUDE_DIR)
    message(STATUS "✓ ASIO: ${ASIO_INCLUDE_DIR}")
    add_definitions(-DHAS_ASIO)
else()
    message(FATAL_ERROR "ASIO not found. Install with: sudo apt install libasio-dev")
endif()

# Afficher les dépendances trouvées
message(STATUS "✓ SQLite3: ${SQLite3_VERSION}")
message(STATUS "✓ ALSA: ${ALSA_LIBRARY}")
if(nlohmann_json_FOUND)
    message(STATUS "✓ nlohmann/json: ${nlohmann_json_VERSION}")
endif()
if(Boost_FOUND)
    message(STATUS "✓ Boost: ${Boost_VERSION} (optional)")
endif()

# ============================================================================
# FICHIERS SOURCE - CORE (2 REQUIS)
# ============================================================================

set(SOURCES_CORE
    src/main.cpp
    src/core/Application.cpp
)

message(STATUS "Core sources (required): 2")

# ============================================================================
# FICHIERS SOURCE - API (3 REQUIS)
# ============================================================================

set(SOURCES_API
    src/api/ApiServer.cpp
    src/api/CommandProcessorV2.cpp
    src/api/MessageEnvelope.cpp
)

message(STATUS "API sources (required): 3")
message(STATUS "  Note: commands/*.cpp included in CommandProcessorV2.cpp")

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

message(STATUS "MIDI Core sources (required): 5")

# ============================================================================
# FICHIERS SOURCE - MIDI DEVICES (3 REQUIS)
# ============================================================================

set(SOURCES_MIDI_DEVICES
    src/midi/devices/MidiDeviceManager.cpp
    src/midi/devices/UsbMidiDevice.cpp
    src/midi/devices/VirtualMidiDevice.cpp
)

message(STATUS "MIDI Devices sources (required): 3")
message(STATUS "  Note: WifiDevice.h is header-only plugin")
message(STATUS "  Note: BleMidiDevice.cpp is in network/ module")

# ============================================================================
# FICHIERS SOURCE - MIDI PLAYER (1 REQUIS)
# ============================================================================

set(SOURCES_MIDI_PLAYER
    src/midi/player/MidiPlayer.cpp
)

message(STATUS "MIDI Player sources (required): 1")

# ============================================================================
# FICHIERS SOURCE - MIDI FILE (2 REQUIS)
# ============================================================================

set(SOURCES_MIDI_FILE
    src/midi/file/MidiFileReader.cpp
    src/midi/file/MidiFileWriter.cpp
)

message(STATUS "MIDI File sources (required): 2")

# ============================================================================
# FICHIERS SOURCE - MIDI PROCESSING (1 REQUIS)
# ============================================================================

set(SOURCES_MIDI_PROCESSING
    src/midi/processing/ProcessorManager.cpp
)

message(STATUS "MIDI Processing sources (required): 1")

# ============================================================================
# FICHIERS SOURCE - SYSEX (2 REQUIS)
# ============================================================================

set(SOURCES_SYSEX
    src/midi/sysex/SysExHandler.cpp
    src/midi/sysex/SysExParser.cpp
)

message(STATUS "SysEx sources (required): 2")
message(STATUS "  Note: DeviceIdentity.h, NoteMap.h are header-only")

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

message(STATUS "Storage sources (required): 6")

# ============================================================================
# FICHIERS SOURCE - NETWORK (7 REQUIS)
# ============================================================================

set(SOURCES_NETWORK
    src/network/NetworkManager.cpp
    src/network/WifiManager.cpp
    src/network/BluetoothManager.cpp
    src/network/bluetooth/BleMidiDevice.cpp
    src/network/wifi/WiFiHotspot.cpp
    src/network/discovery/MdnsDiscovery.cpp
    src/network/rtpmidi/RtpMidiServer.cpp
)

message(STATUS "Network sources (required): 7")
message(STATUS "  Note: BleMidiDevice.cpp is here, not in midi/devices/")

# ============================================================================
# FICHIERS OPTIONNELS
# ============================================================================

set(SOURCES_OPTIONAL)

# EventBus
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/EventBus.cpp")
    list(APPEND SOURCES_OPTIONAL src/core/EventBus.cpp)
    message(STATUS "  ✓ EventBus.cpp")
endif()

# JsonValidator  
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/JsonValidator.cpp")
    list(APPEND SOURCES_OPTIONAL src/core/JsonValidator.cpp)
    message(STATUS "  ✓ JsonValidator.cpp")
endif()

# EditorState
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/editor/EditorState.cpp")
    list(APPEND SOURCES_OPTIONAL src/api/editor/EditorState.cpp)
    message(STATUS "  ✓ EditorState.cpp")
endif()

# MidiClock
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/MidiClock.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/MidiClock.cpp)
    message(STATUS "  ✓ MidiClock.cpp")
endif()

# MidiTransforms
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/MidiTransforms.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/MidiTransforms.cpp)
    message(STATUS "  ✓ MidiTransforms.cpp")
endif()

# TransportController
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/player/TransportController.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/player/TransportController.cpp)
    message(STATUS "  ✓ TransportController.cpp")
endif()

# PlaybackEngine
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/player/PlaybackEngine.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/player/PlaybackEngine.cpp)
    message(STATUS "  ✓ PlaybackEngine.cpp")
endif()

# MidiFileAnalyzer
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/file/MidiFileAnalyzer.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/file/MidiFileAnalyzer.cpp)
    message(STATUS "  ✓ MidiFileAnalyzer.cpp")
endif()

# ArpeggiatorProcessor
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/processing/creative/ArpeggiatorProcessor.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/processing/creative/ArpeggiatorProcessor.cpp)
    message(STATUS "  ✓ ArpeggiatorProcessor.cpp")
endif()

# DelayProcessor
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/processing/creative/DelayProcessor.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/processing/creative/DelayProcessor.cpp)
    message(STATUS "  ✓ DelayProcessor.cpp")
endif()

# PriorityQueueScheduler
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/routing/PriorityQueueScheduler.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/routing/PriorityQueueScheduler.cpp)
    message(STATUS "  ✓ PriorityQueueScheduler.cpp")
endif()

# CustomSysExParser
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/sysex/CustomSysExParser.cpp")
    list(APPEND SOURCES_OPTIONAL src/midi/sysex/CustomSysExParser.cpp)
    message(STATUS "  ✓ CustomSysExParser.cpp")
endif()

# Monitoring
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/monitoring/PerformanceMonitor.cpp")
    list(APPEND SOURCES_OPTIONAL src/monitoring/PerformanceMonitor.cpp)
    message(STATUS "  ✓ PerformanceMonitor.cpp")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/monitoring/SystemMonitor.cpp")
    list(APPEND SOURCES_OPTIONAL src/monitoring/SystemMonitor.cpp)
    message(STATUS "  ✓ SystemMonitor.cpp")
endif()

# ============================================================================
# EXÉCUTABLE
# ============================================================================

add_executable(midimind
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

# ============================================================================
# INCLUDES
# ============================================================================

target_include_directories(midimind PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SQLite3_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIR}
    ${ASIO_INCLUDE_DIR}
)

if(Boost_FOUND)
    target_include_directories(midimind PRIVATE ${Boost_INCLUDE_DIRS})
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
)

# ============================================================================
# INSTALLATION
# ============================================================================

install(TARGETS midimind RUNTIME DESTINATION bin)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config/")
    install(DIRECTORY config/ DESTINATION /etc/midimind OPTIONAL)
endif()

install(FILES README.md LICENSE DESTINATION /usr/share/doc/midimind OPTIONAL)

# ============================================================================
# PACKAGE
# ============================================================================

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "midimind")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MIDI Orchestration System")
set(CPACK_PACKAGE_CONTACT "MidiMind Team")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libasound2, libsqlite3-0, libasio-dev")

include(CPack)

# ============================================================================
# RÉSUMÉ
# ============================================================================

message(STATUS "========================================")
message(STATUS "Configuration complete!")
message(STATUS "  Build dir:    ${CMAKE_BINARY_DIR}")
message(STATUS "  Executable:   ${CMAKE_BINARY_DIR}/bin/midimind")
message(STATUS "========================================")
message(STATUS "Build commands:")
message(STATUS "  make          - Compile")
message(STATUS "  make install  - Install")
message(STATUS "  make package  - Create DEB")
message(STATUS "========================================")

# ============================================================================
# FIN DU FICHIER CMakeLists.txt v4.0.4
# ============================================================================
