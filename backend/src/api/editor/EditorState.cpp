// ============================================================================
// Fichier: backend/src/api/editor/EditorState.cpp
// Version: 3.2.0
// Date: 2025-10-13
// Projet: MidiMind v3.2 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Implémentation du gestionnaire d'état de l'éditeur MIDI.
//   Gère le cycle de vie complet d'une session d'édition avec intégration
//   MidiFileManager pour la sauvegarde réelle.
//
// Modifications v3.2.0:
//   ✅ save() - Intégration complète avec MidiFileManager via DIContainer
//   ✅ Ajout validation JsonMidi avant sauvegarde
//   ✅ Gestion d'erreurs améliorée
//   ✅ Logging détaillé des opérations
//
// Auteur: MidiMind Team
// Statut: ✅ PHASE 1 - COMPLET + INTÉGRATION
// ============================================================================

#include "EditorState.h"
#include "../../core/Logger.h"
#include "../../core/patterns/DIContainer.h"
#include "../../midi/MidiFileManager.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

EditorState::EditorState()
    : fileId_("")
    , filepath_("")
    , jsonMidi_(json::object())
    , modified_(false)
    , maxHistory_(50)
{
    Logger::debug("EditorState", "Constructor called");
}

EditorState::~EditorState() {
    Logger::debug("EditorState", "Destructor called");
    
    // Sauvegarde automatique si modifié
    if (modified_ && !fileId_.empty()) {
        Logger::warn("EditorState", "File modified but not saved, auto-saving...");
        save();
    }
}

// ============================================================================
// GESTION DU FICHIER
// ============================================================================

void EditorState::load(const std::string& fileId, 
                       const json& jsonMidi, 
                       const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("EditorState", "Loading file: " + fileId);
    
    // Sauvegarde du fichier précédent si modifié
    if (modified_ && !fileId_.empty()) {
        Logger::warn("EditorState", "Previous file was modified, saving...");
        // Note: unlock temporairement pour éviter deadlock dans save()
        mutex_.unlock();
        save();
        mutex_.lock();
    }
    
    // Chargement du nouveau fichier
    fileId_ = fileId;
    filepath_ = filepath;
    jsonMidi_ = jsonMidi;
    modified_ = false;
    
    // Réinitialisation de l'historique
    undoStack_.clear();
    redoStack_.clear();
    
    Logger::info("EditorState", "File loaded successfully: " + filepath_);
}

bool EditorState::save() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (fileId_.empty()) {
        Logger::error("EditorState", "Cannot save: no file loaded");
        return false;
    }
    
    if (!modified_) {
        Logger::debug("EditorState", "File not modified, skip save");
        return true;
    }
    
    Logger::info("EditorState", "Saving file: " + filepath_);
    
    try {
        // ✅ INTÉGRATION: Résoudre MidiFileManager via DIContainer
        auto fileManager = DIContainer::instance().resolve<MidiFileManager>();
        
        if (!fileManager) {
            Logger::error("EditorState", "MidiFileManager not available in DIContainer");
            return false;
        }
        
        // ✅ Validation du JsonMidi avant sauvegarde
        if (!fileManager->validateJsonMidi(jsonMidi_)) {
            Logger::error("EditorState", "Invalid JsonMidi structure, cannot save");
            return false;
        }
        
        // ✅ Sauvegarde réelle via MidiFileManager
        bool success = fileManager->saveFromJsonMidi(fileId_, jsonMidi_);
        
        if (!success) {
            Logger::error("EditorState", "MidiFileManager failed to save file");
            return false;
        }
        
        // ✅ Marquer comme sauvegardé
        modified_ = false;
        
        // Vider le redo car save = point de référence
        redoStack_.clear();
        
        // Note: On ne vide PAS l'undo pour permettre undo après save
        // Commenté: undoStack_.clear();
        
        Logger::info("EditorState", "✓ File saved successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("EditorState", "Save failed: " + std::string(e.what()));
        return false;
    }
}

void EditorState::close(bool saveIfModified) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("EditorState", "Closing file: " + fileId_);
    
    if (saveIfModified && modified_) {
        Logger::info("EditorState", "Saving before close");
        // Unlock temporairement pour save()
        mutex_.unlock();
        save();
        mutex_.lock();
    }
    
    fileId_.clear();
    filepath_.clear();
    jsonMidi_ = json::object();
    modified_ = false;
    
    undoStack_.clear();
    redoStack_.clear();
    
    Logger::info("EditorState", "File closed");
}

