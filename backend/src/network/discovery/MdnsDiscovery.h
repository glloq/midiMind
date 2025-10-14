// ============================================================================
// Fichier: backend/src/network/discovery/MdnsDiscovery.cpp
// Implémentation complète avec Avahi pour mDNS/Bonjour
// ============================================================================

#include "MdnsDiscovery.h"
#include "../../core/Logger.h"
#include <chrono>
#include <algorithm>

// Headers Avahi
#ifdef HAS_AVAHI
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#endif

namespace midiMind {

// ============================================================================
// CALLBACKS AVAHI (fonctions C statiques)
// ============================================================================

#ifdef HAS_AVAHI

// Callback client Avahi
static void client_callback(AvahiClient *c, AvahiClientState state, void* userdata) {
    MdnsDiscovery* discovery = static_cast<MdnsDiscovery*>(userdata);
    
    if (state == AVAHI_CLIENT_FAILURE) {
        Logger::error("MdnsDiscovery", "Avahi client failure");
    }
}

// Callback browser
static void browse_callback(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    AvahiLookupResultFlags flags,
    void* userdata)
{
    MdnsDiscovery* discovery = static_cast<MdnsDiscovery*>(userdata);
    
    switch (event) {
        case AVAHI_BROWSER_NEW: {
            Logger::info("MdnsDiscovery", "Service discovered: " + std::string(name));
            
            // Résoudre le service
            AvahiClient* client = avahi_service_browser_get_client(b);
            avahi_service_resolver_new(
                client,
                interface,
                protocol,
                name,
                type,
                domain,
                AVAHI_PROTO_UNSPEC,
                (AvahiLookupFlags)0,
                resolve_callback,
                userdata
            );
            break;
        }
        
        case AVAHI_BROWSER_REMOVE: {
            Logger::info("MdnsDiscovery", "Service removed: " + std::string(name));
            discovery->handleServiceRemoved(name);
            break;
        }
        
        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            break;
            
        case AVAHI_BROWSER_FAILURE:
            Logger::error("MdnsDiscovery", "Browser failure");
            break;
    }
}

// Callback résolution
static void resolve_callback(
    AvahiServiceResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *address,
    uint16_t port,
    AvahiStringList *txt,
    AvahiLookupResultFlags flags,
    void* userdata)
{
    MdnsDiscovery* discovery = static_cast<MdnsDiscovery*>(userdata);
    
    if (event == AVAHI_RESOLVER_FOUND) {
        // Convertir l'adresse IP
        char addr_str[AVAHI_ADDRESS_STR_MAX];
        avahi_address_snprint(addr_str, sizeof(addr_str), address);
        
        // Créer ServiceInfo
        ServiceInfo info;
        info.name = name;
        info.type = type;
        info.domain = domain;
        info.hostname = host_name;
        info.address = addr_str;
        info.port = port;
        
        // Parser les enregistrements TXT
        AvahiStringList* txt_iter = txt;
        while (txt_iter) {
            char *key, *value;
            if (avahi_string_list_get_pair(txt_iter, &key, &value, nullptr) >= 0) {
                info.txtRecords.emplace_back(key, value ? value : "");
                avahi_free(key);
                avahi_free(value);
            }
            txt_iter = avahi_string_list_get_next(txt_iter);
        }
        
        discovery->handleServiceDiscovered(info);
    }
    
    avahi_service_resolver_free(r);
}

// Callback entry group
static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata) {
    if (state == AVAHI_ENTRY_GROUP_ESTABLISHED) {
        Logger::info("MdnsDiscovery", "Service established");
    } else if (state == AVAHI_ENTRY_GROUP_COLLISION) {
        Logger::error("MdnsDiscovery", "Service name collision");
    } else if (state == AVAHI_ENTRY_GROUP_FAILURE) {
        Logger::error("MdnsDiscovery", "Entry group failure");
    }
}

#endif // HAS_AVAHI

