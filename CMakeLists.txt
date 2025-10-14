# ============================================================================
# Fichier: /midiMind/CMakeLists.txt (ROOT)
# Version: 3.0.7-ROOT
# Date: 2025-10-14
# Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
# NOUVEAU v3.0.7-ROOT:
#   ✅ CMakeLists.txt à la racine du projet
#   ✅ Sources dans backend/src/
#   ✅ Build dans build/ (à la racine)
#   ✅ Structure moderne et standard
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(MidiMind VERSION 3.0.7 LANGUAGES CXX)

# ============================================================================
# AFFICHAGE CONFIGURATION
# ============================================================================

message(STATUS "========================================")
message(STATUS "MidiMind v${PROJECT_VERSION} Configuration")
message(STATUS "========================================")
message(STATUS "Source dir:      ${CMAKE_SOURCE_DIR}")
message(STATUS "Binary dir:      ${CMAKE_BINARY_DIR}")
message(STATUS "Backend src:     ${CMAKE_SOURCE_DIR}/backend/src")
message(STATUS "========================================")

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
    backend/src/core/Application.cpp
    backend/src/core/Config.cpp
    backend/src/core/Logger.cpp
    backend/src/core/ErrorManager.cpp
)

# ============================================================================
# FICHIERS SOURCE - CORE UTILS
# ============================================================================

set(SOURCES_CORE_UTILS)

# Ajouter seulement si les fichiers existent
if(EXISTS "${CMAKE_SOURCE_DIR}/backend/src/core/utils/JsonValidator.cpp")
    list(APPEND SOURCES_CORE_UTILS backend/src/core/utils/JsonValidator.cpp)
    message(STATUS "✓ JsonValidator.cpp found")
else()
    message(STATUS "⚠ JsonValidator.cpp not found (optional)")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/backend/src/core/utils/ResourceMonitor.cpp")
    list(APPEND SOURCES_CORE_UTILS backend/src/core/utils/ResourceMonitor.cpp)
    message(STATUS "✓ ResourceMonitor.cpp found")
else()
    message(STATUS "⚠ ResourceMonitor.cpp not found (optional)")
endif()

# ============================================================================
# FICHIERS SOURCE - API
# ============================================================================

set(SOURCES_API
    # API Server
    backend/src/api/ApiServer.cpp
    backend/src/api/CommandProcessorV2.cpp
    
    # Handlers de commandes
    backend/src/api/commands/devices.cpp
    backend/src/api/commands/routing.cpp
    backend/src/api/commands/playback.cpp
    backend/src/api/commands/files.cpp
    backend/src/api/commands/system.cpp
    backend/src/api/commands/editor.cpp
    backend/src/api/commands/network.cpp
)

# Ajouter logger.cpp si existe
if(EXISTS "${CMAKE_SOURCE_DIR}/backend/src/api/commands/logger.cpp")
    list(APPEND SOURCES_API backend/src/api/commands/logger.cpp)
    message(STATUS "✓ logger.cpp found")
else()
    message(STATUS "⚠ logger.cpp not found (optional)")
endif()

# ============================================================================
# FICHIERS SOURCE - MIDI CORE
# ============================================================================

set(SOURCES_MIDI_CORE
    backend/src/midi/MidiMessage.cpp
    backend/src/midi/MidiRouter.cpp
    backend/src/midi/MidiDevice.cpp
)

# ============================================================================
# FICHIERS SOURCE - MIDI DEVICES
# ============================================================================

set(SOURCES_MIDI_DEVICES
    backend/src/midi/devices/MidiDeviceManager.cpp
    backend/src/midi/devices/AlsaDevice.cpp
    backend/src/midi/devices/WifiDevice.cpp
    backend/src/midi/devices/BluetoothDevice.cpp
    backend/src/midi/devices/VirtualDevice.cpp
)

# ============================================================================
# FICHIERS SOURCE - MIDI PROCESSING
# ============================================================================

set(SOURCES_MIDI_PROCESSING
    backend/src/midi/processing/NoteProcessor.cpp
    backend/src/midi/processing/ControlProcessor.cpp
    backend/src/midi/processing/ChainProcessor.cpp
)

# ============================================================================
# FICHIERS SOURCE - MIDI ROUTING
# ============================================================================

set(SOURCES_MIDI_ROUTING
    backend/src/midi/routing/ChannelMapper.cpp
    backend/src/midi/routing/TransformationEngine.cpp
    backend/src/midi/routing/RoutingTable.cpp
)

# ============================================================================
# FICHIERS SOURCE - MIDI FILE
# ============================================================================

