-- ============================================================================
-- Fichier: database/schema.sql
-- Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
-- ============================================================================
-- Description:
--   Schéma complet de la base de données SQLite pour MidiMind.
--   Contient toutes les tables nécessaires au fonctionnement du système.
--
-- Auteur: MidiMind Team
-- Date: 2025-10-05
-- Version: 1.0.0
-- ============================================================================

-- ============================================================================
-- TABLE: schema_version
-- Description: Gestion des versions du schéma pour migrations
-- ============================================================================

CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL DEFAULT (datetime('now')),
    description TEXT
);

-- Version initiale
INSERT OR IGNORE INTO schema_version (version, description) 
VALUES (1, 'Initial schema creation');

-- ============================================================================
-- TABLE: presets
-- Description: Presets de routage et effets MIDI
-- ============================================================================

CREATE TABLE IF NOT EXISTS presets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    category TEXT NOT NULL DEFAULT 'general',
    data TEXT NOT NULL,  -- JSON avec config complète
    tags TEXT,           -- JSON array de tags
    is_favorite BOOLEAN DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_presets_category ON presets(category);
CREATE INDEX IF NOT EXISTS idx_presets_favorite ON presets(is_favorite);

-- Preset par défaut
INSERT OR IGNORE INTO presets (id, name, category, data) VALUES 
(1, 'Default Routing', 'routing', '{"routes": [], "channels": []}');

-- ============================================================================
-- TABLE: sessions
-- Description: Sessions de travail sauvegardées
-- ============================================================================

CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    description TEXT,
    data TEXT NOT NULL,  -- JSON avec état complet
    thumbnail BLOB,      -- Image miniature
    duration INTEGER,    -- Durée en ms
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    last_opened TEXT
);

CREATE INDEX IF NOT EXISTS idx_sessions_last_opened ON sessions(last_opened DESC);

-- ============================================================================
-- TABLE: settings
-- Description: Paramètres globaux de l'application
-- ============================================================================

CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    type TEXT NOT NULL DEFAULT 'string',  -- string, int, bool, json
    description TEXT,
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Settings par défaut
INSERT OR IGNORE INTO settings (key, value, type, description) VALUES
('audio_sample_rate', '48000', 'int', 'Fréquence d\'échantillonnage audio'),
('audio_buffer_size', '256', 'int', 'Taille du buffer audio'),
('midi_clock_bpm', '120', 'int', 'BPM de l\'horloge MIDI'),
('auto_save_enabled', 'true', 'bool', 'Sauvegarde automatique activée'),
('auto_save_interval', '300', 'int', 'Intervalle auto-save (secondes)'),
('ui_theme', 'dark', 'string', 'Thème de l\'interface'),
('visualizer_quality', 'high', 'string', 'Qualité du visualiseur'),
('midi_latency_compensation', '10', 'int', 'Compensation latence MIDI (ms)');

-- ============================================================================
-- TABLE: midi_history
-- Description: Historique des messages MIDI (debug/analyse)
-- ============================================================================

CREATE TABLE IF NOT EXISTS midi_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,  -- Unix timestamp ms
    device_id TEXT NOT NULL,
    message_type TEXT NOT NULL,  -- note_on, note_off, cc, etc.
    channel INTEGER,
    note INTEGER,
    velocity INTEGER,
    cc_number INTEGER,
    cc_value INTEGER,
    data TEXT  -- JSON pour autres données
);

