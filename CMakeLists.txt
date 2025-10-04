# ============================================================================
# Fichier: CMakeLists.txt
# Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(MidiMind VERSION 3.0.0 LANGUAGES CXX)

# ============================================================================
# OPTIONS
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)

# ============================================================================
# COMPILER FLAGS
# ============================================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3)
    endif()
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    endif()
endif()

# ============================================================================
# DEPENDENCIES
# ============================================================================

# Threads
find_package(Threads REQUIRED)

# SQLite3
find_package(SQLite3 REQUIRED)

# ALSA (pour MIDI sur Linux)
find_library(ALSA_LIBRARY asound)
if(NOT ALSA_LIBRARY)
    message(FATAL_ERROR "ALSA library not found")
endif()

# nlohmann/json (peut être installé via package manager ou inclus)
find_package(nlohmann_json 3.11.0 REQUIRED)

# ============================================================================
# INCLUDE DIRECTORIES
# ============================================================================

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${SQLite3_INCLUDE_DIRS}
)

# ============================================================================
# SUBDIRECTORIES
# ============================================================================

add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================================================
# INSTALLATION
# ============================================================================

install(TARGETS midimind
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION /etc/midimind
)

install(FILES README.md LICENSE
    DESTINATION /usr/share/doc/midimind
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
# FIN DU FICHIER CMakeLists.txt
# ============================================================================