// ============================================================================
// Fichier: src/storage/Database.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "Database.h"
#include <sstream>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

Database::Database(const std::string& filepath)
    : filepath_(filepath)
    , db_(nullptr)
    , isOpen_(false)
    , transactionDepth_(0)
    , queryCount_(0)
    , errorCount_(0) {
    
    Logger::info("Database", "Database created: " + filepath_);
}

Database::~Database() {
    close();
    Logger::info("Database", "Database destroyed");
}

// ============================================================================
// CONNEXION
// ============================================================================

bool Database::open() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (isOpen_) {
        Logger::warn("Database", "Already open");
        return true;
    }
    
    Logger::info("Database", "Opening database: " + filepath_);
    
    int rc = sqlite3_open(filepath_.c_str(), &db_);
    
    if (rc != SQLITE_OK) {
        Logger::error("Database", "Failed to open: " + getErrorMessage());
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    
    // Activer les clés étrangères
    sqlite3_exec(db_, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);
    
    // Activer le mode WAL pour meilleures performances
    sqlite3_exec(db_, "PRAGMA journal_mode = WAL", nullptr, nullptr, nullptr);
    
    isOpen_ = true;
    
    Logger::info("Database", "✓ Database opened");
    
    return true;
}

void Database::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isOpen_ || !db_) {
        return;
    }
    
    Logger::info("Database", "Closing database...");
    Logger::info("Database", "  Total queries: " + std::to_string(queryCount_));
    Logger::info("Database", "  Total errors: " + std::to_string(errorCount_));
    
    sqlite3_close(db_);
    db_ = nullptr;
    isOpen_ = false;
    
    Logger::info("Database", "✓ Database closed");
}

bool Database::isOpen() const {
    return isOpen_;
}

// ============================================================================
// REQUÊTES
// ============================================================================

DatabaseResult Database::execute(const std::string& sql, 
                                 const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isOpen_) {
        THROW_ERROR(ErrorCode::DATABASE_QUERY_FAILED, "Database not open");
    }
    
    queryCount_++;
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    bindParameters(stmt, params);
    
    DatabaseResult result;
    
    int rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        errorCount_++;
        std::string error = getErrorMessage();
        sqlite3_finalize(stmt);
        THROW_ERROR(ErrorCode::DATABASE_QUERY_FAILED, error);
    }
    
    result.affectedRows = sqlite3_changes(db_);
    result.lastInsertId = sqlite3_last_insert_rowid(db_);
    
    sqlite3_finalize(stmt);
    
    return result;
}

DatabaseResult Database::query(const std::string& sql,
                               const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isOpen_) {
        THROW_ERROR(ErrorCode::DATABASE_QUERY_FAILED, "Database not open");
    }
    
    queryCount_++;
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    bindParameters(stmt, params);
    
    DatabaseResult result;
    
    // Récupérer les colonnes
    int columnCount = sqlite3_column_count(stmt);
    std::vector<std::string> columnNames;
    
    for (int i = 0; i < columnCount; ++i) {
        columnNames.push_back(sqlite3_column_name(stmt, i));
    }
    
    // Récupérer les lignes
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DatabaseRow row;
        
        for (int i = 0; i < columnCount; ++i) {
            const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row[columnNames[i]] = text ? text : "";
        }
        
        result.rows.push_back(row);
    }
    
    sqlite3_finalize(stmt);
    
    return result;
}

DatabaseRow Database::queryOne(const std::string& sql,
                               const std::vector<std::string>& params) {
    auto result = query(sql, params);
    
    if (result.empty()) {
        return DatabaseRow();
    }
    
    return result.rows[0];
}

std::string Database::queryScalar(const std::string& sql,
                                 const std::vector<std::string>& params) {
    auto row = queryOne(sql, params);
    
    if (row.empty()) {
        return "";
    }
    
    return row.begin()->second;
}

// ============================================================================
// TRANSACTIONS
// ============================================================================

void Database::beginTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (transactionDepth_ == 0) {
        execute("BEGIN TRANSACTION");
    }
    
    transactionDepth_++;
}

void Database::commit() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (transactionDepth_ == 0) {
        Logger::warn("Database", "No transaction to commit");
        return;
    }
    
    transactionDepth_--;
    
    if (transactionDepth_ == 0) {
        execute("COMMIT");
    }
}

void Database::rollback() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (transactionDepth_ == 0) {
        Logger::warn("Database", "No transaction to rollback");
        return;
    }
    
    transactionDepth_ = 0;
    execute("ROLLBACK");
}

