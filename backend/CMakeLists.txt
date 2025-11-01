# ============================================================================
# CMakeLists.txt - MidiMind v4.2.4
# ============================================================================

cmake_minimum_required(VERSION 3.10)

project(MidiMind VERSION 4.2.4 LANGUAGES CXX)

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

pkg_check_modules(GIO REQUIRED gio-2.0)

# ============================================================================
# INCLUDE DIRECTORIES
# ============================================================================

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ALSA_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
)

# ============================================================================
# SOURCE FILES
# ============================================================================

set(ALL_SOURCES
    main.cpp
    Application.cpp
    EventBus.cpp
    JsonValidator.cpp
    Database.cpp
    FileManager.cpp
    PathManager.cpp
    Settings.cpp
    InstrumentDatabase.cpp
    PresetManager.cpp
    SessionManager.cpp
    MidiDatabase.cpp
    PlaylistManager.cpp
    TimestampManager.cpp
    LatencyCompensator.cpp
    MidiMessage.cpp
    MidiProcessor.h
    ProcessorManager.cpp
    ProcessorChain.h
    MidiDeviceManager.cpp
    UsbMidiDevice.cpp
    BleMidiDevice.cpp
    VirtualMidiDevice.cpp
    MidiRouter.cpp
    MidiFileReader.cpp
    MidiFileWriter.cpp
    JsonMidiConverter.cpp
    MidiPlayer.cpp
    SysExParser.cpp
    SysExHandler.cpp
    ApiServer.cpp
    CommandHandler.cpp
    MessageEnvelope.cpp
)

# ============================================================================
# EXECUTABLE
# ============================================================================

add_executable(midimind ${ALL_SOURCES})

target_link_libraries(midimind
    ${CMAKE_THREAD_LIBS_INIT}
    ${SQLite3_LIBRARIES}
    ${ALSA_LIBRARY}
    ${GIO_LIBRARIES}
    stdc++fs
)

# ============================================================================
# OUTPUT
# ============================================================================

set_target_properties(midimind PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ============================================================================
# INSTALLATION
# ============================================================================

install(TARGETS midimind RUNTIME DESTINATION bin)
install(DIRECTORY data/migrations DESTINATION share/midimind)

# ============================================================================
# SUMMARY
# ============================================================================

message(STATUS "========================================")
message(STATUS "MidiMind v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "========================================")