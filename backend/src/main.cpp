// ============================================================================
// File: backend/src/main.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Main entry point for the MidiMind backend application.
//   Handles command-line arguments, daemon mode, and application lifecycle.
//
// Features:
//   - Command-line argument parsing
//   - Daemon mode with PID file
//   - Signal handling (SIGINT, SIGTERM)
//   - Graceful shutdown
//   - Health check
//   - Verbose mode
//
// Exit Codes:
//   0 - SUCCESS
//   1 - INITIALIZATION_FAILED
//   2 - START_FAILED
//   3 - RUNTIME_ERROR
//   4 - INVALID_ARGUMENTS
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Updated for new Application structure
//   - Removed NetworkManager references
//   - Enhanced error handling
//   - Improved logging
//
// ============================================================================

#include "core/Application.h"
#include "core/Logger.h"
#include "core/Error.h"
#include <iostream>
#include <exception>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

using namespace midiMind;

// ============================================================================
// COMMAND LINE ARGUMENTS
// ============================================================================

struct CommandLineArgs {
    std::string configPath;
    std::string pidFile;
    std::string logLevel = "INFO";
    bool daemonMode = false;
    bool verbose = false;
    bool showHelp = false;
    bool showVersion = false;
};

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void printUsage(const char* programName);
void printVersion();
CommandLineArgs parseCommandLine(int argc, char* argv[]);
bool daemonize();
bool writePidFile(const std::string& pidFile);
void removePidFile(const std::string& pidFile);

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    std::string pidFile;
    
    try {
        // ====================================================================
        // 1. PARSE COMMAND LINE ARGUMENTS
        // ====================================================================
        
        CommandLineArgs args = parseCommandLine(argc, argv);
        pidFile = args.pidFile;
        
        // Show help
        if (args.showHelp) {
            printUsage(argv[0]);
            return args.showHelp && argc > 1 ? 
                static_cast<int>(ExitCode::INVALID_ARGUMENTS) : 
                static_cast<int>(ExitCode::SUCCESS);
        }
        
        // Show version
        if (args.showVersion) {
            printVersion();
            return static_cast<int>(ExitCode::SUCCESS);
        }
        
        // ====================================================================
        // 2. SHOW CONFIGURATION (if verbose)
        // ====================================================================
        
        if (args.verbose) {
            std::cout << "╔═════════════════════════════════════╗\n";
            std::cout << "║  Configuration                      ║\n";
            std::cout << "╚═════════════════════════════════════╝\n";
            std::cout << "  Config path: " << (args.configPath.empty() ? 
                "config.json" : args.configPath) << "\n";
            std::cout << "  Log level:   " << args.logLevel << "\n";
            std::cout << "  Daemon mode: " << (args.daemonMode ? "yes" : "no") << "\n";
            if (!args.pidFile.empty()) {
                std::cout << "  PID file:    " << args.pidFile << "\n";
            }
            std::cout << "═════════════════════════════════════\n";
            std::cout << "\n";
        }
        
        // ====================================================================
        // 3. DAEMONIZE (if requested)
        // ====================================================================
        
        if (args.daemonMode) {
            std::cout << "Starting in daemon mode...\n";
            
            if (!daemonize()) {
                std::cerr << "Error: Failed to daemonize\n";
                return static_cast<int>(ExitCode::INITIALIZATION_FAILED);
            }
            
            // After daemonize, stdout/stderr are redirected to /dev/null
            // Logger will write to log files
        }
        
        // ====================================================================
        // 4. WRITE PID FILE
        // ====================================================================
        
        if (!writePidFile(args.pidFile)) {
            return static_cast<int>(ExitCode::INITIALIZATION_FAILED);
        }
        
        // ====================================================================
        // 5. CONFIGURE LOGGER
        // ====================================================================
        
        Logger::setLevel(args.logLevel);
        
        if (args.daemonMode) {
            Logger::setOutputToFile(true);
            Logger::setLogFile("/var/log/midimind/midimind.log");
        }
        
        // ====================================================================
        // 6. GET APPLICATION INSTANCE
        // ====================================================================
        
        Logger::info("Main", "Starting MidiMind v" + Application::getVersion());
        
        Application& app = Application::instance();
        
        // ====================================================================
        // 7. INITIALIZE APPLICATION
        // ====================================================================
        
        if (!app.initialize(args.configPath)) {
            Logger::critical("Main", "Application initialization failed");
            removePidFile(pidFile);
            return static_cast<int>(ExitCode::INITIALIZATION_FAILED);
        }
        
        // ====================================================================
        // 8. START APPLICATION
        // ====================================================================
        
        if (!app.start()) {
            Logger::critical("Main", "Application start failed");
            removePidFile(pidFile);
            return static_cast<int>(ExitCode::START_FAILED);
        }
        
        // ====================================================================
        // 9. RUN (blocking until shutdown)
        // ====================================================================
        
        app.run();
        
        // ====================================================================
        // 10. STOP APPLICATION
        // ====================================================================
        
        app.stop();
        
        // ====================================================================
        // 11. CLEANUP
        // ====================================================================
        
        removePidFile(pidFile);
        
        Logger::info("Main", "MidiMind stopped successfully");
        return static_cast<int>(ExitCode::SUCCESS);
        
    } catch (const MidiMindError& e) {
        std::cerr << "MidiMind Error: " << e.what() << std::endl;
        Logger::critical("Main", "MidiMind error: " + std::string(e.what()));
        removePidFile(pidFile);
        return static_cast<int>(ExitCode::RUNTIME_ERROR);
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        Logger::critical("Main", "Fatal error: " + std::string(e.what()));
        removePidFile(pidFile);
        return static_cast<int>(ExitCode::RUNTIME_ERROR);
        
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        Logger::critical("Main", "Unknown fatal error");
        removePidFile(pidFile);
        return static_cast<int>(ExitCode::RUNTIME_ERROR);
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Print usage information
 */
void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n";
    std::cout << "\n";
    std::cout << "MidiMind v" << Application::getVersion() 
              << " - MIDI Orchestration System for Raspberry Pi\n";
    std::cout << "\n";
    
    std::cout << "Options:\n";
    std::cout << "  -h, --help               Show this help message\n";
    std::cout << "  -V, --version            Show version information\n";
    std::cout << "  -c, --config PATH        Configuration file path (default: config.json)\n";
    std::cout << "  -d, --daemon             Run as daemon in background\n";
    std::cout << "  -p, --pidfile PATH       PID file path (default: /var/run/midimind.pid)\n";
    std::cout << "  -l, --log-level LEVEL    Log level: DEBUG, INFO, WARNING, ERROR (default: INFO)\n";
    std::cout << "  -v, --verbose            Enable verbose output (sets log-level to DEBUG)\n";
    std::cout << "\n";
    
    std::cout << "Examples:\n";
    std::cout << "  " << programName << "\n";
    std::cout << "    Start with default configuration\n\n";
    
    std::cout << "  " << programName << " --config /path/to/config.json\n";
    std::cout << "    Start with custom configuration\n\n";
    
    std::cout << "  " << programName << " --daemon --pidfile /var/run/midimind.pid\n";
    std::cout << "    Start as daemon with PID file\n\n";
    
    std::cout << "  " << programName << " --log-level DEBUG --verbose\n";
    std::cout << "    Start with debug logging\n";
    std::cout << "\n";
    
    std::cout << "Signals:\n";
    std::cout << "  SIGINT (Ctrl+C)          Graceful shutdown\n";
    std::cout << "  SIGTERM                  Graceful shutdown\n";
    std::cout << "  3x SIGINT                Force shutdown\n";
    std::cout << "\n";
    
    std::cout << "Exit Codes:\n";
    std::cout << "  0                        Success\n";
    std::cout << "  1                        Initialization failed\n";
    std::cout << "  2                        Start failed\n";
    std::cout << "  3                        Runtime error\n";
    std::cout << "  4                        Invalid arguments\n";
    std::cout << "\n";
}

