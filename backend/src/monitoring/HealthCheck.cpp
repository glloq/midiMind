// ============================================================================
// Fichier: src/monitoring/HealthCheck.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "HealthCheck.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

HealthCheck::HealthCheck()
    : running_(false)
    , checkIntervalMs_(5000)
    , defaultRulesEnabled_(true) {
    
    Logger::info("HealthCheck", "HealthCheck constructed");
    
    initializeDefaultRules();
}

HealthCheck::~HealthCheck() {
    stop();
    Logger::info("HealthCheck", "HealthCheck destroyed");
}

// ============================================================================
// CONTRÔLE
// ============================================================================

void HealthCheck::start() {
    if (running_) {
        Logger::warn("HealthCheck", "Already running");
        return;
    }
    
    Logger::info("HealthCheck", "Starting health checks...");
    Logger::info("HealthCheck", "  Check interval: " + 
                std::to_string(checkIntervalMs_) + "ms");
    Logger::info("HealthCheck", "  Default rules: " + 
                std::string(defaultRulesEnabled_ ? "enabled" : "disabled"));
    
    running_ = true;
    
    checkThread_ = std::thread([this]() {
        checkLoop();
    });
    
    Logger::info("HealthCheck", "✓ Health checks started");
}

void HealthCheck::stop() {
    if (!running_) {
        return;
    }
    
    Logger::info("HealthCheck", "Stopping health checks...");
    
    running_ = false;
    
    if (checkThread_.joinable()) {
        checkThread_.join();
    }
    
    Logger::info("HealthCheck", "✓ Health checks stopped");
}

bool HealthCheck::isRunning() const {
    return running_;
}

// ============================================================================
// ENREGISTREMENT
// ============================================================================

void HealthCheck::registerMetricsCollector(std::shared_ptr<MetricsCollector> collector) {
    std::lock_guard<std::mutex> lock(mutex_);
    metricsCollector_ = collector;
    Logger::info("HealthCheck", "MetricsCollector registered");
}

// ============================================================================
// RÈGLES DE VÉRIFICATION
// ============================================================================

void HealthCheck::addRule(const HealthCheckRule& rule) {
    std::lock_guard<std::mutex> lock(mutex_);
    customRules_.push_back(rule);
    Logger::info("HealthCheck", "Added custom rule: " + rule.name);
}

bool HealthCheck::removeRule(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(customRules_.begin(), customRules_.end(),
        [&name](const HealthCheckRule& rule) {
            return rule.name == name;
        });
    
    if (it != customRules_.end()) {
        customRules_.erase(it);
        Logger::info("HealthCheck", "Removed custom rule: " + name);
        return true;
    }
    
    return false;
}

void HealthCheck::clearCustomRules() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t count = customRules_.size();
    customRules_.clear();
    
    Logger::info("HealthCheck", "Cleared custom rules (" + std::to_string(count) + ")");
}

// ============================================================================
// VÉRIFICATION
// ============================================================================

HealthStatus HealthCheck::checkNow() {
    auto status = performChecks();
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Vérifier si l'état a changé
        bool changed = (status.level != currentStatus_.level);
        
        currentStatus_ = status;
        
        // Callback si changement
        if (changed && onHealthChanged_) {
            onHealthChanged_(status);
        }
    }
    
    return status;
}

HealthStatus HealthCheck::getCurrentStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentStatus_;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void HealthCheck::setCheckInterval(uint32_t intervalMs) {
    checkIntervalMs_ = std::max(intervalMs, 1000u);
    Logger::info("HealthCheck", "Check interval set to " + 
                std::to_string(checkIntervalMs_) + "ms");
}

uint32_t HealthCheck::getCheckInterval() const {
    return checkIntervalMs_;
}

void HealthCheck::setDefaultRulesEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    defaultRulesEnabled_ = enabled;
    Logger::info("HealthCheck", "Default rules " + 
                std::string(enabled ? "enabled" : "disabled"));
}

// ============================================================================
// CALLBACKS
// ============================================================================

void HealthCheck::setOnHealthChanged(HealthChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onHealthChanged_ = callback;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void HealthCheck::checkLoop() {
    Logger::info("HealthCheck", "Check loop started");
    
    while (running_) {
        // Effectuer les vérifications
        checkNow();
        
        // Attendre l'intervalle
        uint32_t interval = checkIntervalMs_;
        for (uint32_t i = 0; i < interval / 100 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    Logger::info("HealthCheck", "Check loop stopped");
}

void HealthCheck::initializeDefaultRules() {
    // CPU usage > 90%
    defaultRules_.push_back(HealthCheckRule(
        "cpu_high",
        [](const AggregatedMetrics& m) { 
            return m.system.cpuUsagePercent > 90.0f; 
        },
        HealthLevel::WARNING,
        "CPU usage is critically high"
    ));
    
    // CPU temperature > 80°C
    defaultRules_.push_back(HealthCheckRule(
        "cpu_temp_high",
        [](const AggregatedMetrics& m) { 
            return m.system.cpuTemperature > 80.0f; 
        },
        HealthLevel::CRITICAL,
        "CPU temperature is dangerously high"
    ));
    
    // RAM usage > 90%
    defaultRules_.push_back(HealthCheckRule(
        "ram_high",
        [](const AggregatedMetrics& m) { 
            return m.system.ramUsagePercent > 90.0f; 
        },
        HealthLevel::WARNING,
        "RAM usage is critically high"
    ));
    
    // Disk usage > 95%
    defaultRules_.push_back(HealthCheckRule(
        "disk_full",
        [](const AggregatedMetrics& m) { 
            return m.system.diskUsagePercent > 95.0f; 
        },
        HealthLevel::CRITICAL,
        "Disk is almost full"
    ));
    
    // Latence MIDI > 10ms
    defaultRules_.push_back(HealthCheckRule(
        "latency_high",
        [](const AggregatedMetrics& m) { 
            return m.latency.averageLatencyUs > 10000.0f; 
        },
        HealthLevel::WARNING,
        "MIDI latency is too high"
    ));
    
    // Messages perdus
    defaultRules_.push_back(HealthCheckRule(
        "messages_dropped",
        [](const AggregatedMetrics& m) { 
            return m.midi.messagesDropped > 0 || m.latency.droppedMessages > 0; 
        },
        HealthLevel::WARNING,
        "MIDI messages are being dropped"
    ));
    
    Logger::info("HealthCheck", "Initialized " + 
                std::to_string(defaultRules_.size()) + " default rules");
}

HealthStatus HealthCheck::performChecks() {
    // Récupérer les métriques
    if (!metricsCollector_) {
        HealthStatus status(HealthLevel::ERROR, "No metrics collector registered");
        return status;
    }
    
    auto metrics = metricsCollector_->getCurrentMetrics();
    
    HealthStatus status(HealthLevel::HEALTHY, "All systems operational");
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Vérifier les règles par défaut
    if (defaultRulesEnabled_) {
        for (const auto& rule : defaultRules_) {
            if (rule.check(metrics)) {
                // Échec de la règle
                if (rule.severity > status.level) {
                    status.level = rule.severity;
                    status.message = rule.message;
                }
                status.issues.push_back(rule.name + ": " + rule.message);
            }
        }
    }
    
    // Vérifier les règles personnalisées
    for (const auto& rule : customRules_) {
        if (rule.check(metrics)) {
            if (rule.severity > status.level) {
                status.level = rule.severity;
                status.message = rule.message;
            }
            status.issues.push_back(rule.name + ": " + rule.message);
        }
    }
    
    return status;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER HealthCheck.cpp
// ============================================================================