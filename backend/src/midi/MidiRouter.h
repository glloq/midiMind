// ============================================================================
// Fichier: backend/src/midi/MidiRouter.h
// Version: 3.0.2 - AJOUT FORWARD DECLARATION
// ============================================================================

// CORRECTIFS APPLIQUÉS:
// - ✅ Ajout forward declaration class MidiRoute
// - ✅ Correction des types dans les méthodes
// ============================================================================

#pragma once

#include "MidiMessage.h"
#include "../core/Logger.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>

namespace midiMind {

using json = nlohmann::json;

// ✅ AJOUT: Forward declaration de MidiRoute
class MidiRoute;

/**
 * @class MidiRouter
 * @brief Routeur MIDI avec filtrage et transformation
 */
class MidiRouter {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    using RouteCallback = std::function<void(const MidiMessage&, const std::string& routeId)>;
    
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    MidiRouter();
    ~MidiRouter();
    
    // Non-copiable
    MidiRouter(const MidiRouter&) = delete;
    MidiRouter& operator=(const MidiRouter&) = delete;
    
    // ========================================================================
    // GESTION DES ROUTES
    // ========================================================================
    
    /**
     * @brief Ajoute une route
     * @param route Route à ajouter
     */
    void addRoute(std::shared_ptr<MidiRoute> route);
    
    /**
     * @brief Supprime une route par ID
     * @param id ID de la route
     * @return true si supprimée
     */
    bool removeRoute(const std::string& id);
    
    /**
     * @brief Supprime toutes les routes
     */
    void clearRoutes();
    
    /**
     * @brief Récupère une route par ID
     * @param id ID de la route
     * @return Route ou nullptr
     */
    std::shared_ptr<MidiRoute> getRoute(const std::string& id) const;
    
    /**
     * @brief Liste toutes les routes
     * @return Vecteur de routes
     */
    std::vector<std::shared_ptr<MidiRoute>> getRoutes() const;
    
    /**
     * @brief Active/désactive une route
     * @param id ID de la route
     * @param enabled État souhaité
     * @return true si succès
     */
    bool setRouteEnabled(const std::string& id, bool enabled);
    
    // ========================================================================
    // ROUTAGE
    // ========================================================================
    
    /**
     * @brief Route un message MIDI
     * @param message Message à router
     * @param sourceId ID de la source
     * @return Nombre de routes matchées
     */
    int routeMessage(const MidiMessage& message, const std::string& sourceId);
    
    /**
     * @brief Définit le callback de routage
     * @param callback Fonction appelée pour chaque message routé
     */
    void setRouteCallback(RouteCallback callback);
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques
     */
    json getStats() const;
    
    /**
     * @brief Réinitialise les statistiques
     */
    void resetStats();
    
    /**
     * @brief Nombre total de messages routés
     */
    uint64_t getTotalMessagesRouted() const;
    
    /**
     * @brief Nombre de messages filtrés
     */
    uint64_t getTotalMessagesFiltered() const;
    
    // ========================================================================
    // SÉRIALISATION
    // ========================================================================
    
    /**
     * @brief Exporte la configuration en JSON
     */
    json toJson() const;
    
    /**
     * @brief Charge la configuration depuis JSON
     */
    bool fromJson(const json& j);

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Vérifie si un message matche une route
     */
    bool matchesRoute(const MidiMessage& message, const MidiRoute& route) const;
    
    /**
     * @brief Applique les transformations d'une route
     */
    MidiMessage applyTransformations(const MidiMessage& message, const MidiRoute& route) const;
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Routes configurées
    std::vector<std::shared_ptr<MidiRoute>> routes_;
    
    /// Callback de routage
    RouteCallback routeCallback_;
    
    /// Statistiques
    uint64_t totalMessagesRouted_;
    uint64_t totalMessagesFiltered_;
    
    /// Stats par route
    std::map<std::string, uint64_t> routeStats_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiRouter.h
// ============================================================================