// ============================================================================
// IMPLÉMENTATION - MÉTHODES PRIVÉES AVAHI
// ============================================================================

bool MdnsDiscovery::initAvahi() {
    Logger::info("MdnsDiscovery", "Initializing Avahi client...");
    
#ifdef HAS_AVAHI
    int error;
    
    // Créer le poll
    avahiPoll_ = avahi_simple_poll_new();
    if (!avahiPoll_) {
        Logger::error("MdnsDiscovery", "Failed to create Avahi poll");
        return false;
    }
    
    // Créer le client
    avahiClient_ = avahi_client_new(
        avahi_simple_poll_get(static_cast<AvahiSimplePoll*>(avahiPoll_)),
        (AvahiClientFlags)0,
        client_callback,
        this,
        &error
    );
    
    if (!avahiClient_) {
        Logger::error("MdnsDiscovery", "Failed to create Avahi client: " + 
                     std::string(avahi_strerror(error)));
        avahi_simple_poll_free(static_cast<AvahiSimplePoll*>(avahiPoll_));
        avahiPoll_ = nullptr;
        return false;
    }
    
    Logger::info("MdnsDiscovery", "✓ Avahi client initialized");
    return true;
    
#else
    Logger::warn("MdnsDiscovery", "Avahi not available (HAS_AVAHI not defined)");
    // Fallback : simulation pour tests
    avahiClient_ = reinterpret_cast<void*>(0x1);
    avahiPoll_ = reinterpret_cast<void*>(0x2);
    return true;
#endif
}

void MdnsDiscovery::cleanupAvahi() {
    Logger::info("MdnsDiscovery", "Cleaning up Avahi...");
    
#ifdef HAS_AVAHI
    // Libérer les browsers
    for (void* browser : avahiBrowsers_) {
        if (browser) {
            avahi_service_browser_free(static_cast<AvahiServiceBrowser*>(browser));
        }
    }
    avahiBrowsers_.clear();
    
    // Libérer le group
    if (avahiGroup_) {
        avahi_entry_group_free(static_cast<AvahiEntryGroup*>(avahiGroup_));
        avahiGroup_ = nullptr;
    }
    
    // Libérer le client
    if (avahiClient_) {
        avahi_client_free(static_cast<AvahiClient*>(avahiClient_));
        avahiClient_ = nullptr;
    }
    
    // Libérer le poll
    if (avahiPoll_) {
        avahi_simple_poll_free(static_cast<AvahiSimplePoll*>(avahiPoll_));
        avahiPoll_ = nullptr;
    }
    
    Logger::info("MdnsDiscovery", "✓ Avahi cleaned up");
#else
    avahiBrowsers_.clear();
    avahiGroup_ = nullptr;
    avahiClient_ = nullptr;
    avahiPoll_ = nullptr;
#endif
}

bool MdnsDiscovery::startAvahiBrowser(const std::string& serviceType, const std::string& domain) {
    Logger::info("MdnsDiscovery", "Starting Avahi browser for " + serviceType);
    
#ifdef HAS_AVAHI
    AvahiServiceBrowser* browser = avahi_service_browser_new(
        static_cast<AvahiClient*>(avahiClient_),
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_UNSPEC,
        serviceType.c_str(),
        domain.c_str(),
        (AvahiLookupFlags)0,
        browse_callback,
        this
    );
    
    if (!browser) {
        Logger::error("MdnsDiscovery", "Failed to create service browser");
        return false;
    }
    
    avahiBrowsers_.push_back(browser);
    Logger::info("MdnsDiscovery", "✓ Avahi browser started");
    return true;
    
#else
    // Fallback pour tests
    void* browser = reinterpret_cast<void*>(avahiBrowsers_.size() + 1);
    avahiBrowsers_.push_back(browser);
    Logger::info("MdnsDiscovery", "✓ Avahi browser started (stub)");
    return true;
#endif
}

