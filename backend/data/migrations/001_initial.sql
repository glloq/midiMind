-- ============================================================================
-- File: backend/data/migrations/001_initial.sql
-- Version: 4.1.0
-- Project: MidiMind - MIDI Orchestration System for Raspberry Pi
-- ============================================================================
--
-- Description:
--   Initial database schema migration.
--   Creates base tables for MidiMind v4.1.0.
--
-- Tables Created:
--   - schema_version (migration tracking)
--   - settings (key-value configuration)
--   - presets (routing/processing presets)
--   - sessions (playback sessions)
--   - midi_history (MIDI message log)
--
-- Author: MidiMind Team
-- Date: 2025-10-16
--
-- Changes v4.1.0:
--   - Initial schema for v4.1.0
--   - Simplified structure (removed network tables for v4.2.0)
--   - Optimized indexes
--
-- ============================================================================

-- Check if migration already applied
SELECT CASE 
    WHEN EXISTS (SELECT 1 FROM sqlite_master WHERE type='table' AND name='schema_version')
        AND EXISTS (SELECT 1 FROM schema_version WHERE version = 1)
    THEN RAISE(ABORT, 'Migration 001 already applied')
END;

-- ============================================================================
-- BEGIN TRANSACTION
-- ============================================================================

BEGIN TRANSACTION;

-- ============================================================================
-- TABLE: schema_version
-- Description: Track database migrations
-- ============================================================================

CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL DEFAULT (datetime('now')),
    description TEXT NOT NULL,
    checksum TEXT
);

-- ============================================================================
-- TABLE: settings
-- Description: Application settings (key-value store)
-- ============================================================================

CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY NOT NULL,
    value TEXT NOT NULL DEFAULT '',
    type TEXT NOT NULL DEFAULT 'string' CHECK(type IN ('string', 'int', 'float', 'bool', 'json')),
    description TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Index for fast lookups
CREATE INDEX IF NOT EXISTS idx_settings_key ON settings(key);

-- Trigger to auto-update updated_at
CREATE TRIGGER IF NOT EXISTS trg_settings_update
AFTER UPDATE ON settings
BEGIN
    UPDATE settings SET updated_at = datetime('now') WHERE key = NEW.key;
END;

-- ============================================================================
-- TABLE: presets
-- Description: Routing and processing presets
-- ============================================================================

CREATE TABLE IF NOT EXISTS presets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    category TEXT NOT NULL DEFAULT 'routing' CHECK(category IN ('routing', 'processing', 'playback', 'system')),
    description TEXT,
    data TEXT NOT NULL,  -- JSON configuration
    tags TEXT,           -- JSON array of tags
    is_favorite BOOLEAN DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Indexes for fast queries
CREATE INDEX IF NOT EXISTS idx_presets_name ON presets(name);
CREATE INDEX IF NOT EXISTS idx_presets_category ON presets(category);
CREATE INDEX IF NOT EXISTS idx_presets_favorite ON presets(is_favorite);
CREATE INDEX IF NOT EXISTS idx_presets_updated ON presets(updated_at DESC);

-- Trigger to auto-update updated_at
CREATE TRIGGER IF NOT EXISTS trg_presets_update
AFTER UPDATE ON presets
BEGIN
    UPDATE presets SET updated_at = datetime('now') WHERE id = NEW.id;
END;

-- ============================================================================
-- TABLE: sessions
-- Description: Playback sessions
-- ============================================================================

CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    description TEXT,
    data TEXT NOT NULL,  -- JSON configuration
    duration INTEGER DEFAULT 0,  -- Duration in milliseconds
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    last_opened TEXT
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_sessions_name ON sessions(name);
CREATE INDEX IF NOT EXISTS idx_sessions_last_opened ON sessions(last_opened DESC);

-- Trigger to auto-update updated_at
CREATE TRIGGER IF NOT EXISTS trg_sessions_update
AFTER UPDATE ON sessions
BEGIN
    UPDATE sessions SET updated_at = datetime('now') WHERE id = NEW.id;
END;

-- ============================================================================
-- TABLE: midi_history
-- Description: MIDI message history (limited size, auto-prune)
-- ============================================================================

CREATE TABLE IF NOT EXISTS midi_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL DEFAULT (datetime('now')),
    device_id TEXT NOT NULL,
    channel INTEGER CHECK(channel BETWEEN 0 AND 15),
    type TEXT NOT NULL,  -- 'note_on', 'note_off', 'cc', etc.
    data1 INTEGER CHECK(data1 BETWEEN 0 AND 127),
    data2 INTEGER CHECK(data2 BETWEEN 0 AND 127),
    raw_data BLOB
);

-- Index for time-based queries
CREATE INDEX IF NOT EXISTS idx_midi_history_timestamp ON midi_history(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_midi_history_device ON midi_history(device_id);
CREATE INDEX IF NOT EXISTS idx_midi_history_type ON midi_history(type);

-- Trigger to auto-prune old history (keep last 10000 entries)
CREATE TRIGGER IF NOT EXISTS trg_midi_history_prune
AFTER INSERT ON midi_history
WHEN (SELECT COUNT(*) FROM midi_history) > 10000
BEGIN
    DELETE FROM midi_history WHERE id IN (
        SELECT id FROM midi_history 
        ORDER BY timestamp ASC 
        LIMIT (SELECT COUNT(*) - 10000 FROM midi_history)
    );
END;

-- ============================================================================
-- DEFAULT DATA
-- ============================================================================

-- Default settings
INSERT INTO settings (key, value, type, description) VALUES 
('api_port', '8080', 'int', 'WebSocket API server port'),
('log_level', 'INFO', 'string', 'Logging level (DEBUG, INFO, WARNING, ERROR)'),
('midi_clock_bpm', '120', 'int', 'MIDI clock BPM'),
('auto_save_enabled', 'true', 'bool', 'Auto-save configuration'),
('max_history_size', '10000', 'int', 'Maximum MIDI history entries'),
('hot_plug_enabled', 'true', 'bool', 'Enable hot-plug device detection'),
('status_broadcast_interval', '5000', 'int', 'Status broadcast interval (ms)');

-- Default preset
INSERT INTO presets (name, category, description, data) VALUES 
('Default Routing', 'routing', 'Default routing configuration', 
 '{"routes": [], "channels": [], "filters": []}');

-- ============================================================================
-- REGISTER MIGRATION
-- ============================================================================

INSERT INTO schema_version (version, description) 
VALUES (1, 'Initial schema - Base tables and indexes');

-- ============================================================================
-- COMMIT TRANSACTION
-- ============================================================================

COMMIT;

-- ============================================================================
-- POST-MIGRATION VERIFICATION
-- ============================================================================

-- Verify all tables were created
SELECT 
    CASE 
        WHEN (SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN 
            ('schema_version', 'settings', 'presets', 'sessions', 'midi_history')) < 5
        THEN RAISE(ABORT, 'Migration 001 failed - Missing tables')
    END;

-- Display summary
SELECT 
    'Migration 001 completed successfully' as status,
    (SELECT version FROM schema_version ORDER BY version DESC LIMIT 1) as current_version,
    (SELECT COUNT(*) FROM sqlite_master WHERE type='table') as total_tables,
    (SELECT COUNT(*) FROM sqlite_master WHERE type='index') as total_indexes,
    (SELECT COUNT(*) FROM sqlite_master WHERE type='trigger') as total_triggers;

-- ============================================================================
-- END OF FILE 001_initial.sql
-- ============================================================================