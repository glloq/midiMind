# ============================================================================
# Fichier: backend/CMakeLists.txt
# Version: 3.0.7 - CORRIGÉ
# Date: 2025-10-14
# ============================================================================
# CORRECTIONS v3.0.7:
# ✅ Ajout src/api/commands/processing.cpp (CRITIQUE)
# ✅ Ajout gestion conditionnelle instruments.cpp et loops.cpp
# ✅ Messages de statut améliorés
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

message(STATUS "")
message(STATUS "Checking dependencies...")

# Threads (requis)
find_package(Threads REQUIRED)
message(STATUS "✓ Threads found")

# SQLite3 (requis)
find_package(SQLite3 REQUIRED)
if(NOT SQLite3_FOUND)
    message(FATAL_ERROR "SQLite3 not found. Install: sudo apt install libsqlite3-dev")
endif()
message(STATUS "✓ SQLite3 found")

# ALSA (requis pour MIDI sur Linux)
find_library(ALSA_LIBRARY asound)
if(NOT ALSA_LIBRARY)
    message(FATAL_ERROR "ALSA not found. Install: sudo apt install libasound2-dev")
endif()
message(STATUS "✓ ALSA found")

# nlohmann/json (requis)
find_package(nlohmann_json 3.11.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(WARNING "nlohmann/json not found via CMake, assuming header-only in include/")
else()
    message(STATUS "✓ nlohmann/json found")
endif()

# Boost (optionnel)
find_package(Boost QUIET)
if(Boost_FOUND)
    message(STATUS "✓ Boost found: ${Boost_VERSION}")
else()
    message(STATUS "⚠ Boost not found (optional)")
endif()

message(STATUS "")

# ============================================================================
# FICHIERS SOURCE - CORE
# ============================================================================

message(STATUS "Configuring CORE modules...")

set(SOURCES_CORE
    src/core/Application.cpp
    src/core/Config.cpp
    src/core/Logger.cpp
    src/core/ErrorManager.cpp
)

message(STATUS "  CORE: ${SOURCES_CORE}")

# ============================================================================
# FICHIERS SOURCE - CORE UTILS
# ============================================================================

set(SOURCES_CORE_UTILS)

# Ajouter seulement si les fichiers existent
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils/JsonValidator.cpp")
    list(APPEND SOURCES_CORE_UTILS src/core/utils/JsonValidator.cpp)
    message(STATUS "  ✓ JsonValidator.cpp found")
else()
    message(STATUS "  ⚠ JsonValidator.cpp not found (optional)")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils/ResourceMonitor.cpp")
    list(APPEND SOURCES_CORE_UTILS src/core/utils/ResourceMonitor.cpp)
    message(STATUS "  ✓ ResourceMonitor.cpp found")
else()
    message(STATUS "  ⚠ ResourceMonitor.cpp not found (optional)")
endif()

# ============================================================================
# FICHIERS SOURCE - API
# Version: 3.0.7 - CORRIGÉE
# ============================================================================

message(STATUS "")
message(STATUS "Configuring API modules...")

set(SOURCES_API
    # API Server
    src/api/ApiServer.cpp
    src/api/CommandProcessorV2.cpp
    
    # ✅ CORRECTION v3.0.7: TOUS les handlers de commandes
    src/api/commands/devices.cpp          # ✅ devices.*
    src/api/commands/routing.cpp          # ✅ routing.*
    src/api/commands/playback.cpp         # ✅ playback.*
    src/api/commands/files.cpp            # ✅ files.*
    src/api/commands/system.cpp           # ✅ system.*
    src/api/commands/editor.cpp           # ✅ editor.*
    src/api/commands/processing.cpp       # ✅ processing.* (AJOUTÉ v3.0.7)
    src/api/commands/network.cpp          # ✅ network.*
    src/api/commands/logger.cpp           # ✅ logger.*
)

message(STATUS "  Core API handlers: 9 files")

# Handlers optionnels
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/commands/instruments.cpp")
    list(APPEND SOURCES_API src/api/commands/instruments.cpp)
    message(STATUS "  ✓ instruments.cpp found (optional)")
else()
    message(STATUS "  ⚠ instruments.cpp not found (optional)")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/commands/loops.cpp")
    list(APPEND SOURCES_API src/api/commands/loops.cpp)
    message(STATUS "  ✓ loops.cpp found (optional)")
else()
    message(STATUS "  ⚠ loops.cpp not found (optional)")
endif()

list(LENGTH SOURCES_API API_COUNT)
message(STATUS "  Total API files: ${API_COUNT}")

# ============================================================================
# FICHIERS SOURCE - MIDI CORE
# ============================================================================

message(STATUS "")
message(STATUS "Configuring MIDI modules...")

set(SOURCES_MIDI_CORE
    src/midi/MidiMessage.cpp
    src/midi/MidiRouter.cpp
    src/midi/MidiDevice.cpp
)

message(STATUS "  MIDI Core: 3 files")

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

message(STATUS "  MIDI Devices: 5 files")

# ============================================================================
# FICHIERS SOURCE - MIDI PROCESSING
# ============================================================================

set(SOURCES_MIDI_PROCESSING
    src/midi/processing/NoteProcessor.cpp
    src/midi/processing/ControlProcessor.cpp
    src/midi/processing/ChainProcessor.cpp
)

message(STATUS "  MIDI Processing: 3 files")

# ============================================================================
# FICHIERS SOURCE - MIDI FILE
# ============================================================================

set(SOURCES_MIDI_FILE
    src/midi/file/MidiFileReader.cpp
    src/midi/file/MidiFileWriter.cpp
    src/midi/file/MidiPlayer.cpp
)

# Vérifier MidiFileConverter.cpp (optionnel)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/midi/file/MidiFileConverter.cpp")
    list(APPEND SOURCES_MIDI_FILE src/midi/file/MidiFileConverter.cpp)
    message(STATUS "  ✓ MidiFileConverter.cpp found")
else()
    message(STATUS "  ⚠ MidiFileConverter.cpp not found (optional)")
endif()

list(LENGTH SOURCES_MIDI_FILE MIDI_FILE_COUNT)
message(STATUS "  MIDI File: ${MIDI_FILE_COUNT} files")

# ============================================================================
# FICHIERS SOURCE - SYSEX
# ============================================================================

set(SOURCES_SYSEX
    src/midi/sysex/SysExHandler.cpp
    src/midi/sysex/SysExMessage.cpp
    src/midi/sysex/SysExIdentity.cpp
    src/midi/sysex/SysExNotemap.cpp
)

message(STATUS "  SysEx: 4 files")

# ============================================================================
# FICHIERS SOURCE - NETWORK
# ============================================================================

message(STATUS "")
message(STATUS "Configuring NETWORK modules...")

set(SOURCES_NETWORK
    src/network/NetworkManager.cpp
    src/network/WifiManager.cpp
    src/network/BluetoothManager.cpp
)

message(STATUS "  Network: 3 files")

# ============================================================================
# FICHIERS SOURCE - STORAGE
# ============================================================================

message(STATUS "")
message(STATUS "Configuring STORAGE modules...")

set(SOURCES_STORAGE
    src/storage/Database.cpp
    src/storage/FileManager.cpp
    src/storage/PresetManager.cpp
    src/storage/SessionManager.cpp
)

# Vérifier fichiers optionnels
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/storage/PathManager.cpp")
    list(APPEND SOURCES_STORAGE src/storage/PathManager.cpp)
    message(STATUS "  ✓ PathManager.cpp found")
else()
    message(STATUS "  ⚠ PathManager.cpp not found (may be header-only)")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/storage/Settings.cpp")
    list(APPEND SOURCES_STORAGE src/storage/Settings.cpp)
    message(STATUS "  ✓ Settings.cpp found")
else()
    message(STATUS "  ⚠ Settings.cpp not found (may be header-only)")
endif()

list(LENGTH SOURCES_STORAGE STORAGE_COUNT)
message(STATUS "  Total Storage: ${STORAGE_COUNT} files")

# ============================================================================
# FICHIERS SOURCE - MONITORING
# ============================================================================

message(STATUS "")
message(STATUS "Configuring MONITORING modules...")

set(SOURCES_MONITORING
    src/monitoring/PerformanceMonitor.cpp
    src/monitoring/HealthMonitor.cpp
    src/monitoring/MetricsCollector.cpp
)

message(STATUS "  Monitoring: 3 files")

# ============================================================================
# FICHIERS SOURCE - TIMING (optionnels)
# ============================================================================

message(STATUS "")
message(STATUS "Configuring TIMING modules...")

set(SOURCES_TIMING)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/timing/LatencyCompensator.cpp")
    list(APPEND SOURCES_TIMING
        src/timing/LatencyCompensator.cpp
        src/timing/TimestampManager.cpp
    )
    message(STATUS "  ✓ Timing modules found")
else()
    message(STATUS "  ⚠ Timing modules not found (optional)")
endif()

# ============================================================================
# AGRÉGATION DE TOUS LES SOURCES
# ============================================================================

message(STATUS "")
message(STATUS "Aggregating all source files...")

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

# Afficher nombre de fichiers sources
list(LENGTH ALL_SOURCES SOURCE_COUNT)
message(STATUS "========================================")
message(STATUS "Total source files: ${SOURCE_COUNT}")
message(STATUS "========================================")

# ============================================================================
# EXÉCUTABLE PRINCIPAL
# ============================================================================

message(STATUS "")
message(STATUS "Creating executable target...")

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
    message(STATUS "")
    message(STATUS "Enabling tests...")
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "✓ Tests enabled")
endif()

