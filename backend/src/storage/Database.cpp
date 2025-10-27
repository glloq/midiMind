// ============================================================================
// File: backend/src/storage/Database.cpp
// Version: 4.2.1 - DEADLOCK FIX + SQL INJECTION FIX
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Corrections v4.2.1:
//   - Fixed: Deadlock in executeMigration() - removed nested lock
//   - Fixed: Deadlock in getStatistics() - added queryScalarUnlocked()
//   - Fixed: SQL injection in truncateTable() - added strict validation
//   - Fixed: Added regex validation for table names
//
// ============================================================================

#include "Database.h"
#include "../core/TimeUtils.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <regex>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace midiMind {

// ============================================================================
// SINGLETON INSTANCE
// ============================================================================

Database& Database::instance() {
    static Database instance;
    return instance;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

Database::Database() {
    Logger::debug("Database", "Database instance created");
}

Database::~Database() {
    close();
}

// ============================================================================
// CONNECTION MANAGEMENT
// ============================================================================

bool Database::connect(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (isConnected_ && db_) {
        Logger::warning("Database", "Already connected to: " + filepath_);
        return true;
    }
    
    Logger::info("Database", "Connecting to database: " + filepath);
    
    // Create directory if needed
    fs::path dbPath(filepath);
    if (dbPath.has_parent_path()) {
        try {
            fs::create_directories(dbPath.parent_path());
        } catch (const fs::filesystem_error& e) {
            Logger::error("Database", "Failed to create directory: " + 
                         std::string(e.what()));
            return false;
        }
    }
    
    // Open database with FULLMUTEX for thread-safety
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    int rc = sqlite3_open_v2(filepath.c_str(), &db_, flags, nullptr);
    
    if (rc != SQLITE_OK) {
        std::string error = db_ ? sqlite3_errmsg(db_) : "unknown error";
        Logger::error("Database", "Failed to open database: " + error);
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }
    
    filepath_ = filepath;
    isConnected_ = true;
    
    Logger::info("Database", "✓ Connected to database");
    
    // Enable foreign keys
    char* errMsg = nullptr;
    rc = sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        Logger::warning("Database", "Failed to enable foreign keys: " +
                       std::string(errMsg ? errMsg : "unknown error"));
        sqlite3_free(errMsg);
    }
    
    // Set WAL mode for better concurrency
    rc = sqlite3_exec(db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        Logger::warning("Database", "Failed to set WAL mode: " +
                       std::string(errMsg ? errMsg : "unknown error"));
        sqlite3_free(errMsg);
    }
    
    return true;
}

void Database::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (db_) {
        Logger::info("Database", "Closing database connection");
        sqlite3_close(db_);
        db_ = nullptr;
        isConnected_ = false;
    }
}

// ============================================================================
// QUERY EXECUTION
// ============================================================================

DatabaseResult Database::execute(const std::string& sql,
                                 const std::vector<std::string>& params) {
    return executeStatement(sql, params, false);
}

DatabaseResult Database::query(const std::string& sql,
                               const std::vector<std::string>& params) {
    return executeStatement(sql, params, true);
}

std::string Database::queryScalar(const std::string& sql,
                                  const std::vector<std::string>& params) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queryScalarUnlocked(sql, params);
}

// NOUVEAU: Version sans mutex pour appels internes
std::string Database::queryScalarUnlocked(const std::string& sql,
                                          const std::vector<std::string>& params) const {
    if (!isConnected_ || !db_) {
        return "";
    }
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        Logger::error("Database", "Query prepare failed: " + 
                     std::string(sqlite3_errmsg(db_)));
        return "";
    }
    
    // Bind parameters
    for (size_t i = 0; i < params.size(); i++) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), 
                         params[i].c_str(), -1, SQLITE_TRANSIENT);
    }
    
    std::string result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt, 0));
        if (text) {
            result = text;
        }
    }
    
    sqlite3_finalize(stmt);
    return result;
}

// ============================================================================
// TRANSACTION SUPPORT
// ============================================================================

