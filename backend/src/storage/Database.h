// ============================================================================
// File: backend/src/storage/Database.h
// Version: 4.1.1 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   High-performance SQLite3 wrapper with modern C++ API.
//   Handles all database operations with thread-safety and automatic migrations.
//
// Features:
//   - Connection management (open/close)
//   - Parameterized queries (SQL injection protection)
//   - Transactions (ACID compliant)
//   - Automatic schema migrations
//   - Backup and optimization
//   - Thread-safe operations
//
// Dependencies:
//   - SQLite3 (libsqlite3)
//   - Logger
//   - Error handling
//
// Author: MidiMind Team
// Date: 2025-10-17
//
// Changes v4.1.1:
//   - Fixed: queryScalar, getTables, getSchemaVersion now const
//
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include "../core/Logger.h"
#include "../core/Error.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

/**
 * @brief Database row as key-value map
 */
using DatabaseRow = std::map<std::string, std::string>;

/**
 * @brief Query result structure
 */
struct DatabaseResult {
    bool success = true;
    std::string error;
    std::vector<DatabaseRow> rows;
    int affectedRows = 0;
    int64_t lastInsertId = 0;
    
    /**
     * @brief Check if result is empty
     */
    bool empty() const { return rows.empty(); }
    
    /**
     * @brief Get number of rows
     */
    size_t size() const { return rows.size(); }
    
    /**
     * @brief Convert to JSON array
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

// ============================================================================
// CLASS: Database
// ============================================================================

/**
 * @class Database
 * @brief Thread-safe SQLite3 wrapper with automatic migrations
 * 
 * Provides a modern C++ interface to SQLite3 with:
 * - Automatic connection management
 * - Prepared statements for security
 * - Transaction support
 * - Schema migration system
 * - Backup and maintenance
 * 
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses internal mutex for synchronization
 * - Can be safely called from multiple threads
 * 
 * Example:
 * ```cpp
 * Database& db = Database::instance();
 * 
 * if (!db.connect("/path/to/db.sqlite")) {
 *     Logger::error("DB", "Connection failed");
 *     return false;
 * }
 * 
 * // Execute query
 * db.execute("INSERT INTO users (name) VALUES (?)", {"John"});
 * 
 * // Select query
 * auto result = db.query("SELECT * FROM users WHERE name = ?", {"John"});
 * for (const auto& row : result.rows) {
 *     std::cout << row.at("name") << std::endl;
 * }
 * 
 * // Transaction
 * db.transaction([&]() {
 *     db.execute("INSERT INTO table1 ...");
 *     db.execute("INSERT INTO table2 ...");
 * });
 * ```
 */
class Database {
public:
    // ========================================================================
    // SINGLETON PATTERN
    // ========================================================================
    
    /**
     * @brief Get singleton instance
     * @return Reference to Database instance
     */
    static Database& instance();
    
    // Disable copy and assignment
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    
    /**
     * @brief Destructor - closes database connection
     */
    ~Database();
    
    // ========================================================================
    // CONNECTION MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Connect to database file
     * @param filepath Path to SQLite database file
     * @return true if connection successful
     * @note Creates database file if it doesn't exist
     * @note Thread-safe
     */
    bool connect(const std::string& filepath);
    
    /**
     * @brief Close database connection
     * @note Thread-safe
     * @note Automatically called by destructor
     */
    void close();
    
    /**
     * @brief Check if database is connected
     * @return true if connected
     * @note Thread-safe
     */
    bool isConnected() const { return isConnected_; }
    
    /**
     * @brief Get database file path
     * @return Database file path
     */
    std::string getPath() const { return filepath_; }
    
    // ========================================================================
    // QUERY EXECUTION
    // ========================================================================
    
    /**
     * @brief Execute SQL statement (INSERT, UPDATE, DELETE)
     * @param sql SQL statement with ? placeholders
     * @param params Parameter values (optional)
     * @return DatabaseResult with affected rows and last insert ID
     * @throws std::runtime_error on SQL error
     * @note Thread-safe
     * 
     * Example:
     * ```cpp
     * db.execute("INSERT INTO users (name, email) VALUES (?, ?)",
     *            {"John", "john@example.com"});
     * ```
     */
    DatabaseResult execute(const std::string& sql,
                          const std::vector<std::string>& params = {});
    
    /**
     * @brief Execute SELECT query
     * @param sql SQL SELECT statement with ? placeholders
     * @param params Parameter values (optional)
     * @return DatabaseResult with rows
     * @throws std::runtime_error on SQL error
     * @note Thread-safe
     * 
     * Example:
     * ```cpp
     * auto result = db.query("SELECT * FROM users WHERE age > ?", {"18"});
     * for (const auto& row : result.rows) {
     *     std::cout << row.at("name") << std::endl;
     * }
     * ```
     */
    DatabaseResult query(const std::string& sql,
                        const std::vector<std::string>& params = {});
    
