// ============================================================================
// Fichier: src/storage/Database.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Wrapper SQLite pour la persistence des données.
//   Gère la base de données locale pour presets, sessions, historique.
//
// Responsabilités:
//   - Connexion à SQLite
//   - Exécution de requêtes
//   - Transactions
//   - Migration de schéma
//
// Thread-safety: OUI (mutex interne)
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <sqlite3.h>

#include "../core/Logger.h"
#include "../core/Error.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @struct DatabaseRow
 * @brief Une ligne de résultat de requête
 */
using DatabaseRow = std::map<std::string, std::string>;

/**
 * @struct DatabaseResult
 * @brief Résultat d'une requête SELECT
 */
struct DatabaseResult {
    std::vector<DatabaseRow> rows;      ///< Lignes de résultat
    int affectedRows;                   ///< Lignes affectées (INSERT/UPDATE/DELETE)
    int64_t lastInsertId;               ///< ID du dernier insert
    
    DatabaseResult() : affectedRows(0), lastInsertId(0) {}
    
    /**
     * @brief Vérifie si le résultat est vide
     */
    bool empty() const { return rows.empty(); }
    
    /**
     * @brief Récupère le nombre de lignes
     */
    size_t size() const { return rows.size(); }
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j = json::array();
        for (const auto& row : rows) {
            json rowJson;
            for (const auto& [key, value] : row) {
                rowJson[key] = value;
            }
            j.push_back(rowJson);
        }
        return j;
    }
};

/**
 * @class Database
 * @brief Wrapper SQLite haute performance
 * 
 * @details
 * Encapsule SQLite3 avec une API C++ moderne et thread-safe.
 * 
 * Fonctionnalités:
 * - Connexion/déconnexion
 * - Requêtes paramétrées
 * - Transactions
 * - Migration automatique de schéma
 * - Backup
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * Database db("midimind.db");
 * 
 * // Ouvrir
 * db.open();
 * 
 * // Créer une table
 * db.execute("CREATE TABLE presets (id INTEGER PRIMARY KEY, name TEXT)");
 * 
 * // Insérer
 * db.execute("INSERT INTO presets (name) VALUES (?)", {"My Preset"});
 * 
 * // Requête
 * auto result = db.query("SELECT * FROM presets");
 * ```
 */
class Database {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param filepath Chemin de la base de données
     */
    explicit Database(const std::string& filepath);
    
    /**
     * @brief Destructeur
     */
    ~Database();
    
    // Désactiver copie
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    
    // ========================================================================
    // CONNEXION
    // ========================================================================
    
    /**
     * @brief Ouvre la base de données
     * 
     * @return true Si succès
     * 
     * @note Thread-safe
     */
    bool open();
    
    /**
     * @brief Ferme la base de données
     * 
     * @note Thread-safe
     */
    void close();
    
    /**
     * @brief Vérifie si la base est ouverte
     * 
     * @note Thread-safe
     */
    bool isOpen() const;
    
    // ========================================================================
    // REQUÊTES
    // ========================================================================
    
    /**
     * @brief Exécute une requête SQL (INSERT/UPDATE/DELETE)
     * 
     * @param sql Requête SQL
     * @param params Paramètres (optionnels)
     * @return DatabaseResult Résultat
     * 
     * @throws MidiMindException Si erreur SQL
     * 
     * @note Thread-safe
     * 
     * @example
     * ```cpp
     * db.execute("INSERT INTO users (name, email) VALUES (?, ?)", 
     *            {"John", "john@example.com"});
     * ```
     */
    DatabaseResult execute(const std::string& sql, 
                          const std::vector<std::string>& params = {});
    
    /**
     * @brief Exécute une requête SELECT
     * 
     * @param sql Requête SQL
     * @param params Paramètres (optionnels)
     * @return DatabaseResult Résultat avec rows
     * 
     * @throws MidiMindException Si erreur SQL
     * 
     * @note Thread-safe
     */
    DatabaseResult query(const std::string& sql,
                        const std::vector<std::string>& params = {});
    
