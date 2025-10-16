// ============================================================================
// File: backend/src/storage/Database.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of the Database class - SQLite3 wrapper.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Singleton pattern implementation
//   - Enhanced migration system with file discovery
//   - Improved error handling and logging
//   - Statistics tracking
//
// ============================================================================

#include "Database.h"
#include "../core/TimeUtils.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace midiMind {

// ============================================================================
// SINGLETON
// ============================================================================

Database& Database::instance() {
    static Database instance;
    return instance;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

Database::Database()
    : db_(nullptr)
    , filepath_()
    , isConnected_(false)
    , queryCount_(0)
    , errorCount_(0)
{
    Logger::debug("Database", "Database instance created");
}

Database::~Database() {
    close();
    Logger::debug("Database", "Database instance destroyed");
}

// ============================================================================
// CONNECTION MANAGEMENT
// ============================================================================

bool Database::connect(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (isConnected_) {
        Logger::warn("Database", "Already connected to: " + filepath_);
        return true;
    }
    
    Logger::info("Database", "Connecting to database: " + filepath);
    
    filepath_ = filepath;
    
    // Create directory if it doesn't exist
    fs::path dbPath(filepath);
    fs::path dbDir = dbPath.parent_path();
    
    if (!dbDir.empty() && !fs::exists(dbDir)) {
        try {
            fs::create_directories(dbDir);
            Logger::info("Database", "Created directory: " + dbDir.string());
        } catch (const std::exception& e) {
            Logger::error("Database", "Failed to create directory: " + std::string(e.what()));
            return false;
        }
    }
    
    // Open database
    int rc = sqlite3_open(filepath.c_str(), &db_);
    
    if (rc != SQLITE_OK) {
        Logger::error("Database", "Failed to open database: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    
    // Enable foreign keys
    char* errMsg = nullptr;
    rc = sqlite3_exec(db_, "PRAGMA foreign_keys = ON", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        Logger::warn("Database", "Failed to enable foreign keys: " + 
                    std::string(errMsg ? errMsg : "unknown"));
        sqlite3_free(errMsg);
    }
    
    // Set journal mode to WAL for better concurrency
    rc = sqlite3_exec(db_, "PRAGMA journal_mode = WAL", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        Logger::warn("Database", "Failed to set WAL mode: " + 
                    std::string(errMsg ? errMsg : "unknown"));
        sqlite3_free(errMsg);
    }
    
    isConnected_ = true;
    
    Logger::info("Database", "✓ Database connected successfully");
    return true;
}

void Database::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isConnected_ || !db_) {
        return;
    }
    
    Logger::info("Database", "Closing database connection...");
    
    sqlite3_close(db_);
    db_ = nullptr;
    isConnected_ = false;
    
    Logger::info("Database", "✓ Database closed");
}

// ============================================================================
// QUERY EXECUTION
// ============================================================================

DatabaseResult Database::execute(const std::string& sql,
                                 const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    DatabaseResult result;
    result.success = false;
    
    if (!isConnected_ || !db_) {
        result.error = "Database not connected";
        Logger::error("Database", result.error);
        return result;
    }
    
    queryCount_++;
    
    // Prepare statement
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        result.error = sqlite3_errmsg(db_);
        Logger::error("Database", "Failed to prepare statement: " + result.error);
        errorCount_++;
        return result;
    }
    
    // Bind parameters
    for (size_t i = 0; i < params.size(); i++) {
        rc = sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            result.error = "Failed to bind parameter " + std::to_string(i);
            Logger::error("Database", result.error);
            sqlite3_finalize(stmt);
            errorCount_++;
            return result;
        }
    }
    
    // Execute
    rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_DONE || rc == SQLITE_ROW) {
        result.success = true;
        result.affectedRows = sqlite3_changes(db_);
        result.lastInsertId = sqlite3_last_insert_rowid(db_);
    } else {
        result.success = false;
        result.error = sqlite3_errmsg(db_);
        Logger::error("Database", "Execute failed: " + result.error);
        errorCount_++;
    }
    
    sqlite3_finalize(stmt);
    
    return result;
}

