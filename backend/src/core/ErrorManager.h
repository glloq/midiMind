// ============================================================================
// Fichier: src/core/ErrorManager.h
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestionnaire centralisé des erreurs de l'application utilisant le
//   pattern Observer. Permet aux modules de signaler des erreurs et aux
//   observateurs (comme l'API Server) d'être notifiés pour broadcast.
//
// Fonctionnalités:
//   - Enregistrement d'erreurs avec niveaux de gravité
//   - Pattern Observer pour notifications
//   - Thread-safe
//   - Historique des erreurs
//   - Statistiques d'erreurs
//
// Design Pattern:
//   - Singleton (instance unique)
//   - Observer Pattern (notifications aux observers)
//   - Thread-safe via std::mutex
//
// Auteur: midiMind Team
// Date: 2025-10-02
// Version: 3.0.0
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES SYSTÈME
// ============================================================================
#include <string>        // Pour std::string
#include <vector>        // Pour std::vector (liste observers)
#include <functional>    // Pour std::function (callbacks)
#include <mutex>         // Pour std::mutex (thread-safety)
#include <deque>         // Pour std::deque (historique)
#include <chrono>        // Pour timestamps
#include <algorithm>     // Pour std::remove_if

namespace midiMind {

// ============================================================================
// CLASSE: ErrorManager
// ============================================================================

/**
 * @class ErrorManager
 * @brief Gestionnaire centralisé des erreurs avec pattern Observer
 * 
 * Cette classe permet de centraliser la gestion des erreurs dans l'application.
 * Les modules signalent les erreurs via reportError(), et les observateurs
 * enregistrés (via addObserver()) sont notifiés automatiquement.
 * 
 * @details
 * Utilise le pattern Observer pour découpler la génération d'erreurs de
 * leur traitement. Typiquement, l'ApiServer s'enregistre comme observer
 * pour broadcaster les erreurs critiques aux clients WebSocket.
 * 
 * Le gestionnaire maintient également un historique des dernières erreurs
 * et des statistiques.
 * 
 * @note Thread-safe : peut être utilisé depuis n'importe quel thread
 * @note Singleton : une seule instance pour toute l'application
 * 
 * @example Signaler une erreur:
 * @code
 * try {
 *     // Code qui peut échouer
 *     connectToDevice(deviceId);
 * } catch (const std::exception& e) {
 *     ErrorManager::instance().reportError(
 *         "DeviceManager",
 *         "Failed to connect to device",
 *         ErrorManager::Severity::ERROR,
 *         e.what()
 *     );
 * }
 * @endcode
 * 
 * @example S'enregistrer comme observer:
 * @code
 * // Dans Application::setupObservers()
 * ErrorManager::instance().addObserver([this](const ErrorInfo& error) {
 *     // Broadcaster l'erreur via WebSocket
 *     if (error.severity >= ErrorManager::Severity::ERROR) {
 *         json msg;
 *         msg["event"] = "system.error";
 *         msg["module"] = error.module;
 *         msg["message"] = error.message;
 *         apiServer_->broadcast(msg);
 *     }
 * });
 * @endcode
 */
class ErrorManager {
public:
    // ========================================================================
    // ÉNUMÉRATION: Niveaux de Gravité
    // ========================================================================
    
    /**
     * @enum Severity
     * @brief Niveaux de gravité des erreurs
     */
    enum class Severity {
        INFO = 0,     ///< Information (pas vraiment une erreur)
        WARNING = 1,  ///< Avertissement (erreur non critique)
        ERROR = 2,    ///< Erreur (situation anormale)
        CRITICAL = 3  ///< Critique (erreur fatale, arrêt nécessaire)
    };
    
    // ========================================================================
    // STRUCTURE: Informations sur une Erreur
    // ========================================================================
    
    /**
     * @struct ErrorInfo
     * @brief Informations complètes sur une erreur signalée
     */
    struct ErrorInfo {
        std::string module;          ///< Module ayant signalé l'erreur
        std::string message;         ///< Message principal
        Severity severity;           ///< Niveau de gravité
        std::string details;         ///< Détails supplémentaires (optionnel)
        int64_t timestamp;           ///< Timestamp (ms depuis epoch)
        
        /**
         * @brief Constructeur avec tous les champs
         */
        ErrorInfo(const std::string& mod, const std::string& msg,
                 Severity sev, const std::string& det = "")
            : module(mod), message(msg), severity(sev), details(det) {
            // Calculer le timestamp
            timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }
        
