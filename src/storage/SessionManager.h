// ============================================================================
// Fichier: src/storage/SessionManager.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestion des sessions (configurations complètes de l'application).
//   Sauvegarde/chargement de l'état complet du système.
//
// Responsabilités:
//   - Créer/charger/supprimer sessions
//   - Sauvegarder état complet
//   - Auto-save périodique
//   - Import/export JSON
//
// Thread-safety: OUI
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <vector>

#include "Database.h"
#include "../core/Logger.h"
#include "../core/Error.h"

namespace midiMind {

/**
 * @struct Session
 * @brief Une session complète
 */
struct Session {
    int id;                     ///< ID unique
    std::string name;           ///< Nom de la session
    json data;                  ///< Données (configuration complète)
    std::string createdAt;      ///< Date de création
    std::string updatedAt;      ///< Date de mise à jour
    
    Session() : id(0) {}
    
    json toJson() const {
        json j;
        j["id"] = id;
        j["name"] = name;
        j["data"] = data;
        j["created_at"] = createdAt;
        j["updated_at"] = updatedAt;
        return j;
    }
    
    static Session fromJson(const json& j) {
        Session session;
        session.id = j.value("id", 0);
        session.name = j.value("name", "");
        session.data = j.value("data", json::object());
        session.createdAt = j.value("created_at", "");
        session.updatedAt = j.value("updated_at", "");
        return session;
    }
};

/**
 * @class SessionManager
 * @brief Gestionnaire de sessions
 * 
 * @details
 * Gère les sessions (snapshots complets de la configuration).
 * 
 * Une session contient:
 * - Tous les settings
 * - Configuration MIDI (routes, devices)
 * - Presets actifs
 * - État des processors
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * SessionManager manager(database);
 * 
 * // Créer une session
 * json sessionData;
 * sessionData["midi_routes"] = {...};
 * sessionData["presets"] = {...};
 * 
 * int sessionId = manager.create("My Session", sessionData);
 * 
 * // Charger une session
 * auto session = manager.load(sessionId);
 * 
 * // Lister les sessions
 * auto sessions = manager.list();
 * ```
 */
class SessionManager {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param database Base de données
     */
    explicit SessionManager(std::shared_ptr<Database> database);
    
    /**
     * @brief Destructeur
     */
    ~SessionManager();
    
    // Désactiver copie
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    
    // ========================================================================
    // CRUD
    // ========================================================================
    
    /**
     * @brief Crée une nouvelle session
     * 
     * @param name Nom de la session
     * @param data Données de la session
     * @return int ID de la session créée
     * 
     * @throws MidiMindException Si erreur
     * 
     * @note Thread-safe
     */
    int create(const std::string& name, const json& data);
    
    /**
     * @brief Charge une session
     * 
     * @param id ID de la session
     * @return Session Session chargée
     * 
     * @throws MidiMindException Si session non trouvée
     * 
     * @note Thread-safe
     */
    Session load(int id);
    
    /**
     * @brief Met à jour une session
     * 
     * @param id ID de la session
     * @param name Nouveau nom (optionnel)
     * @param data Nouvelles données
     * 
     * @throws MidiMindException Si erreur
     * 
     * @note Thread-safe
     */
    void update(int id, const std::string& name, const json& data);
    
    /**
     * @brief Supprime une session
     * 
     * @param id ID de la session
     * 
     * @throws MidiMindException Si erreur
     * 
     * @note Thread-safe
     */
    void remove(int id);
    
    /**
     * @brief Liste toutes les sessions
     * 
     * @return std::vector<Session> Liste des sessions
     * 
     * @note Thread-safe
     */
    std::vector<Session> list();
    
    /**
     * @brief Vérifie si une session existe
     * 
     * @param id ID de la session
     * 
     * @note Thread-safe
     */
    bool exists(int id);
    
    // ========================================================================
    // IMPORT / EXPORT
    // ========================================================================
    
    /**
     * @brief Exporte une session en JSON
     * 
     * @param id ID de la session
     * @param filepath Chemin du fichier
     * @return true Si succès
     * 
     * @note Thread-safe
     */
    bool exportToFile(int id, const std::string& filepath);
    
    /**
     * @brief Importe une session depuis JSON
     * 
     * @param filepath Chemin du fichier
     * @param name Nom de la session importée
     * @return int ID de la session créée
     * 
     * @throws MidiMindException Si erreur
     * 
     * @note Thread-safe
     */
    int importFromFile(const std::string& filepath, const std::string& name);
    
    // ========================================================================
    // SESSION ACTIVE
    // ========================================================================
    
    /**
     * @brief Définit la session active
     * 
     * @param id ID de la session
     * 
     * @note Thread-safe
     */
    void setActive(int id);
    
    /**
     * @brief Récupère la session active
     * 
     * @return int ID de la session active (0 si aucune)
     * 
     * @note Thread-safe
     */
    int getActive() const;
    
    /**
     * @brief Récupère les données de la session active
     * 
     * @return json Données ou objet vide
     * 
     * @note Thread-safe
     */
    json getActiveData();
    
    // ========================================================================
    // AUTO-SAVE
    // ========================================================================
    
    /**
     * @brief Active/désactive l'auto-save
     * 
     * @param enabled true pour activer
     * @param intervalSec Intervalle en secondes
     * 
     * @note Thread-safe
     */
    void setAutoSave(bool enabled, uint32_t intervalSec = 300);
    
    /**
     * @brief Sauvegarde immédiate de la session active
     * 
     * @param data Données à sauvegarder
     * 
     * @note Thread-safe
     */
    void saveActive(const json& data);

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Thread d'auto-save
     */
    void autoSaveThread();
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Base de données
    std::shared_ptr<Database> database_;
    
    /// Session active
    std::atomic<int> activeSessionId_;
    
    /// Auto-save
    std::atomic<bool> autoSaveEnabled_;
    std::atomic<uint32_t> autoSaveInterval_;
    std::thread autoSaveThread_;
    std::atomic<bool> stopAutoSave_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SessionManager.h
// ============================================================================