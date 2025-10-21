-- ============================================================================
-- File: backend/data/migrations/005_instruments_latency.sql
-- Version: 4.1.0 (Simplified - Manual Latency Only)
-- Project: MidiMind - MIDI Orchestration System for Raspberry Pi
-- ============================================================================
--
-- Description:
--   Migration pour ajouter la compensation de latence par instrument.
--   VERSION SIMPLIFIÉE : Réglage manuel uniquement + SysEx
--
-- Tables Created:
--   - instruments_latency (profils de latence des instruments)
--
-- Features:
--   - Compensation manuelle par l'utilisateur
--   - Récupération info via SysEx (calibration_method = 'sysex')
--   - Pas de calibration automatique
--   - Historique optionnel pour futures évolutions
--
-- Author: MidiMind Team
-- Date: 2025-10-16
--
-- Changes v4.1.0:
--   - Retrait de auto_calibration (compensation manuelle uniquement)
--   - Calibration methods: 'manual' ou 'sysex' uniquement
--   - Schéma simplifié pour usage immédiat
--
-- ============================================================================

-- Vérifier prérequis
SELECT CASE 
    WHEN NOT EXISTS (SELECT 1 FROM schema_version WHERE version = 1)
    THEN RAISE(ABORT, 'Migration 001 must be applied first')
    WHEN EXISTS (SELECT 1 FROM schema_version WHERE version = 5)
    THEN RAISE(ABORT, 'Migration 005 already applied')
END;

-- ============================================================================
-- BEGIN TRANSACTION
-- ============================================================================

BEGIN TRANSACTION;

-- ============================================================================
-- TABLE: instruments_latency
-- Description: Profils de latence par instrument (réglage manuel)
-- ============================================================================