CREATE INDEX IF NOT EXISTS idx_midi_history_timestamp ON midi_history(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_midi_history_device ON midi_history(device_id);
CREATE INDEX IF NOT EXISTS idx_midi_history_type ON midi_history(message_type);

-- ============================================================================
-- TABLE: routes
-- Description: Configuration de routage MIDI
-- ============================================================================

CREATE TABLE IF NOT EXISTS routes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    source_device_id TEXT,
    source_channel INTEGER,
    destination_device_id TEXT NOT NULL,
    destination_channel INTEGER,
    priority INTEGER DEFAULT 0,
    enabled BOOLEAN DEFAULT 1,
    filters TEXT,  -- JSON avec règles de filtrage
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_routes_enabled ON routes(enabled);
CREATE INDEX IF NOT EXISTS idx_routes_priority ON routes(priority DESC);

-- ============================================================================
-- TABLE: midi_files
-- Description: Métadonnées des fichiers MIDI scannés
-- ============================================================================

CREATE TABLE IF NOT EXISTS midi_files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    filepath TEXT NOT NULL UNIQUE,
    filename TEXT NOT NULL,
    size INTEGER NOT NULL,
    duration INTEGER,  -- Durée en ms
    format INTEGER,    -- 0, 1, ou 2
    num_tracks INTEGER,
    tempo INTEGER,
    time_signature TEXT,  -- "4/4", "3/4", etc.
    key_signature TEXT,
    metadata TEXT,  -- JSON avec infos supplémentaires
    hash TEXT,      -- SHA256 pour détecter modifications
    last_scanned TEXT NOT NULL DEFAULT (datetime('now')),
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_midi_files_filepath ON midi_files(filepath);
CREATE INDEX IF NOT EXISTS idx_midi_files_filename ON midi_files(filename);
CREATE INDEX IF NOT EXISTS idx_midi_files_last_scanned ON midi_files(last_scanned DESC);

-- ============================================================================
-- TABLE: playlists
-- Description: Playlists de fichiers MIDI
-- ============================================================================

CREATE TABLE IF NOT EXISTS playlists (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    description TEXT,
    is_favorite BOOLEAN DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS playlist_items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    playlist_id INTEGER NOT NULL,
    midi_file_id INTEGER NOT NULL,
    position INTEGER NOT NULL,
    FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,
    FOREIGN KEY (midi_file_id) REFERENCES midi_files(id) ON DELETE CASCADE,
    UNIQUE(playlist_id, position)
);

CREATE INDEX IF NOT EXISTS idx_playlist_items_playlist ON playlist_items(playlist_id);
CREATE INDEX IF NOT EXISTS idx_playlist_items_position ON playlist_items(playlist_id, position);

-- ============================================================================
-- TABLE: processors
-- Description: Configuration des processeurs MIDI (effets)
-- ============================================================================

CREATE TABLE IF NOT EXISTS processors (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    route_id INTEGER,
    type TEXT NOT NULL,  -- transpose, arpeggiator, delay, etc.
    name TEXT NOT NULL,
    position INTEGER NOT NULL,  -- Position dans la chaîne
    enabled BOOLEAN DEFAULT 1,
    parameters TEXT NOT NULL,  -- JSON avec paramètres
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (route_id) REFERENCES routes(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_processors_route ON processors(route_id);
CREATE INDEX IF NOT EXISTS idx_processors_position ON processors(route_id, position);

-- ============================================================================
-- TABLE: devices
-- Description: Périphériques MIDI connus
-- ============================================================================

CREATE TABLE IF NOT EXISTS devices (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    type TEXT NOT NULL,  -- usb, wifi, bluetooth, virtual
    port TEXT,
    status TEXT DEFAULT 'disconnected',  -- connected, disconnected, error
    last_seen TEXT,
    config TEXT,  -- JSON avec config spécifique
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_devices_type ON devices(type);
CREATE INDEX IF NOT EXISTS idx_devices_status ON devices(status);

-- ============================================================================
-- TABLE: logs
-- Description: Logs système pour debugging
-- ============================================================================

CREATE TABLE IF NOT EXISTS logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL DEFAULT (datetime('now')),
    level TEXT NOT NULL,  -- debug, info, warn, error
    category TEXT,
    message TEXT NOT NULL,
    data TEXT  -- JSON avec contexte
);

CREATE INDEX IF NOT EXISTS idx_logs_timestamp ON logs(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_logs_level ON logs(level);
CREATE INDEX IF NOT EXISTS idx_logs_category ON logs(category);

-- ============================================================================
-- TABLE: performance_metrics
-- Description: Métriques de performance pour monitoring
-- ============================================================================

CREATE TABLE IF NOT EXISTS performance_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    metric_name TEXT NOT NULL,
    value REAL NOT NULL,
    unit TEXT,
    category TEXT,  -- latency, throughput, cpu, memory
    data TEXT  -- JSON avec détails
);

CREATE INDEX IF NOT EXISTS idx_metrics_timestamp ON performance_metrics(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_metrics_name ON performance_metrics(metric_name);
CREATE INDEX IF NOT EXISTS idx_metrics_category ON performance_metrics(category);

-- ============================================================================
-- TABLE: user_shortcuts
-- Description: Raccourcis clavier personnalisés
-- ============================================================================

CREATE TABLE IF NOT EXISTS user_shortcuts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    action TEXT NOT NULL UNIQUE,
    shortcut TEXT NOT NULL,
    description TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Raccourcis par défaut
INSERT OR IGNORE INTO user_shortcuts (action, shortcut, description) VALUES
('play', 'Space', 'Lecture/Pause'),
('stop', 'Escape', 'Arrêter la lecture'),
('save', 'Ctrl+S', 'Sauvegarder'),
('undo', 'Ctrl+Z', 'Annuler'),
('redo', 'Ctrl+Y', 'Refaire'),
('delete', 'Delete', 'Supprimer sélection'),
('select_all', 'Ctrl+A', 'Tout sélectionner'),
('copy', 'Ctrl+C', 'Copier'),
('paste', 'Ctrl+V', 'Coller'),
('quantize', 'Ctrl+Q', 'Quantifier notes');

-- ============================================================================
-- TRIGGERS: Auto-update timestamps
-- ============================================================================

CREATE TRIGGER IF NOT EXISTS update_presets_timestamp
AFTER UPDATE ON presets
BEGIN
    UPDATE presets SET updated_at = datetime('now') WHERE id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS update_sessions_timestamp
AFTER UPDATE ON sessions
BEGIN
    UPDATE sessions SET updated_at = datetime('now') WHERE id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS update_settings_timestamp
AFTER UPDATE ON settings
BEGIN
    UPDATE settings SET updated_at = datetime('now') WHERE key = NEW.key;
END;

CREATE TRIGGER IF NOT EXISTS update_routes_timestamp
AFTER UPDATE ON routes
BEGIN
    UPDATE routes SET updated_at = datetime('now') WHERE id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS update_processors_timestamp
AFTER UPDATE ON processors
BEGIN
    UPDATE processors SET updated_at = datetime('now') WHERE id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS update_devices_timestamp
AFTER UPDATE ON devices
BEGIN
    UPDATE devices SET updated_at = datetime('now') WHERE id = NEW.id;
END;

-- ============================================================================
-- VIEWS: Vues utiles
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
    r.priority,
    r.enabled,
    COUNT(p.id) as processor_count
FROM routes r
LEFT JOIN devices sd ON r.source_device_id = sd.id
LEFT JOIN devices dd ON r.destination_device_id = dd.id
LEFT JOIN processors p ON r.id = p.route_id AND p.enabled = 1
WHERE r.enabled = 1
GROUP BY r.id
ORDER BY r.priority DESC;

-- Vue: Statistiques fichiers MIDI
CREATE VIEW IF NOT EXISTS midi_files_stats AS
SELECT 
    COUNT(*) as total_files,
    SUM(size) as total_size,
    AVG(duration) as avg_duration,
    MIN(duration) as min_duration,
    MAX(duration) as max_duration,
    COUNT(DISTINCT format) as formats_count
FROM midi_files;

-- Vue: Logs récents par niveau
CREATE VIEW IF NOT EXISTS recent_logs_by_level AS
SELECT 
    level,
    COUNT(*) as count,
    MAX(timestamp) as last_occurrence
FROM logs
WHERE timestamp > datetime('now', '-1 hour')
GROUP BY level;

-- ============================================================================
-- NETTOYAGE AUTOMATIQUE
-- ============================================================================

-- Trigger: Limiter l'historique MIDI à 10000 entrées
CREATE TRIGGER IF NOT EXISTS cleanup_midi_history
AFTER INSERT ON midi_history
WHEN (SELECT COUNT(*) FROM midi_history) > 10000
BEGIN
    DELETE FROM midi_history
    WHERE id IN (
        SELECT id FROM midi_history
        ORDER BY timestamp ASC
        LIMIT 1000
    );
END;

-- Trigger: Limiter les logs à 5000 entrées
CREATE TRIGGER IF NOT EXISTS cleanup_logs
AFTER INSERT ON logs
WHEN (SELECT COUNT(*) FROM logs) > 5000
BEGIN
    DELETE FROM logs
    WHERE id IN (
        SELECT id FROM logs
        ORDER BY timestamp ASC
        LIMIT 500
    );
END;

-- ============================================================================
-- FONCTIONS UTILITAIRES (via JSON)
-- ============================================================================

-- Les requêtes courantes peuvent être stockées comme vues
-- Exemple: fichiers MIDI récents
CREATE VIEW IF NOT EXISTS recent_midi_files AS
SELECT * FROM midi_files
ORDER BY last_scanned DESC
LIMIT 50;

-- Exemple: Devices connectés
CREATE VIEW IF NOT EXISTS connected_devices AS
SELECT * FROM devices
WHERE status = 'connected'
ORDER BY name;

-- ============================================================================
-- COMMENTAIRES FINAUX
-- ============================================================================

-- Ce schéma est optimisé pour:
-- - Performance (indexes appropriés)
-- - Maintenance (triggers auto-update)
-- - Monitoring (tables metrics et logs)
-- - Évolutivité (système de versions)
--
-- Pour appliquer ce schéma:
-- sqlite3 midimind.db < schema.sql
--
-- Pour backup:
-- sqlite3 midimind.db ".backup backup.db"
--
-- Pour optimiser:
-- sqlite3 midimind.db "VACUUM; ANALYZE;"

-- ============================================================================
-- FIN DU FICHIER schema.sql
-- ============================================================================