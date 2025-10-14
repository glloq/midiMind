# ============================================================================
# Fichier: CMakeLists.txt (RACINE DU PROJET)
# Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
# Version: 3.0.7
# Date: 2025-10-14
# ============================================================================
# MODIFICATIONS v3.0.7:
# ✅ Vérification robuste des dépendances
# ✅ Messages informatifs clairs
# ✅ Support compilation incrémentale
# ✅ Gestion des fichiers optionnels
# ✅ Configuration optimale pour Raspberry Pi
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(MidiMind VERSION 3.0.7 LANGUAGES CXX)

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

# ============================================================================
# AFFICHAGE CONFIGURATION
# ============================================================================

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
message(STATUS "✓ Threads found")

# SQLite3 (requis)
find_package(SQLite3 REQUIRED)
if(NOT SQLite3_FOUND)
    message(FATAL_ERROR "SQLite3 not found. Install: sudo apt install libsqlite3-dev")
endif()
message(STATUS "✓ SQLite3 found: ${SQLite3_VERSION}")

# ALSA (requis pour MIDI sur Linux)
find_library(ALSA_LIBRARY asound)
if(NOT ALSA_LIBRARY)
    message(FATAL_ERROR "ALSA not found. Install: sudo apt install libasound2-dev")
endif()
message(STATUS "✓ ALSA found: ${ALSA_LIBRARY}")