set(SOURCES_MIDI_FILE
    backend/src/midi/MidiFileManager.cpp
    backend/src/midi/file/MidiFileReader.cpp
    backend/src/midi/file/MidiFileWriter.cpp
    backend/src/midi/file/MidiPlayer.cpp
)

# Vérifier MidiFileConverter.cpp (optionnel)
if(EXISTS "${CMAKE_SOURCE_DIR}/backend/src/midi/file/MidiFileConverter.cpp")
    list(APPEND SOURCES_MIDI_FILE backend/src/midi/file/MidiFileConverter.cpp)
    message(STATUS "✓ MidiFileConverter.cpp found")
else()
    message(STATUS "⚠ MidiFileConverter.cpp not found (optional)")
endif()

# ============================================================================
# FICHIERS SOURCE - SYSEX
# ============================================================================

set(SOURCES_SYSEX
    backend/src/midi/sysex/SysExHandler.cpp
    backend/src/midi/sysex/SysExMessage.cpp
    backend/src/midi/sysex/SysExIdentity.cpp
    backend/src/midi/sysex/SysExNotemap.cpp
)

# ============================================================================
# FICHIERS SOURCE - NETWORK
# ============================================================================

set(SOURCES_NETWORK
    backend/src/network/NetworkManager.cpp
    backend/src/network/WifiManager.cpp
    backend/src/network/BluetoothManager.cpp
)

# ============================================================================
# FICHIERS SOURCE - STORAGE
# ============================================================================

set(SOURCES_STORAGE
    backend/src/storage/Database.cpp
    backend/src/storage/FileManager.cpp
    backend/src/storage/PresetManager.cpp
    backend/src/storage/SessionManager.cpp
)

# ============================================================================
# FICHIERS SOURCE - MONITORING
# ============================================================================

set(SOURCES_MONITORING
    backend/src/monitoring/PerformanceMonitor.cpp
    backend/src/monitoring/HealthMonitor.cpp
    backend/src/monitoring/MetricsCollector.cpp
)

# ============================================================================
# FICHIERS SOURCE - TIMING (optionnels)
# ============================================================================

set(SOURCES_TIMING)

if(EXISTS "${CMAKE_SOURCE_DIR}/backend/src/timing/LatencyCompensator.cpp")
    list(APPEND SOURCES_TIMING
        backend/src/timing/LatencyCompensator.cpp
        backend/src/timing/ClockSync.cpp
        backend/src/timing/TimestampManager.cpp
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
    ${SOURCES_MIDI_ROUTING}
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

add_executable(midimind ${ALL_SOURCES})

# ============================================================================
# INCLUDE DIRECTORIES
# ============================================================================

target_include_directories(midimind PRIVATE
    ${CMAKE_SOURCE_DIR}/backend/src
    ${CMAKE_SOURCE_DIR}/backend/include
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
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests")
        add_subdirectory(tests)
        message(STATUS "Tests enabled")
    else()
        message(WARNING "Tests directory not found")
    endif()
endif()

# ============================================================================
# EXAMPLES (optionnel)
# ============================================================================

if(BUILD_EXAMPLES)
    if(EXISTS "${CMAKE_SOURCE_DIR}/examples")
        add_subdirectory(examples)
        message(STATUS "Examples enabled")
    else()
        message(WARNING "Examples directory not found")
    endif()
endif()

# ============================================================================
# DOCUMENTATION (optionnel)
# ============================================================================

if(BUILD_DOCS)
    find_package(Doxygen QUIET)
    
    if(Doxygen_FOUND)
        set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/docs)
        set(DOXYGEN_GENERATE_HTML YES)
        set(DOXYGEN_GENERATE_MAN NO)
        set(DOXYGEN_EXTRACT_ALL YES)
        set(DOXYGEN_EXTRACT_PRIVATE YES)
        set(DOXYGEN_RECURSIVE YES)
        
        doxygen_add_docs(docs
            ${CMAKE_SOURCE_DIR}/backend/src
            ${CMAKE_SOURCE_DIR}/backend/include
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

# Installer config si existe
if(EXISTS "${CMAKE_SOURCE_DIR}/config")
    install(DIRECTORY config/
        DESTINATION /etc/midimind
        OPTIONAL
    )
endif()

# Installer docs
install(FILES 
    ${CMAKE_SOURCE_DIR}/README.md 
    ${CMAKE_SOURCE_DIR}/LICENSE
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
message(STATUS "")
message(STATUS "Next steps:")
message(STATUS "  cd ${CMAKE_BINARY_DIR}")
message(STATUS "  make -j$(nproc)")
message(STATUS "  sudo make install")
message(STATUS "========================================")

# ============================================================================
# FIN DU FICHIER CMakeLists.txt v3.0.7-ROOT
# ============================================================================
