// ============================================================================
// Fichier: src/storage/Settings.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestion des paramètres persistants de l'application.
//   Stocke et récupère les settings dans la base de données.
//
// Responsabilités:
//   - Sauvegarde/chargement settings
//   - Validation des valeurs
//   - Settings par défaut
//   - Callbacks de changement
//
// Thread-safety: OUI
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <functional>

#include "Database.h"
#include "../core/Logger.h"
#include "../core/Error.h"

namespace midiMind {

/**
 * @class Settings
 * @brief Gestionnaire de paramètres persistants
 * 
 * @details
 * Stocke les paramètres de configuration dans SQLite.
 * 
 * Types supportés:
 * - String
 * - Integer
 * - Float
 * - Boolean
 * - JSON
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * Settings settings(database);
 * 
 * // Définir une valeur
 * settings.set("midi.default_channel", 1);
 * settings.set("audio.sample_rate", 48000);
 * 
 * // Récupérer une valeur
 * int channel = settings.getInt("midi.default_channel", 1);
 * 
 * // Callback de changement
 * settings.setOnChanged("midi.default_channel", [](const std::string& value) {
 *     Logger::info("Channel changed to: " + value);
 * });
 * ```
 */
class Settings {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé lors d'un changement de valeur
     */
    using ChangeCallback = std::function<void(const std::string& key, const std::string& value)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param database Base de données
     */
    explicit Settings(std::shared_ptr<Database> database);
    
    /**
     * @brief Destructeur
     */
    ~Settings();
    
    // Désactiver copie
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
    
    // ========================================================================
    // CHARGEMENT / SAUVEGARDE
    // ========================================================================
    
    /**
     * @brief Charge tous les settings depuis la DB
     * 
     * @note Thread-safe
     */
    void load();
    
    /**
     * @brief Sauvegarde tous les settings dans la DB
     * 
     * @note Thread-safe
     */
    void save();
    
    /**
     * @brief Réinitialise aux valeurs par défaut
     * 
     * @note Thread-safe
     */
    void reset();
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    /**
     * @brief Récupère une valeur string
     * 
     * @param key Clé
     * @param defaultValue Valeur par défaut
     * @return std::string Valeur
     * 
     * @note Thread-safe
     */
    std::string getString(const std::string& key, const std::string& defaultValue = "");
    
    /**
     * @brief Récupère une valeur int
     * 
     * @note Thread-safe
     */
    int getInt(const std::string& key, int defaultValue = 0);
    
    /**
     * @brief Récupère une valeur float
     * 
     * @note Thread-safe
     */
    float getFloat(const std::string& key, float defaultValue = 0.0f);
    
    /**
     * @brief Récupère une valeur bool
     * 
     * @note Thread-safe
     */
    bool getBool(const std::string& key, bool defaultValue = false);
    
    /**
     * @brief Récupère une valeur JSON
     * 
     * @note Thread-safe
     */
    json getJson(const std::string& key, const json& defaultValue = json::object());
    
    // ========================================================================
    // SETTERS
    // ========================================================================
    
    /**
     * @brief Définit une valeur string
     * 
     * @note Thread-safe
     */
    void set(const std::string& key, const std::string& value);
    
    /**
     * @brief Définit une valeur int
     * 
     * @note Thread-safe
     */
    void set(const std::string& key, int value);
    
    /**
     * @brief Définit une valeur float
     * 
     * @note Thread-safe
     */
    void set(const std::string& key, float value);
    
    /**
     * @brief Définit une valeur bool
     * 
     * @note Thread-safe
     */
    void set(const std::string& key, bool value);
    
    /**
     * @brief Définit une valeur JSON
     * 
     * @note Thread-safe
     */
    void set(const std::string& key, const json& value);
    
    // ========================================================================
    // VÉRIFICATIONS
    // ========================================================================
    
    /**
     * @brief Vérifie si une clé existe
     * 
     * @note Thread-safe
     */
    bool has(const std::string& key) const;
    
    /**
     * @brief Supprime une clé
     * 
     * @note Thread-safe
     */
    void remove(const std::string& key);
    
    /**
     * @brief Récupère toutes les clés
     * 
     * @note Thread-safe
     */
    std::vector<std::string> getKeys() const;
    
    /**
     * @brief Récupère tous les settings
     * 
     * @return json Tous les settings
     * 
     * @note Thread-safe
     */
    json getAll() const;
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit un callback pour une clé
     * 
     * @param key Clé à surveiller
     * @param callback Callback
     * 
     * @note Thread-safe
     */
    void setOnChanged(const std::string& key, ChangeCallback callback);
    
    /**
     * @brief Retire un callback
     * 
     * @param key Clé
     * 
     * @note Thread-safe
     */
    void removeOnChanged(const std::string& key);

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Initialise les valeurs par défaut
     */
    void initializeDefaults();
    
    /**
     * @brief Notifie les callbacks
     */
    void notifyChanged(const std::string& key, const std::string& value);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Base de données
    std::shared_ptr<Database> database_;
    
    /// Cache des settings en mémoire
    std::map<std::string, std::string> cache_;
    
    /// Callbacks de changement
    std::map<std::string, ChangeCallback> callbacks_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Settings.h
// ============================================================================