        /**
         * @brief Convertit la gravité en string
         */
        std::string severityToString() const {
            switch (severity) {
                case Severity::INFO:     return "INFO";
                case Severity::WARNING:  return "WARNING";
                case Severity::ERROR:    return "ERROR";
                case Severity::CRITICAL: return "CRITICAL";
                default:                 return "UNKNOWN";
            }
        }
    };
    
    // ========================================================================
    // TYPE: Observer Callback
    // ========================================================================
    
    /**
     * @typedef ObserverCallback
     * @brief Type de fonction callback pour les observers
     * 
     * Les observers sont notifiés avec une ErrorInfo complète.
     */
    using ObserverCallback = std::function<void(const ErrorInfo&)>;
    
    // ========================================================================
    // SINGLETON - ACCÈS À L'INSTANCE
    // ========================================================================
    
    /**
     * @brief Récupère l'instance unique d'ErrorManager (Singleton)
     * 
     * @return ErrorManager& Référence à l'instance unique
     * 
     * @note Thread-safe depuis C++11 (Meyer's Singleton)
     */
    static ErrorManager& instance() {
        static ErrorManager instance;
        return instance;
    }
    
    // ========================================================================
    // DÉSACTIVATION COPIE ET ASSIGNATION
    // ========================================================================
    
    ErrorManager(const ErrorManager&) = delete;
    ErrorManager& operator=(const ErrorManager&) = delete;
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - SIGNALEMENT D'ERREURS
    // ========================================================================
    
