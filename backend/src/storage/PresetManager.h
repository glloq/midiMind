// ============================================================================
// Fichier: backend/src/storage/PresetManager.h
// Version: 3.0.0
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestionnaire centralisé de presets de routage MIDI.
//   Gère la persistance en base de données SQLite et l'organisation des presets.
//
// Fonctionnalités:
//   - CRUD complet (create, load, update, delete, list)
//   - Recherche et filtrage par nom/catégorie
//   - Export/Import JSON
//   - Métadonnées (créé, modifié, catégorie)
//   - Thread-safe
//
// Dépendances:
//   - Database.cpp (persistance SQLite)
//   - Preset.h (classe preset individuel)
//   - Logger.h (logging)
//
// Auteur: MidiMind Team
// Date: 2025-10-13
// Statut: ✅ COMPLET
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>

#include "Database.h"
#include "../utils/Preset.h"
#include "../core/Logger.h"
#include "../core/Error.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct PresetMetadata
 * @brief Métadonnées d'un preset (sans les données complètes)
 * 
 * Utilisé pour lister les presets sans charger toutes les données.
 * Optimise les performances lors de l'affichage de listes.
 */
struct PresetMetadata {
    int id;                         ///< ID unique en base de données
    std::string name;               ///< Nom du preset
    std::string description;        ///< Description
    std::string category;           ///< Catégorie (ex: "Piano", "Drums", "Live")
    std::time_t createdAt;          ///< Date de création (timestamp)
    std::time_t modifiedAt;         ///< Date de dernière modification
    int entryCount;                 ///< Nombre d'entrées dans le preset
    
    /**
     * @brief Constructeur par défaut
     */
    PresetMetadata()
        : id(0)
        , createdAt(0)
        , modifiedAt(0)
        , entryCount(0) {}
    
    /**
     * @brief Conversion en JSON
     */
    json toJson() const {
        json j;
        j["id"] = id;
        j["name"] = name;
        j["description"] = description;
        j["category"] = category;
        j["created_at"] = createdAt;
        j["modified_at"] = modifiedAt;
        j["entry_count"] = entryCount;
        return j;
    }
};

/**
 * @struct PresetRecord
 * @brief Enregistrement complet d'un preset en base de données
 * 
 * Combine les métadonnées et le preset complet.
 */
struct PresetRecord {
    PresetMetadata metadata;        ///< Métadonnées
    Preset preset;                  ///< Preset complet
    
    /**
     * @brief Conversion en JSON
     */
    json toJson() const {
        json j = metadata.toJson();
        j["entries"] = json::array();
        
        for (const auto& entry : preset.getEntries()) {
            json e;
            e["channel"] = entry.channel;
            e["file_id"] = entry.fileId;
            e["device_name"] = entry.deviceName;
            e["offset_ms"] = entry.offsetMs;
            e["muted"] = entry.muted;
            e["solo"] = entry.solo;
            e["volume"] = entry.volume;
            j["entries"].push_back(e);
        }
        
        return j;
    }
};

// ============================================================================
// CLASSE: PresetManager
// ============================================================================

/**
 * @class PresetManager
 * @brief Gestionnaire centralisé de presets de routage MIDI
 * 
 * @details
 * Gère la persistance et l'organisation des presets de routage.
 * Tous les presets sont stockés en base de données SQLite.
 * Thread-safe pour utilisation multi-thread.
 * 
 * Architecture:
 * - PresetManager (ce fichier) : Gestion et persistance
 * - Preset (Preset.h) : Structure d'un preset individuel
 * - Database : Persistance SQLite
 * 
 * @example Utilisation basique
 * @code
 * auto db = std::make_shared<Database>("midimind.db");
 * db->open();
 * 
 * PresetManager manager(db);
 * 
 * // Créer un preset
 * Preset preset;
 * preset.setName("Piano Jazz");
 * preset.addEntry(0, "file_001", "Roland FP-30", 0, false, false, 1.0f);
 * 
 * int id = manager.create(preset, "Jazz", "Piano setup for jazz");
 * 
 * // Lister tous les presets
 * auto presets = manager.list();
 * for (const auto& meta : presets) {
 *     std::cout << meta.name << std::endl;
 * }
 * 
 * // Charger un preset
 * PresetRecord record = manager.load(id);
 * @endcode
 */