# nlohmann/json (requis)
find_package(nlohmann_json 3.11.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(WARNING "nlohmann/json not found via CMake, assuming header-only in include/")
else()
    message(STATUS "✓ nlohmann/json found: ${nlohmann_json_VERSION}")
endif()

# Boost (optionnel)
find_package(Boost QUIET)
if(Boost_FOUND)
    message(STATUS "✓ Boost found: ${Boost_VERSION}")
else()
    message(STATUS "⚠ Boost not found (optional)")
endif()

# ============================================================================
# FICHIERS SOURCE - CORE
# ============================================================================

set(SOURCES_CORE
    src/core/Application.cpp
    src/core/Config.cpp
    src/core/Logger.cpp
    src/core/ErrorManager.cpp
)

# ============================================================================
# FICHIERS SOURCE - CORE UTILS
# ============================================================================

set(SOURCES_CORE_UTILS)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils/JsonValidator.cpp")
    list(APPEND SOURCES_CORE_UTILS src/core/utils/JsonValidator.cpp)
    message(STATUS "✓ JsonValidator.cpp found")
else()
    message(STATUS "⚠ JsonValidator.cpp not found (optional)")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils/ResourceMonitor.cpp")
    list(APPEND SOURCES_CORE_UTILS src/core/utils/ResourceMonitor.cpp)
    message(STATUS "✓ ResourceMonitor.cpp found")
else()
    message(STATUS "⚠ ResourceMonitor.cpp not found (optional)")
endif()

# ============================================================================
# FICHIERS SOURCE - API
# ============================================================================

set(SOURCES_API
    src/api/ApiServer.cpp
    src/api/CommandProcessorV2.cpp
    src/api/commands/devices.cpp
    src/api/commands/routing.cpp
    src/api/commands/playback.cpp
    src/api/commands/files.cpp
    src/api/commands/system.cpp
    src/api/commands/editor.cpp
    src/api/commands/network.cpp
    src/api/commands/processing.cpp
)

# Logger commands est optionnel
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/commands/logger.cpp")
    list(APPEND SOURCES_API src/api/commands/logger.cpp)
    message(STATUS "✓ logger.cpp found")
else()
    message(STATUS "⚠ logger.cpp not found (optional)")
endif()

message(STATUS "API commands: ${SOURCES_API}")

# ============================================================================
# FICHIERS SOURCE - MIDI CORE
# ============================================================================

set(SOURCES_MIDI_CORE
    src/midi/MidiMessage.cpp
    src/midi/MidiRouter.cpp
    src/midi/MidiDevice.cpp
)

# ============================================================================
# FICHIERS SOURCE - MIDI DEVICES
# ============================================================================

set(SOURCES_MIDI_DEVICES
    src/midi/devices/MidiDeviceManager.cpp
    src/midi/devices/AlsaDevice.cpp
    src/midi/devices/WifiDevice.cpp
    src/midi/devices/BluetoothDevice.cpp
    src/midi/devices/VirtualDevice.cpp
)

# ============================================================================
# FICHIERS SOURCE - MIDI PROCESSING
# ============================================================================

set(SOURCES_MIDI_PROCESSING
    src/midi/processing/NoteProcessor.cpp
    src/midi/processing/ControlProcessor.cpp
    src/midi/processing/ChainProcessor.cpp
)

# ============================================================================
# FICHIERS SOURCE - MIDI FILE
# ============================================================================

set(SOURCES_MIDI_FILE
    src/midi/file/MidiFileReader.cpp
    src/midi/file/MidiFileWriter.cpp
    src/midi/file/MidiPlayer.cpp
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/file/MidiFileConverter.cpp")
    list(APPEND SOURCES_MIDI_FILE src/midi/file/MidiFileConverter.cpp)
    message(STATUS "✓ MidiFileConverter.cpp found")
else()
    message(STATUS "⚠ MidiFileConverter.cpp not found (optional)")
endif()

# ============================================================================
# FICHIERS SOURCE - SYSEX
# ============================================================================

set(SOURCES_SYSEX
    src/midi/sysex/SysExHandler.cpp
    src/midi/sysex/SysExMessage.cpp
    src/midi/sysex/SysExIdentity.cpp
    src/midi/sysex/SysExNotemap.cpp
)

# ============================================================================
# FICHIERS SOURCE - NETWORK
# ============================================================================

set(SOURCES_NETWORK
    src/network/NetworkManager.cpp
    src/network/WifiManager.cpp
    src/network/BluetoothManager.cpp
)

# ============================================================================
# FICHIERS SOURCE - STORAGE
# ============================================================================

set(SOURCES_STORAGE
    src/storage/Database.cpp
    src/storage/FileManager.cpp
    src/storage/PresetManager.cpp
    src/storage/SessionManager.cpp
)

# ============================================================================
# FICHIERS SOURCE - MONITORING
# ============================================================================

set(SOURCES_MONITORING
    src/monitoring/PerformanceMonitor.cpp
    src/monitoring/HealthMonitor.cpp
    src/monitoring/MetricsCollector.cpp
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
# AGRÉGATION DE TOUS LES SOURCES
# ============================================================================

set(ALL_SOURCES
    ${SOURCES_CORE}
    ${SOURCES_CORE_UTILS}
    ${SOURCES_API}
    ${SOURCES_MIDI_CORE}
    ${SOURCES_MIDI_DEVICES}
    ${SOURCES_MIDI_PROCESSING}
    ${SOURCES_MIDI_FILE}
    ${SOURCES_SYSEX}
    ${SOURCES_NETWORK}
    ${SOURCES_STORAGE}
    ${SOURCES_MONITORING}
    ${SOURCES_TIMING}
)

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
    stdc++fs
)

if(nlohmann_json_FOUND)
    target_link_libraries(midimind PRIVATE nlohmann_json::nlohmann_json)
endif()

if(Boost_FOUND)
    target_link_libraries(midimind PRIVATE ${Boost_LIBRARIES})
endif()

# ============================================================================
# TESTS
# ============================================================================

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "✓ Tests enabled")
endif()

# ============================================================================
# EXAMPLES
# ============================================================================

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
    message(STATUS "✓ Examples enabled")
endif()

# ============================================================================
# DOCUMENTATION
# ============================================================================

if(BUILD_DOCS)
    find_package(Doxygen QUIET)
    if(DOXYGEN_FOUND)
        add_subdirectory(docs)
        message(STATUS "✓ Documentation enabled")
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
message(STATUS "Executable: ${CMAKE_BINARY_DIR}/midimind")
message(STATUS "========================================")

# ============================================================================
# FIN DU FICHIER CMakeLists.txt v3.0.7
# ============================================================================
