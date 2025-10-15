# ⚡ GUIDE INSTALLATION RAPIDE - MidiMind

**Repo GitHub :** https://github.com/glloq/midiMind  
**Plateforme :** Raspberry Pi (3/4/5)  
**Durée totale :** 20-30 minutes

---

## 📋 PRÉREQUIS

- Raspberry Pi 3, 4 ou 5
- Raspbian OS / Ubuntu (Debian 11+)
- Connexion Internet
- Minimum 2GB d'espace disque
- Accès sudo

---

## 🚀 INSTALLATION EN 5 ÉTAPES

### Étape 1 : Cloner le Repo

```bash
cd ~
git clone https://github.com/glloq/midiMind.git
cd midiMind
```

---

### Étape 2 : Rendre les Scripts Exécutables

```bash
cd scripts/
chmod +x *.sh
```

---

### Étape 3 : Lancer l'Installation

```bash
sudo ./install.sh
```

**Durée :** 15-25 minutes  
**Ce qui est fait :**
- Installation dépendances (ALSA, SQLite, Nginx, etc.)
- Compilation backend C++
- Configuration frontend
- Configuration service systemd
- Optimisations Raspberry Pi

---

### Étape 4 : Redémarrer

```bash
sudo reboot
```

**Important :** Le redémarrage applique les optimisations système.

---

### Étape 5 : Vérifier l'Installation

```bash
# Après redémarrage
./status.sh
```

**Résultat attendu :**
```
📊 STATUT SERVICE
  • État: Actif ✓
  • Port WebSocket: 8080 ✓
  • Port HTTP: 8000 ✓
```

---

## 🖥️ ACCÈS INTERFACE WEB

### URL d'accès

```
Local:  http://localhost:8000
Réseau: http://[IP_DU_PI]:8000
```

**Trouver l'IP du Pi :**
```bash
hostname -I
```

---

## 🎨 CONFIGURATION BUREAU (Optionnel)

Si vous avez un écran connecté au Raspberry Pi :

```bash
./setup-desktop.sh
```

**Crée :**
- Icône cliquable sur le bureau
- Lancement automatique au démarrage
- Mode kiosque optionnel

---

## 🔧 COMMANDES UTILES

| Action | Commande |
|--------|----------|
| **Démarrer** | `sudo ./start.sh` |
| **Arrêter** | `sudo ./stop.sh` |
| **Redémarrer** | `sudo ./restart.sh` |
| **Statut** | `./status.sh` |
| **Logs live** | `journalctl -u midimind -f` |
| **Désinstaller** | `sudo ./uninstall.sh` |

---

## 📊 VÉRIFICATION RAPIDE

### Checklist Post-Installation

```bash
# 1. Service actif ?
sudo systemctl status midimind

# 2. Ports ouverts ?
sudo netstat -tulnp | grep -E ':8000|:8080'

# 3. Frontend accessible ?
curl http://localhost:8000

# 4. API WebSocket ?
curl http://localhost:8080
```

**Tout doit répondre OK** ✓

---

## 🐛 PROBLÈMES COURANTS

### Installation échoue

```bash
# Vérifier espace disque
df -h

# Vérifier Internet
ping -c 3 8.8.8.8

# Relancer
sudo ./install.sh
```

### Service ne démarre pas

```bash
# Voir les logs
journalctl -u midimind -n 50

# Redémarrer
sudo ./restart.sh
```

### Port déjà utilisé

```bash
# Trouver processus
sudo netstat -tulnp | grep :8080

# Arrêter proprement
sudo ./stop.sh --force
sudo ./start.sh
```

---

## 📚 STRUCTURE DU PROJET

```
midiMind/
├── backend/              # Code C++ (30,500+ lignes)
│   ├── src/
│   ├── CMakeLists.txt
│   └── build/
├── frontend/             # Interface web (25,000+ lignes)
│   ├── index.html
│   ├── styles/
│   └── scripts/
├── config/               # Configuration
│   └── config.json
├── install.sh            # Installation automatique
├── start.sh              # Démarrer
├── stop.sh               # Arrêter
├── restart.sh            # Redémarrer
├── status.sh             # Statut
├── uninstall.sh          # Désinstaller
└── setup-desktop.sh      # Interface bureau
```

---

## 🔄 MISE À JOUR

### Mettre à jour le code

```bash
cd ~/midiMind

# 1. Arrêter le service
sudo ./stop.sh

# 2. Récupérer dernière version
git pull origin main

# 3. Recompiler backend
cd backend/build
make -j$(nproc)

# 4. Redémarrer
sudo systemctl start midimind

# 5. Vérifier
cd ~/midiMind
./status.sh
```

---

## 📝 CONFIGURATION

### Fichier principal

```bash
# Éditer la configuration
sudo nano /etc/midimind/config.json
```

**Sections importantes :**
```json
{
  "api": {
    "port": 8080,
    "host": "0.0.0.0"
  },
  "midi": {
    "buffer_size": 256,
    "latency_ms": 10
  },
  "logger": {
    "level": "INFO"
  }
}
```

**Après modification :**
```bash
sudo ./restart.sh
```

---

## 🌐 ACCÈS DISTANT

### Via réseau local

```bash
# 1. Trouver IP du Pi
hostname -I
# Exemple: 192.168.1.100

# 2. Accéder depuis autre appareil
# http://192.168.1.100:8000
```

### Via SSH

```bash
# Depuis votre PC
ssh pi@192.168.1.100

# Une fois connecté
cd ~/midiMind
./status.sh
```

---

## 🎯 RÉSUMÉ EXPRESS

```bash
# Installation complète en une séquence
cd ~
git clone https://github.com/glloq/midiMind.git
cd midiMind
chmod +x *.sh
sudo ./install.sh
# Attendre fin installation (~20 min)
sudo reboot
# Après reboot
./status.sh
# Accéder à http://[IP_DU_PI]:8000
```

---

## 📞 SUPPORT

### Documentation complète
- **Backend :** `/backend/docs/`
- **Frontend :** `/frontend/docs/`
- **API :** Accessible via interface web

### Logs
```bash
# Logs système
journalctl -u midimind -f

# Logs application
tail -f /var/log/midimind/midimind.log
```

### Issues GitHub
https://github.com/glloq/midiMind/issues

---

## ✅ CHECKLIST FINALE

Après installation complète, vérifier :

- [ ] `./status.sh` affiche "Actif ✓"
- [ ] Interface web accessible (port 8000)
- [ ] API WebSocket accessible (port 8080)
- [ ] Service auto-démarre au boot
- [ ] Logs sans erreur
- [ ] CPU < 10%, RAM < 150MB
- [ ] Température CPU < 60°C

**Si tout est ✓ → Installation réussie !** 🎉

---

## 🚀 PROCHAINES ÉTAPES

1. **Connecter périphériques MIDI** (clavier, synthé)
2. **Uploader fichiers MIDI** via interface web
3. **Créer playlists**
4. **Configurer routage** (instruments → canaux)
5. **Tester lecture** et contrôle en temps réel

---

**Version :** 1.0.0  
**Date :** 14 octobre 2025  
**Repo :** https://github.com/glloq/midiMind  
**Status :** ✅ Production Ready
