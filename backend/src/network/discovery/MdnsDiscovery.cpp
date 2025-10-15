// ============================================================================
// Fichier: backend/src/network/discovery/MdnsDiscovery.cpp
// Version: 1.0.0
// Date: 2025-10-15
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Implémentation complète de la découverte mDNS/Bonjour.
//   Utilise Avahi (Linux) pour découvrir les services RTP-MIDI sur le réseau.
//   Compatible avec Apple Network MIDI et autres implémentations.
//
// Fonctionnalités:
//   - Découverte services mDNS (_apple-midi._udp)
//   - Surveillance continue avec callbacks
//   - Résolution DNS automatique
//   - Cache des services découverts
//
// Architecture:
//   - Thread-safe avec mutex
//   - Thread dédié pour découverte
//   - Utilise Avahi Client API
//
// Auteur: MidiMind Team
// Statut: ✅ COMPLET - Implémentation avec Avahi
// ============================================================================

#include "MdnsDiscovery.h"
#include "../../core/Logger.h"
#include <algorithm>
#include <chrono>

// Avahi headers
#ifdef HAS_AVAHI
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#endif

namespace midiMind {

// ============================================================================
// STRUCTURE INTERNE AVAHI
// ============================================================================

#ifdef HAS_AVAHI
struct AvahiContext {
    AvahiSimplePoll* simplePoll;
    AvahiClient* client;
    AvahiServiceBrowser* browser;
    MdnsDiscovery* discovery;
};

// ============================================================================
// CALLBACKS AVAHI
// ============================================================================

static void resolve_callback(
    AvahiServiceResolver* r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char* name,
    const char* type,
    const char* domain,
    const char* host_name,
    const AvahiAddress* address,
    uint16_t port,
    AvahiStringList* txt,
    AvahiLookupResultFlags flags,
    void* userdata
) {
    AvahiContext* context = static_cast<AvahiContext*>(userdata);
    
    if (event == AVAHI_RESOLVER_FOUND) {
        // Convertir l'adresse en string
        char addr[AVAHI_ADDRESS_STR_MAX];
        avahi_address_snprint(addr, sizeof(addr), address);
        
        // Créer le ServiceInfo
        ServiceInfo service;
        service.id = std::string(name) + "@" + addr;
        service.name = name;
        service.type = type;
        service.domain = domain;
        service.hostname = host_name;
        service.address = addr;
        service.port = port;
        service.discovered = std::chrono::system_clock::now();
        
        // Ajouter au cache et appeler callback
        if (context && context->discovery) {
            context->discovery->addDiscoveredService(service);
        }
        
        Logger::info("MdnsDiscovery", 
            "Service resolved: " + service.name + " at " + service.address + ":" + std::to_string(service.port));
    }
    
    avahi_service_resolver_free(r);
}

static void browse_callback(
    AvahiServiceBrowser* b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char* name,
    const char* type,
    const char* domain,
    AvahiLookupResultFlags flags,
    void* userdata
) {
    AvahiContext* context = static_cast<AvahiContext*>(userdata);
    
    switch (event) {
        case AVAHI_BROWSER_NEW:
            Logger::debug("MdnsDiscovery", 
                "New service: " + std::string(name) + " (" + std::string(type) + ")");
            
            // Créer un resolver pour obtenir les détails
            if (context && context->client) {
                avahi_service_resolver_new(
                    context->client,
                    interface,
                    protocol,
                    name,
                    type,
                    domain,
                    AVAHI_PROTO_UNSPEC,
                    static_cast<AvahiLookupFlags>(0),
                    resolve_callback,
                    userdata
                );
            }
            break;
            
        case AVAHI_BROWSER_REMOVE:
            Logger::debug("MdnsDiscovery", "Service removed: " + std::string(name));
            
            // Retirer du cache
            if (context && context->discovery) {
                context->discovery->removeService(name);
            }
            break;
            
        case AVAHI_BROWSER_ALL_FOR_NOW:
            Logger::debug("MdnsDiscovery", "Initial scan complete");
            break;
            
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            Logger::debug("MdnsDiscovery", "Cache exhausted");
            break;
            
        case AVAHI_BROWSER_FAILURE:
            Logger::error("MdnsDiscovery", 
                "Browser failure: " + std::string(avahi_strerror(avahi_client_errno(context->client))));
            break;
    }
}

static void client_callback(
    AvahiClient* c,
    AvahiClientState state,
    void* userdata
) {
    AvahiContext* context = static_cast<AvahiContext*>(userdata);
    
    switch (state) {
        case AVAHI_CLIENT_S_RUNNING:
            Logger::info("MdnsDiscovery", "Avahi client running");
            
            // Créer le browser pour _apple-midi._udp
            if (context && !context->browser) {
                context->browser = avahi_service_browser_new(
                    c,
                    AVAHI_IF_UNSPEC,
                    AVAHI_PROTO_UNSPEC,
                    "_apple-midi._udp",
                    nullptr,
                    static_cast<AvahiLookupFlags>(0),
                    browse_callback,
                    userdata
                );
                
                if (!context->browser) {
                    Logger::error("MdnsDiscovery", 
                        "Failed to create service browser: " + 
                        std::string(avahi_strerror(avahi_client_errno(c))));
                }
            }
            break;
            
        case AVAHI_CLIENT_FAILURE:
            Logger::error("MdnsDiscovery", 
                "Avahi client failure: " + std::string(avahi_strerror(avahi_client_errno(c))));
            break;
            
        case AVAHI_CLIENT_S_COLLISION:
        case AVAHI_CLIENT_S_REGISTERING:
            Logger::warn("MdnsDiscovery", "Avahi client state change");
            break;
            
        case AVAHI_CLIENT_CONNECTING:
            Logger::debug("MdnsDiscovery", "Connecting to Avahi...");
            break;
    }
}
#endif // HAS_AVAHI

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

MdnsDiscovery::MdnsDiscovery()
    : running_(false)
    , avahiContext_(nullptr)
{
    Logger::info("MdnsDiscovery", "╔═══════════════════════════════════════╗");
    Logger::info("MdnsDiscovery", "  MdnsDiscovery v1.0.0");
    Logger::info("MdnsDiscovery", "╚═══════════════════════════════════════╝");
    
#ifdef HAS_AVAHI
    Logger::info("MdnsDiscovery", "Avahi support enabled");
#else
    Logger::warn("MdnsDiscovery", "Avahi support disabled (HAS_AVAHI not defined)");
    Logger::warn("MdnsDiscovery", "mDNS discovery will not be available");
#endif
}

MdnsDiscovery::~MdnsDiscovery() {
    stop();
    Logger::info("MdnsDiscovery", "MdnsDiscovery destroyed");
}

// ============================================================================
// CONTRÔLE
// ============================================================================

bool MdnsDiscovery::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        Logger::warn("MdnsDiscovery", "Already running");
        return false;
    }
    
