// ============================================================================
// Fichier: backend/src/api/editor/EditorState.h
// Version: 3.1.0
// Date: 2025-10-10
// Projet: MidiMind v3.1 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestionnaire d'état centralisé pour l'éditeur MIDI.
//   Gère le fichier en cours d'édition, l'historique undo/redo,
//   et la synchronisation avec la base de données.
//
// Fonctionnalités:
//   - Chargement/sauvegarde de fichiers MIDI en JsonMidi
//   - Historique undo/redo avec snapshots complets (50 niveaux max)
//   - Détection de modifications non sauvegardées
//   - Thread-safety avec mutex
//   - Statistiques d'édition
//
// Architecture:
//   - Singleton pattern (instance globale)
//   - Snapshots immuables pour undo/redo
//   - Deque circulaire pour historique limité
//
// Auteur: MidiMind Team
// Statut: ✅ PHASE 1 - COMPLET
// ============================================================================

#ifndef MIDIMIND_EDITOR_STATE_H
#define MIDIMIND_EDITOR_STATE_H

#include <string>
#include <deque>
#include <mutex>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>
namespace midiMind {

using json = nlohmann::json;

// ============================================================================
// STRUCTURE: Snapshot
// Représente un état sauvegardé pour undo/redo
// ============================================================================
struct Snapshot {
    json data;                              // JsonMidi complet
    std::string description;                // Description de l'action
    std::chrono::system_clock::time_point timestamp;
    
    Snapshot(const json& d, const std::string& desc)
        : data(d)
        , description(desc)
        , timestamp(std::chrono::system_clock::now())
    {}
};

// ============================================================================
// CLASSE: EditorState
// Gestionnaire d'état de l'éditeur MIDI
// ============================================================================
class EditorState {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    EditorState();
    ~EditorState();
    
    // Non-copiable
    EditorState(const EditorState&) = delete;
    EditorState& operator=(const EditorState&) = delete;
    
    // ========================================================================
    // GESTION DU FICHIER
    // ========================================================================
    
    /**
     * @brief Charge un fichier MIDI dans l'éditeur
     * 
     * @param fileId ID du fichier dans la base de données
     * @param jsonMidi Données JsonMidi du fichier
     * @param filepath Chemin complet du fichier
     * 
     * @note Réinitialise l'historique undo/redo
     * @note Thread-safe
     */
    void load(const std::string& fileId, 
              const json& jsonMidi, 
              const std::string& filepath);
    
    /**
     * @brief Sauvegarde le fichier actuel
     * 
     * @return true si sauvegarde réussie
     * @note Marque le fichier comme non modifié
     * @note Vide l'historique redo
     * @note Thread-safe
     */
    bool save();
    
    /**
     * @brief Ferme le fichier actuel
     * 
     * @param saveIfModified Si true, sauvegarde avant fermeture
     * @note Thread-safe
     */
    void close(bool saveIfModified = false);
    
    /**
     * @brief Vérifie si un fichier est chargé
     */
    bool hasFile() const;
    
    /**
     * @brief Vérifie si le fichier a été modifié
     */
    bool isModified() const;
    
    /**
     * @brief Marque le fichier comme modifié
     */
    void markModified();
    
    /**
     * @brief Marque le fichier comme sauvegardé
     */
    void markSaved();
    
    // ========================================================================
    // ACCÈS AUX DONNÉES
    // ========================================================================
    
    /**
     * @brief Obtient une référence au JsonMidi actuel
     * 
     * @return json& Référence mutable au JsonMidi
     * @warning Appeler markModified() après modification
     * @note Thread-safe (lock pendant l'accès)
     */
    json& getData();
    
    /**
     * @brief Obtient une copie du JsonMidi actuel
     * 
     * @return json Copie du JsonMidi
     * @note Thread-safe
     */
    json getDataCopy() const;
    
    /**
     * @brief Remplace le JsonMidi actuel
     * 
     * @param newData Nouvelles données JsonMidi
     * @note Marque automatiquement comme modifié
     * @note Thread-safe
     */
    void setData(const json& newData);
    
    /**
     * @brief Obtient l'ID du fichier actuel
     */
    std::string getFileId() const;
    
    /**
     * @brief Obtient le chemin du fichier actuel
     */
    std::string getFilePath() const;
    
    // ========================================================================
    // HISTORIQUE UNDO/REDO
    // ========================================================================
    