/**
 * @brief Print version information
 */
void printVersion() {
    std::cout << "MidiMind v" << Application::getVersion() << "\n";
    std::cout << "Protocol: " << Application::getProtocolVersion() << "\n";
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << "\n";
    std::cout << "\n";
    std::cout << "Copyright (c) 2025 MidiMind Team\n";
    std::cout << "MIDI Orchestration System for Raspberry Pi\n";
}

/**
 * @brief Parse command line arguments
 */
CommandLineArgs parseCommandLine(int argc, char* argv[]) {
    CommandLineArgs args;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        // Help
        if (arg == "-h" || arg == "--help") {
            args.showHelp = true;
        }
        // Version
        else if (arg == "-V" || arg == "--version") {
            args.showVersion = true;
        }
        // Config
        else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                args.configPath = argv[++i];
            } else {
                std::cerr << "Error: --config requires a path\n";
                args.showHelp = true;
            }
        }
        // Daemon
        else if (arg == "-d" || arg == "--daemon") {
            args.daemonMode = true;
        }
        // PID file
        else if (arg == "-p" || arg == "--pidfile") {
            if (i + 1 < argc) {
                args.pidFile = argv[++i];
            } else {
                std::cerr << "Error: --pidfile requires a path\n";
                args.showHelp = true;
            }
        }
        // Log level
        else if (arg == "-l" || arg == "--log-level") {
            if (i + 1 < argc) {
                args.logLevel = argv[++i];
                // Validate log level
                if (args.logLevel != "DEBUG" && args.logLevel != "INFO" &&
                    args.logLevel != "WARNING" && args.logLevel != "ERROR") {
                    std::cerr << "Error: Invalid log level '" << args.logLevel << "'\n";
                    std::cerr << "Valid levels: DEBUG, INFO, WARNING, ERROR\n";
                    args.showHelp = true;
                }
            } else {
                std::cerr << "Error: --log-level requires a level\n";
                args.showHelp = true;
            }
        }
        // Verbose
        else if (arg == "-v" || arg == "--verbose") {
            args.verbose = true;
            args.logLevel = "DEBUG";
        }
        // Unknown
        else {
            std::cerr << "Error: Unknown option '" << arg << "'\n\n";
            args.showHelp = true;
        }
    }
    
    // Default PID file if daemon mode
    if (args.daemonMode && args.pidFile.empty()) {
        args.pidFile = "/var/run/midimind.pid";
    }
    
    return args;
}

