-- ============================================================================
-- Fichier: database/migrations/003_add_jsonmidi.sql
-- Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
-- ============================================================================
-- Description:
--   Migration v2 → v3 - Ajout du support JsonMidi
--   - Colonne jsonmidi pour stockage JSON
--   - Table versions pour historique
--   - Index optimisés pour recherche
--   - Triggers de synchronisation
--
-- Auteur: MidiMind Team
-- Date: 2025-10-05
-- Version: 3.0.0
-- ============================================================================

-- Vérifier prérequis
SELECT CASE 
    WHEN NOT EXISTS (SELECT 1 FROM schema_version WHERE version = 2)
    THEN RAISE(ABORT, 'Migration 002 must be applied first')
    WHEN EXISTS (SELECT 1 FROM schema_version WHERE version = 3)
    THEN RAISE(ABORT, 'Migration 003 already applied')
END;

-- ============================================================================
-- DÉBUT TRANSACTION
-- ============================================================================

BEGIN TRANSACTION;

-- ============================================================================
-- AJOUT COLONNE jsonmidi À midi_files
-- ============================================================================

-- Ajouter colonne jsonmidi
ALTER TABLE midi_files ADD COLUMN jsonmidi TEXT;

-- Ajouter colonnes de cache
ALTER TABLE midi_files ADD COLUMN jsonmidi_version TEXT DEFAULT '1.0.0';
ALTER TABLE midi_files ADD COLUMN jsonmidi_generated_at TEXT;

-- Commentaire
-- La colonne jsonmidi contient la représentation JSON complète du fichier MIDI
-- Cela évite de reconvertir à chaque chargement et permet l'édition directe

-- ============================================================================
-- TABLE: editor_sessions
-- Description: Sessions d'édition en cours (auto-save)
-- ============================================================================

CREATE TABLE IF NOT EXISTS editor_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER,
    user_id TEXT,
    jsonmidi TEXT NOT NULL,
    is_autosave BOOLEAN DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (file_id) REFERENCES midi_files(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_editor_sessions_file ON editor_sessions(file_id);
CREATE INDEX IF NOT EXISTS idx_editor_sessions_user ON editor_sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_editor_sessions_updated ON editor_sessions(updated_at DESC);

-- ============================================================================
-- TABLE: editor_versions
-- Description: Historique des versions éditées (undo/redo persistence)
-- ============================================================================

CREATE TABLE IF NOT EXISTS editor_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER NOT NULL,
    version_number INTEGER NOT NULL,
    jsonmidi TEXT NOT NULL,
    change_description TEXT,
    event_count INTEGER,
    duration INTEGER,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (file_id) REFERENCES midi_files(id) ON DELETE CASCADE,
    UNIQUE(file_id, version_number)
);

CREATE INDEX IF NOT EXISTS idx_editor_versions_file ON editor_versions(file_id);
CREATE INDEX IF NOT EXISTS idx_editor_versions_number ON editor_versions(file_id, version_number DESC);
CREATE INDEX IF NOT EXISTS idx_editor_versions_created ON editor_versions(created_at DESC);

-- ============================================================================
-- TABLE: jsonmidi_cache_stats
-- Description: Statistiques du cache JsonMidi
-- ============================================================================

CREATE TABLE IF NOT EXISTS jsonmidi_cache_stats (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    date TEXT NOT NULL,
    conversions_midi_to_json INTEGER DEFAULT 0,
    conversions_json_to_midi INTEGER DEFAULT 0,
    cache_hits INTEGER DEFAULT 0,
    cache_misses INTEGER DEFAULT 0,
    avg_conversion_time_ms REAL,
    total_size_bytes INTEGER
);

CREATE INDEX IF NOT EXISTS idx_cache_stats_date ON jsonmidi_cache_stats(date DESC);

-- ============================================================================
-- VUES UTILES
-- ============================================================================

-- Vue: Fichiers avec JsonMidi disponible
CREATE VIEW IF NOT EXISTS files_with_jsonmidi AS
SELECT 
    id,
    filepath,
    filename,
    size,
    duration,
    CASE 
        WHEN jsonmidi IS NOT NULL THEN 'cached'
        ELSE 'not_cached'
    END as jsonmidi_status,
    jsonmidi_generated_at,
    last_scanned
FROM midi_files
ORDER BY last_scanned DESC;

-- Vue: Sessions d'édition actives
CREATE VIEW IF NOT EXISTS active_editor_sessions AS
SELECT 
    es.id,
    es.file_id,
    mf.filename,
    es.user_id,
    es.updated_at,
    ROUND((julianday('now') - julianday(es.updated_at)) * 24 * 60) as minutes_since_update