bool MdnsDiscovery::publishAvahiService(const std::string& name, 
                                       uint16_t port, 
                                       const std::string& type) {
    Logger::info("MdnsDiscovery", "Publishing Avahi service...");
    
#ifdef HAS_AVAHI
    int error;
    
    // Créer le group si nécessaire
    if (!avahiGroup_) {
        avahiGroup_ = avahi_entry_group_new(
            static_cast<AvahiClient*>(avahiClient_),
            entry_group_callback,
            this
        );
        
        if (!avahiGroup_) {
            Logger::error("MdnsDiscovery", "Failed to create entry group");
            return false;
        }
    }
    
    // Ajouter le service
    error = avahi_entry_group_add_service(
        static_cast<AvahiEntryGroup*>(avahiGroup_),
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_UNSPEC,
        (AvahiPublishFlags)0,
        name.c_str(),
        type.c_str(),
        nullptr,  // domain (NULL = .local)
        nullptr,  // host (NULL = localhost)
        port,
        nullptr   // TXT records (NULL = none)
    );
    
    if (error < 0) {
        Logger::error("MdnsDiscovery", "Failed to add service: " + 
                     std::string(avahi_strerror(error)));
        return false;
    }
    
    // Commit le group
    error = avahi_entry_group_commit(static_cast<AvahiEntryGroup*>(avahiGroup_));
    if (error < 0) {
        Logger::error("MdnsDiscovery", "Failed to commit entry group: " + 
                     std::string(avahi_strerror(error)));
        return false;
    }
    
    Logger::info("MdnsDiscovery", "✓ Avahi service published");
    return true;
    
#else
    // Fallback pour tests
    avahiGroup_ = reinterpret_cast<void*>(0x3);
    Logger::info("MdnsDiscovery", "✓ Avahi service published (stub)");
    return true;
#endif
}

// ============================================================================
// BOUCLE DE DÉCOUVERTE
// ============================================================================

void MdnsDiscovery::discoveryLoop() {
    Logger::info("MdnsDiscovery", "Discovery loop started");
    
#ifdef HAS_AVAHI
    // Utiliser Avahi poll loop
    while (running_ && avahiPoll_) {
        avahi_simple_poll_iterate(static_cast<AvahiSimplePoll*>(avahiPoll_), 100);
    }
#else
    // Fallback : simulation périodique
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // Simuler découverte pour tests
        if (running_ && discoveredServices_.empty()) {
            ServiceInfo testService;
            testService.name = "Test RTP-MIDI Device";
            testService.type = "_apple-midi._udp";
            testService.domain = "local.";
            testService.hostname = "test.local";
            testService.address = "192.168.1.100";
            testService.port = 5004;
            
            handleServiceDiscovered(testService);
        }
    }
#endif
    
    Logger::info("MdnsDiscovery", "Discovery loop stopped");
}

// ============================================================================
// HANDLERS
// ============================================================================

void MdnsDiscovery::handleServiceDiscovered(const ServiceInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Vérifier si déjà découvert
    auto it = std::find_if(discoveredServices_.begin(), discoveredServices_.end(),
        [&info](const ServiceInfo& s) {
            return s.name == info.name;
        });
    
    if (it == discoveredServices_.end()) {
        discoveredServices_.push_back(info);
        
        Logger::info("MdnsDiscovery", "Service added: " + info.name + 
                    " at " + info.address + ":" + std::to_string(info.port));
        
        // Callback
        if (onServiceDiscovered_) {
            onServiceDiscovered_(info);
        }
    }
}

void MdnsDiscovery::handleServiceRemoved(const std::string& serviceName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(discoveredServices_.begin(), discoveredServices_.end(),
        [&serviceName](const ServiceInfo& s) {
            return s.name == serviceName;
        });
    
    if (it != discoveredServices_.end()) {
        discoveredServices_.erase(it);
        
        Logger::info("MdnsDiscovery", "Service removed: " + serviceName);
        
        // Callback
        if (onServiceRemoved_) {
            onServiceRemoved_(serviceName);
        }
    }
}

} // namespace midiMind