DatabaseResult Database::query(const std::string& sql,
                               const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    DatabaseResult result;
    result.success = false;
    
    if (!isConnected_ || !db_) {
        result.error = "Database not connected";
        Logger::error("Database", result.error);
        return result;
    }
    
    queryCount_++;
    
    // Prepare statement
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        result.error = sqlite3_errmsg(db_);
        Logger::error("Database", "Failed to prepare statement: " + result.error);
        errorCount_++;
        return result;
    }
    
    // Bind parameters
    for (size_t i = 0; i < params.size(); i++) {
        rc = sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            result.error = "Failed to bind parameter " + std::to_string(i);
            Logger::error("Database", result.error);
            sqlite3_finalize(stmt);
            errorCount_++;
            return result;
        }
    }
    
    // Execute and fetch results
    result.success = true;
    
    while (true) {
        rc = sqlite3_step(stmt);
        
        if (rc == SQLITE_ROW) {
            int columnCount = sqlite3_column_count(stmt);
            DatabaseRow row;
            
            for (int i = 0; i < columnCount; i++) {
                const char* columnName = sqlite3_column_name(stmt, i);
                const char* columnValue = reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, i)
                );
                
                row[columnName] = columnValue ? columnValue : "";
            }
            
            result.rows.push_back(row);
            
        } else if (rc == SQLITE_DONE) {
            break;
            
        } else {
            result.success = false;
            result.error = sqlite3_errmsg(db_);
            Logger::error("Database", "Query failed: " + result.error);
            errorCount_++;
            break;
        }
    }
    
    sqlite3_finalize(stmt);
    
    return result;
}

DatabaseRow Database::queryOne(const std::string& sql,
                               const std::vector<std::string>& params) {
    auto result = query(sql, params);
    
    if (result.success && !result.rows.empty()) {
        return result.rows[0];
    }
    
    return DatabaseRow();
}

std::string Database::queryScalar(const std::string& sql,
                                  const std::vector<std::string>& params) {
    auto row = queryOne(sql, params);
    
    if (!row.empty()) {
        return row.begin()->second;
    }
    
    return "";
}

// ============================================================================
// TRANSACTIONS
// ============================================================================

bool Database::beginTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isConnected_ || !db_) {
        Logger::error("Database", "Cannot begin transaction: not connected");
        return false;
    }
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        Logger::error("Database", "Begin transaction failed: " + 
                     std::string(errMsg ? errMsg : "unknown error"));
        sqlite3_free(errMsg);
        return false;
    }
    
    Logger::debug("Database", "Transaction started");
    return true;
}

bool Database::commit() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isConnected_ || !db_) {
        Logger::error("Database", "Cannot commit: not connected");
        return false;
    }
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        Logger::error("Database", "Commit failed: " + 
                     std::string(errMsg ? errMsg : "unknown error"));
        sqlite3_free(errMsg);
        return false;
    }
    
    Logger::debug("Database", "Transaction committed");
    return true;
}

bool Database::rollback() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isConnected_ || !db_) {
        Logger::error("Database", "Cannot rollback: not connected");
        return false;
    }
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        Logger::error("Database", "Rollback failed: " + 
                     std::string(errMsg ? errMsg : "unknown error"));
        sqlite3_free(errMsg);
        return false;
    }
    
    Logger::debug("Database", "Transaction rolled back");
    return true;
}

bool Database::transaction(std::function<void()> fn) {
    if (!beginTransaction()) {
        return false;
    }
    
    try {
        fn();
        return commit();
    } catch (const std::exception& e) {
        Logger::error("Database", "Transaction failed: " + std::string(e.what()));
        rollback();
        return false;
    }
}

// ============================================================================
// SCHEMA MANAGEMENT
// ============================================================================

void Database::initializeSchema() {
    Logger::info("Database", "Initializing schema...");
    
    // Create schema_version table if it doesn't exist
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version INTEGER PRIMARY KEY,
            applied_at TEXT NOT NULL DEFAULT (datetime('now')),
            description TEXT NOT NULL,
            checksum TEXT
        )
    )";
    
    auto result = execute(sql);
    if (!result.success) {
        THROW_ERROR(ErrorCode::DATABASE_ERROR, 
                   "Failed to create schema_version table: " + result.error);
    }
    
    Logger::info("Database", "✓ Schema initialized");
}