CREATE TABLE IF NOT EXISTS instruments_latency (
    -- ========================================================================
    -- IDENTIFIANTS
    -- ========================================================================
    id TEXT PRIMARY KEY NOT NULL,
    
    -- Association au device
    device_id TEXT NOT NULL,
    channel INTEGER NOT NULL CHECK(channel BETWEEN 0 AND 15),
    
    -- ========================================================================
    -- MÉTADONNÉES
    -- ========================================================================
    name TEXT NOT NULL DEFAULT 'Unnamed Instrument',
    instrument_type TEXT DEFAULT 'unknown',
    
    -- ========================================================================
    -- COMPENSATION (microsecondes)
    -- ========================================================================
    -- Note: compensation_offset est NÉGATIF pour avancer le signal
    -- Exemple: -15000 = avancer de 15ms
    compensation_offset INTEGER DEFAULT 0,
    
    -- Latence moyenne mesurée (valeur absolue, pour stats)
    avg_latency INTEGER DEFAULT 0,
    
    -- Min/Max latency (optionnel, pour futures mesures)
    min_latency INTEGER DEFAULT 0,
    max_latency INTEGER DEFAULT 0,
    
    -- ========================================================================
    -- STATISTIQUES (optionnel, pour futures évolutions)
    -- ========================================================================
    jitter REAL DEFAULT 0.0,
    std_deviation REAL DEFAULT 0.0,
    measurement_count INTEGER DEFAULT 0,
    
    -- ========================================================================
    -- CALIBRATION
    -- ========================================================================
    -- Confidence: 0.0 = pas calibré, 1.0 = parfaitement calibré
    calibration_confidence REAL DEFAULT 0.0 
        CHECK(calibration_confidence BETWEEN 0.0 AND 1.0),
    
    -- Timestamp dernière calibration
    last_calibration TEXT,
    
    -- Méthode de calibration:
    --   'manual' = réglé manuellement par l'utilisateur
    --   'sysex'  = récupéré via Identity Request SysEx
    calibration_method TEXT DEFAULT 'manual' 
        CHECK(calibration_method IN ('manual', 'sysex')),
    
    -- ========================================================================
    -- ACTIVATION
    -- ========================================================================
    enabled BOOLEAN DEFAULT 1,
    
    -- ========================================================================
    -- HISTORIQUE (JSON optionnel pour futures évolutions)
    -- ========================================================================
    -- Format: [{"timestamp": "2025-10-16T10:00:00Z", "latency": 15000, "method": "manual"}]
    measurement_history TEXT,
    
    -- ========================================================================
    -- TIMESTAMPS
    -- ========================================================================
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    
    -- ========================================================================
    -- FOREIGN KEYS
    -- ========================================================================
    FOREIGN KEY(device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- ============================================================================
-- INDEXES POUR PERFORMANCE
-- ============================================================================

-- Recherche rapide par device
CREATE INDEX IF NOT EXISTS idx_instruments_device 
ON instruments_latency(device_id);

-- Recherche rapide par device + channel (requête courante)
CREATE INDEX IF NOT EXISTS idx_instruments_device_channel 
ON instruments_latency(device_id, channel);

-- Recherche rapide par channel
CREATE INDEX IF NOT EXISTS idx_instruments_channel 
ON instruments_latency(channel);

-- Filtrage rapide par enabled
CREATE INDEX IF NOT EXISTS idx_instruments_enabled 
ON instruments_latency(enabled) WHERE enabled = 1;

-- Tri par confidence (instruments les mieux calibrés en premier)
CREATE INDEX IF NOT EXISTS idx_instruments_confidence 
ON instruments_latency(calibration_confidence DESC);

-- Tri par dernière calibration
CREATE INDEX IF NOT EXISTS idx_instruments_last_calibration 
ON instruments_latency(last_calibration DESC);

-- ============================================================================
-- TRIGGERS
-- ============================================================================

-- Auto-update du timestamp updated_at
CREATE TRIGGER IF NOT EXISTS trg_instruments_latency_update
AFTER UPDATE ON instruments_latency
FOR EACH ROW
BEGIN
    UPDATE instruments_latency 
    SET updated_at = datetime('now') 
    WHERE id = NEW.id;
END;

-- Auto-update de calibration_confidence basé sur measurement_count
-- Plus de mesures = plus de confiance (max 1.0)
CREATE TRIGGER IF NOT EXISTS trg_instruments_latency_confidence
AFTER UPDATE OF measurement_count ON instruments_latency
FOR EACH ROW
WHEN NEW.measurement_count > OLD.measurement_count
BEGIN
    UPDATE instruments_latency 
    SET calibration_confidence = MIN(1.0, NEW.measurement_count * 0.05)
    WHERE id = NEW.id;
END;

-- Auto-update de compensation_offset = -avg_latency
-- La compensation est l'opposé de la latence mesurée
CREATE TRIGGER IF NOT EXISTS trg_instruments_latency_compensation
AFTER UPDATE OF avg_latency ON instruments_latency
FOR EACH ROW
WHEN NEW.avg_latency != OLD.avg_latency AND NEW.avg_latency > 0
BEGIN
    UPDATE instruments_latency 
    SET compensation_offset = -NEW.avg_latency
    WHERE id = NEW.id;
END;

-- ============================================================================
-- VIEWS UTILES
-- ============================================================================

-- Vue: Instruments actifs avec compensation
CREATE VIEW IF NOT EXISTS active_instruments AS
SELECT 
    id,
    device_id,
    channel,
    name,
    instrument_type,
    compensation_offset,
    calibration_method,
    last_calibration,
    enabled
FROM instruments_latency
WHERE enabled = 1
ORDER BY device_id, channel;

-- Vue: Instruments nécessitant une calibration
CREATE VIEW IF NOT EXISTS instruments_needing_calibration AS
SELECT 
    id,
    device_id,
    channel,
    name,
    calibration_confidence,
    last_calibration,
    measurement_count
FROM instruments_latency
WHERE calibration_confidence < 0.5 OR measurement_count = 0
ORDER BY calibration_confidence ASC;

-- Vue: Statistiques de calibration
CREATE VIEW IF NOT EXISTS calibration_stats AS
SELECT 
    COUNT(*) as total_instruments,
    SUM(CASE WHEN enabled = 1 THEN 1 ELSE 0 END) as enabled_count,
    SUM(CASE WHEN calibration_method = 'manual' THEN 1 ELSE 0 END) as manual_count,
    SUM(CASE WHEN calibration_method = 'sysex' THEN 1 ELSE 0 END) as sysex_count,
    AVG(CASE WHEN enabled = 1 THEN calibration_confidence ELSE NULL END) as avg_confidence,
    AVG(CASE WHEN enabled = 1 THEN compensation_offset ELSE NULL END) as avg_compensation_us,
    AVG(CASE WHEN enabled = 1 THEN compensation_offset ELSE NULL END) / 1000.0 as avg_compensation_ms
FROM instruments_latency;

-- ============================================================================
-- DONNÉES D'EXEMPLE (optionnel, commenter en production)
-- ============================================================================

-- Exemple 1: Piano avec compensation manuelle
INSERT INTO instruments_latency (
    id, device_id, channel, name, instrument_type,
    avg_latency, compensation_offset, calibration_confidence,
    calibration_method, enabled
) VALUES (
    'piano_device_usb_128_0',
    'device_usb_128_0',
    0,
    'Grand Piano',
    'piano',
    15000,              -- 15ms latency
    -15000,             -- Avancer de 15ms
    1.0,                -- Parfaitement calibré
    'manual',
    1
);

-- Exemple 2: Strings avec info SysEx
INSERT INTO instruments_latency (
    id, device_id, channel, name, instrument_type,
    avg_latency, compensation_offset, calibration_confidence,
    calibration_method, enabled, 
    measurement_history
) VALUES (
    'strings_device_usb_128_1',
    'device_usb_128_0',
    1,
    'Orchestral Strings',
    'strings',
    18000,              -- 18ms latency
    -18000,             -- Avancer de 18ms
    0.9,                -- Bonne confiance
    'sysex',            -- Récupéré via SysEx
    1,
    '[{"timestamp":"2025-10-16T10:00:00Z","latency":18000,"method":"sysex"}]'
);

-- Exemple 3: Bass non calibré (compensation par défaut)
INSERT INTO instruments_latency (
    id, device_id, channel, name, instrument_type,
    compensation_offset, calibration_confidence,
    calibration_method, enabled
) VALUES (
    'bass_device_usb_129_0',
    'device_usb_129_0',
    0,
    'Synth Bass',
    'bass',
    0,                  -- Pas de compensation
    0.0,                -- Non calibré
    'manual',
    1
);

-- ============================================================================
-- ENREGISTRER LA MIGRATION
-- ============================================================================

INSERT INTO schema_version (version, description) 
VALUES (5, 'Add instruments_latency table (manual compensation only)');

-- ============================================================================
-- COMMIT TRANSACTION
-- ============================================================================

COMMIT;

-- ============================================================================
-- VÉRIFICATION POST-MIGRATION
-- ============================================================================

-- Vérifier création de la table
SELECT 
    CASE 
        WHEN NOT EXISTS (SELECT 1 FROM sqlite_master WHERE type='table' AND name='instruments_latency')
        THEN RAISE(ABORT, 'Migration 005 failed - instruments_latency table not created')
    END;

-- Vérifier indexes (devrait être 6)
SELECT 
    CASE 
        WHEN (SELECT COUNT(*) FROM sqlite_master 
              WHERE type='index' AND tbl_name='instruments_latency') < 6
        THEN RAISE(ABORT, 'Migration 005 failed - Missing indexes')
    END;

-- Vérifier triggers (devrait être 3)
SELECT 
    CASE 
        WHEN (SELECT COUNT(*) FROM sqlite_master 
              WHERE type='trigger' AND tbl_name='instruments_latency') < 3
        THEN RAISE(ABORT, 'Migration 005 failed - Missing triggers')
    END;

-- Vérifier views (devrait être 3)
SELECT 
    CASE 
        WHEN (SELECT COUNT(*) FROM sqlite_master 
              WHERE type='view' AND name IN ('active_instruments', 
                                             'instruments_needing_calibration', 
                                             'calibration_stats')) < 3
        THEN RAISE(ABORT, 'Migration 005 failed - Missing views')
    END;

-- Afficher résumé de migration
SELECT 
    'Migration 005 completed successfully' as status,
    (SELECT version FROM schema_version ORDER BY version DESC LIMIT 1) as current_version,
    (SELECT COUNT(*) FROM instruments_latency) as instruments_count,
    (SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND tbl_name='instruments_latency') as indexes_count,
    (SELECT COUNT(*) FROM sqlite_master WHERE type='trigger' AND tbl_name='instruments_latency') as triggers_count,
    (SELECT COUNT(*) FROM sqlite_master WHERE type='view') as views_count;

-- Afficher statistiques
SELECT * FROM calibration_stats;

-- ============================================================================
-- INSTRUCTIONS DE ROLLBACK (pour documentation)
-- ============================================================================

-- Pour annuler cette migration, exécuter:
/*
BEGIN TRANSACTION;

-- Supprimer les views
DROP VIEW IF EXISTS calibration_stats;
DROP VIEW IF EXISTS instruments_needing_calibration;
DROP VIEW IF EXISTS active_instruments;

-- Supprimer les triggers
DROP TRIGGER IF EXISTS trg_instruments_latency_compensation;
DROP TRIGGER IF EXISTS trg_instruments_latency_confidence;
DROP TRIGGER IF EXISTS trg_instruments_latency_update;

-- Supprimer les indexes
DROP INDEX IF EXISTS idx_instruments_last_calibration;
DROP INDEX IF EXISTS idx_instruments_confidence;
DROP INDEX IF EXISTS idx_instruments_enabled;
DROP INDEX IF EXISTS idx_instruments_channel;
DROP INDEX IF EXISTS idx_instruments_device_channel;
DROP INDEX IF EXISTS idx_instruments_device;

-- Supprimer la table
DROP TABLE IF EXISTS instruments_latency;

-- Supprimer l'entrée de version
DELETE FROM schema_version WHERE version = 5;

COMMIT;
*/

-- ============================================================================
-- NOTES D'UTILISATION
-- ============================================================================

-- Créer un nouvel instrument (manual):
/*
INSERT INTO instruments_latency (id, device_id, channel, name, compensation_offset, calibration_method)
VALUES ('my_synth_dev1_0', 'device_1', 0, 'My Synth', -12000, 'manual');
*/

-- Mettre à jour la compensation:
/*
UPDATE instruments_latency 
SET compensation_offset = -15000, 
    avg_latency = 15000,
    last_calibration = datetime('now')
WHERE id = 'my_synth_dev1_0';
*/

-- Récupérer tous les instruments actifs:
/*
SELECT * FROM active_instruments;
*/

-- Trouver instruments nécessitant calibration:
/*
SELECT * FROM instruments_needing_calibration;
*/

-- Voir statistiques globales:
/*
SELECT * FROM calibration_stats;
*/

-- ============================================================================
-- END OF FILE 005_instruments_latency.sql
-- ============================================================================
