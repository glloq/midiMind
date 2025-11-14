# AUDIT COMPLET DU FRONTEND - midiMind
**Date**: 2025-11-14
**Version Auditée**: v4.x
**Auditeur**: Claude (Anthropic)

---

## TABLE DES MATIÈRES
1. [Vue d'ensemble](#vue-densemble)
2. [Sélection de fichiers MIDI](#1-sélection-de-fichiers-midi)
3. [Modification/Édition de fichiers MIDI](#2-modificationédition-de-fichiers-midi)
4. [Routing des canaux MIDI](#3-routing-des-canaux-midi)
5. [Affichage des notes à venir](#4-affichage-des-notes-à-venir-sur-le-visualiseur)
6. [Sélection de playlist](#5-sélection-de-playlist-depuis-home)
7. [Bugs critiques identifiés](#bugs-critiques-identifiés)
8. [Recommandations](#recommandations)

---

## VUE D'ENSEMBLE

### Architecture
- **Type**: Vanilla JavaScript (pas de React)
- **Pattern**: MVC (Model-View-Controller)
- **EventBus**: Communication pub/sub globale
- **Fichiers**: 108 fichiers JavaScript

### Composants Clés
- **HomeView** (`frontend/js/views/HomeView.js` v4.2.0)
- **FileView** (`frontend/js/views/FileView.js` v4.3.0)
- **EditorView** (`frontend/js/views/EditorView.js` v4.0.1)
- **RoutingView** (`frontend/js/views/RoutingView.js` v4.1.0)
- **FileSelectionModal** (`frontend/js/views/components/FileSelectionModal.js` v4.2.2)

---

## 1. SÉLECTION DE FICHIERS MIDI

### 📁 Depuis l'accueil (HomeView)

#### ✅ Fonctionnement Actuel
**Fichier**: `frontend/js/views/HomeView.js:422-465`

La sélection de fichiers se fait via :
1. **Interface compacte** avec liste de fichiers affichés par `renderFilesList()`
2. **Actions disponibles**:
   - Bouton "Play" : `playFile()` → émet `home:play_file_requested`
   - Bouton "Load" : `loadFile()` → émet `home:load_file_requested`

```javascript
// HomeView.js:438-463
renderFileItem(file) {
    return `
        <div class="file-item ${isActive ? 'active' : ''}"
             data-file-path="${file.path || file.name}">
            <div class="file-icon">🎵</div>
            <div class="file-info">
                <div class="file-name">${file.name}</div>
                <div class="file-meta">
                    <span>${duration}</span>
                    <span>•</span>
                    <span>${size}</span>
                </div>
            </div>
            <div class="file-actions">
                <button class="btn-play" data-action="play-file" title="Lire">▶</button>
                <button class="btn-load" data-action="load-file" title="Charger">📂</button>
            </div>
        </div>
    `;
}
```

#### 🔁 Flux de Données
```
HomeView.playFile()
  → emit('home:play_file_requested', {file_path})
  → HomeController.loadAndPlayFile()
  → GlobalPlaybackController.load() + play()
```

#### ⚠️ BUGS IDENTIFIÉS

**BUG #1: Double initialisation possible**
- **Fichier**: `HomeView.js:60-100`
- **Sévérité**: MOYENNE
- **Description**: Les flags `state.initialized` et `state.rendered` peuvent être contournés si `init()` est appelé plusieurs fois
- **Preuve**:
```javascript
// HomeView.js:60-65
init() {
    if (this.state.initialized) {
        this.logger.warn('[HomeView] Already initialized, skipping');
        return;  // ⚠️ Mais les event listeners peuvent déjà être attachés
    }
```

**BUG #2: Métadonnées manquantes**
- **Fichier**: `HomeView.js:442-443`
- **Sévérité**: BASSE
- **Description**: `duration` et `size` affichent "—" si non disponibles, mais ne tente pas de charger ces données
- **Impact**: Expérience utilisateur dégradée

**BUG #3: Gestion d'erreur manquante**
- **Fichier**: `HomeView.js:710-732`
- **Sévérité**: MOYENNE
- **Description**: `playFile()` et `loadFile()` émettent des événements mais ne gèrent pas les erreurs de chargement
```javascript
async playFile(file) {
    if (!this.eventBus) return;
    try {
        this.eventBus.emit('home:play_file_requested', {
            file_path: file.path || file.name
        });
    } catch (error) {
        this.logger.error('[HomeView] Play file error:', error);
        // ⚠️ Pas de feedback utilisateur!
    }
}
```

### 📋 Depuis la page Files (FileView)

#### ✅ Fonctionnement Actuel
**Fichier**: `frontend/js/views/FileView.js:149-209`

Interface compacte (40px par ligne) avec **5 boutons d'action**:
1. **Détails** (📋) : Affiche les métadonnées
2. **Éditer** (✏️) : Ouvre dans l'éditeur
3. **Router** (🔀) : Configure le routing
4. **Jouer** (▶️) : Lance la lecture
5. **Supprimer** (🗑️) : Supprime le fichier

```javascript
// FileView.js:172-209
buildFileRow(file) {
    return `
        <div class="file-row ${isSelected ? 'selected' : ''}">
            <div class="file-icon">🎵</div>
            <div class="file-info">
                <div class="file-name">${this.escapeHtml(file.name)}</div>
                <div class="file-meta">
                    ${this.formatFileSize(file.size)} • ${this.formatDate(file.modified)}
                    ${file.tracks ? ` • ${file.tracks} pistes` : ''}
                </div>
            </div>
            <div class="file-actions">
                <button data-action="select-file" title="Détails">📋</button>
                <button data-action="edit-file" title="Éditer">✏️</button>
                <button data-action="route-file" title="Router">🔀</button>
                <button data-action="play-file" title="Jouer">▶️</button>
                <button data-action="delete-file" title="Supprimer">🗑️</button>
            </div>
        </div>
    `;
}
```

#### ⚠️ BUGS IDENTIFIÉS

**BUG #4: Duplication d'event listeners**
- **Fichier**: `FileView.js:504-579`
- **Sévérité**: CRITIQUE
- **Description**: Flag `domEventsAttached` vérifié mais le listener global click reste attaché
- **Preuve**:
```javascript
// FileView.js:504-519
attachEvents() {
    if (!this.container) return;

    if (this.domEventsAttached) {
        this.log('debug', 'FileView', 'DOM events already attached, skipping');
        return;  // ⚠️ Mais l'event listener sur container est déjà là!
    }

    this.container.addEventListener('click', (e) => {
        // Ce listener peut être attaché plusieurs fois si render() est appelé
    });
}
```
**Solution**: Utiliser `removeEventListener` ou stocker la référence au handler

**BUG #5: Metadata enrichment optionnelle**
- **Fichier**: `FileController.js:115-162`
- **Sévérité**: MOYENNE
- **Description**: L'enrichissement des métadonnées (durée, pistes) est optionnel mais ne gère pas les erreurs de chargement MIDI
```javascript
// FileController.js:186-204
if (isMidiFile) {
    try {
        const midiData = await this.backend.loadMidi(filePath);
        // ⚠️ Appel backend pour CHAQUE fichier = potentiel goulot
    } catch (error) {
        // Erreur silencieuse, continue avec données de base
        this.log('warn', 'FileController', `Failed to enrich ${file.name}`);
    }
}
```
**Impact**: Si 50 fichiers, 50 appels backend séquentiels!

---

## 2. MODIFICATION/ÉDITION DE FICHIERS MIDI

### ✅ Fonctionnement Actuel
**Fichier**: `frontend/js/views/EditorView.js`

#### Architecture de l'Éditeur
1. **EditorView** (v4.0.1): Interface principale
2. **MidiVisualizer**: Rendu graphique des notes
3. **PianoRollView**: Édition graphique type DAW

#### Fonctionnalités
- ✅ Chargement de fichiers MIDI
- ✅ Édition des notes (ajout/suppression/déplacement)
- ✅ Outils: Select, Pencil, Eraser
- ✅ Zoom/Pan
- ✅ Undo/Redo (via EditorModel)
- ✅ Sauvegarde

```javascript
// EditorView.js:43-88
buildTemplate(data = {}) {
    return `
        <div class="editor-view">
            <div class="editor-toolbar">
                <button data-action="load">📂</button>
                <button data-action="save">💾</button>
                <button data-action="tool-select">↖️</button>
                <button data-action="tool-pencil">✏️</button>
                <button data-action="tool-eraser">🗑️</button>
                <button data-action="zoom-in">🔍+</button>
                <button data-action="zoom-out">🔍-</button>
            </div>
            <div class="editor-main">
                <div class="editor-sidebar">
                    <h3>Pistes</h3>
                    <div class="tracks-list">...</div>
                </div>
                <div class="editor-canvas-container">
                    <canvas id="editorCanvas"></canvas>
                </div>
            </div>
        </div>
    `;
}
```

#### 🔁 Flux d'Édition
```
FileView.handleEditFile()
  → emit('file:load_in_editor', {file_path})
  → FileController.handleLoadInEditor()
  → backend.loadMidi(filePath)
  → emit('editor:fileLoaded', {midi_json})
  → EditorView.render() + extractNotes()
```

#### ⚠️ BUGS IDENTIFIÉS

**BUG #6: Perte de données non sauvegardées**
- **Fichier**: `EditorView.js:295-312`
- **Sévérité**: CRITIQUE
- **Description**: Chargement d'un nouveau fichier écrase `viewState.currentFile` sans vérifier `isModified`
```javascript
// EditorView.js:244-263
this.eventBus.on('editor:fileLoaded', (data) => {
    this.viewState.currentFile = {
        name: data.file_path?.split(/[/\\]/).pop() || 'Unknown',
        path: data.file_path
    };

    if (data.midi_json) {
        this.viewState.midiData = data.midi_json;  // ⚠️ Écrase les données!
        this.viewState.tracks = data.midi_json.tracks || [];
        this.extractNotes();
    }

    this.render();  // ⚠️ Pas de confirmation si isModified = true
});
```

**BUG #7: Canvas resize non géré**
- **Fichier**: `EditorView.js:392-398`
- **Sévérité**: MOYENNE
- **Description**: `resizeCanvas()` est appelé mais pas sur window.resize
```javascript
// EditorView.js:165-167
initializeCanvas() {
    this.setupCanvas();  // Appelle resizeCanvas()
    // ⚠️ Mais pas de listener window.resize!
}
```
**Impact**: Canvas conserve la taille initiale même si la fenêtre est redimensionnée

**BUG #8: Duplication d'event listeners (EditorView)**
- **Fichier**: `EditorView.js:180-216`
- **Sévérité**: HAUTE
- **Description**: Même problème que FileView - flag vérifié mais listeners pas nettoyés
```javascript
// EditorView.js:180-192
attachEvents() {
    super.attachEvents();

    if (this.domEventsAttached) {
        this.log('debug', 'EditorView', 'DOM events already attached, skipping');
        return;
    }

    this.container.addEventListener('click', (e) => {
        // ⚠️ Peut être attaché plusieurs fois
    });
}
```

**BUG #9: Gestion canvas events**
- **Fichier**: `EditorView.js:218-230`
- **Sévérité**: MOYENNE
- **Description**: Les event listeners canvas ne sont jamais retirés
```javascript
// EditorView.js:226-229
this.canvas.addEventListener('mousedown', (e) => this.handleCanvasMouseDown(e));
this.canvas.addEventListener('mousemove', (e) => this.handleCanvasMouseMove(e));
this.canvas.addEventListener('mouseup', (e) => this.handleCanvasMouseUp(e));
this.canvas.addEventListener('wheel', (e) => this.handleCanvasWheel(e));
// ⚠️ Jamais de removeEventListener!
```

---

## 3. ROUTING DES CANAUX MIDI

### ✅ Fonctionnement Actuel
**Fichier**: `frontend/js/views/RoutingView.js` (v4.1.0)

#### Composants
1. **RoutingView**: Interface principale
2. **RoutingMatrix**: Grille visuelle source→destination
3. **RoutingController**: Logique de routing
4. **RoutingModel**: État du routing

#### Fonctionnalités
- ✅ Matrice interactive source → destination
- ✅ Création/Suppression de routes
- ✅ Enable/Disable routes
- ✅ Clear all routes
- ✅ Affichage des devices connectés

```javascript
// RoutingView.js:242-286
renderMatrix() {
    return `
        <div class="matrix-grid">
            <div class="matrix-header">
                <div class="matrix-corner"></div>
                ${destinations.map(dst => `
                    <div class="matrix-col-header">${dst.name}</div>
                `).join('')}
            </div>
            ${sources.map(src => `
                <div class="matrix-row">
                    <div class="matrix-row-header">${src.name}</div>
                    ${destinations.map(dst => {
                        const route = routes.find(r =>
                            r.source_id === src.id && r.destination_id === dst.id
                        );
                        return `
                            <div class="matrix-cell ${isConnected ? 'connected' : ''}">
                                ${isConnected ? (isEnabled ? '✓' : '●') : ''}
                            </div>
                        `;
                    }).join('')}
                </div>
            `).join('')}
        </div>
    `;
}
```

#### 🔁 Flux de Routing
```
RoutingView.createRoute()
  → emit('routing:add_route_requested', {source_id, destination_id})
  → RoutingController.assignMidiRouting()
  → backend.addMidiRouting()
  → emit('routing:assigned')
  → RoutingView.loadRoutes()
```

#### ⚠️ BUGS IDENTIFIÉS

**BUG #10: Re-render complet à chaque modification**
- **Fichier**: `RoutingView.js:70-76, 467-479`
- **Sévérité**: HAUTE
- **Description**: Modifier une route déclenche `render()` + `cacheElements()` + `attachEvents()` complet
```javascript
// RoutingView.js:467-472
setLoadedFile(fileData) {
    this.state.loadedFile = fileData;
    this.render();          // ⚠️ Re-render complet!
    this.cacheElements();   // ⚠️ Re-cache tout!
    this.attachEvents();    // ⚠️ Re-attache tout!
}
```
**Impact**: Performance dégradée + risque de duplication d'events

**BUG #11: Matrice non interactive**
- **Fichier**: `RoutingView.js:242-286`
- **Sévérité**: MOYENNE
- **Description**: Les cellules de la matrice ne sont pas cliquables - il faut utiliser les selects
```javascript
// RoutingView.js:274-280
return `
    <div class="matrix-cell ${isConnected ? 'connected' : ''}">
        ${isConnected ? (isEnabled ? '✓' : '●') : ''}
    </div>
`;
// ⚠️ Pas de data-action ou event handler!
```
**Impact**: UX sous-optimale, utilisateur doit passer par les dropdowns

**BUG #12: Flag rendered mal utilisé**
- **Fichier**: `RoutingView.js:70-76`
- **Sévérité**: BASSE
- **Description**: Flag `rendered` vérifié mais jamais réinitialisé
```javascript
// RoutingView.js:74-76
if (this.state.rendered) {
    return;  // ⚠️ Empêche toute modification de l'UI après premier render!
}
```

---

## 4. AFFICHAGE DES NOTES À VENIR SUR LE VISUALISEUR

### ✅ Fonctionnement Actuel
**Fichier**: `frontend/js/views/HomeView.js:625-658`

#### Mécanisme
1. **updateNotePreview(notes)**: Affiche les 5 prochaines notes
2. **getUpcomingNotes()**: Extrait les notes dans les 2 prochaines secondes
3. **Mise à jour**: Timer toutes les 100ms pendant la lecture

```javascript
// HomeView.js:625-651
updateNotePreview(notes) {
    if (!this.elements.notePreview || !notes || notes.length === 0) {
        if (this.elements.notePreview) {
            this.elements.notePreview.style.display = 'none';
        }
        return;
    }

    this.elements.notePreview.style.display = 'block';

    const html = notes.slice(0, 5).map(note => `
        <div class="note-preview-item">
            <span class="note-name">${this.getMidiNoteName(note.note)}</span>
            <span class="note-time">+${(note.time / 1000).toFixed(1)}s</span>
        </div>
    `).join('');

    this.elements.notePreview.innerHTML = `
        <div class="note-preview-title">Notes à venir</div>
        <div class="note-preview-list">${html}</div>
    `;
}
```

```javascript
// HomeController.js:1377-1396
getUpcomingNotes(currentTime) {
    if (!this.currentFile || !this.currentFile.midiJson) {
        return [];
    }

    const previewTime = 2000; // 2 secondes
    const endTime = currentTime + previewTime;

    return this.currentFile.midiJson.timeline
        .filter(event =>
            event.type === 'noteOn' &&
            event.time >= currentTime &&
            event.time <= endTime
        )
        .map(event => ({
            ...event,
            timeOffset: event.time - currentTime
        }))
        .slice(0, 10);
}
```

#### ⚠️ BUGS IDENTIFIÉS

**BUG #13: Timer 100ms trop fréquent**
- **Fichier**: `HomeController.js:1405-1428`
- **Sévérité**: MOYENNE
- **Description**: Mise à jour toutes les 100ms = 10 fois/sec, trop fréquent
```javascript
// HomeController.js:1408-1422
this.playbackTimer = setInterval(() => {
    this.currentTime += 100; // ⚠️ 100ms = 10 FPS

    if (this.currentFile) {
        if (this.view && this.view.updateProgress) {
            this.view.updateProgress(this.currentTime, this.currentFile.duration);
        }

        const upcomingNotes = this.getUpcomingNotes(this.currentTime);
        if (this.view && this.view.updateNotePreview) {
            this.view.updateNotePreview(upcomingNotes);  // ⚠️ 10x/sec!
        }
    }
}, 100);
```
**Impact**: Consommation CPU élevée, UI peut être saccadée

**BUG #14: Notes préview dépendent du timer local**
- **Fichier**: `HomeController.js:1409`
- **Sévérité**: HAUTE
- **Description**: `currentTime` incrémenté localement au lieu de venir du backend
```javascript
this.currentTime += 100; // ⚠️ Dérive progressive!
```
**Impact**: Position affichée peut différer de la position réelle du backend

**BUG #15: Aperçu notes manquant dans le visualiseur**
- **Fichier**: `HomeView.js:184-187`
- **Sévérité**: BASSE
- **Description**: L'élément `#homeNotePreview` est créé mais jamais affiché par défaut
```html
<!-- HomeView.js:184-187 -->
<div class="home-note-preview" id="homeNotePreview" style="display: none;">
    <!-- Généré dynamiquement -->
</div>
```
**Solution**: Afficher automatiquement quand des notes sont disponibles

---

## 5. SÉLECTION DE PLAYLIST DEPUIS HOME

### ✅ Fonctionnement Actuel
**Fichier**: `frontend/js/views/HomeView.js:476-522`

#### Interface
- **Tabs**: Switcher entre "Fichiers MIDI" et "Playlists"
- **Actions**:
  - Bouton "Play" : Lance la lecture de la playlist
  - Bouton "Load" : Charge la playlist dans le lecteur

```javascript
// HomeView.js:489-515
renderPlaylistItem(playlist) {
    const isActive = this.state.currentPlaylist &&
                    this.state.currentPlaylist.id === playlist.id;

    return `
        <div class="playlist-item ${isActive ? 'active' : ''}"
             data-playlist-id="${playlist.id}">
            <div class="playlist-icon">📋</div>
            <div class="playlist-info">
                <div class="playlist-name">${playlist.name}</div>
                <div class="playlist-meta">
                    <span>${itemCount} morceaux</span>
                    <span>•</span>
                    <span>${duration}</span>
                </div>
            </div>
            <div class="playlist-actions">
                <button class="btn-play" data-action="play-playlist">▶</button>
                <button class="btn-load" data-action="load-playlist">📂</button>
            </div>
        </div>
    `;
}
```

#### 🔁 Flux de Sélection Playlist
```
HomeView.playPlaylist()
  → emit('home:play_playlist_requested', {playlist_id})
  → HomeController (handler manquant!)
  → ??? (flux incomplet)
```

#### ⚠️ BUGS IDENTIFIÉS

**BUG #16: Handler playlist manquant**
- **Fichier**: `HomeController.js:321-344`
- **Sévérité**: CRITIQUE
- **Description**: `home:play_playlist_requested` émis mais pas de handler dans HomeController
```javascript
// HomeView.js:784-793
async playPlaylist(playlist) {
    if (!this.eventBus) return;

    try {
        this.eventBus.emit('home:play_playlist_requested', {
            playlist_id: playlist.id
        });  // ⚠️ Événement émis mais personne n'écoute!
    } catch (error) {
        this.logger.error('[HomeView] Play playlist error:', error);
    }
}
```

**Recherche dans HomeController.js**: Aucune occurrence de `home:play_playlist_requested`

**BUG #17: Événement `home:load_playlist_requested` non géré**
- **Fichier**: `HomeView.js:796-806`
- **Sévérité**: CRITIQUE
- **Description**: Même problème pour le chargement de playlist
```javascript
// HomeView.js:796-806
async loadPlaylist(playlist) {
    if (!this.eventBus) return;

    try {
        this.eventBus.emit('home:load_playlist_requested', {
            playlist_id: playlist.id
        });  // ⚠️ Événement émis mais pas de handler!
    } catch (error) {
        this.logger.error('[HomeView] Load playlist error:', error);
    }
}
```

**BUG #18: FileSelectionModal dépend de FileModel**
- **Fichier**: `FileSelectionModal.js:36-43`
- **Sévérité**: HAUTE
- **Description**: Modal appelle `fileModel.getMidiFiles()` qui peut ne pas exister
```javascript
// FileSelectionModal.js:36-43
try {
    this.midiFiles = await this.fileModel.getMidiFiles();
} catch (error) {
    this.log('error', 'FileSelectionModal', `Failed to load MIDI files: ${error.message}`);
    this.showError('Erreur de chargement des fichiers MIDI');
    return;  // ⚠️ Modal échoue silencieusement
}
```

---

## BUGS CRITIQUES IDENTIFIÉS

### 🔴 Critiques (Blocants)

| # | Bug | Fichier | Ligne | Impact |
|---|-----|---------|-------|--------|
| 6 | Perte de données non sauvegardées | EditorView.js | 244-263 | Perte de travail utilisateur |
| 4 | Duplication d'event listeners | FileView.js | 504-579 | Fuite mémoire + bugs |
| 8 | Duplication listeners (EditorView) | EditorView.js | 180-216 | Fuite mémoire + bugs |
| 16 | Handler playlist manquant | HomeController.js | - | Playlist non fonctionnelle |
| 17 | Load playlist non géré | HomeController.js | - | Playlist non fonctionnelle |

### 🟠 Hautes (Importantes)

| # | Bug | Fichier | Ligne | Impact |
|---|-----|---------|-------|--------|
| 10 | Re-render complet routing | RoutingView.js | 467-479 | Performance |
| 14 | Timer local vs backend | HomeController.js | 1409 | Dérive temporelle |
| 18 | Modal dépend de FileModel | FileSelectionModal.js | 36-43 | Playlist peut échouer |
| 5 | Metadata enrichment séquentiel | FileController.js | 186-204 | Performance (50 fichiers = lent) |

### 🟡 Moyennes (À corriger)

| # | Bug | Fichier | Ligne | Impact |
|---|-----|---------|-------|--------|
| 1 | Double initialisation | HomeView.js | 60-100 | Fiabilité |
| 3 | Gestion d'erreur manquante | HomeView.js | 710-732 | UX |
| 7 | Canvas resize non géré | EditorView.js | 165-167 | UX |
| 9 | Canvas events non nettoyés | EditorView.js | 226-229 | Fuite mémoire |
| 11 | Matrice non interactive | RoutingView.js | 274-280 | UX |
| 13 | Timer 100ms trop fréquent | HomeController.js | 1408-1422 | Performance |

---

## RECOMMANDATIONS

### 🎯 Priorité 1 (Immédiate)

#### 1. **Corriger la duplication d'event listeners**
**Fichiers**: `FileView.js`, `EditorView.js`

**Solution**:
```javascript
class BaseView {
    constructor(containerId, eventBus) {
        this._clickHandler = null;
    }

    attachEvents() {
        // Nettoyer avant réattacher
        if (this._clickHandler) {
            this.container.removeEventListener('click', this._clickHandler);
        }

        // Créer et stocker le handler
        this._clickHandler = (e) => this.handleClick(e);
        this.container.addEventListener('click', this._clickHandler);
    }

    destroy() {
        if (this._clickHandler) {
            this.container.removeEventListener('click', this._clickHandler);
            this._clickHandler = null;
        }
    }
}
```

#### 2. **Implémenter les handlers playlist manquants**
**Fichier**: `HomeController.js`

**Ajout nécessaire**:
```javascript
// Dans bindEvents()
this.subscribe('home:play_playlist_requested', async (data) => {
    await this.playPlaylist(data.playlist_id);
});

this.subscribe('home:load_playlist_requested', async (data) => {
    await this.loadPlaylist(data.playlist_id);
});

// Nouvelles méthodes
async playPlaylist(playlistId) {
    if (this.playlistController && this.playlistController.loadPlaylist) {
        const playlist = await this.playlistController.loadPlaylist(playlistId);
        if (playlist && playlist.files && playlist.files.length > 0) {
            const firstFileId = playlist.files[0].id || playlist.files[0];
            await this.loadFile(firstFileId);
            await this.play();
        }
    }
}

async loadPlaylist(playlistId) {
    if (this.playlistController && this.playlistController.loadPlaylist) {
        await this.playlistController.loadPlaylist(playlistId);
        this.showSuccess('Playlist chargée');
    }
}
```

#### 3. **Vérifier modifications avant de charger nouveau fichier**
**Fichier**: `EditorView.js`

**Solution**:
```javascript
this.eventBus.on('editor:fileLoaded', (data) => {
    // ✅ Vérifier si modifications non sauvegardées
    if (this.viewState.isModified) {
        const confirmed = confirm(
            'Vous avez des modifications non sauvegardées. ' +
            'Voulez-vous les abandonner ?'
        );

        if (!confirmed) {
            return; // Annuler le chargement
        }
    }

    // Charger le nouveau fichier
    this.viewState.currentFile = {
        name: data.file_path?.split(/[/\\]/).pop() || 'Unknown',
        path: data.file_path
    };

    if (data.midi_json) {
        this.viewState.midiData = data.midi_json;
        this.viewState.tracks = data.midi_json.tracks || [];
        this.extractNotes();
    }

    this.viewState.isModified = false;
    this.render();
});
```

### 🎯 Priorité 2 (Court terme)

#### 4. **Optimiser metadata enrichment**
**Fichier**: `FileController.js`

**Solution**: Utiliser Promise.all pour paralléliser
```javascript
async enrichFilesWithMetadata(files) {
    const midiFiles = files.filter(f =>
        f.name && (f.name.toLowerCase().endsWith('.mid') ||
                   f.name.toLowerCase().endsWith('.midi'))
    );

    // ✅ Paralléliser avec limite de concurrence
    const BATCH_SIZE = 5; // 5 fichiers en parallèle max
    const enrichedFiles = [...files];

    for (let i = 0; i < midiFiles.length; i += BATCH_SIZE) {
        const batch = midiFiles.slice(i, i + BATCH_SIZE);

        await Promise.all(batch.map(async (file) => {
            try {
                const filePath = file.path || file.name;
                const midiData = await this.backend.loadMidi(filePath);

                const index = enrichedFiles.findIndex(f => f.path === file.path);
                if (index !== -1 && midiData?.midi_json) {
                    enrichedFiles[index].duration = midiData.midi_json.duration || 0;
                    enrichedFiles[index].tracks = midiData.midi_json.tracks?.length || 0;
                }
            } catch (error) {
                this.log('warn', 'FileController', `Failed to enrich ${file.name}`);
            }
        }));
    }

    return enrichedFiles;
}
```

#### 5. **Réduire fréquence du timer**
**Fichier**: `HomeController.js`

**Solution**: Passer de 100ms à 250ms (4 FPS suffisant)
```javascript
startProgressTimer() {
    this.stopProgressTimer();

    this.playbackTimer = setInterval(() => {
        // ✅ Récupérer la position depuis le backend au lieu d'incrémenter
        if (this.backend && this.backend.getPlaybackPosition) {
            this.backend.getPlaybackPosition().then(position => {
                this.currentTime = position;
                this.homeState.currentTime = position;

                if (this.currentFile) {
                    if (this.view && this.view.updateProgress) {
                        this.view.updateProgress(this.currentTime, this.currentFile.duration);
                    }

                    const upcomingNotes = this.getUpcomingNotes(this.currentTime);
                    if (this.view && this.view.updateNotePreview) {
                        this.view.updateNotePreview(upcomingNotes);
                    }
                }
            });
        }
    }, 250);  // ✅ 250ms au lieu de 100ms
}
```

#### 6. **Rendre la matrice routing interactive**
**Fichier**: `RoutingView.js`

**Solution**: Ajouter data-action et handler
```javascript
renderMatrix() {
    // ...
    return `
        <div class="matrix-cell ${isConnected ? 'connected' : ''} ${!isEnabled ? 'disabled' : ''}"
             data-source="${src.id}"
             data-destination="${dst.id}"
             data-action="toggle-route-cell">  <!-- ✅ Ajout action -->
            ${isConnected ? (isEnabled ? '✓' : '●') : ''}
        </div>
    `;
}

// Dans attachEvents()
if (action === 'toggle-route-cell') {
    const sourceId = e.target.dataset.source;
    const destinationId = e.target.dataset.destination;

    const route = this.state.routes.find(r =>
        r.source_id === sourceId && r.destination_id === destinationId
    );

    if (route) {
        // Toggle enable/disable
        this.toggleRoute(`${sourceId}_${destinationId}`, route.enabled !== false);
    } else {
        // Créer nouvelle route
        this.state.selectedSource = sourceId;
        this.state.selectedDestination = destinationId;
        this.createRoute();
    }
}
```

### 🎯 Priorité 3 (Moyen terme)

#### 7. **Gestion centralisée des event listeners**
Créer un `EventManager` pour gérer automatiquement les listeners:

```javascript
class EventManager {
    constructor(element) {
        this.element = element;
        this.listeners = [];
    }

    on(eventType, selector, handler) {
        const wrappedHandler = (e) => {
            if (e.target.closest(selector)) {
                handler(e);
            }
        };

        this.element.addEventListener(eventType, wrappedHandler);
        this.listeners.push({ eventType, handler: wrappedHandler });
    }

    removeAll() {
        this.listeners.forEach(({ eventType, handler }) => {
            this.element.removeEventListener(eventType, handler);
        });
        this.listeners = [];
    }
}

// Utilisation dans BaseView
class BaseView {
    constructor(containerId, eventBus) {
        this.eventManager = null;
    }

    attachEvents() {
        if (this.eventManager) {
            this.eventManager.removeAll();
        }

        this.eventManager = new EventManager(this.container);
        this.eventManager.on('click', '[data-action]', (e) => {
            const action = e.target.closest('[data-action]').dataset.action;
            this.handleAction(action, e);
        });
    }

    destroy() {
        if (this.eventManager) {
            this.eventManager.removeAll();
        }
    }
}
```

#### 8. **Ajouter resize observer pour canvas**
**Fichier**: `EditorView.js`

```javascript
initializeCanvas() {
    this.setupCanvas();

    // ✅ Utiliser ResizeObserver moderne
    const container = this.canvas.parentElement;
    this.resizeObserver = new ResizeObserver(() => {
        this.resizeCanvas();
    });
    this.resizeObserver.observe(container);
}

destroy() {
    if (this.resizeObserver) {
        this.resizeObserver.disconnect();
        this.resizeObserver = null;
    }
}
```

#### 9. **Système de cache pour métadonnées**
Créer un cache IndexedDB pour les métadonnées des fichiers MIDI:

```javascript
class MidiMetadataCache {
    async get(filePath) {
        // Récupérer depuis IndexedDB
    }

    async set(filePath, metadata) {
        // Sauvegarder dans IndexedDB
    }

    async invalidate(filePath) {
        // Supprimer du cache
    }
}

// Dans FileController
async enrichFilesWithMetadata(files) {
    const cache = new MidiMetadataCache();

    for (const file of files) {
        // ✅ Vérifier cache d'abord
        const cached = await cache.get(file.path);
        if (cached) {
            Object.assign(file, cached);
            continue;
        }

        // Sinon charger et mettre en cache
        try {
            const midiData = await this.backend.loadMidi(file.path);
            const metadata = {
                duration: midiData.midi_json.duration,
                tracks: midiData.midi_json.tracks.length
            };

            Object.assign(file, metadata);
            await cache.set(file.path, metadata);
        } catch (error) {
            this.log('warn', 'Failed to enrich', error);
        }
    }
}
```

---

## RÉSUMÉ EXÉCUTIF

### État Général: 🟡 MOYEN

**Points Forts**:
- ✅ Architecture MVC claire et modulaire
- ✅ EventBus bien implémenté
- ✅ Fonctionnalités complètes (édition, routing, playback)
- ✅ Code bien documenté avec versions

**Points Faibles**:
- 🔴 **18 bugs identifiés** dont 5 critiques
- 🔴 Handlers playlist manquants (fonctionnalité cassée)
- 🟠 Fuites mémoire potentielles (event listeners)
- 🟠 Performance sous-optimale (enrichment séquentiel, timer 100ms)

### Actions Immédiates Recommandées

1. **Corriger handlers playlist** (2h de travail)
2. **Nettoyer event listeners** (4h de travail)
3. **Ajouter confirmation perte données** (1h de travail)

**Temps total estimé pour fixes critiques**: ~7 heures

### Metrics

| Catégorie | Count |
|-----------|-------|
| Bugs Critiques | 5 |
| Bugs Hautes | 4 |
| Bugs Moyennes | 6 |
| Bugs Basses | 3 |
| **TOTAL** | **18** |

---

**Fin du rapport d'audit**
