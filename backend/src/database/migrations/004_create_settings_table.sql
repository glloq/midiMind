-- ============================================================================
-- Fichier: backend/src/database/migrations/004_create_settings_table.sql
-- Projet: MidiMind v3.0
-- Version: 1.0.0
-- Date: 2025-10-15
-- ============================================================================
-- Description:
--   Migration pour créer la table `settings` utilisée par Settings.cpp
--
-- Fonctionnalités:
--   - Stockage key/value pour paramètres application
--   - Clé primaire sur `key` (unique)
--   - Timestamps de création et modification
--   - Index pour recherche rapide
-- ============================================================================

-- Créer la table settings
CREATE TABLE IF NOT EXISTS settings (
    -- Clé du paramètre (unique)
    key TEXT PRIMARY KEY NOT NULL,
    
    -- Valeur du paramètre (stockée comme TEXT)
    value TEXT NOT NULL DEFAULT '',
    
    -- Timestamps
    created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
);

-- Index pour recherche rapide par clé
CREATE INDEX IF NOT EXISTS idx_settings_key ON settings(key);

-- Index pour tri par date de modification
CREATE INDEX IF NOT EXISTS idx_settings_updated ON settings(updated_at DESC);

-- Trigger pour mettre à jour automatiquement updated_at
CREATE TRIGGER IF NOT EXISTS update_settings_timestamp 
AFTER UPDATE ON settings
BEGIN
    UPDATE settings 
    SET updated_at = strftime('%s', 'now')
    WHERE key = NEW.key;
END;

-- ============================================================================
-- VALEURS PAR DÉFAUT (optionnel - peut être géré par Settings.cpp)
-- ============================================================================

-- Paramètres MIDI
INSERT OR IGNORE INTO settings (key, value) VALUES ('midi.input_device', 'default');
INSERT OR IGNORE INTO settings (key, value) VALUES ('midi.output_device', 'default');
INSERT OR IGNORE INTO settings (key, value) VALUES ('midi.clock_source', 'internal');
INSERT OR IGNORE INTO settings (key, value) VALUES ('midi.sync_enabled', 'true');
INSERT OR IGNORE INTO settings (key, value) VALUES ('midi.default_channel', '1');
INSERT OR IGNORE INTO settings (key, value) VALUES ('midi.clock_bpm', '120');

-- Paramètres Audio
INSERT OR IGNORE INTO settings (key, value) VALUES ('audio.sample_rate', '48000');
INSERT OR IGNORE INTO settings (key, value) VALUES ('audio.buffer_size', '256');
INSERT OR IGNORE INTO settings (key, value) VALUES ('audio.channels', '2');

-- Paramètres UI
INSERT OR IGNORE INTO settings (key, value) VALUES ('ui.theme', 'dark');
INSERT OR IGNORE INTO settings (key, value) VALUES ('ui.auto_save', 'true');
INSERT OR IGNORE INTO settings (key, value) VALUES ('ui.auto_save_interval', '300');
INSERT OR IGNORE INTO settings (key, value) VALUES ('ui.show_tooltips', 'true');

-- Paramètres Système
INSERT OR IGNORE INTO settings (key, value) VALUES ('system.log_level', 'info');
INSERT OR IGNORE INTO settings (key, value) VALUES ('system.startup_mode', 'normal');
INSERT OR IGNORE INTO settings (key, value) VALUES ('system.enable_monitoring', 'true');

-- Paramètres Réseau
INSERT OR IGNORE INTO settings (key, value) VALUES ('network.wifi_enabled', 'false');
INSERT OR IGNORE INTO settings (key, value) VALUES ('network.rtpmidi_enabled', 'false');
INSERT OR IGNORE INTO settings (key, value) VALUES ('network.rtpmidi_port', '5004');

-- ============================================================================
-- VUES UTILES
-- ============================================================================

-- Vue pour lister tous les paramètres avec timestamps lisibles
CREATE VIEW IF NOT EXISTS v_settings_readable AS
SELECT 
    key,
    value,
    datetime(created_at, 'unixepoch') AS created_at_readable,
    datetime(updated_at, 'unixepoch') AS updated_at_readable
FROM settings
ORDER BY key;

-- ============================================================================
-- REQUÊTES UTILES POUR DEBUG
-- ============================================================================

-- Afficher tous les paramètres
-- SELECT * FROM v_settings_readable;

-- Rechercher un paramètre
-- SELECT * FROM settings WHERE key LIKE 'midi.%';

-- Compter les paramètres
-- SELECT COUNT(*) FROM settings;

-- Paramètres modifiés récemment (dernières 24h)
-- SELECT * FROM v_settings_readable 
-- WHERE updated_at > strftime('%s', 'now', '-1 day');

-- ============================================================================
-- FIN DU FICHIER 004_create_settings_table.sql
-- ============================================================================