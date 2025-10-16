# ============================================================================
# File: backend/CMakeLists.txt
# Version: 4.1.0
# Date: 2025-10-16
# Project: MidiMind - MIDI Orchestration System for Raspberry Pi
# ============================================================================
#
# CHANGES v4.1.0:
#   ✅ ADDED: InstrumentDatabase sources
#   ✅ ADDED: Timing system (InstrumentLatencyProfile, CalibrationEngine)
#   ✅ ADDED: Database migration 005_instruments_latency.sql
#   ⚠️  REMOVED: Network sources (deferred to v4.2.0)
#   ⚠️  FOCUS: USB MIDI only (WiFi/Bluetooth in v4.2.0)
#
# TOTAL FILES REQUIRED: 40 (confirmed for v4.1.0)
#
# ============================================================================

cmake_minimum_required(VERSION 3.10)

project(MidiMind 
    VERSION 4.1.0
    LANGUAGES CXX
    DESCRIPTION "MIDI Orchestration System for Raspberry Pi - USB Focus"
)

# ============================================================================
# OPTIONS
# ============================================================================

option(BUILD_TESTS "Build test suite" OFF)
option(BUILD_DOCS "Build documentation" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers (Debug only)" ON)

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

message(STATUS "========================================")
message(STATUS "MidiMind v${PROJECT_VERSION}")
message(STATUS "========================================")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:    17")
message(STATUS "Focus:           USB MIDI (v4.1.0)")
message(STATUS "========================================")

# ============================================================================
# FLAGS
# ============================================================================

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")

if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")
endif()

# ============================================================================
# DEPENDENCIES
# ============================================================================

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

find_library(ALSA_LIBRARY asound)
find_path(ALSA_INCLUDE_DIR alsa/asoundlib.h)
if(NOT ALSA_LIBRARY OR NOT ALSA_INCLUDE_DIR)
    message(FATAL_ERROR "ALSA required. Install: sudo apt install libasound2-dev")
endif()

find_package(nlohmann_json 3.11.0 QUIET)
find_package(Boost QUIET)

message(STATUS "✓ SQLite3: ${SQLite3_VERSION}")
message(STATUS "✓ ALSA: ${ALSA_LIBRARY}")
if(nlohmann_json_FOUND)
    message(STATUS "✓ nlohmann/json: ${nlohmann_json_VERSION}")
endif()
if(Boost_FOUND)
    message(STATUS "✓ Boost: ${Boost_VERSION} (optional)")
endif()

# ============================================================================
# SOURCE FILES - CORE (4 REQUIRED)
# ============================================================================
# NOTE: Logger.h, Config.h, Error.h, ErrorManager.h are HEADERS-ONLY

set(SOURCES_CORE
    src/main.cpp
    src/core/Application.cpp
    src/core/EventBus.cpp
    src/core/JsonValidator.cpp
)

message(STATUS "Core sources (required): 4")

# ============================================================================
# SOURCE FILES - STORAGE (8 REQUIRED)
# ============================================================================

set(SOURCES_STORAGE
    src/storage/Database.cpp
    src/storage/FileManager.cpp
    src/storage/PathManager.cpp
    src/storage/Settings.cpp
    src/storage/InstrumentDatabase.cpp
    src/storage/PresetManager.cpp
    src/storage/SessionManager.cpp
)

message(STATUS "Storage sources (required): 7")
message(STATUS "  Note: InstrumentDatabase added in v4.1.0")

# ============================================================================
# SOURCE FILES - TIMING (4 REQUIRED) - NEW IN v4.1.0
# ============================================================================

set(SOURCES_TIMING
    src/timing/TimestampManager.cpp
    src/timing/LatencyCompensator.cpp
    
)

message(STATUS "Timing sources (required): 3")
message(STATUS "  Note: InstrumentLatencyProfile.h is header-only")

# ============================================================================
# SOURCE FILES - MIDI CORE (5 REQUIRED)
# ============================================================================

set(SOURCES_MIDI_CORE
    src/midi/MidiMessage.cpp
    src/midi/MidiRouter.cpp
    src/midi/MidiValidator.cpp
    src/midi/JsonMidiConverter.cpp
)

message(STATUS "MIDI Core sources (required): 4")

# ============================================================================
# SOURCE FILES - MIDI DEVICES (3 REQUIRED)
# ============================================================================

set(SOURCES_MIDI_DEVICES
    src/midi/devices/MidiDeviceManager.cpp
    src/midi/devices/UsbMidiDevice.cpp
    src/midi/devices/VirtualMidiDevice.cpp
)

message(STATUS "MIDI Devices sources (required): 3")
message(STATUS "  Note: USB focus only (WiFi/BLE in v4.2.0)")

# ============================================================================
# SOURCE FILES - MIDI FILE (2 REQUIRED)
# ============================================================================

set(SOURCES_MIDI_FILE
    src/midi/file/MidiFileReader.cpp
    src/midi/file/MidiFileWriter.cpp
)

message(STATUS "MIDI File sources (required): 2")

# ============================================================================
# SOURCE FILES - MIDI PLAYER (1 REQUIRED)
# ============================================================================

set(SOURCES_MIDI_PLAYER
    src/midi/player/MidiPlayer.cpp
)

message(STATUS "MIDI Player sources (required): 1")

# ============================================================================
# SOURCE FILES - MIDI PROCESSING (1 REQUIRED)
# ============================================================================

set(SOURCES_MIDI_PROCESSING
    src/midi/processing/ProcessorManager.cpp
)

message(STATUS "MIDI Processing sources (required): 1")

# ============================================================================
# SOURCE FILES - SYSEX (2 REQUIRED)
# ============================================================================

set(SOURCES_SYSEX
    src/midi/sysex/SysExHandler.cpp
    src/midi/sysex/SysExParser.cpp
)

message(STATUS "SysEx sources (required): 2")

# ============================================================================
# SOURCE FILES - API (3 REQUIRED)
# ============================================================================

set(SOURCES_API
    src/api/ApiServer.cpp
    src/api/CommandHandler.cpp
    src/api/MessageEnvelope.cpp
)

message(STATUS "API sources (required): 3")


# ============================================================================
# AGGREGATION
# ============================================================================

set(ALL_SOURCES
    ${SOURCES_CORE}
    ${SOURCES_STORAGE}
    ${SOURCES_TIMING}
    ${SOURCES_MIDI_CORE}
    ${SOURCES_MIDI_DEVICES}
    ${SOURCES_MIDI_FILE}
    ${SOURCES_MIDI_PLAYER}
    ${SOURCES_MIDI_PROCESSING}
    ${SOURCES_SYSEX}
    ${SOURCES_API}
)

list(LENGTH ALL_SOURCES TOTAL_COUNT)

message(STATUS "========================================")
message(STATUS "Total source files: ${TOTAL_COUNT}")
message(STATUS "========================================")

# ============================================================================
# EXECUTABLE
# ============================================================================

add_executable(midimind ${ALL_SOURCES})

target_compile_definitions(midimind PRIVATE
    MIDIMIND_VERSION="${PROJECT_VERSION}"
)

# ============================================================================
# INCLUDE DIRECTORIES
# ============================================================================

target_include_directories(midimind PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ALSA_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

# ============================================================================
# LINK LIBRARIES
# ============================================================================

target_link_libraries(midimind PRIVATE
    Threads::Threads
    ${ALSA_LIBRARY}
    SQLite::SQLite3
)

if(nlohmann_json_FOUND)
    target_link_libraries(midimind PRIVATE nlohmann_json::nlohmann_json)
else()
    message(STATUS "Using embedded nlohmann/json")
endif()

if(Boost_FOUND)
    target_link_libraries(midimind PRIVATE Boost::boost)
endif()

# ============================================================================
# OUTPUT DIRECTORIES
# ============================================================================

set_target_properties(midimind PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ============================================================================
# INSTALLATION
# ============================================================================

install(TARGETS midimind
    RUNTIME DESTINATION bin
)

# Install database migrations
install(DIRECTORY data/migrations
    DESTINATION share/midimind
)

# ============================================================================
# SUMMARY
# ============================================================================

message(STATUS "========================================")
message(STATUS "MidiMind v${PROJECT_VERSION} configured")
message(STATUS "Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "========================================")