# Test des Boutons Play/Pause/Stop

## ✅ Code Déjà Implémenté

Le code pour les boutons est **DÉJÀ COMPLET** dans le projet. Voici comment tester:

## 🧪 Tests à Effectuer

### Test 1: Vérifier que les boutons sont connectés

Ouvrez la console du navigateur (F12) et tapez:

```javascript
// Vérifier que les boutons existent
const btnPlay = document.getElementById('globalPlay');
const btnPause = document.getElementById('globalPause');
const btnStop = document.getElementById('globalStop');

console.log('Play button:', btnPlay);
console.log('Pause button:', btnPause);
console.log('Stop button:', btnStop);
```

**Résultat attendu**: Les 3 boutons doivent s'afficher dans la console.

---

### Test 2: Vérifier que GlobalPlaybackController existe

```javascript
// Vérifier le contrôleur
const gpc = window.app?.controllers?.globalPlayback;
console.log('GlobalPlaybackController:', gpc);

// Vérifier les méthodes
console.log('play() exists:', typeof gpc?.play === 'function');
console.log('pause() exists:', typeof gpc?.pause === 'function');
console.log('stop() exists:', typeof gpc?.stop === 'function');
```

**Résultat attendu**:
- GlobalPlaybackController doit exister
- Les 3 méthodes doivent être des fonctions

---

### Test 3: Vérifier le backend

```javascript
// Vérifier la connexion backend
const backend = window.app?.services?.backend;
console.log('Backend:', backend);
console.log('Backend connected:', backend?.isConnected());
```

**Résultat attendu**: Backend connecté = `true`

---

### Test 4: Charger et jouer un fichier manuellement

```javascript
// Charger un fichier de test
const gpc = window.app?.controllers?.globalPlayback;

// Remplacer 'test.mid' par un fichier MIDI existant dans midi-files/
await gpc.load('test.mid');
console.log('File loaded');

// Jouer
await gpc.play();
console.log('Playing');
```

---

## 🐛 Solutions aux Problèmes Courants

### Problème 1: Boutons ne répondent pas

**Cause possible**: Event listeners pas attachés

**Solution**: Dans la console:
```javascript
// Forcer la reconnexion des boutons
window.app.setupGlobalPlaybackControls();
```

---

### Problème 2: "Backend not connected"

**Cause**: Le backend C++ n'est pas démarré

**Solution**:
1. Démarrer le backend:
   ```bash
   cd /home/user/midiMind/backend/build
   ./midimind_backend
   ```

2. Vérifier dans l'UI que le status indique "Connecté"

---

### Problème 3: "No file loaded"

**Cause**: Aucun fichier MIDI chargé

**Solution**: Charger un fichier avant de jouer:

1. Via l'interface:
   - Aller dans "Fichiers"
   - Cliquer sur le bouton ▶️ à côté d'un fichier

2. Via la console:
   ```javascript
   const gpc = window.app.controllers.globalPlayback;
   await gpc.load('votre-fichier.mid');
   await gpc.play();
   ```

---

## 📝 Workflow Normal

### Via l'Interface Utilisateur

1. **Démarrer le backend** (si pas déjà fait)
   ```bash
   cd backend/build
   ./midimind_backend
   ```

2. **Ouvrir l'application** dans Chrome
   ```
   http://localhost:8000
   ```

3. **Aller dans l'onglet "Fichiers"**

4. **Cliquer sur ▶️ à côté d'un fichier** dans la liste
   - Cela charge automatiquement le fichier
   - Le nom du fichier apparaît dans le header

5. **Utiliser les boutons du header**:
   - ▶️ Play: Démarre la lecture
   - ⏸ Pause: Met en pause
   - ⏹ Stop: Arrête et revient à 0

---

## 🔧 Si les Boutons Ne Fonctionnent Toujours Pas

### Vérification Complète

