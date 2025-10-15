// ============================================================================
// Fichier: backend/src/storage/Settings.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.1 - HEADER COMPLET
// Date: 2025-10-15
// ============================================================================
// Description:
//   Gestion des paramètres persistants de l'application.
//   Stocke et récupère les settings dans la base de données.
//
// CORRECTIONS v3.0.1:
//   ✅ Ajout de toutes les déclarations de méthodes
//   ✅ Ajout de méthodes utilitaires (has, remove, getAllKeys, count)
//   ✅ Ajout de méthodes export/import JSON
//   ✅ Documentation complète
//
// Responsabilités:
//   - Sauvegarde/chargement settings en base de données
//   - Validation des valeurs
//   - Settings par défaut
//   - Cache en mémoire pour performances
//   - Export/import JSON
//
// Thread-safety: OUI (std::mutex)
//
// Auteur: MidiMind Team
// Date: 2025-10-15
// ============================================================================

#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>

#include "Database.h"
#include "../core/Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
 * - Float/Double
 * - Boolean
 * - JSON
 * 
 * Architecture:
 * - Cache en mémoire (std::map<string, string>)
 * - Persistence en base SQLite (table `settings`)
 * - Thread-safety avec std::mutex
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation basique
 * ```cpp
 * Settings settings(database);
 * 
 * // Définir une valeur
 * settings.set("midi.default_channel", 1);
 * settings.set("audio.sample_rate", 48000);
 * settings.set("ui.theme", "dark");
 * 
 * // Récupérer une valeur
 * int channel = settings.getInt("midi.default_channel", 1);
 * std::string theme = settings.getString("ui.theme", "light");
 * 
 * // Sauvegarder
 * settings.save();
 * ```
 * 
 * @example Export/Import
 * ```cpp
 * // Export vers JSON
 * json exportData = settings.toJson();
 * std::cout << exportData.dump(2) << std::endl;
 * 
 * // Import depuis JSON
 * settings.fromJson(importData);
 * settings.save();
 * ```
 */
class Settings {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param database Base de données pour persistence
     * 
     * Initialise le gestionnaire de settings avec une base de données.
     * Charge automatiquement les settings depuis la BDD.
     */
    explicit Settings(std::shared_ptr<Database> database);
    
    /**
     * @brief Destructeur
     * 
     * Sauvegarde automatiquement les settings avant destruction.
     */
    ~Settings();
    
    // Désactiver copie
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
    
    // ========================================================================
    // CHARGEMENT / SAUVEGARDE
    // ========================================================================
    
    /**
     * @brief Charge tous les settings depuis la base de données
     * 
     * Remplace le cache actuel par les valeurs de la BDD.
     * Les valeurs par défaut sont utilisées pour les clés manquantes.
     * 
     * @note Thread-safe
     * @note Appelle automatiquement loadFromDatabase()
     */
    void load();
    
    /**
     * @brief Sauvegarde tous les settings dans la base de données
     * 
     * Écrit tous les paramètres du cache dans la table `settings`.
     * Utilise une transaction pour garantir l'atomicité.
     * 
     * @note Thread-safe
     * @note Appelé automatiquement au destructeur
     * 
     * @example
     * ```cpp
     * settings.set("midi.channel", 5);
     * settings.save();  // Persistence immédiate
     * ```
     */
    void save();
    
    /**
     * @brief Réinitialise tous les settings aux valeurs par défaut
     * 
     * Vide le cache, réinitialise les valeurs par défaut,
     * et sauvegarde dans la BDD.
     * 
     * @note Thread-safe
     * @warning Cette opération est irréversible
     * 
     * @example
     * ```cpp
     * settings.reset();  // Retour aux valeurs par défaut
     * ```
     */
    void reset();
    
    // ========================================================================
    // GETTERS TYPÉS
    // ========================================================================
    
    /**
     * @brief Récupère une valeur string
     * 
     * @param key Clé du paramètre
     * @param defaultValue Valeur par défaut si clé absente
     * @return std::string Valeur du paramètre
     * 
     * @note Thread-safe
     * 
     * @example
     * ```cpp
     * std::string theme = settings.getString("ui.theme", "dark");
     * ```
     */
    std::string getString(const std::string& key, 
                         const std::string& defaultValue = "");
    
    /**
     * @brief Récupère une valeur entière
     * 
     * @param key Clé du paramètre
     * @param defaultValue Valeur par défaut si clé absente
     * @return int Valeur du paramètre
     * 
     * @note Thread-safe
     * @note Conversion automatique avec std::stoi
     * @note Retourne defaultValue si conversion échoue
     * 
     * @example
     * ```cpp
     * int channel = settings.getInt("midi.default_channel", 1);
     * ```
     */
    int getInt(const std::string& key, int defaultValue = 0);
    
    /**
     * @brief Récupère une valeur booléenne
     * 
     * @param key Clé du paramètre
     * @param defaultValue Valeur par défaut si clé absente
     * @return bool Valeur du paramètre
     * 
     * @note Thread-safe
     * @note Supporte: "true"/"false", "1"/"0", "yes"/"no", "on"/"off"
     * @note Retourne defaultValue si format invalide
     * 
     * @example
     * ```cpp
     * bool autoSave = settings.getBool("ui.auto_save", true);
     * ```
     */
    bool getBool(const std::string& key, bool defaultValue = false);
    
    /**
     * @brief Récupère une valeur double
     * 
     * @param key Clé du paramètre
     * @param defaultValue Valeur par défaut si clé absente
     * @return double Valeur du paramètre
     * 
     * @note Thread-safe
     * @note Conversion automatique avec std::stod
     * @note Retourne defaultValue si conversion échoue
     * 
     * @example
     * ```cpp
     * double volume = settings.getDouble("audio.volume", 0.75);
     * ```
     */
    double getDouble(const std::string& key, double defaultValue = 0.0);
    