bool EditorState::hasFile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !fileId_.empty();
}

bool EditorState::isModified() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return modified_;
}

void EditorState::markModified() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!modified_) {
        modified_ = true;
        Logger::debug("EditorState", "File marked as modified");
    }
}

void EditorState::markSaved() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (modified_) {
        modified_ = false;
        Logger::debug("EditorState", "File marked as saved");
    }
}

// ============================================================================
// ACCÈS AUX DONNÉES
// ============================================================================

json& EditorState::getData() {
    std::lock_guard<std::mutex> lock(mutex_);
    return jsonMidi_;
}

json EditorState::getDataCopy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return jsonMidi_;
}

void EditorState::setData(const json& newData) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    jsonMidi_ = newData;
    modified_ = true;
    
    Logger::debug("EditorState", "Data replaced, marked as modified");
}

std::string EditorState::getFileId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return fileId_;
}

std::string EditorState::getFilePath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return filepath_;
}

// ============================================================================
// HISTORIQUE UNDO/REDO
// ============================================================================

void EditorState::pushUndo(const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (fileId_.empty()) {
        Logger::warn("EditorState", "Cannot push undo: no file loaded");
        return;
    }
    
    Logger::debug("EditorState", "Pushing undo: " + description);
    
    // Créer un snapshot de l'état actuel
    Snapshot snapshot(jsonMidi_, description);
    undoStack_.push_back(snapshot);
    
    // Limiter la taille du stack
    limitUndoStack();
    
    // Vider le redo car nouvelle branche d'historique
    redoStack_.clear();
    
    Logger::debug("EditorState", 
        "Undo stack size: " + std::to_string(undoStack_.size()));
}

bool EditorState::canUndo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !undoStack_.empty();
}

bool EditorState::canRedo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !redoStack_.empty();
}

bool EditorState::undo() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (undoStack_.empty()) {
        Logger::warn("EditorState", "Cannot undo: stack is empty");
        return false;
    }
    
    Logger::debug("EditorState", "Performing undo");
    
    // Sauvegarder l'état actuel dans redo
    Snapshot currentSnapshot(jsonMidi_, "Current state");
    redoStack_.push_back(currentSnapshot);
    
    // Restaurer l'état précédent
    Snapshot& previousSnapshot = undoStack_.back();
    jsonMidi_ = previousSnapshot.data;
    modified_ = true;
    
    Logger::info("EditorState", "Undo: " + previousSnapshot.description);
    
    // Retirer du stack
    undoStack_.pop_back();
    
    return true;
}

bool EditorState::redo() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (redoStack_.empty()) {
        Logger::warn("EditorState", "Cannot redo: stack is empty");
        return false;
    }
    
    Logger::debug("EditorState", "Performing redo");
    
    // Sauvegarder l'état actuel dans undo
    Snapshot currentSnapshot(jsonMidi_, "Current state");
    undoStack_.push_back(currentSnapshot);
    
    // Restaurer l'état suivant
    Snapshot& nextSnapshot = redoStack_.back();
    jsonMidi_ = nextSnapshot.data;
    modified_ = true;
    
    Logger::info("EditorState", "Redo: " + nextSnapshot.description);
    
    // Retirer du stack
    redoStack_.pop_back();
    
    return true;
}

void EditorState::clearHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    undoStack_.clear();
    redoStack_.clear();
    
    Logger::debug("EditorState", "History cleared");
}

size_t EditorState::getUndoStackSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return undoStack_.size();
}

size_t EditorState::getRedoStackSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return redoStack_.size();
}

// ============================================================================
// STATISTIQUES
// ============================================================================

json EditorState::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    return {
        {"file_id", fileId_},
        {"filepath", filepath_},
        {"modified", modified_},
        {"undo_available", undoStack_.size()},
        {"redo_available", redoStack_.size()},
        {"max_history", maxHistory_}
    };
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void EditorState::limitUndoStack() {
    // Appelé avec lock déjà acquis
    
    if (undoStack_.size() > maxHistory_) {
        size_t toRemove = undoStack_.size() - maxHistory_;
        
        Logger::debug("EditorState", 
            "Limiting undo stack: removing " + std::to_string(toRemove) + " entries");
        
        undoStack_.erase(undoStack_.begin(), undoStack_.begin() + toRemove);
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER EditorState.cpp v3.2.0
// ============================================================================