bool Database::transaction(const std::function<void()>& func) {
    try {
        beginTransaction();
        func();
        commit();
        return true;
    } catch (const std::exception& e) {
        Logger::error("Database", "Transaction failed: " + std::string(e.what()));
        rollback();
        return false;
    }
}

void Database::beginTransaction() {
    execute("BEGIN TRANSACTION");
}

void Database::commit() {
    execute("COMMIT");
}

void Database::rollback() {
    execute("ROLLBACK");
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void Database::bindParameters(sqlite3_stmt* stmt,
                              const std::vector<std::string>& params) {
    for (size_t i = 0; i < params.size(); i++) {
        int rc = sqlite3_bind_text(stmt, static_cast<int>(i + 1), 
                                  params[i].c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to bind parameter " + 
                                   std::to_string(i + 1));
        }
    }
}

DatabaseResult Database::executeStatement(const std::string& sql,
                                         const std::vector<std::string>& params,
                                         bool isQuery) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    DatabaseResult result;
    
    if (!isConnected_ || !db_) {
        result.success = false;
        result.error = "Database not connected";
        errorCount_++;
        return result;
    }
    
    queryCount_++;
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        result.success = false;
        result.error = sqlite3_errmsg(db_);
        Logger::error("Database", "Query prepare failed: " + result.error);
        errorCount_++;
        return result;
    }
    
    // Bind parameters
    try {
        bindParameters(stmt, params);
    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
        sqlite3_finalize(stmt);
        errorCount_++;
        return result;
    }
    
    // Execute query
    if (isQuery) {
        // SELECT query - fetch rows
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            DatabaseRow row;
            int columnCount = sqlite3_column_count(stmt);
            
            for (int i = 0; i < columnCount; i++) {
                const char* columnName = sqlite3_column_name(stmt, i);
                const char* value = reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, i));
                
                row[columnName ? columnName : ""] = value ? value : "";
            }
            
            result.rows.push_back(std::move(row));
        }
    } else {
        // Non-SELECT query - just execute
        rc = sqlite3_step(stmt);
    }
    
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        result.success = false;
        result.error = sqlite3_errmsg(db_);
        Logger::error("Database", "Query execution failed: " + result.error);
        errorCount_++;
    } else {
        result.affectedRows = sqlite3_changes(db_);
        result.lastInsertId = sqlite3_last_insert_rowid(db_);
    }
    
    sqlite3_finalize(stmt);
    return result;
}

// ============================================================================
// SCHEMA MANAGEMENT
// ============================================================================

bool Database::runMigrations(const std::string& migrationDir) {
    Logger::info("Database", "Running migrations from: " + migrationDir);
    
    // Initialize schema version table
    initSchemaVersionTable();
    
    // Get current schema version
    int currentVersion = getSchemaVersion();
    Logger::info("Database", "Current schema version: " + 
                std::to_string(currentVersion));
    
    // Get migration files
    auto migrationFiles = getMigrationFiles(migrationDir);
    
    if (migrationFiles.empty()) {
        Logger::info("Database", "No migrations found");
        return true;
    }
    
    // Execute migrations
    bool success = true;
    for (const auto& file : migrationFiles) {
        // Extract version from filename (e.g., 001_initial.sql -> 1)
        std::string filename = fs::path(file).filename().string();
        
        // Validation stricte du nom de fichier
        if (filename.length() < 3) {
            Logger::error("Database", "Invalid migration filename (too short): " + filename);
            continue;
        }
        
        int version = 0;
        try {
            std::string versionStr = filename.substr(0, 3);
            if (!std::all_of(versionStr.begin(), versionStr.end(), ::isdigit)) {
                throw std::invalid_argument("Not a number");
            }
            version = std::stoi(versionStr);
        } catch (const std::exception& e) {
            Logger::error("Database", "Invalid migration filename: " + filename + 
                         " (" + e.what() + ")");
            continue;
        }
        
        if (version <= currentVersion) {
            continue; // Skip already applied migrations
        }
        
        Logger::info("Database", "Applying migration " + 
                    std::to_string(version) + ": " + filename);
        
        if (!executeMigration(file, version)) {
            Logger::error("Database", "Migration failed: " + filename);
            success = false;
            break;
        }
        
        Logger::info("Database", "✓ Migration " + 
                    std::to_string(version) + " applied");
    }
    
    if (success) {
        Logger::info("Database", "✓ All migrations completed successfully");
    }
    
    return success;
}

