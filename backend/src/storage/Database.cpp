// ============================================================================
// File: backend/src/storage/Database.cpp
// Version: 4.1.1 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of the Database class - SQLite3 wrapper.
//
// Changes v4.1.1:
//   - Fixed: Logger::warn → Logger::warning (5 occurrences)
//   - Fixed: queryScalar, getTables, getSchemaVersion now const
//
// ============================================================================

#include "Database.h"
#include "../core/TimeUtils.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
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
        fs::create_directories(dbPath.parent_path());
    }
    
    // Open database
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX;
    int rc = sqlite3_open_v2(filepath.c_str(), &db_, flags, nullptr);
    
    if (rc != SQLITE_OK) {
        std::string error = sqlite3_errmsg(db_);
        Logger::error("Database", "Failed to open database: " + error);
        sqlite3_close(db_);
        db_ = nullptr;
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
    
    // Bind parameters (need to cast away const for binding)
    for (size_t i = 0; i < params.size(); i++) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
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

bool Database::transaction(std::function<void()> func) {
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
        int rc = sqlite3_bind_text(stmt, i + 1, params[i].c_str(), 
                                  -1, SQLITE_TRANSIENT);
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
                const char* columnValue = reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, i));
                
                row[columnName] = columnValue ? columnValue : "";
            }
            
            result.rows.push_back(row);
        }
    } else {
        // Non-SELECT query (INSERT, UPDATE, DELETE)
        rc = sqlite3_step(stmt);
        
        if (rc == SQLITE_DONE) {
            result.affectedRows = sqlite3_changes(db_);
            result.lastInsertId = sqlite3_last_insert_rowid(db_);
        }
    }
    
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        result.success = false;
        result.error = sqlite3_errmsg(db_);
        Logger::error("Database", "Query execution failed: " + result.error);
        errorCount_++;
    }
    
    sqlite3_finalize(stmt);
    return result;
}

// ============================================================================
// SCHEMA MANAGEMENT
// ============================================================================

bool Database::runMigrations(const std::string& migrationDir) {
    Logger::info("Database", "Running migrations from: " + migrationDir);
    
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
        int version = std::stoi(filename.substr(0, 3));
        
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

int Database::getSchemaVersion() const {
    // Create schema_version table if it doesn't exist
    const_cast<Database*>(this)->execute(
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "    version INTEGER PRIMARY KEY"
        ")"
    );
    
    std::string versionStr = queryScalar("SELECT version FROM schema_version");
    return versionStr.empty() ? 0 : std::stoi(versionStr);
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
    
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() && 
            entry.path().extension() == ".sql") {
            files.push_back(entry.path().string());
        }
    }
    
    // Sort by filename (which includes version number)
    std::sort(files.begin(), files.end());
    
    return files;
}

bool Database::executeMigration(const std::string& filepath, int version) {
    // Read migration file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        Logger::error("Database", "Cannot open migration file: " + filepath);
        return false;
    }
    
    std::string sql((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
    
    // Execute in transaction
    bool success = transaction([&]() {
        // Execute migration SQL
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
        
        if (rc != SQLITE_OK) {
            std::string error = errMsg ? errMsg : "unknown error";
            sqlite3_free(errMsg);
            throw std::runtime_error("Migration SQL failed: " + error);
        }
        
        // Update schema version
        execute("DELETE FROM schema_version");
        execute("INSERT INTO schema_version (version) VALUES (?)",
               {std::to_string(version)});
    });
    
    return success;
}

void Database::truncateTable(const std::string& tableName) {
    Logger::warning("Database", "Truncating table: " + tableName);
    execute("DELETE FROM " + tableName);
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

json Database::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats = {
        {"connected", isConnected_},
        {"filepath", filepath_},
        {"query_count", queryCount_},
        {"error_count", errorCount_}
    };
    
    if (isConnected_ && db_) {
        // File size
        struct stat fileStat;
        if (stat(filepath_.c_str(), &fileStat) == 0) {
            stats["file_size_bytes"] = fileStat.st_size;
        }
        
        // Page count and size
        auto pageCount = queryScalar("PRAGMA page_count");
        auto pageSize = queryScalar("PRAGMA page_size");
        
        if (!pageCount.empty()) stats["page_count"] = std::stoll(pageCount);
        if (!pageSize.empty()) stats["page_size"] = std::stoll(pageSize);
        
        // Table count
        auto tables = getTables();
        stats["table_count"] = tables.size();
        
        // Schema version
        stats["schema_version"] = getSchemaVersion();
    }
    
    return stats;
}

} // namespace midiMind

// ============================================================================
// END OF FILE Database.cpp
// ============================================================================
