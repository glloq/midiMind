# 🎹 MidiMind v4.1.0

Système d'Orchestration MIDI Professionnel pour Raspberry Pi

## ⚡ Installation Rapide

### Prérequis

- Raspberry Pi (ou système Linux compatible)
- Connexion Internet
- Minimum 2GB d'espace disque disponible
- Accès sudo/root

### Installation en une commande

```bash
# Cloner le dépôt
git clone https://github.com/glloq/midiMind.git
cd midiMind
cd script

# Lancer l'installation
chmod +x install.sh
sudo ./scripts/install.sh
```

### Options d'installation

Le script vous proposera 3 modes d'installation :

1. **Installation complète** (Recommandé)
   - Backend API WebSocket
   - Interface web frontend
   - Serveur Nginx configuré
   - Interface accessible sur port 8000

2. **Backend uniquement**
   - API WebSocket seule
   - Idéal pour intégration custom
   - Pas d'interface web

3. **Mode développeur**
   - Backend + Frontend
   - Sans Nginx (serveur dev manuel)
   - Pour développement local

### Durée d'installation

- Téléchargement des dépendances : ~5 min
- Compilation du backend : ~5-10 min
- Configuration système : ~2 min

**Total : environ 15-20 minutes**

## 🚀 Démarrage

```bash
# Démarrer le service
sudo systemctl start midimind

# Vérifier le status
sudo systemctl status midimind

# Consulter les logs
sudo journalctl -u midimind -f
```

## 🌐 Accès

- **Interface Web** : `http://<IP_RASPBERRY>:8000`
- **API WebSocket** : `ws://<IP_RASPBERRY>:8080`

## 📁 Architecture

```
midiMind/
├── backend/          # Code C++ du backend
│   ├── src/          # Sources
│   ├── build/        # Compilation
│   └── CMakeLists.txt
├── frontend/         # Interface web (racine)
│   ├── index.html
│   ├── css/
│   └── js/
└── scripts/          # Scripts d'installation
    └── install.sh
```

## ⚙️ Fichiers de configuration

- **Service** : `/etc/systemd/system/midimind.service`
- **Config** : `/etc/midimind/config.json`
- **Logs** : `/var/log/midimind/`
- **Données** : `/opt/midimind/`

## 🔧 Commandes utiles

```bash
# Gestion du service
sudo systemctl start midimind    # Démarrer
sudo systemctl stop midimind     # Arrêter
sudo systemctl restart midimind  # Redémarrer
sudo systemctl enable midimind   # Activer au démarrage

# Logs et debug
sudo journalctl -u midimind -f   # Logs en temps réel
tail -f /var/log/midimind/backend.log

# Configuration Nginx (si installé)
sudo systemctl status nginx
sudo nginx -t                     # Tester la config
```

## ⚠️ Important

**Après l'installation, redémarrez le système :**
```bash
sudo reboot
```

Cela active toutes les optimisations système et permissions.

## 📚 Documentation complète

Consultez le log d'installation : `/var/log/midimind_install.log`

## 🆘 Support

En cas de problème :
1. Vérifiez les logs : `sudo journalctl -u midimind -f`
2. Consultez le log d'installation
3. Vérifiez les permissions : `groups $USER`

## 📝 Licence

Voir le fichier LICENSE pour plus de détails.