bool Database::transaction(std::function<void()> fn) {
    try {
        beginTransaction();
        fn();
        commit();
        return true;
    } catch (const std::exception& e) {
        rollback();
        Logger::error("Database", "Transaction failed: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// SCHÉMA
// ============================================================================

void Database::initializeSchema() {
    Logger::info("Database", "Initializing database schema...");
    
    transaction([this]() {
        // Table des versions
        execute(R"(
            CREATE TABLE IF NOT EXISTS schema_version (
                version INTEGER PRIMARY KEY,
                applied_at TEXT NOT NULL
            )
        )");
        
        // Table des presets
        execute(R"(
            CREATE TABLE IF NOT EXISTS presets (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                category TEXT,
                data TEXT NOT NULL,
                created_at TEXT NOT NULL,
                updated_at TEXT NOT NULL
            )
        )");
        
        // Table des sessions
        execute(R"(
            CREATE TABLE IF NOT EXISTS sessions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                data TEXT NOT NULL,
                created_at TEXT NOT NULL,
                updated_at TEXT NOT NULL
            )
        )");
        
        // Table des settings
        execute(R"(
            CREATE TABLE IF NOT EXISTS settings (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL,
                updated_at TEXT NOT NULL
            )
        )");
        
        // Table de l'historique MIDI
        execute(R"(
            CREATE TABLE IF NOT EXISTS midi_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp INTEGER NOT NULL,
                message_type TEXT NOT NULL,
                channel INTEGER,
                data TEXT NOT NULL
            )
        )");
        
        // Index
        execute("CREATE INDEX IF NOT EXISTS idx_presets_category ON presets(category)");
        execute("CREATE INDEX IF NOT EXISTS idx_midi_history_timestamp ON midi_history(timestamp)");
        
        // Enregistrer la version
        if (getSchemaVersion() == 0) {
            execute("INSERT INTO schema_version (version, applied_at) VALUES (1, datetime('now'))");
        }
    });
    
    Logger::info("Database", "✓ Schema initialized");
}

bool Database::migrate(int version) {
    int currentVersion = getSchemaVersion();
    
    if (currentVersion >= version) {
        Logger::info("Database", "Already at version " + std::to_string(version));
        return true;
    }
    
    Logger::info("Database", "Migrating from v" + std::to_string(currentVersion) + 
                " to v" + std::to_string(version));
    
    // TODO: Implémenter les migrations selon la version
    
    return true;
}

int Database::getSchemaVersion() {
    if (!tableExists("schema_version")) {
        return 0;
    }
    
    auto version = queryScalar("SELECT MAX(version) FROM schema_version");
    
    if (version.empty()) {
        return 0;
    }
    
    return std::stoi(version);
}

// ============================================================================
// UTILITAIRES
// ============================================================================

bool Database::tableExists(const std::string& tableName) {
    auto result = queryScalar(
        "SELECT name FROM sqlite_master WHERE type='table' AND name=?",
        {tableName}
    );
    
    return !result.empty();
}

std::vector<std::string> Database::getTables() {
    auto result = query("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
    
    std::vector<std::string> tables;
    for (const auto& row : result.rows) {
        tables.push_back(row.at("name"));
    }
    
    return tables;
}

void Database::truncateTable(const std::string& tableName) {
    // 1. Valider le nom de table
    if (!tableExists(tableName)) {
        THROW_ERROR(ErrorCode::DATABASE_NOT_FOUND, "Table not found: " + tableName);
    }
    
    // 2. Whitelist des tables autorisées
    static const std::vector<std::string> ALLOWED_TABLES = {
        "presets", "sessions", "settings", "midi_history"
    };
    
    if (std::find(ALLOWED_TABLES.begin(), ALLOWED_TABLES.end(), tableName) == ALLOWED_TABLES.end()) {
        THROW_ERROR(ErrorCode::DATABASE_QUERY_FAILED, "Cannot truncate table: " + tableName);
    }
    
    // 3. Utiliser des requêtes paramétrées (mais SQLite ne supporte pas TRUNCATE)
    execute("DELETE FROM " + tableName);  // OK car validé
    execute("DELETE FROM sqlite_sequence WHERE name=?", {tableName});
}

void Database::optimize() {
    Logger::info("Database", "Optimizing database...");
    
    execute("VACUUM");
    execute("ANALYZE");
    
    Logger::info("Database", "✓ Database optimized");
}

bool Database::backup(const std::string& backupPath) {
    Logger::info("Database", "Backing up to: " + backupPath);
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    sqlite3* backupDb;
    int rc = sqlite3_open(backupPath.c_str(), &backupDb);
    
    if (rc != SQLITE_OK) {
        Logger::error("Database", "Backup failed: cannot create backup file");
        return false;
    }
    
    sqlite3_backup* backup = sqlite3_backup_init(backupDb, "main", db_, "main");
    
    if (!backup) {
        sqlite3_close(backupDb);
        Logger::error("Database", "Backup failed: cannot init backup");
        return false;
    }
    
    sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    
    sqlite3_close(backupDb);
    
    Logger::info("Database", "✓ Backup completed");
    
    return true;
}

json Database::getStatistics() const {
    json stats;
    
    stats["filepath"] = filepath_;
    stats["is_open"] = isOpen_.load();
    stats["query_count"] = queryCount_;
    stats["error_count"] = errorCount_;
    stats["schema_version"] = const_cast<Database*>(this)->getSchemaVersion();
    
    return stats;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

sqlite3_stmt* Database::prepareStatement(const std::string& sql) {
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        errorCount_++;
        THROW_ERROR(ErrorCode::DATABASE_QUERY_FAILED,
                   "Failed to prepare statement: " + getErrorMessage());
    }
    
    return stmt;
}

void Database::bindParameters(sqlite3_stmt* stmt, const std::vector<std::string>& params) {
    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
    }
}

std::string Database::getErrorMessage() const {
    if (!db_) {
        return "Database not initialized";
    }
    
    return sqlite3_errmsg(db_);
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Database.cpp
// ============================================================================