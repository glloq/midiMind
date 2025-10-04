// ============================================================================
// Fichier: src/network/discovery/MdnsDiscovery.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Découverte automatique de services réseau via mDNS/Zeroconf (Avahi).
//   Permet de découvrir automatiquement les périphériques RTP-MIDI sur
//   le réseau local et d'annoncer notre propre service.
//
// Responsabilités:
//   - Découvrir les services RTP-MIDI (_apple-midi._udp)
//   - Annoncer le service MidiMind
//   - Résoudre les adresses IP des services découverts
//   - Notifier les callbacks lors de découvertes
//
// Thread-safety: OUI
//
// Dépendances: Avahi (libavahi-client)
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <thread>

#include "../../core/Logger.h"
#include "ServiceInfo.h"

namespace midiMind {

/**
 * @class MdnsDiscovery
 * @brief Découverte mDNS/Zeroconf
 * 
 * @details
 * Utilise Avahi (implémentation Linux de Zeroconf) pour:
 * - Découvrir les services RTP-MIDI sur le réseau local
 * - Annoncer notre propre service MidiMind
 * - Résoudre automatiquement les adresses IP
 * 
 * Services recherchés:
 * - _apple-midi._udp : Protocole RTP-MIDI d'Apple
 * - _midi._udp : MIDI générique sur UDP
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @note Nécessite Avahi daemon (avahi-daemon) actif sur le système
 * 
 * @example Utilisation
 * ```cpp
 * MdnsDiscovery discovery;
 * 
 * discovery.setOnServiceDiscovered([](const ServiceInfo& info) {
 *     Logger::info("mDNS", "Found: " + info.name + " at " + info.address);
 * });
 * 
 * discovery.start();
 * discovery.browse("_apple-midi._udp");
 * ```
 */
class MdnsDiscovery {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé lors de la découverte d'un service
     */
    using ServiceDiscoveredCallback = std::function<void(const ServiceInfo&)>;
    
    /**
     * @brief Callback appelé quand un service disparaît
     */
    using ServiceRemovedCallback = std::function<void(const std::string& serviceName)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    MdnsDiscovery();
    
    /**
     * @brief Destructeur
     */
    ~MdnsDiscovery();
    
    // Désactiver copie
    MdnsDiscovery(const MdnsDiscovery&) = delete;
    MdnsDiscovery& operator=(const MdnsDiscovery&) = delete;
    
    // ========================================================================
    // CONTRÔLE
    // ========================================================================
    
    /**
     * @brief Démarre la découverte mDNS
     * 
     * @return true Si le démarrage a réussi
     * 
     * @note Thread-safe
     */
    bool start();
    
    /**
     * @brief Arrête la découverte
     * 
     * @note Thread-safe
     */
    void stop();
    
    /**
     * @brief Vérifie si la découverte est active
     * 
     * @return true Si actif
     * 
     * @note Thread-safe
     */
    bool isRunning() const;
    
    // ========================================================================
    // DÉCOUVERTE DE SERVICES
    // ========================================================================
    
    /**
     * @brief Lance la recherche d'un type de service
     * 
     * @param serviceType Type de service (ex: "_apple-midi._udp")
     * @param domain Domaine (défaut: "local.")
     * @return true Si la recherche a démarré
     * 
     * Types courants:
     * - "_apple-midi._udp" : RTP-MIDI d'Apple
     * - "_midi._udp" : MIDI générique
     * - "_osc._udp" : Open Sound Control
     * 
     * @note Thread-safe. Peut être appelé plusieurs fois pour différents types.
     */
    bool browse(const std::string& serviceType, const std::string& domain = "local.");
    
    /**
     * @brief Arrête la recherche d'un type de service
     * 
     * @param serviceType Type de service
     * @return true Si arrêté avec succès
     * 
     * @note Thread-safe
     */
    bool stopBrowse(const std::string& serviceType);
    