class PresetManager {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param database Pointeur partagé vers la base de données
     * 
     * @throws MidiMindException Si la base n'est pas ouverte
     * 
     * @note La base de données doit être ouverte avant de créer le manager
     */
    explicit PresetManager(std::shared_ptr<Database> database);
    
    /**
     * @brief Destructeur
     * 
     * Libère les ressources. Les presets restent en base de données.
     */
    ~PresetManager();
    
    // Désactiver copie
    PresetManager(const PresetManager&) = delete;
    PresetManager& operator=(const PresetManager&) = delete;
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    /**
     * @brief Initialise le schéma de base de données
     * 
     * Crée la table 'presets' si elle n'existe pas.
     * Appelé automatiquement au constructeur.
     * 
     * @return true Si succès
     * 
     * @note Thread-safe
     */
    bool initializeSchema();
    
    // ========================================================================
    // CRUD - CREATE
    // ========================================================================
    
    /**
     * @brief Crée un nouveau preset
     * 
     * @param preset Preset à sauvegarder
     * @param category Catégorie (optionnel)
     * @param description Description (optionnel)
     * @return int ID du preset créé (> 0)
     * 
     * @throws MidiMindException Si erreur base de données
     * 
     * @note Thread-safe
     * @note Le preset.name ne doit pas être vide
     * 
     * @example
     * @code
     * Preset p;
     * p.setName("My Preset");
     * int id = manager.create(p, "Live", "Setup for live performance");
     * @endcode
     */
    int create(const Preset& preset, 
              const std::string& category = "",
              const std::string& description = "");
    
    // ========================================================================
    // CRUD - READ
    // ========================================================================
    
    /**
     * @brief Charge un preset complet
     * 
     * @param id ID du preset
     * @return PresetRecord Enregistrement complet (métadonnées + preset)
     * 
     * @throws MidiMindException Si preset non trouvé
     * 
     * @note Thread-safe
     */
    PresetRecord load(int id);
    
    /**
     * @brief Charge uniquement les métadonnées
     * 
     * @param id ID du preset
     * @return std::optional<PresetMetadata> Métadonnées ou nullopt
     * 
     * @note Thread-safe
     * @note Plus rapide que load() car ne charge pas les entrées
     */
    std::optional<PresetMetadata> getMetadata(int id);
    
    /**
     * @brief Liste tous les presets (métadonnées seulement)
     * 
     * @return std::vector<PresetMetadata> Liste des presets
     * 
     * @note Thread-safe
     * @note Triés par date de modification (plus récent en premier)
     */
    std::vector<PresetMetadata> list();
    
    /**
     * @brief Liste les presets d'une catégorie
     * 
     * @param category Catégorie à filtrer
     * @return std::vector<PresetMetadata> Liste filtrée
     * 
     * @note Thread-safe
     */
    std::vector<PresetMetadata> listByCategory(const std::string& category);
    
    // ========================================================================
    // CRUD - UPDATE
    // ========================================================================
    
    /**
     * @brief Met à jour un preset existant
     * 
     * @param id ID du preset
     * @param preset Nouvelles données
     * @param category Nouvelle catégorie (optionnel)
     * @param description Nouvelle description (optionnel)
     * 
     * @throws MidiMindException Si preset non trouvé ou erreur BDD
     * 
     * @note Thread-safe
     * @note Met à jour automatiquement modified_at
     */
    void update(int id, 
               const Preset& preset,
               const std::string& category = "",
               const std::string& description = "");
    