```javascript
// Script de diagnostic complet
(async function diagnosticPlayback() {
  console.log('=== DIAGNOSTIC PLAYBACK ===');

  // 1. Vérifier les boutons
  const btnPlay = document.getElementById('globalPlay');
  const btnPause = document.getElementById('globalPause');
  const btnStop = document.getElementById('globalStop');

  console.log('✓ Boutons:', {
    play: !!btnPlay,
    pause: !!btnPause,
    stop: !!btnStop
  });

  // 2. Vérifier le contrôleur
  const gpc = window.app?.controllers?.globalPlayback;
  console.log('✓ GlobalPlaybackController:', !!gpc);

  // 3. Vérifier le backend
  const backend = window.app?.services?.backend;
  const connected = backend?.isConnected();
  console.log('✓ Backend connected:', connected);

  // 4. Vérifier EventBus
  const eventBus = window.eventBus;
  console.log('✓ EventBus:', !!eventBus);

  // 5. Tester un événement
  if (eventBus) {
    eventBus.once('test:playback', (data) => {
      console.log('✓ EventBus works:', data);
    });
    eventBus.emit('test:playback', { message: 'OK' });
  }

  // 6. Lister les fichiers disponibles
  if (backend && connected) {
    try {
      const files = await backend.sendCommand('files.list');
      console.log('✓ Available files:', files);
    } catch (e) {
      console.error('✗ Failed to list files:', e);
    }
  }

  console.log('=== FIN DIAGNOSTIC ===');
})();
```

---

## 🎯 Test Rapide Complet

```javascript
// Test rapide tout-en-un
async function quickTest() {
  try {
    console.log('🧪 Test de playback...');

    const gpc = window.app.controllers.globalPlayback;
    const backend = window.app.services.backend;

    // Vérifier connexion
    if (!backend.isConnected()) {
      throw new Error('Backend not connected');
    }
    console.log('✓ Backend connected');

    // Lister les fichiers
    const response = await backend.sendCommand('files.list');
    const files = response.files || [];

    if (files.length === 0) {
      throw new Error('No MIDI files found');
    }
    console.log(`✓ Found ${files.length} files`);

    // Charger le premier fichier
    const firstFile = files[0].filename || files[0];
    console.log(`📂 Loading: ${firstFile}`);
    await gpc.load(firstFile);
    console.log('✓ File loaded');

    // Attendre 1 seconde
    await new Promise(r => setTimeout(r, 1000));

    // Jouer
    console.log('▶️ Playing...');
    await gpc.play();
    console.log('✓ Playing!');

    // Attendre 3 secondes
    await new Promise(r => setTimeout(r, 3000));

    // Stop
    console.log('⏹ Stopping...');
    await gpc.stop();
    console.log('✓ Stopped!');

    console.log('✅ Test complet réussi!');

  } catch (error) {
    console.error('❌ Test échoué:', error.message);
    console.error(error);
  }
}

// Lancer le test
quickTest();
```

---

## 📊 Logs à Surveiller

Quand vous cliquez sur les boutons, vous devriez voir dans la console:

```
[INFO] GlobalPlaybackController: Global play button clicked
[INFO] GlobalPlaybackController: ▶️ Playing with latency compensation
```

---

## 🚨 Si Rien Ne Fonctionne

Il y a 3 possibilités:

### 1. Le Backend n'est pas démarré
**Solution**: Démarrer le backend C++

### 2. Les événements ne sont pas connectés
**Solution**: Recharger la page ou forcer:
```javascript
window.app.setupGlobalPlaybackControls();
```

### 3. Pas de fichier MIDI chargé
**Solution**: Charger un fichier d'abord via l'UI ou:
```javascript
const gpc = window.app.controllers.globalPlayback;
await gpc.load('fichier.mid');
await gpc.play();
```

---

## ✅ Checklist de Validation

- [ ] Backend démarré et connecté (indicateur vert dans l'UI)
- [ ] Au moins 1 fichier MIDI dans `midi-files/`
- [ ] Fichier chargé (nom visible dans le header)
- [ ] Boutons play/pause/stop visibles
- [ ] Clic sur play déclenche la lecture
- [ ] Temps et barre de progression se mettent à jour
- [ ] Pause met en pause
- [ ] Stop revient à 0

---

## 🔗 Fichiers Impliqués

| Fichier | Rôle |
|---------|------|
| `frontend/index.html:42-46` | Boutons HTML |
| `frontend/js/controllers/GlobalPlaybackController.js` | Logique de playback |
| `frontend/js/core/Application.js:808-903` | Connexion des boutons |
| `frontend/js/services/BackendService.js` | Communication WebSocket |
| `backend/` | Backend C++ (moteur MIDI) |

---

## 🎓 Prochaines Étapes

1. **Tester avec le script de diagnostic ci-dessus**
2. **Regarder les logs dans la console** (F12)
3. **Vérifier le statut de connexion** dans l'UI
4. **Si erreur**: Noter le message exact et chercher dans les logs

Besoin d'aide? Copiez le résultat du script de diagnostic!