    /**
     * @brief Signale une erreur
     * 
     * Enregistre l'erreur dans l'historique, met à jour les statistiques,
     * et notifie tous les observers enregistrés.
     * 
     * @param module Nom du module signalant l'erreur
     * @param message Message d'erreur principal
     * @param severity Niveau de gravité
     * @param details Détails supplémentaires (optionnel)
     * 
     * @note Thread-safe
     * @note Les observers sont notifiés de manière synchrone
     * 
     * @example
     * @code
     * ErrorManager::instance().reportError(
     *     "MidiRouter",
     *     "Failed to route message",
     *     ErrorManager::Severity::ERROR,
     *     "Device disconnected unexpectedly"
     * );
     * @endcode
     */
    void reportError(const std::string& module,
                    const std::string& message,
                    Severity severity,
                    const std::string& details = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Créer l'ErrorInfo
        ErrorInfo error(module, message, severity, details);
        
        // Ajouter à l'historique
        history_.push_back(error);
        
        // Limiter la taille de l'historique
        if (history_.size() > maxHistorySize_) {
            history_.pop_front();
        }
        
        // Mettre à jour les statistiques
        updateStats(severity);
        
        // Notifier les observers
        notifyObservers(error);
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - GESTION DES OBSERVERS
    // ========================================================================
    
    /**
     * @brief Ajoute un observer
     * 
     * Enregistre une fonction callback qui sera appelée à chaque erreur.
     * 
     * @param observer Fonction callback (lambda, fonction, ou functor)
     * 
     * @note Thread-safe
     * @note Les observers sont appelés dans l'ordre d'enregistrement
     * 
     * @example
     * @code
     * // Enregistrer un observer
     * ErrorManager::instance().addObserver([](const ErrorInfo& error) {
     *     if (error.severity >= ErrorManager::Severity::ERROR) {
     *         std::cerr << "ERROR in " << error.module << ": " 
     *                   << error.message << std::endl;
     *     }
     * });
     * @endcode
     */
    void addObserver(ObserverCallback observer) {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_.push_back(observer);
    }
    
    /**
     * @brief Supprime tous les observers
     * 
     * @note Thread-safe
     */
    void clearObservers() {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_.clear();
    }
    
    /**
     * @brief Récupère le nombre d'observers enregistrés
     * 
     * @return size_t Nombre d'observers
     * 
     * @note Thread-safe
     */
    size_t getObserverCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return observers_.size();
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - HISTORIQUE
    // ========================================================================
    
    /**
     * @brief Récupère l'historique des erreurs
     * 
     * Retourne une copie de l'historique pour éviter les problèmes
     * de synchronisation.
     * 
     * @param maxCount Nombre maximum d'erreurs à retourner (0 = toutes)
     * @return std::vector<ErrorInfo> Historique des erreurs (plus récentes en dernier)
     * 
     * @note Thread-safe
     * @note Retourne une COPIE de l'historique
     * 
     * @example
     * @code
     * // Récupérer les 10 dernières erreurs
     * auto errors = ErrorManager::instance().getHistory(10);
     * for (const auto& error : errors) {
     *     std::cout << error.module << ": " << error.message << std::endl;
     * }
     * @endcode
     */
    std::vector<ErrorInfo> getHistory(size_t maxCount = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<ErrorInfo> result;
        
        if (maxCount == 0 || maxCount >= history_.size()) {
            // Retourner tout l'historique
            result.assign(history_.begin(), history_.end());
        } else {
            // Retourner les N dernières erreurs
            auto start = history_.end() - maxCount;
            result.assign(start, history_.end());
        }
        
        return result;
    }
    
    /**
     * @brief Efface l'historique des erreurs
     * 
     * @note Thread-safe
     * @note Les statistiques ne sont pas réinitialisées
     */
    void clearHistory() {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.clear();
    }
    
    /**
     * @brief Définit la taille maximale de l'historique
     * 
     * @param size Taille maximale (défaut: 100)
     * 
     * @note Thread-safe
     */
    void setMaxHistorySize(size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        maxHistorySize_ = size;
        
        // Tronquer si nécessaire
        while (history_.size() > maxHistorySize_) {
            history_.pop_front();
        }
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - STATISTIQUES
    // ========================================================================
    
    /**
     * @struct Statistics
     * @brief Statistiques des erreurs
     */
    struct Statistics {
        size_t totalErrors = 0;      ///< Nombre total d'erreurs
        size_t infoCount = 0;        ///< Nombre d'infos
        size_t warningCount = 0;     ///< Nombre d'avertissements
        size_t errorCount = 0;       ///< Nombre d'erreurs
        size_t criticalCount = 0;    ///< Nombre d'erreurs critiques
        int64_t lastErrorTimestamp = 0;  ///< Timestamp de la dernière erreur
    };
    
    /**
     * @brief Récupère les statistiques d'erreurs
     * 
     * @return Statistics Structure contenant les statistiques
     * 
     * @note Thread-safe
     * 
     * @example
     * @code
     * auto stats = ErrorManager::instance().getStatistics();
     * std::cout << "Total errors: " << stats.totalErrors << std::endl;
     * std::cout << "Critical: " << stats.criticalCount << std::endl;
     * @endcode
     */
    Statistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    /**
     * @brief Réinitialise les statistiques
     * 
     * @note Thread-safe
     * @note L'historique n'est pas effacé
     */
    void resetStatistics() {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = Statistics();
    }

private:
    // ========================================================================
    // CONSTRUCTEUR PRIVÉ (SINGLETON)
    // ========================================================================
    
    /**
     * @brief Constructeur privé (Singleton)
     */
    ErrorManager() : maxHistorySize_(100) {}
    
    /**
     * @brief Destructeur
     */
    ~ErrorManager() = default;
    
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Notifie tous les observers
     * 
     * Appelle chaque observer avec l'ErrorInfo.
     * 
     * @param error Information sur l'erreur
     * 
     * @note Appelée avec le mutex déjà verrouillé
     */
    void notifyObservers(const ErrorInfo& error) {
        // Note: mutex déjà verrouillé par reportError()
        for (auto& observer : observers_) {
            try {
                observer(error);
            } catch (const std::exception& e) {
                // Ne pas propager les exceptions des observers
                // pour éviter de bloquer les autres observers
            }
        }
    }
    
    /**
     * @brief Met à jour les statistiques
     * 
     * @param severity Gravité de l'erreur signalée
     * 
     * @note Appelée avec le mutex déjà verrouillé
     */
    void updateStats(Severity severity) {
        // Note: mutex déjà verrouillé par reportError()
        stats_.totalErrors++;
        
        switch (severity) {
            case Severity::INFO:
                stats_.infoCount++;
                break;
            case Severity::WARNING:
                stats_.warningCount++;
                break;
            case Severity::ERROR:
                stats_.errorCount++;
                break;
            case Severity::CRITICAL:
                stats_.criticalCount++;
                break;
        }
        
        stats_.lastErrorTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /**
     * @brief Mutex pour thread-safety
     */
    mutable std::mutex mutex_;
    
    /**
     * @brief Liste des observers enregistrés
     */
    std::vector<ObserverCallback> observers_;
    
    /**
     * @brief Historique des erreurs (FIFO avec taille limitée)
     */
    std::deque<ErrorInfo> history_;
    
    /**
     * @brief Taille maximale de l'historique
     */
    size_t maxHistorySize_;
    
    /**
     * @brief Statistiques des erreurs
     */
    Statistics stats_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ErrorManager.h
// ============================================================================