bool Database::migrate() {
    Logger::info("Database", "Running database migrations...");
    
    // Initialize schema first
    initializeSchema();
    
    // Get current version
    int currentVersion = getSchemaVersion();
    Logger::info("Database", "Current schema version: " + std::to_string(currentVersion));
    
    // Get migration files
    std::vector<std::string> migrations = getMigrationFiles();
    
    if (migrations.empty()) {
        Logger::info("Database", "No migrations found");
        return true;
    }
    
    Logger::info("Database", "Found " + std::to_string(migrations.size()) + " migration(s)");
    
    // Apply migrations
    int appliedCount = 0;
    for (const auto& migrationFile : migrations) {
        // Extract version from filename (e.g., "001_initial.sql" -> 1)
        std::string filename = fs::path(migrationFile).filename().string();
        int version = std::stoi(filename.substr(0, 3));
        
        if (version <= currentVersion) {
            Logger::debug("Database", "Skipping migration " + filename + " (already applied)");
            continue;
        }
        
        Logger::info("Database", "Applying migration: " + filename);
        
        if (!executeMigrationFile(migrationFile)) {
            Logger::error("Database", "Migration failed: " + filename);
            return false;
        }
        
        appliedCount++;
        Logger::info("Database", "✓ Migration applied: " + filename);
    }
    
    if (appliedCount > 0) {
        Logger::info("Database", "✓ Applied " + std::to_string(appliedCount) + " migration(s)");
    } else {
        Logger::info("Database", "Database schema is up to date");
    }
    
    return true;
}

int Database::getSchemaVersion() {
    // Check if schema_version table exists
    if (!tableExists("schema_version")) {
        return 0;
    }
    
    std::string version = queryScalar("SELECT MAX(version) FROM schema_version");
    
    if (version.empty()) {
        return 0;
    }
    
    return std::stoi(version);
}

std::vector<std::string> Database::getMigrationFiles() {
    std::vector<std::string> files;
    
    // Look for migrations in data/migrations/
    std::string migrationDir = "data/migrations";
    
    if (!fs::exists(migrationDir)) {
        Logger::warn("Database", "Migration directory not found: " + migrationDir);
        return files;
    }
    
    try {
        for (const auto& entry : fs::directory_iterator(migrationDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".sql") {
                files.push_back(entry.path().string());
            }
        }
        
        // Sort files by name (e.g., 001_*.sql, 002_*.sql, ...)
        std::sort(files.begin(), files.end());
        
    } catch (const std::exception& e) {
        Logger::error("Database", "Failed to list migrations: " + std::string(e.what()));
    }
    
    return files;
}

bool Database::executeMigrationFile(const std::string& filepath) {
    // Read file content
    std::ifstream file(filepath);
    if (!file.is_open()) {
        Logger::error("Database", "Failed to open migration file: " + filepath);
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sql = buffer.str();
    file.close();
    
    // Execute SQL
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        Logger::error("Database", "Migration execution failed: " + 
                     std::string(errMsg ? errMsg : "unknown error"));
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

// ============================================================================
// UTILITY METHODS
// ============================================================================

bool Database::tableExists(const std::string& tableName) {
    std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
    auto result = queryScalar(sql, {tableName});
    return !result.empty();
}

std::vector<std::string> Database::getTables() {
    std::vector<std::string> tables;
    
    auto result = query("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
    
    for (const auto& row : result.rows) {
        tables.push_back(row.at("name"));
    }
    
    return tables;
}

void Database::truncateTable(const std::string& tableName) {
    Logger::warn("Database", "Truncating table: " + tableName);
    
    auto result = execute("DELETE FROM " + tableName);
    
    if (result.success) {
        Logger::info("Database", "✓ Table truncated (" + 
                    std::to_string(result.affectedRows) + " rows deleted)");
    }
}

void Database::optimize() {
    Logger::info("Database", "Optimizing database...");
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, "VACUUM", nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        Logger::error("Database", "Optimize failed: " + 
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