FROM editor_sessions es
JOIN midi_files mf ON es.file_id = mf.id
WHERE es.updated_at > datetime('now', '-1 hour')
ORDER BY es.updated_at DESC;

-- Vue: Historique versions par fichier
CREATE VIEW IF NOT EXISTS version_history AS
SELECT 
    ev.file_id,
    mf.filename,
    ev.version_number,
    ev.event_count,
    ev.duration,
    ev.change_description,
    ev.created_at
FROM editor_versions ev
JOIN midi_files mf ON ev.file_id = mf.id
ORDER BY ev.file_id, ev.version_number DESC;

-- ============================================================================
-- TRIGGERS
-- ============================================================================

-- Trigger: Auto-update timestamp pour editor_sessions
CREATE TRIGGER IF NOT EXISTS update_editor_sessions_timestamp
AFTER UPDATE ON editor_sessions
BEGIN
    UPDATE editor_sessions 
    SET updated_at = datetime('now') 
    WHERE id = NEW.id;
END;

-- Trigger: Limiter versions à 20 par fichier
CREATE TRIGGER IF NOT EXISTS limit_editor_versions
AFTER INSERT ON editor_versions
BEGIN
    DELETE FROM editor_versions
    WHERE file_id = NEW.file_id
    AND version_number NOT IN (
        SELECT version_number 
        FROM editor_versions
        WHERE file_id = NEW.file_id
        ORDER BY version_number DESC
        LIMIT 20
    );
END;

-- Trigger: Nettoyer anciennes sessions (> 7 jours)
CREATE TRIGGER IF NOT EXISTS cleanup_old_sessions
AFTER INSERT ON editor_sessions
BEGIN
    DELETE FROM editor_sessions
    WHERE updated_at < datetime('now', '-7 days');
END;

-- Trigger: Invalider cache JsonMidi quand fichier modifié
CREATE TRIGGER IF NOT EXISTS invalidate_jsonmidi_cache
AFTER UPDATE OF size, hash ON midi_files
BEGIN
    UPDATE midi_files
    SET 
        jsonmidi = NULL,
        jsonmidi_generated_at = NULL
    WHERE id = NEW.id;
END;

-- ============================================================================
-- FONCTIONS UTILITAIRES (via SQL)
-- ============================================================================

-- Fonction: Obtenir la dernière version d'un fichier
-- Usage: SELECT * FROM get_latest_version(file_id)
CREATE VIEW IF NOT EXISTS get_latest_version AS
SELECT 
    file_id,
    MAX(version_number) as latest_version,
    jsonmidi,
    created_at
FROM editor_versions
GROUP BY file_id;

-- ============================================================================
-- MIGRATION DONNÉES EXISTANTES
-- ============================================================================

-- Pas de données à migrer (nouvelle fonctionnalité)
-- Le cache JsonMidi sera généré à la demande

-- ============================================================================
-- NOUVEAUX SETTINGS
-- ============================================================================

INSERT OR IGNORE INTO settings (key, value, type, description) VALUES
('jsonmidi_cache_enabled', 'true', 'bool', 'Activer le cache JsonMidi'),
('jsonmidi_auto_generate', 'true', 'bool', 'Générer JsonMidi automatiquement au scan'),
('editor_autosave_enabled', 'true', 'bool', 'Sauvegarde automatique de l''éditeur'),
('editor_autosave_interval', '60', 'int', 'Intervalle auto-save éditeur (secondes)'),
('editor_max_versions', '20', 'int', 'Nombre max de versions par fichier'),
('editor_max_undo', '50', 'int', 'Nombre max d''actions undo'),
('jsonmidi_compression', 'true', 'bool', 'Compresser JsonMidi en DB');

-- ============================================================================
-- STATISTIQUES INITIALES
-- ============================================================================

INSERT INTO jsonmidi_cache_stats (date, conversions_midi_to_json, conversions_json_to_midi, cache_hits, cache_misses)
VALUES (date('now'), 0, 0, 0, 0);

-- ============================================================================
-- ENREGISTREMENT MIGRATION
-- ============================================================================

INSERT INTO schema_version (version, description) 
VALUES (3, 'JsonMidi support - Cache, versions, editor sessions');

-- ============================================================================
-- FIN TRANSACTION
-- ============================================================================

COMMIT;

-- ============================================================================
-- VÉRIFICATION POST-MIGRATION
-- ============================================================================

-- Vérifier nouvelles tables
SELECT 
    CASE 
        WHEN (SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN 
            ('editor_sessions', 'editor_versions', 'jsonmidi_cache_stats')) < 3
        THEN RAISE(ABORT, 'Migration 003 failed - Missing tables')
    END;