    /**
     * @brief Récupère une valeur JSON
     * 
     * @param key Clé du paramètre
     * @param defaultValue Valeur par défaut si clé absente
     * @return json Valeur du paramètre
     * 
     * @note Thread-safe
     * @note Parse automatiquement la string en JSON
     * @note Retourne defaultValue si parsing échoue
     * 
     * @example
     * ```cpp
     * json routing = settings.getJson("midi.routing", json::array());
     * ```
     */
    json getJson(const std::string& key, const json& defaultValue = json::object());
    
    // ========================================================================
    // SETTERS
    // ========================================================================
    
    /**
     * @brief Définit une valeur string
     * 
     * @param key Clé du paramètre
     * @param value Nouvelle valeur
     * 
     * @note Thread-safe
     * @note Ne sauvegarde PAS immédiatement (appeler save() pour persister)
     * 
     * @example
     * ```cpp
     * settings.set("ui.theme", "dark");
     * settings.save();
     * ```
     */
    void set(const std::string& key, const std::string& value);
    
    /**
     * @brief Définit une valeur entière
     * 
     * @param key Clé du paramètre
     * @param value Nouvelle valeur
     * 
     * @note Thread-safe
     * @note Convertit automatiquement en string
     */
    void set(const std::string& key, int value);
    
    /**
     * @brief Définit une valeur booléenne
     * 
     * @param key Clé du paramètre
     * @param value Nouvelle valeur
     * 
     * @note Thread-safe
     * @note Stocké comme "true" ou "false"
     */
    void set(const std::string& key, bool value);
    
    /**
     * @brief Définit une valeur double
     * 
     * @param key Clé du paramètre
     * @param value Nouvelle valeur
     * 
     * @note Thread-safe
     * @note Convertit automatiquement en string
     */
    void set(const std::string& key, double value);
    
    /**
     * @brief Définit une valeur JSON
     * 
     * @param key Clé du paramètre
     * @param value Nouvelle valeur JSON
     * 
     * @note Thread-safe
     * @note Sérialise automatiquement en string JSON
     */
    void set(const std::string& key, const json& value);
    
    // ========================================================================
    // MÉTHODES UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Vérifie si une clé existe
     * 
     * @param key Clé à vérifier
     * @return true Si la clé existe dans le cache
     * 
     * @note Thread-safe
     * 
     * @example
     * ```cpp
     * if (settings.has("midi.custom_config")) {
     *     // Utiliser la config custom
     * }
     * ```
     */
    bool has(const std::string& key) const;
    
    /**
     * @brief Supprime une clé
     * 
     * @param key Clé à supprimer
     * 
     * Supprime la clé du cache ET de la base de données.
     * 
     * @note Thread-safe
     * 
     * @example
     * ```cpp
     * settings.remove("midi.old_param");
     * ```
     */
    void remove(const std::string& key);
    
    /**
     * @brief Récupère toutes les clés
     * 
     * @return std::vector<std::string> Liste de toutes les clés
     * 
     * @note Thread-safe
     * 
     * @example
     * ```cpp
     * auto keys = settings.getAllKeys();
     * for (const auto& key : keys) {
     *     std::cout << key << " = " << settings.getString(key) << std::endl;
     * }
     * ```
     */
    std::vector<std::string> getAllKeys() const;
    
    /**
     * @brief Compte le nombre de paramètres
     * 
     * @return size_t Nombre de paramètres dans le cache
     * 
     * @note Thread-safe
     */
    size_t count() const;
    
    // ========================================================================
    // EXPORT / IMPORT
    // ========================================================================
    
    /**
     * @brief Exporte tous les settings en JSON
     * 
     * @return json Objet JSON contenant tous les paramètres
     * 
     * @note Thread-safe
     * @note Tente de parser les valeurs JSON, sinon garde comme string
     * 
     * @example
     * ```cpp
     * json exportData = settings.toJson();
     * std::ofstream file("settings_backup.json");
     * file << exportData.dump(2);
     * ```
     */
    json toJson() const;
    
    /**
     * @brief Importe des settings depuis JSON
     * 
     * @param j Objet JSON contenant les paramètres
     * 
     * Remplace les valeurs actuelles par celles du JSON.
     * N'appelle PAS save() automatiquement.
     * 
     * @note Thread-safe
     * @note Appeler save() après pour persister
     * 
     * @example
     * ```cpp
     * std::ifstream file("settings_backup.json");
     * json importData;
     * file >> importData;
     * settings.fromJson(importData);
     * settings.save();
     * ```
     */
    void fromJson(const json& j);

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Initialise les valeurs par défaut
     * 
     * Définit toutes les valeurs par défaut de l'application.
     * Appelé au constructeur et lors du reset().
     */
    void initializeDefaults();
    
    /**
     * @brief Charge depuis la base de données (implémentation)
     * 
     * Lit la table `settings` et peuple le cache.
     * Appelé par load() avec le mutex déjà verrouillé.
     */
    void loadFromDatabase();
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /**
     * @brief Base de données pour persistence
     */
    std::shared_ptr<Database> database_;
    
    /**
     * @brief Cache en mémoire (key → value string)
     * 
     * Toutes les valeurs sont stockées comme strings
     * et converties à la demande par les getters typés.
     */
    std::map<std::string, std::string> cache_;
    
    /**
     * @brief Mutex pour thread-safety
     * 
     * Protège l'accès au cache_ et à la base de données.
     * Mutable pour permettre le lock dans les méthodes const.
     */
    mutable std::mutex mutex_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Settings.h v3.0.1 - HEADER COMPLET
// ============================================================================