# ============================================================================
# EXAMPLES (optionnel)
# ============================================================================

if(BUILD_EXAMPLES)
    message(STATUS "")
    message(STATUS "Building examples...")
    add_subdirectory(examples)
    message(STATUS "✓ Examples enabled")
endif()

# ============================================================================
# DOCUMENTATION (optionnel)
# ============================================================================

if(BUILD_DOCS)
    message(STATUS "")
    message(STATUS "Checking Doxygen...")
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
        
        message(STATUS "✓ Documentation target 'docs' created")
    else()
        message(WARNING "⚠ Doxygen not found. Documentation disabled.")
    endif()
endif()

# ============================================================================
# RÉSUMÉ FINAL
# ============================================================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Configuration Summary")
message(STATUS "========================================")
message(STATUS "Project:         MidiMind v${PROJECT_VERSION}")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "Source files:    ${SOURCE_COUNT}")
message(STATUS "Binary output:   ${CMAKE_BINARY_DIR}/bin/midimind")
message(STATUS "========================================")
message(STATUS "Configuration complete!")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "Next steps:")
message(STATUS "  make -j$(nproc)     # Compile")
message(STATUS "  make install        # Install (if configured)")
message(STATUS "  ./bin/midimind      # Run")
message(STATUS "")

# ============================================================================
# FIN DU FICHIER CMakeLists.txt v3.0.7 - CORRIGÉ
# ============================================================================
