# ============================================================================
# CMakeLists.txt - MidiMind v4.1.0
# ============================================================================

cmake_minimum_required(VERSION 3.10)

project(MidiMind VERSION 4.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")

# ============================================================================
# DEPENDENCIES
# ============================================================================

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)

find_library(ALSA_LIBRARY asound)
find_path(ALSA_INCLUDE_DIR alsa/asoundlib.h)
if(NOT ALSA_LIBRARY OR NOT ALSA_INCLUDE_DIR)
    message(FATAL_ERROR "ALSA required: sudo apt install libasound2-dev")
endif()

# GLib/GIO for Bluetooth D-Bus support
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)

# ============================================================================
# SOURCE FILES
# ============================================================================

set(ALL_SOURCES
    src/main.cpp
    src/core/Application.cpp
    src/core/EventBus.cpp
    src/core/JsonValidator.cpp
    src/storage/Database.cpp
    src/storage/FileManager.cpp
    src/storage/PathManager.cpp
    src/storage/Settings.cpp
    src/storage/InstrumentDatabase.cpp
    src/storage/PresetManager.cpp
    src/storage/SessionManager.cpp
    src/timing/TimestampManager.cpp
    src/timing/LatencyCompensator.cpp
    src/midi/MidiMessage.cpp
    src/midi/MidiRouter.cpp
    src/midi/JsonMidiConverter.cpp
    src/midi/devices/MidiDeviceManager.cpp
    src/midi/devices/UsbMidiDevice.cpp
    src/midi/devices/VirtualMidiDevice.cpp
    src/midi/devices/BleMidiDevice.cpp
    src/midi/file/MidiFileReader.cpp
    src/midi/file/MidiFileWriter.cpp
    src/midi/player/MidiPlayer.cpp
    src/midi/processing/ProcessorManager.cpp
    src/midi/sysex/SysExHandler.cpp
    src/midi/sysex/SysExParser.cpp
    src/api/ApiServer.cpp
    src/api/CommandHandler.cpp
    src/api/MessageEnvelope.cpp
)

# ============================================================================
# EXECUTABLE
# ============================================================================

add_executable(midimind ${ALL_SOURCES})

target_compile_definitions(midimind PRIVATE MIDIMIND_VERSION="${PROJECT_VERSION}")

target_include_directories(midimind PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ALSA_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
)

target_link_libraries(midimind PRIVATE
    Threads::Threads
    ${ALSA_LIBRARY}
    SQLite::SQLite3
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
)

set_target_properties(midimind PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ============================================================================
# INSTALLATION
# ============================================================================

install(TARGETS midimind RUNTIME DESTINATION /opt/midimind/bin)

install(FILES 
    data/migrations/001_initial.sql 
    data/migrations/005_instruments_latency.sql 
    DESTINATION /opt/midimind/data/migrations
)

# Installer le service systemd
set(SERVICE_FILE "${CMAKE_SOURCE_DIR}/../scripts/midimind.service")
if(EXISTS ${SERVICE_FILE})
    install(FILES ${SERVICE_FILE} DESTINATION /etc/systemd/system/)
    install(CODE "execute_process(COMMAND systemctl daemon-reload)")
    install(CODE "execute_process(COMMAND systemctl enable midimind.service OUTPUT_QUIET ERROR_QUIET)")
    message(STATUS "Service systemd sera installé: ${SERVICE_FILE}")
endif()