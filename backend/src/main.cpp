// ============================================================================
// File: backend/src/main.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.1.0:
//   - Fixed Logger::setLevel (uses enum, not string)
//   - Removed non-existent Logger methods (setOutputToFile, setLogFile)
//   - Fixed MidiMindError → MidiMindException
//   - Improved error handling in parseCommandLine
//   - Added fd verification in daemonize
//   - Reduced polling interval in main loop
//
// ============================================================================
#include <thread>
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
            return EXIT_SUCCESS;
        }
        
        // Show version
        if (args.showVersion) {
            printVersion();
            return EXIT_SUCCESS;
        }
        
        // ====================================================================
        // 2. SETUP LOGGING
        // ====================================================================
        
        if (args.logLevel == "DEBUG") {
            Logger::setLevel(Logger::Level::DEBUG);
        } else if (args.logLevel == "INFO") {
            Logger::setLevel(Logger::Level::INFO);
        } else if (args.logLevel == "WARNING") {
            Logger::setLevel(Logger::Level::WARNING);
        } else if (args.logLevel == "ERROR") {
            Logger::setLevel(Logger::Level::ERROR);
        } else {
            Logger::setLevel(Logger::Level::INFO);
        }
        
        if (args.verbose) {
            Logger::setLevel(Logger::Level::DEBUG);
        }
        
        Logger::info("main", "MidiMind Backend v4.1.0 starting...");
        
        // ====================================================================
        // 3. DAEMONIZE (if requested)
        // ====================================================================
        
        if (args.daemonMode) {
            Logger::info("main", "Entering daemon mode...");
            if (!daemonize()) {
                Logger::error("main", "Failed to daemonize");
                return EXIT_FAILURE;
            }
        }
        
        // ====================================================================
        // 4. WRITE PID FILE
        // ====================================================================
        
        if (!pidFile.empty()) {
            if (!writePidFile(pidFile)) {
                Logger::error("main", "Failed to write PID file: " + pidFile);
                return EXIT_FAILURE;
            }
        }
        
        // ====================================================================
        // 5. INITIALIZE APPLICATION
        // ====================================================================
        
        Logger::info("main", "Initializing application...");
        
        Application& app = Application::instance();
        
        if (!app.initialize(args.configPath)) {
            Logger::error("main", "Application initialization failed");
            return EXIT_FAILURE;
        }
        
        Logger::info("main", "Application initialized successfully");
        
        // ====================================================================
        // 6. START APPLICATION
        // ====================================================================
        
        Logger::info("main", "Starting application...");
        
        if (!app.start()) {
            Logger::error("main", "Application start failed");
            return EXIT_FAILURE;
        }
        
        Logger::info("main", "Application started successfully");
        Logger::info("main", "MidiMind Backend is now running");
        
        // ====================================================================
        // 7. MAIN LOOP (wait for shutdown signal)
        // ====================================================================
        
        // Note: Using polling with reduced interval. For production, consider
        // implementing signal handlers (SIGTERM, SIGINT) with condition_variable
        // to avoid active polling and enable immediate shutdown response.
        while (app.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // ====================================================================
        // 8. SHUTDOWN
        // ====================================================================
        
        Logger::info("main", "Shutting down...");
        app.shutdown();
        Logger::info("main", "Shutdown complete");
        
        // ====================================================================
        // 9. CLEANUP
        // ====================================================================
        
        if (!pidFile.empty()) {
            removePidFile(pidFile);
        }
        
        return EXIT_SUCCESS;
        
    } catch (const MidiMindException& e) {
        std::cerr << "MidiMind Exception: " << e.what() << std::endl;
        std::cerr << "Error Code: " << e.getCodeString() << std::endl;
        Logger::error("main", "Fatal exception: " + std::string(e.what()));
        
        if (!pidFile.empty()) {
            removePidFile(pidFile);
        }
        
        return EXIT_FAILURE;
        
    } catch (const std::exception& e) {
        std::cerr << "Standard Exception: " << e.what() << std::endl;
        Logger::error("main", "Fatal exception: " + std::string(e.what()));
        
        if (!pidFile.empty()) {
            removePidFile(pidFile);
        }
        
        return EXIT_FAILURE;
        
    } catch (...) {
        std::cerr << "Unknown Exception" << std::endl;
        Logger::error("main", "Fatal unknown exception");
        
        if (!pidFile.empty()) {
            removePidFile(pidFile);
        }
        
        return EXIT_FAILURE;
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n"
              << "\n"
              << "Options:\n"
              << "  -c, --config PATH    Path to config file\n"
              << "  -d, --daemon         Run as daemon\n"
              << "  -p, --pid FILE       Write PID to file\n"
              << "  -l, --log-level LVL  Log level (DEBUG|INFO|WARNING|ERROR)\n"
              << "  -v, --verbose        Verbose output (DEBUG level)\n"
              << "  -h, --help           Show this help\n"
              << "  -V, --version        Show version\n"
              << "\n"
              << "Example:\n"
              << "  " << programName << " --config /etc/midimind/config.json\n"
              << "  " << programName << " --daemon --pid /var/run/midimind.pid\n"
              << std::endl;
}

