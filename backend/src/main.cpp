// ============================================================================
// File: backend/src/main.cpp
// Version: 4.1.0 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.1.0:
//   - Fixed Logger::setLevel (uses enum, not string)
//   - Removed non-existent Logger methods (setOutputToFile, setLogFile)
//   - Fixed MidiMindError → MidiMindException
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
    std::string logLevel = "INFO";  // String, sera converti en enum
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
        
        // ✅ CORRIGÉ: Utiliser l'enum Logger::Level au lieu de string
        if (args.logLevel == "DEBUG") {
            Logger::setLevel(Logger::Level::DEBUG);
        } else if (args.logLevel == "INFO") {
            Logger::setLevel(Logger::Level::INFO);
        } else if (args.logLevel == "WARNING") {
            Logger::setLevel(Logger::Level::WARNING);
        } else if (args.logLevel == "ERROR") {
            Logger::setLevel(Logger::Level::ERROR);
        } else {
            Logger::setLevel(Logger::Level::INFO);  // Default
        }
        
        if (args.verbose) {
            Logger::setLevel(Logger::Level::DEBUG);
        }
        
        // ✅ SUPPRIMÉ: Ces méthodes n'existent pas dans Logger
        // Logger::setOutputToFile(true);
        // Logger::setLogFile("/var/log/midimind/midimind.log");
        
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
        
        while (app.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
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
        
    } catch (const MidiMindException& e) {  // ✅ CORRIGÉ: MidiMindException au lieu de MidiMindError
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
              << "  -V, --version        Show version\