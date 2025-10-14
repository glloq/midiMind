// ============================================================================
// Fichier: backend/src/storage/Database.cpp
// Version: 3.0.1 - COMPLET
// Date: 2025-10-13
// ============================================================================
// Description:
//   Wrapper SQLite pour gestion base de données
//   Thread-safe avec transactions et requêtes préparées
//
// CORRECTIONS v3.0.1:
//   ✅ Méthode prepareStatement() complétée
//   ✅ Méthode bindParameters() complétée
//   ✅ Gestion complète des types de paramètres
//   ✅ Gestion erreurs améliorée
//
// Fonctionnalités:
//   - Connexion/déconnexion SQLite
//   - Requêtes préparées (prepared statements)
//   - Transactions ACID
//   - Thread-safety
//   - Gestion erreurs détaillée
// ============================================================================

#include "Database.h"
#include "../core/Logger.h"
#include "../core/Error.h"
#include <stdexcept>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

Database::Database()
    : db_(nullptr)
    , isOpen_(false)
    , queryCount_(0)
    , errorCount_(0) {
    
    Logger::debug("Database", "Database instance created");
}

Database::~Database() {
    close();
}

// ============================================================================
// CONNEXION / DÉCONNEXION
// ============================================================================

bool Database::open(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (isOpen_) {
        Logger::warn("Database", "Database already open");
        return true;
    }
    
    Logger::info("Database", "Opening database: " + path);
    
    int rc = sqlite3_open(path.c_str(), &db_);
    
    if (rc != SQLITE_OK) {
        Logger::error("Database", "Failed to open database: " + 
                     std::string(sqlite3_errmsg(db_)));
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
        THROW_ERROR(ErrorCode::DATABASE_ERROR, "Database not open");
    }
    
    queryCount_++;
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) {
        errorCount_++;
        return DatabaseResult{false, "Failed to prepare statement"};
    }
    
    // Binder les paramètres
    if (!bindParameters(stmt, params)) {
        sqlite3_finalize(stmt);
        errorCount_++;
        return DatabaseResult{false, "Failed to bind parameters"};
    }
    
    // Exécuter
    int rc = sqlite3_step(stmt);
    
    DatabaseResult result;
    result.success = (rc == SQLITE_DONE || rc == SQLITE_ROW);
    
    if (!result.success) {
        result.error = sqlite3_errmsg(db_);
        Logger::error("Database", "Query failed: " + result.error);
        errorCount_++;
    }
    
    // Récupérer lastInsertId si INSERT
    if (result.success && rc == SQLITE_DONE) {
        result.lastInsertId = sqlite3_last_insert_rowid(db_);
        result.affectedRows = sqlite3_changes(db_);
    }
    
    sqlite3_finalize(stmt);
    
    return result;
}

DatabaseResult Database::query(const std::string& sql,
                               const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isOpen_) {
        THROW_ERROR(ErrorCode::DATABASE_ERROR, "Database not open");
    }
    
    queryCount_++;
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) {
        errorCount_++;
        return DatabaseResult{false, "Failed to prepare statement"};
    }
    
    // Binder les paramètres
    if (!bindParameters(stmt, params)) {
        sqlite3_finalize(stmt);
        errorCount_++;
        return DatabaseResult{false, "Failed to bind parameters"};
    }
    
    DatabaseResult result;
    result.success = true;
    
    // Récupérer toutes les lignes
    while (true) {
        int rc = sqlite3_step(stmt);
        
        if (rc == SQLITE_ROW) {
            // Récupérer la ligne
            std::vector<std::string> row;
            int columnCount = sqlite3_column_count(stmt);
            
            for (int i = 0; i < columnCount; i++) {
                const char* text = reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, i)
                );
                row.push_back(text ? text : "");
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

// ============================================================================
// TRANSACTIONS
// ============================================================================

bool Database::beginTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isOpen_) {
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
    
    if (!isOpen_) {
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
    
    if (!isOpen_) {
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

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

sqlite3_stmt* Database::prepareStatement(const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(
        db_,
        sql.c_str(),
        -1,  // Lire jusqu'au '\0'
        &stmt,
        nullptr
    );
    
    if (rc != SQLITE_OK) {
        std::string error = sqlite3_errmsg(db_);
        Logger::error("Database", "Failed to prepare statement: " + error);
        Logger::error("Database", "SQL: " + sql);
        
        if (stmt) {
            sqlite3_finalize(stmt);
        }
        
        return nullptr;
    }
    
    return stmt;
}

bool Database::bindParameters(sqlite3_stmt* stmt, 
                              const std::vector<std::string>& params) {
    if (params.empty()) {
        return true;
    }
    
    int paramCount = sqlite3_bind_parameter_count(stmt);
    
    if (static_cast<int>(params.size()) != paramCount) {
        Logger::error("Database", 
            "Parameter count mismatch: expected " + std::to_string(paramCount) + 
            ", got " + std::to_string(params.size()));
        return false;
    }
    
    for (size_t i = 0; i < params.size(); i++) {
        int rc = sqlite3_bind_text(
            stmt,
            static_cast<int>(i + 1),  // SQLite indices commencent à 1
            params[i].c_str(),
            -1,
            SQLITE_TRANSIENT  // SQLite copie la string
        );
        
        if (rc != SQLITE_OK) {
            Logger::error("Database", 
                "Failed to bind parameter " + std::to_string(i) + 
                ": " + sqlite3_errmsg(db_));
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// STATISTIQUES
// ============================================================================

uint64_t Database::getQueryCount() const {
    return queryCount_;
}

uint64_t Database::getErrorCount() const {
    return errorCount_;
}

std::string Database::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) {
        return "Database not open";
    }
    
    return sqlite3_errmsg(db_);
}

// ============================================================================
// UTILITAIRES
// ============================================================================

bool Database::tableExists(const std::string& tableName) {
    std::string sql = 
        "SELECT name FROM sqlite_master "
        "WHERE type='table' AND name=?";
    
    auto result = query(sql, {tableName});
    
    return result.success && !result.rows.empty();
}

int64_t Database::getLastInsertId() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) {
        return -1;
    }
    
    return sqlite3_last_insert_rowid(db_);
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Database.cpp v3.0.1 - COMPLET
// ============================================================================
