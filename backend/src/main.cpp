// ============================================================================
// Fichier: src/main.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Point d'entrée principal de l'application MidiMind.
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#include "core/Application.h"
#include <iostream>
#include <exception>
#include <cstdlib>

using namespace midiMind;

// ============================================================================
// HELPERS
// ============================================================================

/**
 * @brief Affiche l'usage
 */
void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -c, --config <path>   Path to configuration file\n";
    std::cout << "  -h, --help            Show this help message\n";
    std::cout << "  -v, --version         Show version information\n";
    std::cout << "  -d, --daemon          Run as daemon (background)\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << "\n";
    std::cout << "  " << programName << " --config /path/to/config.json\n";
    std::cout << "  " << programName << " --daemon\n";
    std::cout << "\n";
}

/**
 * @brief Affiche la version
 */
void printVersion() {
    std::cout << "MidiMind v3.0.0\n";
    std::cout << "Build date: " << __DATE__ << " " << __TIME__ << "\n";
    std::cout << "Copyright (c) 2025 MidiMind Team\n";
}

/**
 * @brief Parse les arguments de ligne de commande
 */
struct CommandLineArgs {
    std::string configPath;
    bool showHelp = false;
    bool showVersion = false;
    bool daemonMode = false;
};

CommandLineArgs parseCommandLine(int argc, char* argv[]) {
    CommandLineArgs args;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            args.showHelp = true;
        } else if (arg == "-v" || arg == "--version") {
            args.showVersion = true;
        } else if (arg == "-d" || arg == "--daemon") {
            args.daemonMode = true;
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            args.configPath = argv[++i];
        }
    }
    
    return args;
}

/**
 * @brief Daemonize le processus
 */
void daemonize() {
    pid_t pid = fork();
    
    if (pid < 0) {
        std::cerr << "Failed to fork process\n";
        exit(EXIT_FAILURE);
    }
    
    if (pid > 0) {
        // Parent process
        exit(EXIT_SUCCESS);
    }
    
    // Child process
    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }
    
    // Fork again
    pid = fork();
    
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    // Change working directory
    chdir("/");
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    std::cout << "Running as daemon (PID: " << getpid() << ")\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    try {
        // Parse command line
        auto args = parseCommandLine(argc, argv);
        
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
        
        // Daemonize if requested
        if (args.daemonMode) {
            daemonize();
        }
        
        // Get application instance
        Application& app = Application::instance();
        
        // Initialize
        std::cout << "Initializing MidiMind...\n";
        
        if (!app.initialize(args.configPath)) {
            std::cerr << "ERROR: Failed to initialize application\n";
            return EXIT_FAILURE;
        }
        
        std::cout << "Initialization complete.\n\n";
        
        // Start
        std::cout << "Starting MidiMind...\n";
        
        if (!app.start()) {
            std::cerr << "ERROR: Failed to start application\n";
            return EXIT_FAILURE;
        }
        
        std::cout << "\nMidiMind is running.\n";
        std::cout << "Press Ctrl+C to stop.\n\n";
        
        // Wait for shutdown signal
        app.waitForShutdown();
        
        // Stop
        std::cout << "\nStopping MidiMind...\n";
        app.stop();
        
        std::cout << "MidiMind stopped.\n";
        
        return EXIT_SUCCESS;
        
    } catch (const MidiMindException& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return EXIT_FAILURE;
        
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << "\n";
        return EXIT_FAILURE;
        
    } catch (...) {
        std::cerr << "FATAL ERROR: Unknown exception\n";
        return EXIT_FAILURE;
    }
}

// ============================================================================
// FIN DU FICHIER main.cpp
// ============================================================================