#ifndef HAS_AVAHI
    Logger::error("MdnsDiscovery", "Cannot start: Avahi support not compiled");
    Logger::error("MdnsDiscovery", "Please install libavahi-client-dev and recompile with -DHAS_AVAHI");
    return false;
#else
    
    Logger::info("MdnsDiscovery", "Starting mDNS discovery...");
    
    // Créer le contexte Avahi
    AvahiContext* context = new AvahiContext();
    context->simplePoll = nullptr;
    context->client = nullptr;
    context->browser = nullptr;
    context->discovery = this;
    
    int error;
    
    // Créer simple poll
    context->simplePoll = avahi_simple_poll_new();
    if (!context->simplePoll) {
        Logger::error("MdnsDiscovery", "Failed to create simple poll");
        delete context;
        return false;
    }
    
    // Créer client
    context->client = avahi_client_new(
        avahi_simple_poll_get(context->simplePoll),
        static_cast<AvahiClientFlags>(0),
        client_callback,
        context,
        &error
    );
    
    if (!context->client) {
        Logger::error("MdnsDiscovery", "Failed to create client: " + std::string(avahi_strerror(error)));
        avahi_simple_poll_free(context->simplePoll);
        delete context;
        return false;
    }
    
    avahiContext_ = context;
    running_ = true;
    
    // Démarrer le thread de polling
    discoveryThread_ = std::thread([this]() {
        Logger::info("MdnsDiscovery", "Discovery thread started");
        
        AvahiContext* ctx = static_cast<AvahiContext*>(avahiContext_);
        if (ctx && ctx->simplePoll) {
            avahi_simple_poll_loop(ctx->simplePoll);
        }
        
        Logger::info("MdnsDiscovery", "Discovery thread stopped");
    });
    
    Logger::info("MdnsDiscovery", "✓ mDNS discovery started");
    return true;
    
#endif // HAS_AVAHI
}

void MdnsDiscovery::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!running_) {
        return;
    }
    
    Logger::info("MdnsDiscovery", "Stopping mDNS discovery...");
    
