// ============================================================================
// Fichier: backend/src/main.cpp
// Version: 3.0.4 - COMPLET AVEC AMÉLIORATIONS
// Date: 2025-10-15
// ============================================================================
// Description:
//   Point d'entrée principal de l'application MidiMind
//
// CHANGEMENTS v3.0.4:
//   ✅ Parsing arguments amélioré (--config, --daemon, --pidfile, --log-level)
//   ✅ Mode daemon complet avec fork()
//   ✅ Gestion PID file
//   ✅ Health check au démarrage
//   ✅ Exit codes précis (0-4)
//   ✅ Gestion erreurs robuste
//   ✅ Support verbose mode
//
// PRÉSERVÉ DE v3.0.0:
//   ✅ Structure originale
//   ✅ Gestion signaux via Application
//   ✅ Séquence initialize → start → wait → stop
//   ✅ Try-catch global
//   ✅ Messages console user-friendly
//
// Exit Codes:
//   0 - SUCCESS
//   1 - INITIALIZATION_FAILED
//   2 - START_FAILED
//   3 - RUNTIME_ERROR
//   4 - INVALID_ARGUMENTS
//
// Auteur: MidiMind Team
// ============================================================================

#include "core/Application.h"
#include "core/Logger.h"
#include "core/exceptions/MidiMindException.h"
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
// EXIT CODES
// ============================================================================

enum ExitCode {
    SUCCESS = 0,
    INITIALIZATION_FAILED = 1,
    START_FAILED = 2,
    RUNTIME_ERROR = 3,
    INVALID_ARGUMENTS = 4
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @brief Arguments de ligne de commande
 */
struct CommandLineArgs {
    std::string configPath;
    std::string pidFile;
    std::string logLevel = "info";
    bool showHelp = false;
    bool showVersion = false;
    bool daemonMode = false;
    bool verbose = false;
};

// ============================================================================
// HELPERS - VERSION & USAGE
// ============================================================================

/**
 * @brief Affiche la version
 */
void printVersion() {
    std::cout << "╔═══════════════════════════════════════════╗\n";
    std::cout << "║           midiMind v3.0.4                 ║\n";
    std::cout << "╚═══════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "MIDI Orchestration System for Raspberry Pi\n";
    std::cout << "Build date: " << __DATE__ << " " << __TIME__ << "\n";
    std::cout << "Protocol version: 3.0\n";
    std::cout << "Copyright (c) 2025 MidiMind Team\n";
    std::cout << "\n";
}

/**
 * @brief Affiche l'usage
 */
void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n\n";
    
    std::cout << "Options:\n";
    std::cout << "  -c, --config <path>      Path to configuration file\n";
    std::cout << "                           (default: ./config/config.json)\n";
    std::cout << "  -d, --daemon             Run as daemon (background)\n";
    std::cout << "  -p, --pidfile <path>     Write PID to file (daemon mode)\n";
    std::cout << "                           (default: /var/run/midimind.pid)\n";
    std::cout << "  -l, --log-level <level>  Log level: debug, info, warn, error\n";
    std::cout << "                           (default: info)\n";
    std::cout << "  -v, --verbose            Enable verbose output (debug level)\n";
    std::cout << "  -h, --help               Show this help message\n";
    std::cout << "  -V, --version            Show version information\n";
    std::cout << "\n";
    
    std::cout << "Examples:\n";
    std::cout << "  " << programName << "\n";
    std::cout << "    Start with default configuration\n\n";
    
    std::cout << "  " << programName << " --config /path/to/config.json\n";
    std::cout << "    Start with custom configuration\n\n";
    
    std::cout << "  " << programName << " --daemon --pidfile /var/run/midimind.pid\n";
    std::cout << "    Start as daemon with PID file\n\n";
    