void Database::initSchemaVersionTable() {
    execute(
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "    version INTEGER PRIMARY KEY"
        ")"
    );
}

int Database::getSchemaVersion() const {
    std::string versionStr = queryScalar("SELECT version FROM schema_version");
    
    if (versionStr.empty()) {
        return 0;
    }
    
    try {
        return std::stoi(versionStr);
    } catch (const std::exception& e) {
        Logger::error("Database", "Invalid schema version: " + versionStr);
        return 0;
    }
}

bool Database::tableExists(const std::string& tableName) const {
    std::string result = queryScalar(
        "SELECT name FROM sqlite_master WHERE type='table' AND name=?",
        {tableName}
    );
    return !result.empty();
}

std::vector<std::string> Database::getTables() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> tables;
    
    if (!isConnected_ || !db_) {
        return tables;
    }
    
    const char* sql = "SELECT name FROM sqlite_master WHERE type='table' "
                     "AND name NOT LIKE 'sqlite_%' ORDER BY name";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* name = reinterpret_cast<const char*>(
                sqlite3_column_text(stmt, 0));
            if (name) {
                tables.push_back(name);
            }
        }
    }
    
    sqlite3_finalize(stmt);
    return tables;
}

std::vector<std::string> Database::getMigrationFiles(const std::string& dir) {
    std::vector<std::string> files;
    
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        Logger::warning("Database", "Migration directory not found: " + dir);
        return files;
    }
    
    try {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && 
                entry.path().extension() == ".sql") {
                files.push_back(entry.path().string());
            }
        }
    } catch (const fs::filesystem_error& e) {
        Logger::error("Database", "Error reading migration directory: " + 
                     std::string(e.what()));
        return files;
    }
    
    // Sort by filename (which includes version number)
    std::sort(files.begin(), files.end());
    
    return files;
}

// CORRECTION CRITIQUE: Pas de lock supplémentaire dans le callback de transaction
bool Database::executeMigration(const std::string& filepath, int version) {
    // Read migration file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        Logger::error("Database", "Cannot open migration file: " + filepath);
        return false;
    }
    
    std::string sql((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
    file.close();
    
    // Execute in transaction - PAS de lock ici, transaction() le fait déjà
    bool success = transaction([&]() {
        // NE PAS prendre mutex_ ici - déjà pris par executeStatement dans transaction()
        
        // Execute migration SQL directly - mutex déjà pris
        sqlite3_stmt* stmt = nullptr;
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
        
        if (rc != SQLITE_OK) {
            std::string error = errMsg ? errMsg : "unknown error";
            sqlite3_free(errMsg);
            throw std::runtime_error("Migration SQL failed: " + error);
        }
        
        // Update schema version - utiliser sqlite3 directement car mutex déjà pris
        rc = sqlite3_prepare_v2(db_, "DELETE FROM schema_version", -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            if (rc != SQLITE_DONE) {
                throw std::runtime_error("Failed to delete old schema version");
            }
        }
        
        rc = sqlite3_prepare_v2(db_, 
            "INSERT INTO schema_version (version) VALUES (?)", -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, version);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            if (rc != SQLITE_DONE) {
                throw std::runtime_error("Failed to insert new schema version");
            }
        } else {
            throw std::runtime_error("Failed to prepare schema version update");
        }
    });
    
    return success;
}

// CORRECTION SQL INJECTION: Validation stricte du nom de table
void Database::truncateTable(const std::string& tableName) {
    Logger::warning("Database", "Truncating table: " + tableName);
    
    // Validation stricte: alphanumérique et underscore uniquement
    static const std::regex validTableName("^[a-zA-Z_][a-zA-Z0-9_]*$");
    if (!std::regex_match(tableName, validTableName)) {
        Logger::error("Database", "Invalid table name (contains invalid characters): " + 
                     tableName);
        return;
    }
    
    // Vérifier que la table existe
    if (!tableExists(tableName)) {
        Logger::error("Database", "Table does not exist: " + tableName);
        return;
    }
    
    // Maintenant sûr d'utiliser la concaténation
    execute("DELETE FROM " + tableName);
    Logger::info("Database", "Table truncated: " + tableName);
}