    /**
     * @brief Query single scalar value
     * @param sql SQL query returning single value
     * @param params Parameter values (optional)
     * @return String value (empty if no result)
     * @note Thread-safe
     * @note FIXED: Now const
     * 
     * Example:
     * ```cpp
     * auto count = db.queryScalar("SELECT COUNT(*) FROM users");
     * ```
     */
    std::string queryScalar(const std::string& sql,
                           const std::vector<std::string>& params = {}) const;
    
    // ========================================================================
    // TRANSACTION SUPPORT
    // ========================================================================
    
    /**
     * @brief Execute function within transaction
     * @param func Function to execute
     * @return true if transaction committed successfully
     * @note Thread-safe
     * @note Automatically rolls back on exception
     * 
     * Example:
     * ```cpp
     * bool success = db.transaction([&]() {
     *     db.execute("INSERT INTO table1 ...");
     *     db.execute("INSERT INTO table2 ...");
     * });
     * ```
     */
    bool transaction(std::function<void()> func);
    
    /**
     * @brief Begin transaction manually
     * @note Thread-safe
     * @note Use transaction() method instead for automatic rollback
     */
    void beginTransaction();
    
    /**
     * @brief Commit transaction
     * @note Thread-safe
     */
    void commit();
    
    /**
     * @brief Rollback transaction
     * @note Thread-safe
     */
    void rollback();
    
    // ========================================================================
    // SCHEMA MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Run database migrations
     * @param migrationDir Directory containing .sql migration files
     * @return true if migrations successful
     * @note Thread-safe
     * @note Migration files must be named: 001_description.sql, 002_...
     */
    bool runMigrations(const std::string& migrationDir);
    
    /**
     * @brief Get current schema version
     * @return Schema version number
     * @note Thread-safe
     * @note FIXED: Now const
     */
    int getSchemaVersion() const;
    
    /**
     * @brief Check if table exists
     * @param tableName Table name
     * @return true if table exists
     * @note Thread-safe
     */
    bool tableExists(const std::string& tableName) const;
    
    /**
     * @brief Get list of all tables
     * @return Vector of table names
     * @note Thread-safe
     * @note FIXED: Now const
     */
    std::vector<std::string> getTables() const;
    
    /**
     * @brief Truncate table (delete all rows)
     * @param tableName Table name
     * @note Thread-safe
     */
    void truncateTable(const std::string& tableName);
    
    /**
     * @brief Optimize database (VACUUM)
     * @note Thread-safe
     * @note Reclaims unused space and defragments
     */
    void optimize();
    
    /**
     * @brief Create database backup
     * @param backupPath Path for backup file
     * @return true if backup successful
     * @note Thread-safe
     */
    bool backup(const std::string& backupPath);
    
    /**
     * @brief Get database statistics
     * @return JSON with statistics
     * @note Thread-safe
     * 
     * Returns:
     * - file_size: Database file size in bytes
     * - page_count: Number of pages
     * - page_size: Page size in bytes
     * - table_count: Number of tables
     * - index_count: Number of indexes
     * - query_count: Number of queries executed
     * - error_count: Number of errors
     */
    json getStatistics() const;
    
private:
    // ========================================================================
    // PRIVATE CONSTRUCTOR (SINGLETON)
    // ========================================================================
    
    Database();
    
    // ========================================================================
    // PRIVATE HELPERS
    // ========================================================================
    
    /**
     * @brief Bind parameters to prepared statement
     */
    void bindParameters(sqlite3_stmt* stmt, 
                       const std::vector<std::string>& params);
    
    /**
     * @brief Execute prepared statement
     */
    DatabaseResult executeStatement(const std::string& sql,
                                   const std::vector<std::string>& params,
                                   bool isQuery);
    
    /**
     * @brief Get migration files sorted by version
     */
    std::vector<std::string> getMigrationFiles(const std::string& dir);
    
    /**
     * @brief Execute migration file
     */
    bool executeMigration(const std::string& filepath, int version);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    mutable std::mutex mutex_;         ///< Thread synchronization
    sqlite3* db_ = nullptr;            ///< SQLite database handle
    std::string filepath_;             ///< Database file path
    bool isConnected_ = false;         ///< Connection status
    
    // Statistics
    mutable uint64_t queryCount_ = 0;  ///< Total queries executed
    mutable uint64_t errorCount_ = 0;  ///< Total errors encountered
};

} // namespace midiMind