    // ========================================================================
    // CRUD - DELETE
    // ========================================================================
    
    /**
     * @brief Supprime un preset
     * 
     * @param id ID du preset
     * 
     * @throws MidiMindException Si erreur base de données
     * 
     * @note Thread-safe
     * @note Ne génère pas d'erreur si le preset n'existe pas
     */
    void remove(int id);
    
    /**
     * @brief Supprime tous les presets (DANGER)
     * 
     * @throws MidiMindException Si erreur base de données
     * 
     * @note Thread-safe
     * @warning Opération irréversible !
     */
    void removeAll();
    
    // ========================================================================
    // RECHERCHE & FILTRAGE
    // ========================================================================
    
    /**
     * @brief Recherche par nom (LIKE)
     * 
     * @param query Terme de recherche
     * @return std::vector<PresetMetadata> Résultats
     * 
     * @note Thread-safe
     * @note Case-insensitive
     * @note Recherche dans name et description
     * 
     * @example
     * @code
     * auto results = manager.search("piano"); // Trouve "Piano Jazz", "Grand Piano", etc.
     * @endcode
     */
    std::vector<PresetMetadata> search(const std::string& query);
    
    /**
     * @brief Liste toutes les catégories utilisées
     * 
     * @return std::vector<std::string> Liste des catégories uniques
     * 
     * @note Thread-safe
     */
    std::vector<std::string> listCategories();
    
    /**
     * @brief Vérifie si un preset existe
     * 
     * @param id ID du preset
     * @return bool True si existe
     * 
     * @note Thread-safe
     */
    bool exists(int id);
    
    // ========================================================================
    // IMPORT/EXPORT
    // ========================================================================
    
    /**
     * @brief Exporte un preset en fichier JSON
     * 
     * @param id ID du preset
     * @param filepath Chemin du fichier destination
     * @return bool True si succès
     * 
     * @note Thread-safe
     * @note Utilise Preset::saveToFile() en interne
     */
    bool exportToFile(int id, const std::string& filepath);
    
    /**
     * @brief Importe un preset depuis fichier JSON
     * 
     * @param filepath Chemin du fichier source
     * @param category Catégorie du preset importé (optionnel)
     * @return int ID du preset créé
     * 
     * @throws MidiMindException Si erreur lecture ou format invalide
     * 
     * @note Thread-safe
     * @note Utilise Preset::loadFromFile() en interne
     */
    int importFromFile(const std::string& filepath, 
                       const std::string& category = "Imported");
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Compte total de presets
     * 
     * @return int Nombre de presets
     * 
     * @note Thread-safe
     */
    int count() const;
    
    /**
     * @brief Statistiques détaillées
     * 
     * @return json Stats JSON
     * 
     * Format:
     * @code{.json}
     * {
     *   "total_presets": 42,
     *   "categories": ["Piano", "Drums", "Synth"],
     *   "total_entries": 156,
     *   "average_entries_per_preset": 3.7
     * }
     * @endcode
     * 
     * @note Thread-safe
     */
    json getStatistics() const;

private:
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Base de données
    std::shared_ptr<Database> database_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Sérialise un preset en JSON pour stockage BDD
     * 
     * @param preset Preset à sérialiser
     * @return std::string JSON string
     */
    std::string serializePreset(const Preset& preset) const;
    
    /**
     * @brief Désérialise un preset depuis JSON
     * 
     * @param data JSON string
     * @return Preset Preset désérialisé
     * 
     * @throws MidiMindException Si format invalide
     */
    Preset deserializePreset(const std::string& data) const;
    
    /**
     * @brief Parse une row SQL en PresetMetadata
     * 
     * @param row Row SQL (map<string, string>)
     * @return PresetMetadata Métadonnées parsées
     */
    PresetMetadata parseMetadata(const DatabaseRow& row) const;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER PresetManager.h
// ============================================================================