    std::cout << "  " << programName << " --log-level debug --verbose\n";
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

// ============================================================================
// HELPERS - ARGUMENT PARSING
// ============================================================================

/**
 * @brief Parse les arguments de ligne de commande
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
                // Valider log level
                if (args.logLevel != "debug" && args.logLevel != "info" &&
                    args.logLevel != "warn" && args.logLevel != "error") {
                    std::cerr << "Error: Invalid log level '" << args.logLevel << "'\n";
                    std::cerr << "Valid levels: debug, info, warn, error\n";
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
            args.logLevel = "debug";
        }
        // Unknown
        else {
            std::cerr << "Error: Unknown option '" << arg << "'\n\n";
            args.showHelp = true;
        }
    }
    
    // PID file par défaut si daemon mode
    if (args.daemonMode && args.pidFile.empty()) {
        args.pidFile = "/var/run/midimind.pid";
    }
    
    return args;
}

// ============================================================================
// HELPERS - DAEMON
// ============================================================================

/**
 * @brief Daemonize le processus
 */
bool daemonize() {
    std::cout << "Daemonizing process...\n";
    
    // Fork 1: Créer un processus enfant
    pid_t pid = fork();
    
    if (pid < 0) {
        std::cerr << "Error: fork() failed: " << strerror(errno) << "\n";
        return false;
    }
    
    // Parent: terminer
    if (pid > 0) {
        std::exit(EXIT_SUCCESS);
    }
    
    // Enfant: devenir leader de session
    if (setsid() < 0) {
        std::cerr << "Error: setsid() failed: " << strerror(errno) << "\n";
        return false;
    }
    
    // Ignorer SIGHUP
    signal(SIGHUP, SIG_IGN);
    
    // Fork 2: Prévenir réacquisition terminal
    pid = fork();
    
    if (pid < 0) {
        std::cerr << "Error: second fork() failed: " << strerror(errno) << "\n";
        return false;
    }
    
    if (pid > 0) {
        std::exit(EXIT_SUCCESS);
    }
    
    // Changer répertoire de travail vers root
    if (chdir("/") < 0) {
        std::cerr << "Error: chdir() failed: " << strerror(errno) << "\n";
        return false;
    }
    
    // Fermer descripteurs de fichiers standard
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Rediriger vers /dev/null
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
 * @brief Écrit le PID dans un fichier
 */
bool writePidFile(const std::string& pidFile) {
    if (pidFile.empty()) {
        return true;
    }
    
    std::ofstream file(pidFile);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write PID file: " << pidFile << "\n";
        std::cerr << "Reason: " << strerror(errno) << "\n";
        return false;
    }
    
    file << getpid() << "\n";
    file.close();
    
    return true;
}

/**
 * @brief Supprime le PID file
 */
void removePidFile(const std::string& pidFile) {
    if (!pidFile.empty()) {
        std::remove(pidFile.c_str());
    }
}

// ============================================================================
// HELPERS - HEALTH CHECK
// ============================================================================

/**
 * @brief Vérifie la santé de l'application au démarrage
 */
bool performHealthCheck(Application& app) {
    std::cout << "\nPerforming health check...\n";
    
    // Vérifier initialization
    if (!app.isInitialized()) {
        std::cerr << "✗ Health check failed: Not initialized\n";
        return false;
    }
    std::cout << "  ✓ Initialized\n";
    
    // Vérifier running
    if (!app.isRunning()) {
        std::cerr << "✗ Health check failed: Not running\n";
        return false;
    }
    std::cout << "  ✓ Running\n";
    
    // Vérifier modules
    if (!app.getDeviceManager()) {
        std::cerr << "✗ Health check failed: DeviceManager missing\n";
        return false;
    }
    std::cout << "  ✓ DeviceManager OK\n";
    
    if (!app.getRouter()) {
        std::cerr << "✗ Health check failed: Router missing\n";
        return false;
    }
    std::cout << "  ✓ Router OK\n";
    
    if (!app.getPlayer()) {
        std::cerr << "✗ Health check failed: Player missing\n";
        return false;
    }
    std::cout << "  ✓ Player OK\n";
    
    if (!app.getApiServer()) {
        std::cerr << "✗ Health check failed: ApiServer missing\n";
        return false;
    }
    std::cout << "  ✓ ApiServer OK\n";
    
    std::cout << "✓ Health check passed\n\n";
    return true;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    try {
        // ====================================================================
        // 1. PARSING ARGUMENTS
        // ====================================================================
        
        CommandLineArgs args = parseCommandLine(argc, argv);
        
        // Afficher aide
        if (args.showHelp) {
            printUsage(argv[0]);
            return args.showHelp && argc > 1 ? INVALID_ARGUMENTS : SUCCESS;
        }
        
        // Afficher version
        if (args.showVersion) {
            printVersion();
            return SUCCESS;
        }
        
        // ====================================================================
        // 2. AFFICHER CONFIGURATION (si verbose)
        // ====================================================================
        
        if (args.verbose) {
            std::cout << "═══════════════════════════════════════════\n";
            std::cout << "  Configuration\n";
            std::cout << "═══════════════════════════════════════════\n";
            std::cout << "  Config path: " << (args.configPath.empty() ? 
                "./config/config.json" : args.configPath) << "\n";
            std::cout << "  Log level:   " << args.logLevel << "\n";
            std::cout << "  Daemon mode: " << (args.daemonMode ? "yes" : "no") << "\n";
            if (!args.pidFile.empty()) {
                std::cout << "  PID file:    " << args.pidFile << "\n";
            }
            std::cout << "═══════════════════════════════════════════\n";
            std::cout << "\n";
        }
        
        // ====================================================================
        // 3. DAEMONIZE (si demandé) - AVANT création Application
        // ====================================================================
        
        if (args.daemonMode) {
            std::cout << "Starting in daemon mode...\n";
            
            if (!daemonize()) {
                std::cerr << "Error: Failed to daemonize\n";
                return INITIALIZATION_FAILED;
            }
            
            // Après daemonize, stdout/stderr sont redirigés vers /dev/null
            // Logger va écrire dans les fichiers de log
        }
        
        // ====================================================================
        // 4. ÉCRIRE PID FILE
        // ====================================================================
        
        if (!writePidFile(args.pidFile)) {
            return INITIALIZATION_FAILED;
        }
        
        // ====================================================================
        // 5. CONFIGURER LOGGER
        // ====================================================================
        
        Logger::setLevel(args.logLevel);
        
        if (args.daemonMode) {
            Logger::setLogToFile(true);
            Logger::setLogFile("./data/logs/midimind.log");
        }
        
        // ====================================================================
        // 6. RÉCUPÉRER APPLICATION INSTANCE
        // ====================================================================
        
        std::cout << "\n";
        std::cout << "═══════════════════════════════════════════\n";
        std::cout << "  midiMind v3.0.4\n";
        std::cout << "═══════════════════════════════════════════\n";
        std::cout << "\n";
        
        Application& app = Application::instance();
        
        // ====================================================================
        // 7. INITIALIZE (SCÉNARIO 1: STARTUP)
        // ====================================================================
        
        std::cout << "Initializing midiMind...\n";
        
        if (!app.initialize(args.configPath)) {
            std::cerr << "\n✗ ERROR: Failed to initialize application\n";
            std::cerr << "Check logs for details\n";
            removePidFile(args.pidFile);
            return INITIALIZATION_FAILED;
        }
        
        std::cout << "\n✓ Initialization complete\n\n";
        
        // ====================================================================
        // 8. START (SCÉNARIO 1: STARTUP)
        // ====================================================================
        
        std::cout << "Starting midiMind...\n";
        
        if (!app.start()) {
            std::cerr << "\n✗ ERROR: Failed to start application\n";
            std::cerr << "Check logs for details\n";
            app.stop();
            removePidFile(args.pidFile);
            return START_FAILED;
        }
        
        std::cout << "\n✓ midiMind is running\n";
        
        // ====================================================================
        // 9. HEALTH CHECK
        // ====================================================================
        
        if (!performHealthCheck(app)) {
            std::cerr << "\n✗ ERROR: Health check failed\n";
            app.stop();
            removePidFile(args.pidFile);
            return START_FAILED;
        }
        
        // ====================================================================
        // 10. WAIT FOR SHUTDOWN (SCÉNARIO 5: SHUTDOWN)
        // ====================================================================
        
        if (!args.daemonMode) {
            std::cout << "Press Ctrl+C to stop\n";
            std::cout << "(Press 3x Ctrl+C for force quit)\n";
            std::cout << "\n";
        }
        
        // Bloquer jusqu'à signal
        app.waitForShutdown();
        
        // ====================================================================
        // 11. STOP (SCÉNARIO 5: SHUTDOWN)
        // ====================================================================
        
        std::cout << "\nStopping midiMind...\n";
        app.stop();
        
        // Supprimer PID file
        removePidFile(args.pidFile);
        
        std::cout << "\n✓ midiMind stopped cleanly\n";
        
        return SUCCESS;
        
    } catch (const MidiMindException& e) {
        std::cerr << "\n✗ MIDIMIND ERROR: " << e.what() << "\n";
        std::cerr << "Error code: " << e.getErrorCode() << "\n";
        return RUNTIME_ERROR;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ FATAL ERROR: " << e.what() << "\n";
        return RUNTIME_ERROR;
        
    } catch (...) {
        std::cerr << "\n✗ FATAL ERROR: Unknown exception\n";
        return RUNTIME_ERROR;
    }
}

// ============================================================================
// FIN DU FICHIER main.cpp v3.0.4 - COMPLET
// ============================================================================