    /**
     * @brief Exécute une requête et retourne une seule ligne
     * 
     * @param sql Requête SQL
     * @param params Paramètres
     * @return DatabaseRow Première ligne ou vide
     * 
     * @note Thread-safe
     */
    DatabaseRow queryOne(const std::string& sql,
                        const std::vector<std::string>& params = {});
    
    /**
     * @brief Exécute une requête et retourne une valeur scalaire
     * 
     * @param sql Requête SQL
     * @param params Paramètres
     * @return std::string Valeur ou chaîne vide
     * 
     * @note Thread-safe
     */
    std::string queryScalar(const std::string& sql,
                           const std::vector<std::string>& params = {});
    
    // ========================================================================
    // TRANSACTIONS
    // ========================================================================
    
    /**
     * @brief Démarre une transaction
     * 
     * @note Thread-safe
     */
    void beginTransaction();
    
    /**
     * @brief Commit la transaction
     * 
     * @note Thread-safe
     */
    void commit();
    
    /**
     * @brief Rollback la transaction
     * 
     * @note Thread-safe
     */
    void rollback();
    
    /**
     * @brief Exécute une fonction dans une transaction
     * 
     * @param fn Fonction à exécuter
     * @return true Si succès (commit), false si erreur (rollback)
     * 
     * @note Thread-safe
     * 
     * @example
     * ```cpp
     * db.transaction([&]() {
     *     db.execute("INSERT INTO table1 ...");
     *     db.execute("INSERT INTO table2 ...");
     * });
     * ```
     */
    bool transaction(std::function<void()> fn);
    
    // ========================================================================
    // SCHÉMA
    // ========================================================================
    
    /**
     * @brief Initialise le schéma de base
     * 
     * Crée les tables nécessaires pour MidiMind.
     * 
     * @note Thread-safe
     */
    void initializeSchema();
    
    /**
     * @brief Migre le schéma vers une version
     * 
     * @param version Version cible
     * @return true Si succès
     * 
     * @note Thread-safe
     */
    bool migrate(int version);
    
    /**
     * @brief Récupère la version actuelle du schéma
     * 
     * @note Thread-safe
     */
    int getSchemaVersion();
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Vérifie si une table existe
     * 
     * @param tableName Nom de la table
     * 
     * @note Thread-safe
     */
    bool tableExists(const std::string& tableName);
    
    /**
     * @brief Récupère la liste des tables
     * 
     * @note Thread-safe
     */
    std::vector<std::string> getTables();
    
    /**
     * @brief Vide une table
     * 
     * @param tableName Nom de la table
     * 
     * @note Thread-safe
     */
    void truncateTable(const std::string& tableName);
    
    /**
     * @brief Optimise la base de données (VACUUM)
     * 
     * @note Thread-safe
     */
    void optimize();
    
    /**
     * @brief Sauvegarde la base de données
     * 
     * @param backupPath Chemin de backup
     * @return true Si succès
     * 
     * @note Thread-safe
     */
    bool backup(const std::string& backupPath);
    
    /**
     * @brief Récupère les statistiques
     * 
     * @note Thread-safe
     */
    json getStatistics() const;

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    void executeMigrationFile(const std::string& filepath);
    /**
     * @brief Prépare une requête
     */
    sqlite3_stmt* prepareStatement(const std::string& sql);
    
    /**
     * @brief Bind les paramètres
     */
    void bindParameters(sqlite3_stmt* stmt, const std::vector<std::string>& params);
    
    /**
     * @brief Récupère le message d'erreur SQLite
     */
    std::string getErrorMessage() const;
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Chemin de la base de données
    std::string filepath_;
    
    /// Handle SQLite
    sqlite3* db_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Flag d'ouverture
    std::atomic<bool> isOpen_;
    
    /// Compteur de transactions
    int transactionDepth_;
    
    /// Statistiques
    mutable uint64_t queryCount_;
    mutable uint64_t errorCount_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Database.h
// ============================================================================