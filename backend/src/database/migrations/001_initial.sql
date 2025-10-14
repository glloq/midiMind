-- ============================================================================
-- Fichier: database/migrations/001_initial.sql
-- Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
-- ============================================================================
-- Description:
--   Migration initiale - Création du schéma de base v1.0
--   Cette migration est appliquée lors de la première installation
--
-- Auteur: MidiMind Team
-- Date: 2025-10-05
-- Version: 1.0.0
-- ============================================================================

-- Vérifier si cette migration a déjà été appliquée
-- La table schema_version doit exister pour ce check
SELECT CASE 
    WHEN EXISTS (SELECT 1 FROM schema_version WHERE version = 1)
    THEN RAISE(ABORT, 'Migration 001 already applied')
END;

-- ============================================================================
-- DÉBUT TRANSACTION
-- ============================================================================

BEGIN TRANSACTION;

-- ============================================================================
-- CRÉATION DES TABLES DE BASE
-- ============================================================================

-- Table de versioning (si elle n'existe pas déjà)
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL DEFAULT (datetime('now')),
    description TEXT
);

-- Presets
CREATE TABLE IF NOT EXISTS presets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    category TEXT NOT NULL DEFAULT 'general',
    data TEXT NOT NULL,
    tags TEXT,
    is_favorite BOOLEAN DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Sessions
CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    description TEXT,
    data TEXT NOT NULL,
    thumbnail BLOB,
    duration INTEGER,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    last_opened TEXT
);

-- Settings
CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    type TEXT NOT NULL DEFAULT 'string',
    description TEXT,
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- MIDI History
CREATE TABLE IF NOT EXISTS midi_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    device_id TEXT NOT NULL,
    message_type TEXT NOT NULL,
    channel INTEGER,
    note INTEGER,
    velocity INTEGER,
    cc_number INTEGER,
    cc_value INTEGER,
    data TEXT
);

-- ============================================================================
-- CRÉATION DES INDEX
-- ============================================================================

CREATE INDEX IF NOT EXISTS idx_presets_category ON presets(category);
CREATE INDEX IF NOT EXISTS idx_presets_favorite ON presets(is_favorite);
CREATE INDEX IF NOT EXISTS idx_sessions_last_opened ON sessions(last_opened DESC);
CREATE INDEX IF NOT EXISTS idx_midi_history_timestamp ON midi_history(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_midi_history_device ON midi_history(device_id);

-- ============================================================================
-- INSERTION DES DONNÉES INITIALES
-- ============================================================================

-- Settings par défaut
INSERT OR IGNORE INTO settings (key, value, type, description) VALUES
('audio_sample_rate', '48000', 'int', 'Fréquence d''échantillonnage audio'),
('audio_buffer_size', '256', 'int', 'Taille du buffer audio'),
('midi_clock_bpm', '120', 'int', 'BPM de l''horloge MIDI'),
('auto_save_enabled', 'true', 'bool', 'Sauvegarde automatique activée'),
('ui_theme', 'dark', 'string', 'Thème de l''interface');

-- Preset par défaut
INSERT OR IGNORE INTO presets (id, name, category, data) VALUES 
(1, 'Default Routing', 'routing', '{"routes": [], "channels": []}');

-- ============================================================================
-- ENREGISTREMENT DE LA MIGRATION
-- ============================================================================

INSERT INTO schema_version (version, description) 
VALUES (1, 'Initial schema - Base tables and indexes');

-- ============================================================================
-- FIN TRANSACTION
-- ============================================================================

COMMIT;

-- ============================================================================
-- VÉRIFICATION POST-MIGRATION
-- ============================================================================

-- Vérifier que toutes les tables ont été créées
SELECT 
    CASE 
        WHEN (SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN 
            ('schema_version', 'presets', 'sessions', 'settings', 'midi_history')) < 5
        THEN RAISE(ABORT, 'Migration 001 failed - Missing tables')
    END;

-- Afficher un résumé
SELECT 
    'Migration 001 completed successfully' as status,
    (SELECT version FROM schema_version ORDER BY version DESC LIMIT 1) as current_version,
    (SELECT COUNT(*) FROM sqlite_master WHERE type='table') as total_tables,
    (SELECT COUNT(*) FROM sqlite_master WHERE type='index') as total_indexes;

-- ============================================================================
-- FIN DU FICHIER 001_initial.sql
-- ============================================================================