    /**
     * @brief Sauvegarde l'état actuel dans l'historique
     * 
     * @param description Description de l'action (ex: "Add note")
     * 
     * @note À appeler AVANT toute modification
     * @note Vide automatiquement le stack redo
     * @note Limite à maxHistory_ snapshots
     * @note Thread-safe
     */
    void pushUndo(const std::string& description);
    
    /**
     * @brief Vérifie si undo est possible
     */
    bool canUndo() const;
    
    /**
     * @brief Vérifie si redo est possible
     */
    bool canRedo() const;
    
    /**
     * @brief Annule la dernière action
     * 
     * @return true si undo réussi
     * @note Restaure le snapshot précédent
     * @note Déplace le snapshot actuel vers redo
     * @note Thread-safe
     */
    bool undo();
    
    /**
     * @brief Refait la dernière action annulée
     * 
     * @return true si redo réussi
     * @note Restaure le snapshot suivant
     * @note Déplace le snapshot actuel vers undo
     * @note Thread-safe
     */
    bool redo();
    
    /**
     * @brief Vide l'historique undo/redo
     * 
     * @note Généralement appelé après save()
     * @note Thread-safe
     */
    void clearHistory();
    
    /**
     * @brief Obtient la description de la prochaine action undo
     * 
     * @return std::optional<std::string> Description ou nullopt si pas d'undo
     */
    std::optional<std::string> getUndoDescription() const;
    
    /**
     * @brief Obtient la description de la prochaine action redo
     * 
     * @return std::optional<std::string> Description ou nullopt si pas de redo
     */
    std::optional<std::string> getRedoDescription() const;
    
    // ========================================================================
    // INFORMATIONS D'ÉTAT
    // ========================================================================
    
    /**
     * @brief Obtient les informations d'état complètes
     * 
     * @return json Objet contenant:
     *   - fileId: ID du fichier
     *   - filepath: Chemin du fichier
     *   - modified: Fichier modifié
     *   - hasFile: Fichier chargé
     *   - canUndo: Undo possible
     *   - canRedo: Redo possible
     *   - undoCount: Nombre d'undo disponibles
     *   - redoCount: Nombre de redo disponibles
     *   - undoDescription: Description prochaine undo
     *   - redoDescription: Description prochaine redo
     * 
     * @note Thread-safe
     */
    json getStateInfo() const;
    
    /**
     * @brief Obtient les statistiques d'édition
     * 
     * @return json Objet contenant:
     *   - totalNotes: Nombre total de notes
     *   - totalCC: Nombre de Control Changes
     *   - totalTracks: Nombre de pistes
     *   - duration: Durée en ms
     *   - undoStackSize: Taille historique undo
     *   - redoStackSize: Taille historique redo
     * 
     * @note Thread-safe
     */
    json getStats() const;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit la taille maximale de l'historique
     * 
     * @param maxHistory Nombre maximum de snapshots (défaut: 50)
     */
    void setMaxHistory(size_t maxHistory);
    
    /**
     * @brief Obtient la taille maximale de l'historique
     */
    size_t getMaxHistory() const;
    
private:
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    // Données du fichier actuel
    std::string fileId_;                    // ID base de données
    std::string filepath_;                  // Chemin complet
    json jsonMidi_;                         // Données JsonMidi
    bool modified_;                         // Fichier modifié
    
    // Historique undo/redo
    std::deque<Snapshot> undoStack_;        // Stack undo
    std::deque<Snapshot> redoStack_;        // Stack redo
    size_t maxHistory_;                     // Limite historique (défaut: 50)
    
    // Thread-safety
    mutable std::mutex mutex_;              // Mutex pour accès concurrent
    
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Limite la taille du stack undo
     * 
     * @note Supprime les snapshots les plus anciens si dépassement
     */
    void limitUndoStack();
    
    /**
     * @brief Calcule les statistiques du JsonMidi actuel
     * 
     * @return json Statistiques
     */
    json calculateStats() const;
};

// ============================================================================
// INSTANCE GLOBALE (Singleton)
// ============================================================================

/**
 * @brief Obtient l'instance globale d'EditorState
 * 
 * @return EditorState& Instance singleton
 * 
 * @note Thread-safe (initialisé de manière thread-safe en C++11+)
 */
EditorState& getEditorState();

} // namespace midiMind

#endif // MIDIMIND_EDITOR_STATE_H

// ============================================================================
// FIN DU FICHIER EditorState.h
// ============================================================================