/**
 * @brief Daemonize the process
 */
bool daemonize() {
    // Fork the parent process
    pid_t pid = fork();
    
    if (pid < 0) {
        std::cerr << "Error: Fork failed\n";
        return false;
    }
    
    // Exit parent process
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    // Create new session
    if (setsid() < 0) {
        std::cerr << "Error: setsid failed\n";
        return false;
    }
    
    // Fork again to prevent acquiring controlling terminal
    pid = fork();
    
    if (pid < 0) {
        std::cerr << "Error: Second fork failed\n";
        return false;
    }
    
    // Exit first child
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    // Set file permissions
    umask(0);
    
    // Change working directory
    if (chdir("/") < 0) {
        std::cerr << "Error: chdir failed\n";
        return false;
    }
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Redirect to /dev/null
    int fd = open("/dev/null", O_RDWR);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2) {
            close(fd);
        }
    }
    
    return true;
}

/**
 * @brief Write PID to file
 */
bool writePidFile(const std::string& pidFile) {
    if (pidFile.empty()) {
        return true; // No PID file requested
    }
    
    std::ofstream file(pidFile);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write PID file: " << pidFile << std::endl;
        return false;
    }
    
    file << getpid() << std::endl;
    file.close();
    
    return true;
}

/**
 * @brief Remove PID file
 */
void removePidFile(const std::string& pidFile) {
    if (!pidFile.empty()) {
        std::remove(pidFile.c_str());
    }
}

// ============================================================================
// END OF FILE main.cpp
// ============================================================================