#ifdef HAS_AVAHI
    AvahiContext* context = static_cast<AvahiContext*>(avahiContext_);
    
    if (context) {
        // Arrêter la boucle
        if (context->simplePoll) {
            avahi_simple_poll_quit(context->simplePoll);
        }
        
        // Attendre le thread
        if (discoveryThread_.joinable()) {
            discoveryThread_.join();
        }
        
        // Libérer les ressources
        if (context->browser) {
            avahi_service_browser_free(context->browser);
        }
        
        if (context->client) {
            avahi_client_free(context->client);
        }
        
        if (context->simplePoll) {
            avahi_simple_poll_free(context->simplePoll);
        }
        
        delete context;
        avahiContext_ = nullptr;
    }
#endif
    
    running_ = false;
    discoveredServices_.clear();
    
    Logger::info("MdnsDiscovery", "✓ mDNS discovery stopped");
}

bool MdnsDiscovery::isRunning() const {
    return running_;
}

// ============================================================================
// SERVICES
// ============================================================================

std::vector<ServiceInfo> MdnsDiscovery::getDiscoveredServices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return discoveredServices_;
}

std::optional<ServiceInfo> MdnsDiscovery::getServiceById(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(discoveredServices_.begin(), discoveredServices_.end(),
        [&id](const ServiceInfo& s) { return s.id == id; });
    
    if (it != discoveredServices_.end()) {
        return *it;
    }
    
    return std::nullopt;
}

std::vector<ServiceInfo> MdnsDiscovery::getServicesByType(const std::string& type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ServiceInfo> result;
    std::copy_if(discoveredServices_.begin(), discoveredServices_.end(), 
        std::back_inserter(result),
        [&type](const ServiceInfo& s) { return s.type == type; });
    
    return result;
}

void MdnsDiscovery::clearDiscoveredServices() {
    std::lock_guard<std::mutex> lock(mutex_);
    discoveredServices_.clear();
    Logger::debug("MdnsDiscovery", "Discovered services cleared");
}

// ============================================================================
// CALLBACKS
// ============================================================================

void MdnsDiscovery::setOnServiceDiscovered(ServiceDiscoveredCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onServiceDiscovered_ = callback;
}

void MdnsDiscovery::setOnServiceRemoved(ServiceRemovedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onServiceRemoved_ = callback;
}

// ============================================================================
// STATUS
// ============================================================================

json MdnsDiscovery::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json status;
    status["running"] = running_;
    status["services_count"] = discoveredServices_.size();
    
    json servicesArray = json::array();
    for (const auto& service : discoveredServices_) {
        json s;
        s["id"] = service.id;
        s["name"] = service.name;
        s["type"] = service.type;
        s["address"] = service.address;
        s["port"] = service.port;
        s["hostname"] = service.hostname;
        servicesArray.push_back(s);
    }
    status["services"] = servicesArray;
    
    return status;
}

// ============================================================================
// MÉTHODES INTERNES (PUBLIC POUR CALLBACKS AVAHI)
// ============================================================================

void MdnsDiscovery::addDiscoveredService(const ServiceInfo& service) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Vérifier si déjà présent
    auto it = std::find_if(discoveredServices_.begin(), discoveredServices_.end(),
        [&service](const ServiceInfo& s) { return s.id == service.id; });
    
    if (it != discoveredServices_.end()) {
        // Mettre à jour
        *it = service;
        Logger::debug("MdnsDiscovery", "Service updated: " + service.name);
    } else {
        // Ajouter nouveau
        discoveredServices_.push_back(service);
        Logger::info("MdnsDiscovery", "Service added: " + service.name);
        
        // Callback
        if (onServiceDiscovered_) {
            onServiceDiscovered_(service);
        }
    }
}

void MdnsDiscovery::removeService(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(discoveredServices_.begin(), discoveredServices_.end(),
        [&name](const ServiceInfo& s) { return s.name == name; });
    
    if (it != discoveredServices_.end()) {
        std::string id = it->id;
        discoveredServices_.erase(it);
        
        Logger::info("MdnsDiscovery", "Service removed: " + name);
        
        // Callback
        if (onServiceRemoved_) {
            onServiceRemoved_(id);
        }
    }
}

// ============================================================================
// UTILITAIRES
// ============================================================================

bool MdnsDiscovery::areDependenciesInstalled() {
#ifdef HAS_AVAHI
    // Vérifier que le daemon Avahi tourne
    int result = system("systemctl is-active --quiet avahi-daemon 2>/dev/null");
    return result == 0;
#else
    return false;
#endif
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MdnsDiscovery.cpp v1.0.0
// ============================================================================