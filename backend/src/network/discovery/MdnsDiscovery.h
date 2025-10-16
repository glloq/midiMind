// ============================================================================
// Fichier: backend/src/network/discovery/MdnsDiscovery.h
// Version: 1.0.0
// Date: 2025-10-15
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Header de la découverte mDNS/Bonjour pour services RTP-MIDI.
//   Utilise Avahi (Linux) pour découvrir automatiquement les périphériques
//   MIDI réseau compatibles Apple Network MIDI et autres implémentations.
//
// Fonctionnalités:
//   - Découverte automatique services _apple-midi._udp
//   - Surveillance continue avec callbacks
//   - Résolution DNS et cache des services
//   - Thread-safe avec mutex
//
// Dépendances:
//   - Avahi Client (libavahi-client-dev)
//   - Avahi Common (libavahi-common-dev)
//   - nlohmann/json
//
// Auteur: MidiMind Team
// Statut: ✅ COMPLET
// ============================================================================

#ifndef MIDIMIND_MDNS_DISCOVERY_H
#define MIDIMIND_MDNS_DISCOVERY_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @brief Informations sur un service découvert
 */
struct ServiceInfo {
    std::string id;           // Identifiant unique (name@address)
    std::string name;         // Nom du service
    std::string type;         // Type de service (_apple-midi._udp)
    std::string domain;       // Domaine (généralement "local")
    std::string hostname;     // Nom d'hôte
    std::string address;      // Adresse IP
    uint16_t port;           // Port
    std::chrono::system_clock::time_point discovered; // Horodatage de découverte
    
    // Constructeur par défaut
    ServiceInfo() : port(0) {}
};

// ============================================================================
// CALLBACKS
// ============================================================================

using ServiceDiscoveredCallback = std::function<void(const ServiceInfo&)>;
using ServiceRemovedCallback = std::function<void(const std::string& serviceId)>;

// ============================================================================
// CLASSE PRINCIPALE
// ============================================================================

/**
 * @brief Découverte de services MIDI via mDNS/Bonjour
 * 
 * Cette classe utilise Avahi pour découvrir automatiquement les services
 * RTP-MIDI sur le réseau local. Elle maintient un cache des services
 * découverts et notifie via callbacks lors des changements.
 * 
 * Thread Safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * Exemple d'utilisation:
 * @code
 *   MdnsDiscovery discovery;
 *   
 *   // Configurer les callbacks
 *   discovery.setOnServiceDiscovered([](const ServiceInfo& service) {
 *       std::cout << "Service trouvé: " << service.name << std::endl;
 *   });
 *   
 *   // Démarrer la découverte
 *   if (discovery.start()) {
 *       // Attendre et récupérer les services
 *       auto services = discovery.getDiscoveredServices();
 *   }
 *   
 *   // Arrêter proprement
 *   discovery.stop();
 * @endcode
 */
class MdnsDiscovery {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    MdnsDiscovery();
    
    /**
     * @brief Destructeur - Arrête la découverte si active
     */
    ~MdnsDiscovery();
    
    // Interdire la copie
    MdnsDiscovery(const MdnsDiscovery&) = delete;
    MdnsDiscovery& operator=(const MdnsDiscovery&) = delete;
    
    // ========================================================================
    // CONTRÔLE
    // ========================================================================
    
    /**
     * @brief Démarre la découverte mDNS
     * @return true si démarré avec succès, false sinon
     * 
     * Lance un thread dédié pour la découverte Avahi.
     * Nécessite que le daemon Avahi soit actif sur le système.
     */
    bool start();
    
    /**
     * @brief Arrête la découverte mDNS
     * 
     * Arrête le thread de découverte et libère les ressources Avahi.
     * Vide également le cache des services découverts.
     */
    void stop();
    
    /**
     * @brief Vérifie si la découverte est active
     * @return true si la découverte est en cours
     */
    bool isRunning() const;
    
    // ========================================================================
    // SERVICES
    // ========================================================================
    
    /**
     * @brief Récupère tous les services découverts
     * @return Vecteur de ServiceInfo
     */
    std::vector<ServiceInfo> getDiscoveredServices() const;
    
    /**
     * @brief Récupère un service par son ID
     * @param id Identifiant unique du service
     * @return ServiceInfo si trouvé, std::nullopt sinon
     */
    std::optional<ServiceInfo> getServiceById(const std::string& id) const;
    
    /**
     * @brief Récupère les services d'un type spécifique
     * @param type Type de service (ex: "_apple-midi._udp")
     * @return Vecteur de ServiceInfo correspondants
     */
    std::vector<ServiceInfo> getServicesByType(const std::string& type) const;
    
    /**
     * @brief Vide le cache des services découverts
     */
    void clearDiscoveredServices();
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback appelé lors de la découverte d'un service
     * @param callback Fonction appelée avec le ServiceInfo du nouveau service
     */
    void setOnServiceDiscovered(ServiceDiscoveredCallback callback);
    
    /**
     * @brief Définit le callback appelé lors de la suppression d'un service
     * @param callback Fonction appelée avec l'ID du service supprimé
     */
    void setOnServiceRemoved(ServiceRemovedCallback callback);
    
    // ========================================================================
    // STATUS
    // ========================================================================
    
    /**
     * @brief Récupère le status complet au format JSON
     * @return JSON contenant l'état et la liste des services
     * 
     * Format:
     * {
     *   "running": bool,
     *   "services_count": int,
     *   "services": [
     *     {
     *       "id": string,
     *       "name": string,
     *       "type": string,
     *       "address": string,
     *       "port": int,
     *       "hostname": string
     *     },
     *     ...
     *   ]
     * }
     */
    json getStatus() const;
    
    // ========================================================================
    // MÉTHODES PUBLIQUES (POUR CALLBACKS AVAHI)
    // ========================================================================
    
    /**
     * @brief Ajoute un service découvert au cache
     * @param service ServiceInfo à ajouter
     * 
     * Note: Méthode publique car appelée par les callbacks Avahi C.
     * Met à jour le service s'il existe déjà, sinon l'ajoute.
     */
    void addDiscoveredService(const ServiceInfo& service);
    
    /**
     * @brief Retire un service du cache
     * @param name Nom du service à retirer
     * 
     * Note: Méthode publique car appelée par les callbacks Avahi C.
     */
    void removeService(const std::string& name);
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Vérifie si les dépendances Avahi sont installées
     * @return true si Avahi est disponible et le daemon actif
     */
    static bool areDependenciesInstalled();

private:
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    bool running_;                              // État de la découverte
    void* avahiContext_;                        // Contexte Avahi (AvahiContext*)
    std::thread discoveryThread_;               // Thread de découverte
    mutable std::mutex mutex_;                  // Mutex pour thread-safety
    std::vector<ServiceInfo> discoveredServices_; // Cache des services
    ServiceDiscoveredCallback onServiceDiscovered_; // Callback découverte
    ServiceRemovedCallback onServiceRemoved_;   // Callback suppression
};

} // namespace midiMind

#endif // MIDIMIND_MDNS_DISCOVERY_H

// ============================================================================
// FIN DU FICHIER MdnsDiscovery.h v1.0.0
// ============================================================================