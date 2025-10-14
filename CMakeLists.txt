# ============================================================================
# Fichier: /midiMind/CMakeLists.txt (ROOT - AUTO-DETECT)
# Version: 3.0.8-AUTO
# Date: 2025-10-14
# Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
# NOUVEAU v3.0.8-AUTO:
#   ✅ Détection automatique de TOUS les fichiers .cpp
#   ✅ Pas d'erreur si fichier manquant
#   ✅ Liste uniquement les fichiers existants
#   ✅ Headers-only supportés (Config.h, etc.)
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(MidiMind VERSION 3.0.8 LANGUAGES CXX)

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
# DÉTECTION AUTOMATIQUE DES FICHIERS SOURCES
# ============================================================================

message(STATUS "========================================")
message(STATUS "Auto-detecting source files...")
message(STATUS "========================================")

# Fonction helper pour collecter les .cpp
function(collect_sources output_var directory)
    file(GLOB_RECURSE sources 
        "${CMAKE_SOURCE_DIR}/${directory}/*.cpp"
    )
    set(${output_var} ${sources} PARENT_SCOPE)
endfunction()

# Collecter tous les .cpp dans backend/src
file(GLOB_RECURSE ALL_SOURCES 
    "${CMAKE_SOURCE_DIR}/backend/src/*.cpp"
)

# Filtrer les fichiers à exclure (si nécessaire)
set(EXCLUDED_FILES
    # Ajouter ici les fichiers à exclure
)

foreach(exclude ${EXCLUDED_FILES})
    list(FILTER ALL_SOURCES EXCLUDE REGEX ${exclude})
endforeach()

# Afficher les fichiers trouvés par module
message(STATUS "")
message(STATUS "Source files by module:")
message(STATUS "")

# Core
file(GLOB CORE_SOURCES "${CMAKE_SOURCE_DIR}/backend/src/core/*.cpp")
list(LENGTH CORE_SOURCES CORE_COUNT)
message(STATUS "  Core:         ${CORE_COUNT} files")

# Core Utils
file(GLOB CORE_UTILS_SOURCES "${CMAKE_SOURCE_DIR}/backend/src/core/utils/*.cpp")
list(LENGTH CORE_UTILS_SOURCES CORE_UTILS_COUNT)
message(STATUS "  Core Utils:   ${CORE_UTILS_COUNT} files")

# API
file(GLOB_RECURSE API_SOURCES "${CMAKE_SOURCE_DIR}/backend/src/api/*.cpp")
list(LENGTH API_SOURCES API_COUNT)
message(STATUS "  API:          ${API_COUNT} files")

# MIDI
file(GLOB_RECURSE MIDI_SOURCES "${CMAKE_SOURCE_DIR}/backend/src/midi/*.cpp")
list(LENGTH MIDI_SOURCES MIDI_COUNT)
message(STATUS "  MIDI:         ${MIDI_COUNT} files")

# Network
file(GLOB_RECURSE NETWORK_SOURCES "${CMAKE_SOURCE_DIR}/backend/src/network/*.cpp")
list(LENGTH NETWORK_SOURCES NETWORK_COUNT)
message(STATUS "  Network:      ${NETWORK_COUNT} files")

# Storage
file(GLOB_RECURSE STORAGE_SOURCES "${CMAKE_SOURCE_DIR}/backend/src/storage/*.cpp")
list(LENGTH STORAGE_SOURCES STORAGE_COUNT)
message(STATUS "  Storage:      ${STORAGE_COUNT} files")

# Monitoring
file(GLOB_RECURSE MONITORING_SOURCES "${CMAKE_SOURCE_DIR}/backend/src/monitoring/*.cpp")
list(LENGTH MONITORING_SOURCES MONITORING_COUNT)
message(STATUS "  Monitoring:   ${MONITORING_COUNT} files")

# Timing
file(GLOB_RECURSE TIMING_SOURCES "${CMAKE_SOURCE_DIR}/backend/src/timing/*.cpp")
list(LENGTH TIMING_SOURCES TIMING_COUNT)
message(STATUS "  Timing:       ${TIMING_COUNT} files")

# Total
list(LENGTH ALL_SOURCES TOTAL_COUNT)
message(STATUS "")
message(STATUS "  TOTAL:        ${TOTAL_COUNT} files")
message(STATUS "")

if(TOTAL_COUNT EQUAL 0)
    message(FATAL_ERROR "No source files found in backend/src/")
endif()

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
message(STATUS "Source files: ${TOTAL_COUNT}")
message(STATUS "")
message(STATUS "Next steps:")
message(STATUS "  cd ${CMAKE_BINARY_DIR}")
message(STATUS "  make -j$(nproc)")
message(STATUS "  sudo make install")
message(STATUS "========================================")

# ============================================================================
# FIN DU FICHIER CMakeLists.txt v3.0.8-AUTO
# ============================================================================