void Database::optimize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isConnected_ || !db_) {
        return;
    }
    
    Logger::info("Database", "Optimizing database...");
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, "VACUUM;", nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        Logger::error("Database", "Vacuum failed: " +
                     std::string(errMsg ? errMsg : "unknown error"));
        sqlite3_free(errMsg);
    } else {
        Logger::info("Database", "✓ Database optimized");
    }
}

bool Database::backup(const std::string& backupPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isConnected_ || !db_) {
        Logger::error("Database", "Cannot backup: not connected");
        return false;
    }
    
    Logger::info("Database", "Creating backup: " + backupPath);
    
    sqlite3* backupDb = nullptr;
    int rc = sqlite3_open(backupPath.c_str(), &backupDb);
    
    if (rc != SQLITE_OK) {
        Logger::error("Database", "Failed to open backup file");
        return false;
    }
    
    sqlite3_backup* backup = sqlite3_backup_init(backupDb, "main", db_, "main");
    
    if (!backup) {
        Logger::error("Database", "Failed to initialize backup");
        sqlite3_close(backupDb);
        return false;
    }
    
    rc = sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    sqlite3_close(backupDb);
    
    if (rc == SQLITE_DONE) {
        Logger::info("Database", "✓ Backup created successfully");
        return true;
    } else {
        Logger::error("Database", "Backup failed");
        return false;
    }
}

// CORRECTION CRITIQUE: Utiliser queryScalarUnlocked pour éviter deadlock
json Database::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats = {
        {"connected", isConnected_},
        {"filepath", filepath_},
        {"query_count", queryCount_.load()},
        {"error_count", errorCount_.load()}
    };
    
    if (isConnected_ && db_) {
        // File size
        struct stat fileStat;
        if (stat(filepath_.c_str(), &fileStat) == 0) {
            stats["file_size_bytes"] = fileStat.st_size;
        }
        
        // Page count and size - UTILISER VERSION UNLOCKED
        auto pageCount = queryScalarUnlocked("PRAGMA page_count");
        auto pageSize = queryScalarUnlocked("PRAGMA page_size");
        
        try {
            if (!pageCount.empty()) {
                stats["page_count"] = std::stoll(pageCount);
            }
        } catch (const std::exception& e) {
            Logger::error("Database", "Invalid page_count: " + pageCount);
        }
        
        try {
            if (!pageSize.empty()) {
                stats["page_size"] = std::stoll(pageSize);
            }
        } catch (const std::exception& e) {
            Logger::error("Database", "Invalid page_size: " + pageSize);
        }
        
        // Table count - getTables() prend déjà le mutex donc on ne peut pas l'appeler ici
        // Il faut une version unlocked ou faire le comptage directement
        const char* sql = "SELECT COUNT(*) FROM sqlite_master WHERE type='table' "
                         "AND name NOT LIKE 'sqlite_%'";
        auto tableCount = queryScalarUnlocked(sql);
        try {
            if (!tableCount.empty()) {
                stats["table_count"] = std::stoi(tableCount);
            }
        } catch (const std::exception& e) {
            Logger::error("Database", "Invalid table_count: " + tableCount);
        }
        
        // Schema version
        auto schemaVersion = queryScalarUnlocked("SELECT version FROM schema_version");
        try {
            if (!schemaVersion.empty()) {
                stats["schema_version"] = std::stoi(schemaVersion);
            } else {
                stats["schema_version"] = 0;
            }
        } catch (const std::exception& e) {
            Logger::error("Database", "Invalid schema_version");
            stats["schema_version"] = 0;
        }
    }
    
    return stats;
}

} // namespace midiMind

// ============================================================================
// END OF FILE Database.cpp v4.2.1
// ============================================================================