void printVersion() {
    std::cout << "MidiMind Backend v4.1.0\n"
              << "MIDI Orchestration System for Raspberry Pi\n"
              << "Copyright (c) 2025 MidiMind Team\n"
              << std::endl;
}

CommandLineArgs parseCommandLine(int argc, char* argv[]) {
    CommandLineArgs args;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            args.showHelp = true;
        }
        else if (arg == "-V" || arg == "--version") {
            args.showVersion = true;
        }
        else if (arg == "-v" || arg == "--verbose") {
            args.verbose = true;
        }
        else if (arg == "-d" || arg == "--daemon") {
            args.daemonMode = true;
        }
        else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                args.configPath = argv[++i];
            } else {
                throw std::runtime_error("--config requires an argument");
            }
        }
        else if (arg == "-p" || arg == "--pid") {
            if (i + 1 < argc) {
                args.pidFile = argv[++i];
            } else {
                throw std::runtime_error("--pid requires an argument");
            }
        }
        else if (arg == "-l" || arg == "--log-level") {
            if (i + 1 < argc) {
                args.logLevel = argv[++i];
            } else {
                throw std::runtime_error("--log-level requires an argument");
            }
        }
        else {
            throw std::runtime_error("Unknown option: " + arg + ". Use --help for usage information");
        }
    }
    
    return args;
}

bool daemonize() {
    // Fork process
    pid_t pid = fork();
    
    if (pid < 0) {
        return false;
    }
    
    // Parent exits
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    // Child continues
    
    // Create new session
    if (setsid() < 0) {
        return false;
    }
    
    // Fork again to prevent acquiring terminal
    pid = fork();
    
    if (pid < 0) {
        return false;
    }
    
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    // Set working directory
    if (chdir("/") < 0) {
        return false;
    }
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Redirect to /dev/null and verify file descriptors
    int fd0 = open("/dev/null", O_RDONLY);  // stdin
    int fd1 = open("/dev/null", O_WRONLY);  // stdout
    int fd2 = open("/dev/null", O_WRONLY);  // stderr
    
    // Verify that we got the expected file descriptors
    if (fd0 != STDIN_FILENO || fd1 != STDOUT_FILENO || fd2 != STDERR_FILENO) {
        // If not, close them and return failure
        if (fd0 >= 0) close(fd0);
        if (fd1 >= 0) close(fd1);
        if (fd2 >= 0) close(fd2);
        return false;
    }
    
    return true;
}

bool writePidFile(const std::string& pidFile) {
    std::ofstream ofs(pidFile);
    if (!ofs) {
        return false;
    }
    
    ofs << getpid() << std::endl;
    ofs.close();
    
    return true;
}

void removePidFile(const std::string& pidFile) {
    unlink(pidFile.c_str());
}

// ============================================================================
// END OF FILE main.cpp
// ============================================================================