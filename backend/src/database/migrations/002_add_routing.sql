-- ============================================================================
-- Fichier: database/migrations/002_add_routing.sql
-- Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
-- ============================================================================
-- Description:
--   Migration v1 → v2 - Ajout des tables de routage MIDI
--   - Routes MIDI configurables
--   - Gestion des devices
--   - Playlists de fichiers MIDI
--
-- Auteur: MidiMind Team
-- Date: 2025-10-05
-- Version: 2.0.0
-- ============================================================================

-- Vérifier le prérequis: version 1 doit être appliquée
SELECT CASE 
    WHEN NOT EXISTS (SELECT 1 FROM schema_version WHERE version = 1)
    THEN RAISE(ABORT, 'Migration 001 must be applied first')
    WHEN EXISTS (SELECT 1 FROM schema_version WHERE version = 2)
    THEN RAISE(ABORT, 'Migration 002 already applied')
END;

-- ============================================================================
-- DÉBUT TRANSACTION
-- ============================================================================

BEGIN TRANSACTION;

-- ============================================================================
-- NOUVELLES TABLES
-- ============================================================================

-- Table des routes MIDI
CREATE TABLE IF NOT EXISTS routes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    source_device_id TEXT,
    source_channel INTEGER CHECK(source_channel BETWEEN 1 AND 16),
    destination_device_id TEXT NOT NULL,
    destination_channel INTEGER CHECK(destination_channel BETWEEN 1 AND 16),
    priority INTEGER DEFAULT 0,
    enabled BOOLEAN DEFAULT 1,
    filters TEXT,  -- JSON: {"note_range": [60, 72], "velocity_min": 0}
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Table des devices MIDI
CREATE TABLE IF NOT EXISTS devices (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    type TEXT NOT NULL CHECK(type IN ('usb', 'wifi', 'bluetooth', 'virtual')),
    port TEXT,
    status TEXT DEFAULT 'disconnected' CHECK(status IN ('connected', 'disconnected', 'error')),
    last_seen TEXT,
    config TEXT,  -- JSON avec config spécifique
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Table des fichiers MIDI
CREATE TABLE IF NOT EXISTS midi_files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    filepath TEXT NOT NULL UNIQUE,
    filename TEXT NOT NULL,
    size INTEGER NOT NULL,
    duration INTEGER,
    format INTEGER CHECK(format IN (0, 1, 2)),
    num_tracks INTEGER,
    tempo INTEGER,
    time_signature TEXT,
    key_signature TEXT,
    metadata TEXT,
    hash TEXT,
    last_scanned TEXT NOT NULL DEFAULT (datetime('now')),
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Table des playlists
CREATE TABLE IF NOT EXISTS playlists (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    description TEXT,
    is_favorite BOOLEAN DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Table des items de playlist
CREATE TABLE IF NOT EXISTS playlist_items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    playlist_id INTEGER NOT NULL,
    midi_file_id INTEGER NOT NULL,
    position INTEGER NOT NULL,
    FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,
    FOREIGN KEY (midi_file_id) REFERENCES midi_files(id) ON DELETE CASCADE,
    UNIQUE(playlist_id, position)
);

-- ============================================================================
-- NOUVEAUX INDEX
-- ============================================================================

-- Routes
CREATE INDEX IF NOT EXISTS idx_routes_enabled ON routes(enabled);
CREATE INDEX IF NOT EXISTS idx_routes_priority ON routes(priority DESC);
CREATE INDEX IF NOT EXISTS idx_routes_source ON routes(source_device_id);
CREATE INDEX IF NOT EXISTS idx_routes_destination ON routes(destination_device_id);

-- Devices
CREATE INDEX IF NOT EXISTS idx_devices_type ON devices(type);
CREATE INDEX IF NOT EXISTS idx_devices_status ON devices(status);

-- MIDI Files
CREATE INDEX IF NOT EXISTS idx_midi_files_filepath ON midi_files(filepath);
CREATE INDEX IF NOT EXISTS idx_midi_files_filename ON midi_files(filename);
CREATE INDEX IF NOT EXISTS idx_midi_files_last_scanned ON midi_files(last_scanned DESC);

-- Playlists
CREATE INDEX IF NOT EXISTS idx_playlist_items_playlist ON playlist_items(playlist_id);
CREATE INDEX IF NOT EXISTS idx_playlist_items_position ON playlist_items(playlist_id, position);

-- ============================================================================
-- TRIGGERS AUTO-UPDATE
-- ============================================================================

CREATE TRIGGER IF NOT EXISTS update_routes_timestamp
AFTER UPDATE ON routes
BEGIN
    UPDATE routes SET updated_at = datetime('now') WHERE id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS update_devices_timestamp
AFTER UPDATE ON devices
BEGIN
    UPDATE devices SET updated_at = datetime('now') WHERE id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS update_playlists_timestamp
AFTER UPDATE ON playlists
BEGIN
    UPDATE playlists SET updated_at = datetime('now') WHERE id = NEW.id;
END;

-- ============================================================================
-- VUES UTILES
-- ============================================================================

-- Vue: Routes actives avec détails
CREATE VIEW IF NOT EXISTS active_routes_view AS
SELECT 
    r.id,
    r.name,
    r.source_device_id,
    sd.name as source_device_name,
    r.destination_device_id,
    dd.name as destination_device_name,
    r.source_channel,
    r.destination_channel,
    r.priority,
    r.enabled
FROM routes r
LEFT JOIN devices sd ON r.source_device_id = sd.id
LEFT JOIN devices dd ON r.destination_device_id = dd.id
WHERE r.enabled = 1
ORDER BY r.priority DESC;

-- Vue: Devices connectés
CREATE VIEW IF NOT EXISTS connected_devices AS
SELECT * FROM devices
WHERE status = 'connected'
ORDER BY name;

-- Vue: Statistiques fichiers MIDI
CREATE VIEW IF NOT EXISTS midi_files_stats AS
SELECT 
    COUNT(*) as total_files,
    SUM(size) as total_size,
    AVG(duration) as avg_duration,
    COUNT(DISTINCT format) as formats_count
FROM midi_files;

-- ============================================================================
-- DONNÉES INITIALES
-- ============================================================================

-- Device virtuel par défaut
INSERT OR IGNORE INTO devices (id, name, type, status) VALUES
('virtual_0', 'Virtual MIDI Port', 'virtual', 'connected');

-- Route par défaut (tous vers virtual_0)
INSERT OR IGNORE INTO routes (id, name, destination_device_id, priority, enabled) VALUES
(1, 'Default Route', 'virtual_0', 100, 1);

-- Playlist par défaut
INSERT OR IGNORE INTO playlists (id, name, description) VALUES
(1, 'Favorites', 'Morceaux favoris');

-- ============================================================================
-- MIGRATION DE DONNÉES EXISTANTES (si nécessaire)
-- ============================================================================

-- Exemple: Migrer ancien format de preset vers nouveau
-- UPDATE presets SET data = json_set(data, '$.routing_enabled', 1) 
-- WHERE category = 'routing' AND json_extract(data, '$.routing_enabled') IS NULL;

-- ============================================================================
-- NOUVEAUX SETTINGS
-- ============================================================================

INSERT OR IGNORE INTO settings (key, value, type, description) VALUES
('routing_enabled', 'true', 'bool', 'Système de routage activé'),
('max_routes', '32', 'int', 'Nombre maximum de routes simultanées'),
('auto_scan_devices', 'true', 'bool', 'Scan automatique des devices'),
('midi_files_directory', '/home/pi/midi-files', 'string', 'Répertoire des fichiers MIDI'),
('auto_scan_midi_files', 'true', 'bool', 'Scan automatique des fichiers MIDI');

-- ============================================================================
-- ENREGISTREMENT DE LA MIGRATION
-- ============================================================================

INSERT INTO schema_version (version, description) 
VALUES (2, 'Routing system - Routes, devices, playlists, MIDI files');

-- ============================================================================
-- FIN TRANSACTION
-- ============================================================================

COMMIT;

-- ============================================================================
-- VÉRIFICATION POST-MIGRATION
-- ============================================================================

-- Vérifier que toutes les nouvelles tables existent
SELECT 
    CASE 
        WHEN (SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN 
            ('routes', 'devices', 'midi_files', 'playlists', 'playlist_items')) < 5
        THEN RAISE(ABORT, 'Migration 002 failed - Missing tables')
    END;

-- Afficher un résumé
SELECT 
    'Migration 002 completed successfully' as status,
    (SELECT version FROM schema_version ORDER BY version DESC LIMIT 1) as current_version,
    (SELECT COUNT(*) FROM routes) as total_routes,
    (SELECT COUNT(*) FROM devices) as total_devices,
    (SELECT COUNT(*) FROM midi_files) as total_midi_files,
    (SELECT COUNT(*) FROM playlists) as total_playlists;

-- ============================================================================
-- NOTES DE MIGRATION
-- ============================================================================

-- Cette migration ajoute:
-- ✓ Système de routage MIDI flexible
-- ✓ Gestion des périphériques MIDI
-- ✓ Base de données de fichiers MIDI
-- ✓ Système de playlists
-- ✓ Vues optimisées pour requêtes fréquentes
-- ✓ Triggers pour maintenance automatique

-- Pour rollback (si nécessaire):
-- BEGIN TRANSACTION;
-- DROP TABLE IF EXISTS playlist_items;
-- DROP TABLE IF EXISTS playlists;
-- DROP TABLE IF EXISTS midi_files;
-- DROP TABLE IF EXISTS devices;
-- DROP TABLE IF EXISTS routes;
-- DROP VIEW IF EXISTS active_routes_view;
-- DROP VIEW IF EXISTS connected_devices;
-- DROP VIEW IF EXISTS midi_files_stats;
-- DELETE FROM schema_version WHERE version = 2;
-- COMMIT;

-- ============================================================================
-- FIN DU FICHIER 002_add_routing.sql
-- ============================================================================