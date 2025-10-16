// ============================================================================
// File: backend/src/storage/Database.h
// Version: 4.1.0
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
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Singleton pattern for global access
//   - Enhanced migration system
//   - Improved error handling
//   - Statistics and monitoring
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
     * @brief Execute query and return first row
     * @param sql SQL SELECT statement
     * @param params Parameter values
     * @return DatabaseRow (empty if no results)
     * @note Thread-safe
     * 
     * Example:
     * ```cpp
     * auto row = db.queryOne("SELECT * FROM users WHERE id = ?", {"1"});
     * if (!row.empty()) {
     *     std::cout << row.at("name") << std::endl;
     * }
     * ```
     */
    DatabaseRow queryOne(const std::string& sql,
                        const std::vector<std::string>& params = {});
    
    /**
     * @brief Execute query and return single scalar value
     * @param sql SQL SELECT statement
     * @param params Parameter values
     * @return std::string value (empty if no result)
     * @note Thread-safe
     * 
     * Example:
     * ```cpp
     * std::string count = db.queryScalar("SELECT COUNT(*) FROM users");
     * ```
     */
    std::string queryScalar(const std::string& sql,
                           const std::vector<std::string>& params = {});
    
    // ========================================================================
    // TRANSACTIONS
    // ========================================================================
    
    /**
     * @brief Begin transaction
     * @return true if successful
     * @note Thread-safe
     */
    bool beginTransaction();
    
    /**
     * @brief Commit transaction
     * @return true if successful
     * @note Thread-safe
     */
    bool commit();
    
    /**
     * @brief Rollback transaction
     * @return true if successful
     * @note Thread-safe
     */
    bool rollback();
    
    /**
     * @brief Execute function within transaction
     * @param fn Function to execute
     * @return true if transaction committed, false if rolled back
     * @note Thread-safe
     * @note Automatically commits on success, rolls back on exception
     * 
     * Example:
     * ```cpp
     * bool success = db.transaction([&]() {
     *     db.execute("INSERT INTO table1 VALUES (...)");
     *     db.execute("INSERT INTO table2 VALUES (...)");
     * });
     * ```
     */
    bool transaction(std::function<void()> fn);
    
    // ========================================================================
    // SCHEMA MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Run all pending migrations
     * @return true if all migrations successful
     * @note Thread-safe
     * @note Looks for .sql files in data/migrations/
     */
    bool migrate();
    
    /**
     * @brief Get current schema version
     * @return Current version number
     * @note Thread-safe
     */
    int getSchemaVersion();
    
    /**
     * @brief Initialize schema (create version table)
     * @note Thread-safe
     * @note Called automatically by migrate()
     */
    void initializeSchema();
    
    // ========================================================================
    // UTILITY METHODS
    // ========================================================================
    
    /**
     * @brief Check if table exists
     * @param tableName Table name
     * @return true if table exists
     * @note Thread-safe
     */
    bool tableExists(const std::string& tableName);
    
    /**
     * @brief Get list of all tables
     * @return Vector of table names
     * @note Thread-safe
     */
    std::vector<std::string> getTables();
    
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
    // PRIVATE HELPER METHODS
    // ========================================================================
    
    /**
     * @brief Prepare SQL statement
     * @param sql SQL with placeholders
     * @return sqlite3_stmt pointer
     * @throws std::runtime_error on error
     */
    sqlite3_stmt* prepareStatement(const std::string& sql);
    
    /**
     * @brief Bind parameters to prepared statement
     * @param stmt Statement handle
     * @param params Parameter values
     */
    void bindParameters(sqlite3_stmt* stmt, const std::vector<std::string>& params);
    
    /**
     * @brief Execute migration file
     * @param filepath Path to .sql file
     * @return true if successful
     */
    bool executeMigrationFile(const std::string& filepath);
    
    /**
     * @brief Get migration files in order
     * @return Sorted list of migration file paths
     */
    std::vector<std::string> getMigrationFiles();
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    // Database connection
    sqlite3* db_;
    std::string filepath_;
    bool isConnected_;
    
    // Thread synchronization
    mutable std::mutex mutex_;
    
    // Statistics
    mutable uint64_t queryCount_;
    mutable uint64_t errorCount_;
};

} // namespace midiMind