    /**
     * @brief Liste tous les services découverts
     * 
     * @return std::vector<ServiceInfo> Liste des services
     * 
     * @note Thread-safe
     */
    std::vector<ServiceInfo> listServices() const;
    
    /**
     * @brief Récupère un service spécifique
     * 
     * @param serviceName Nom du service
     * @return std::optional<ServiceInfo> Info ou nullopt
     * 
     * @note Thread-safe
     */
    std::optional<ServiceInfo> getService(const std::string& serviceName) const;
    
    // ========================================================================
    // ANNONCE DE SERVICE
    // ========================================================================
    
    /**
     * @brief Annonce notre service MidiMind
     * 
     * @param serviceName Nom du service (ex: "MidiMind Studio")
     * @param port Port du service
     * @param serviceType Type (défaut: "_apple-midi._udp")
     * @return true Si l'annonce a réussi
     * 
     * @note Thread-safe
     * 
     * @example
     * ```cpp
     * discovery.publishService("MidiMind Studio", 5004, "_apple-midi._udp");
     * ```
     */
    bool publishService(const std::string& serviceName,
                       uint16_t port,
                       const std::string& serviceType = "_apple-midi._udp");
    
    /**
     * @brief Retire l'annonce de service
     * 
     * @return true Si retiré avec succès
     * 
     * @note Thread-safe
     */
    bool unpublishService();
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de découverte
     */
    void setOnServiceDiscovered(ServiceDiscoveredCallback callback);
    
    /**
     * @brief Définit le callback de suppression
     */
    void setOnServiceRemoved(ServiceRemovedCallback callback);
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Résout manuellement un nom d'hôte en adresse IP
     * 
     * @param hostname Nom d'hôte (ex: "raspberrypi.local")
     * @return std::string Adresse IP ou chaîne vide si échec
     * 
     * @note Bloquant. Utilisé pour résoudre des noms explicites.
     */
    std::string resolveHostname(const std::string& hostname) const;
    
    /**
     * @brief Vérifie si Avahi est disponible sur le système
     * 
     * @return true Si Avahi daemon est actif
     */
    static bool isAvahiAvailable();

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Thread principal de découverte
     */
    void discoveryLoop();
    
    /**
     * @brief Gère un service découvert
     */
    void handleServiceDiscovered(const ServiceInfo& info);
    
    /**
     * @brief Gère un service supprimé
     */
    void handleServiceRemoved(const std::string& serviceName);
    
    /**
     * @brief Parse les enregistrements TXT
     */
    std::vector<std::pair<std::string, std::string>> parseTxtRecords(const std::string& txt) const;
    
    // ========================================================================
    // MÉTHODES PRIVÉES - AVAHI (implémentation système-spécifique)
    // ========================================================================
    
    /**
     * @brief Initialise le client Avahi
     */
    bool initAvahi();
    
    /**
     * @brief Libère les ressources Avahi
     */
    void cleanupAvahi();
    
    /**
     * @brief Lance un browser Avahi
     */
    bool startAvahiBrowser(const std::string& serviceType, const std::string& domain);
    
    /**
     * @brief Publie via Avahi
     */
    bool publishAvahiService(const std::string& name, uint16_t port, const std::string& type);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// État
    std::atomic<bool> running_;
    
    /// Thread de découverte
    std::thread discoveryThread_;
    
    /// Services découverts
    std::vector<ServiceInfo> discoveredServices_;
    
    /// Callbacks
    ServiceDiscoveredCallback onServiceDiscovered_;
    ServiceRemovedCallback onServiceRemoved_;
    
    /// Handle Avahi (opaque pointer)
    void* avahiClient_;
    void* avahiPoll_;
    std::vector<void*> avahiBrowsers_;
    void* avahiGroup_;
    
    /// Configuration
    std::string publishedServiceName_;
    bool servicePublished_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MdnsDiscovery.h
// ============================================================================