-- Vérifier nouvelle colonne
SELECT 
    CASE 
        WHEN NOT EXISTS (
            SELECT 1 FROM pragma_table_info('midi_files') WHERE name = 'jsonmidi'
        )
        THEN RAISE(ABORT, 'Migration 003 failed - Missing jsonmidi column')
    END;

-- Afficher résumé
SELECT 
    'Migration 003 completed successfully' as status,
    (SELECT version FROM schema_version ORDER BY version DESC LIMIT 1) as current_version,
    (SELECT COUNT(*) FROM midi_files) as total_files,
    (SELECT COUNT(*) FROM midi_files WHERE jsonmidi IS NOT NULL) as files_with_jsonmidi_cache,
    (SELECT COUNT(*) FROM editor_sessions) as active_sessions,
    (SELECT COUNT(*) FROM editor_versions) as total_versions;

-- ============================================================================
-- REQUÊTES UTILES POST-MIGRATION
-- ============================================================================

-- Lister fichiers sans cache JsonMidi
-- SELECT * FROM midi_files WHERE jsonmidi IS NULL;

-- Obtenir sessions actives
-- SELECT * FROM active_editor_sessions;

-- Obtenir versions d'un fichier
-- SELECT * FROM version_history WHERE file_id = 1;

-- Statistiques cache
-- SELECT * FROM jsonmidi_cache_stats ORDER BY date DESC LIMIT 7;

-- Nettoyer cache manuellement
-- UPDATE midi_files SET jsonmidi = NULL, jsonmidi_generated_at = NULL;

-- Générer cache pour tous les fichiers (à faire via API)
-- SELECT id, filepath FROM midi_files WHERE jsonmidi IS NULL;

-- ============================================================================
-- ROLLBACK (si nécessaire)
-- ============================================================================

-- Pour annuler cette migration:
/*
BEGIN TRANSACTION;

-- Supprimer tables
DROP TABLE IF EXISTS jsonmidi_cache_stats;
DROP TABLE IF EXISTS editor_versions;
DROP TABLE IF EXISTS editor_sessions;

-- Supprimer vues
DROP VIEW IF EXISTS files_with_jsonmidi;
DROP VIEW IF EXISTS active_editor_sessions;
DROP VIEW IF EXISTS version_history;
DROP VIEW IF EXISTS get_latest_version;

-- Supprimer colonne (SQLite ne supporte pas ALTER TABLE DROP COLUMN avant 3.35)
-- Créer nouvelle table sans la colonne, copier données, renommer
CREATE TABLE midi_files_new AS 
SELECT 
    id, filepath, filename, size, duration, format, num_tracks, 
    tempo, time_signature, key_signature, metadata, hash, 
    last_scanned, created_at
FROM midi_files;

DROP TABLE midi_files;
ALTER TABLE midi_files_new RENAME TO midi_files;

-- Recréer indexes
CREATE INDEX idx_midi_files_filepath ON midi_files(filepath);
CREATE INDEX idx_midi_files_filename ON midi_files(filename);
CREATE INDEX idx_midi_files_last_scanned ON midi_files(last_scanned DESC);

-- Supprimer settings
DELETE FROM settings WHERE key LIKE 'jsonmidi_%' OR key LIKE 'editor_%';

-- Supprimer version
DELETE FROM schema_version WHERE version = 3;

COMMIT;
*/

-- ============================================================================
-- NOTES IMPORTANTES
-- ============================================================================

-- 1. PERFORMANCE
--    - La colonne jsonmidi peut être volumineuse (quelques Ko à plusieurs Mo)
--    - Activer la compression SQLite si possible
--    - Indexer correctement pour requêtes rapides

-- 2. CACHE
--    - Le cache JsonMidi est invalidé automatiquement si le fichier change
--    - La régénération se fait à la demande (lazy loading)
--    - Surveiller la taille de la DB

-- 3. VERSIONS
--    - Limité à 20 versions par fichier (configurable)
--    - Nettoyer régulièrement les anciennes versions
--    - Compresser les versions si nécessaire

-- 4. SESSIONS
--    - Auto-nettoyage des sessions > 7 jours
--    - Une session par user/fichier
--    - Utilisé pour auto-save et récupération

-- 5. MAINTENANCE
--    - VACUUM régulièrement pour optimiser
--    - Surveiller les statistiques cache
--    - Nettoyer les sessions inactives

-- ============================================================================
-- FIN DU FICHIER 003_add_jsonmidi.sql
-- ============================================================================