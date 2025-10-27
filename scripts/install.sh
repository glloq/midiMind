#!/bin/bash
# ============================================================================
# Fichier: scripts/install.sh
# Version: 4.1.4 - USB + WiFi + Réseau + Bluetooth
# Date: 2025-10-27
# Projet: MidiMind - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
#
# CORRECTIONS v4.1.4:
#   ✅ Support USB: libusb-1.0-0-dev, usbutils
#   ✅ Support WiFi: wpasupplicant, wireless-tools, iw
#   ✅ Support Réseau: net-tools, ifupdown
#   ✅ Support Bluetooth: bluez, bluez-tools, libbluetooth-dev, pi-bluetooth
#
# CORRECTIONS v4.1.3:
#   ✅ ALSA Utils ajouté (alsa-utils, alsa-tools, aconnect, amidi)
#   ✅ config.json: Structure COMPLÈTE conforme à Config.h v4.1.0
#   ✅ config.json: Tous les champs manquants ajoutés
#   ✅ Copie automatique des migrations SQL
#   ✅ Application automatique des migrations SQL
#   ✅ Configuration ALSA temps réel (/etc/asound.conf)
#   ✅ Règles udev MIDI temps réel
#   ✅ Vérification complète post-installation
#   ✅ Test de démarrage du service
#
# CORRECTIONS v4.1.2:
#   ✅ Création du dossier migrations
#   ✅ Copie des fichiers SQL de migration
#   ✅ Initialisation correcte de la base de données
#   ✅ Vérification des permissions sur tous les fichiers
#   ✅ Test de démarrage après installation
#
# FIX v4.1.2-3:
#   ✅ config.json: "server" → "api"
#   ✅ config.json: Structure complète (6 sections)
#   ✅ config.json: Compatibilité avec Config.h v4.1.0
#
# ============================================================================

set -e

# ============================================================================
# COULEURS
# ============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'
BOLD='\033[1m'

# ============================================================================
# VARIABLES GLOBALES
# ============================================================================

# Chemins du projet
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"
FRONTEND_DIR="$PROJECT_ROOT/frontend"
BUILD_DIR="$BACKEND_DIR/build"

# Chemins d'installation
INSTALL_DIR="/opt/midimind"
WEB_DIR="/var/www/midimind"
LOG_FILE="/var/log/midimind_install.log"
REAL_USER="${SUDO_USER:-$USER}"
USER_DIR="/home/$REAL_USER/.midimind"

# Détection système
RPI_MODEL=""
ARCH=""
NPROC=$(nproc)

# ============================================================================
# FONCTIONS UTILITAIRES
# ============================================================================

log() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} $1" | tee -a "$LOG_FILE"
}

success() {
    echo -e "${GREEN}✓${NC} $1" | tee -a "$LOG_FILE"
}

error() {
    echo -e "${RED}✗ ERREUR:${NC} $1" | tee -a "$LOG_FILE"
    echo -e "${RED}Installation interrompue.${NC}" | tee -a "$LOG_FILE"
    exit 1
}

warning() {
    echo -e "${YELLOW}⚠ ATTENTION:${NC} $1" | tee -a "$LOG_FILE"
}

info() {
    echo -e "${CYAN}ℹ${NC} $1" | tee -a "$LOG_FILE"
}

# ============================================================================
# BANNIÈRE
# ============================================================================

print_banner() {
    clear
    echo -e "${CYAN}"
    cat << "EOF"
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║              🎹 MidiMind v4.1.4 Installation ⚡               ║
║                                                              ║
║          Système d'Orchestration MIDI Professionnel          ║
║                  pour Raspberry Pi                           ║
║                                                              ║
║              Installation Complète Automatique               ║
║           USB + WiFi + Réseau + Bluetooth + ALSA             ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
    echo ""
}

# ============================================================================
# DÉTECTION ET VÉRIFICATION STRUCTURE
# ============================================================================

detect_system() {
    log "🔍 Détection du système et vérification structure..."
    
    echo ""
    echo -e "${BOLD}${CYAN}📂 Chemins détectés:${NC}"
    echo -e "  ${BLUE}•${NC} Script:     ${GREEN}$SCRIPT_DIR${NC}"
    echo -e "  ${BLUE}•${NC} Projet:     ${GREEN}$PROJECT_ROOT${NC}"
    echo -e "  ${BLUE}•${NC} Backend:    ${GREEN}$BACKEND_DIR${NC}"
    echo -e "  ${BLUE}•${NC} Frontend:   ${GREEN}$FRONTEND_DIR${NC}"
    echo ""
    
    # ✅ VÉRIFICATION 1: Répertoire backend/
    if [ ! -d "$BACKEND_DIR" ]; then
        error "Répertoire backend/ introuvable: $BACKEND_DIR\n  Exécutez ce script depuis le dossier scripts/"
    fi
    success "Répertoire backend/ trouvé"
    
    # ✅ VÉRIFICATION 2: CMakeLists.txt dans backend/
    if [ ! -f "$BACKEND_DIR/CMakeLists.txt" ]; then
        error "CMakeLists.txt introuvable dans backend/: $BACKEND_DIR/CMakeLists.txt"
    fi
    success "CMakeLists.txt trouvé dans backend/"
    
    # ✅ VÉRIFICATION 3: Sources backend
    if [ ! -d "$BACKEND_DIR/src" ]; then
        error "Répertoire backend/src/ introuvable: $BACKEND_DIR/src"
    fi
    success "Sources backend trouvées (backend/src/)"
    
    # Vérifier fichiers critiques backend
    local critical_files=(
        "$BACKEND_DIR/src/main.cpp"
        "$BACKEND_DIR/src/core/Application.cpp"
        "$BACKEND_DIR/src/api/ApiServer.cpp"
    )
    
    for file in "${critical_files[@]}"; do
        if [ ! -f "$file" ]; then
            error "Fichier critique manquant: $file"
        fi
    done
    success "Fichiers critiques backend vérifiés"
    
    # ✅ VÉRIFICATION 4: Frontend
    if [ ! -d "$FRONTEND_DIR" ]; then
        error "Frontend introuvable: $FRONTEND_DIR\n  Le frontend doit être dans: $FRONTEND_DIR"
    fi
    success "Répertoire frontend/ trouvé"
    
    if [ ! -f "$FRONTEND_DIR/index.html" ]; then
        error "index.html manquant dans: $FRONTEND_DIR"
    fi
    success "Frontend index.html trouvé"
    
    if [ ! -d "$FRONTEND_DIR/js" ]; then
        error "Dossier js/ manquant dans: $FRONTEND_DIR"
    fi
    success "Dossier js/ trouvé dans frontend/"
    
    # ✅ VÉRIFICATION 5: Fichiers de migration SQL
    if [ ! -d "$BACKEND_DIR/data/migrations" ]; then
        warning "Dossier migrations manquant dans backend/data/"
        info "Création du dossier migrations..."
        mkdir -p "$BACKEND_DIR/data/migrations"
    fi
    
    # Compter les fichiers SQL
    local sql_count=$(find "$BACKEND_DIR/data/migrations" -name "*.sql" 2>/dev/null | wc -l)
    if [ $sql_count -eq 0 ]; then
        warning "Aucun fichier SQL de migration trouvé"
        info "Les migrations seront créées avec les valeurs par défaut"
    else
        success "Fichiers SQL de migration trouvés: $sql_count fichiers"
    fi
    
    echo ""
    success "✅ Structure du projet validée"
    echo ""
    
    # Détection plateforme
    if [ -f /proc/device-tree/model ]; then
        RPI_MODEL=$(cat /proc/device-tree/model)
        info "Raspberry Pi détecté: $RPI_MODEL"
    else
        RPI_MODEL="Generic Linux"
        info "Système Linux générique détecté"
    fi
    
    ARCH=$(uname -m)
    info "Architecture: $ARCH ($NPROC cœurs disponibles)"
}

# ============================================================================
# VÉRIFICATION PRÉREQUIS
# ============================================================================

check_prerequisites() {
    log "🔍 Vérification des prérequis..."
    
    # Root requis
    if [[ $EUID -ne 0 ]]; then
        error "Ce script doit être exécuté avec sudo\n  Commande: sudo ./install.sh"
    fi
    success "Exécution avec privilèges root"
    
    # Connexion internet
    if ! ping -c 1 8.8.8.8 &> /dev/null; then
        error "Pas de connexion internet\n  Vérifiez votre connexion réseau"
    fi
    success "Connexion internet OK"
    
    # Espace disque (minimum 2GB)
    local available_space=$(df / | tail -1 | awk '{print $4}')
    local available_gb=$((available_space / 1024 / 1024))
    
    if [ $available_space -lt 2097152 ]; then
        error "Espace disque insuffisant: ${available_gb}GB disponible\n  Minimum requis: 2GB"
    fi
    success "Espace disque suffisant: ${available_gb}GB disponibles"
}

# ============================================================================
# ÉTAPE 1: MISE À JOUR SYSTÈME
# ============================================================================

update_system() {
    log "⚙️ ÉTAPE 1/11: Mise à jour du système"
    
    info "Mise à jour de la liste des paquets..."
    apt-get update -qq 2>&1 | tee -a "$LOG_FILE" || error "Échec apt-get update"
    
    info "Mise à niveau des paquets installés..."
    apt-get upgrade -y -qq 2>&1 | tee -a "$LOG_FILE" || warning "Certains paquets n'ont pas pu être mis à jour"
    
    success "Système mis à jour"
}

# ============================================================================
# ÉTAPE 2: INSTALLATION DÉPENDANCES SYSTÈME
# ============================================================================

install_system_dependencies() {
    log "📦 ÉTAPE 2/11: Installation des dépendances système"
    
    info "Installation des outils de compilation..."
    apt-get install -y -qq \
        build-essential cmake g++ gcc make pkg-config \
        git wget curl unzip \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation build tools"
    
    info "Installation des bibliothèques Audio/MIDI (+ ALSA Utils)..."
    apt-get install -y -qq \
        libasound2-dev \
        alsa-utils \
        alsa-tools \
        libjack-jackd2-dev \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation audio libs"
    
    # ✅ VÉRIFICATION ALSA UTILS
    if command -v aconnect &> /dev/null; then
        success "ALSA Utils installé (aconnect, amidi disponibles)"
    else
        error "ALSA Utils manquant après installation"
    fi
    
    info "Installation des bibliothèques USB..."
    apt-get install -y -qq \
        libusb-1.0-0-dev \
        usbutils \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation USB libs"
    
    # ✅ VÉRIFICATION USB
    if command -v lsusb &> /dev/null; then
        success "USB Utils installé (lsusb disponible)"
    else
        warning "USB Utils manquant après installation"
    fi
    
    info "Installation du support WiFi..."
    apt-get install -y -qq \
        wpasupplicant \
        wireless-tools \
        iw \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation WiFi"
    
    # ✅ VÉRIFICATION WiFi
    if command -v iwconfig &> /dev/null; then
        success "WiFi Utils installé (iwconfig, iw disponibles)"
    else
        warning "WiFi Utils manquant après installation"
    fi
    
    info "Installation des outils réseau..."
    apt-get install -y -qq \
        net-tools \
        ifupdown \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation réseau"
    
    # ✅ VÉRIFICATION Réseau
    if command -v ifconfig &> /dev/null && command -v netstat &> /dev/null; then
        success "Outils réseau installés (ifconfig, netstat disponibles)"
    else
        warning "Certains outils réseau manquants"
    fi
    
    info "Installation du support Bluetooth..."
    apt-get install -y -qq \
        bluez \
        bluez-tools \
        libbluetooth-dev \
        pi-bluetooth \
        2>&1 | tee -a "$LOG_FILE" || {
            warning "pi-bluetooth non disponible (normal sur non-Raspberry Pi)"
            apt-get install -y -qq bluez bluez-tools libbluetooth-dev 2>&1 | tee -a "$LOG_FILE" || error "Échec installation Bluetooth"
        }
    
    # ✅ VÉRIFICATION Bluetooth
    if command -v bluetoothctl &> /dev/null; then
        success "Bluetooth installé (bluetoothctl disponible)"
        # Activer et démarrer le service Bluetooth
        systemctl enable bluetooth 2>&1 | tee -a "$LOG_FILE" || warning "Impossible d'activer le service Bluetooth"
        systemctl start bluetooth 2>&1 | tee -a "$LOG_FILE" || warning "Impossible de démarrer le service Bluetooth"
    else
        warning "Bluetooth manquant après installation"
    fi
    
    info "Installation de WebSocketpp..."
    apt-get install -y -qq \
        libwebsocketpp-dev \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation websocketpp"
    
    info "Installation des bibliothèques système..."
    apt-get install -y -qq \
        libsqlite3-dev sqlite3 \
        libboost-all-dev \
        libssl-dev \
        libudev-dev \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation system libs"
    
    info "Installation de Nginx..."
    apt-get install -y -qq nginx 2>&1 | tee -a "$LOG_FILE" || error "Échec installation nginx"
    
    success "Dépendances système installées"
}

# ============================================================================
# ÉTAPE 3: INSTALLATION DÉPENDANCES C++
# ============================================================================

install_cpp_dependencies() {
    log "📚 ÉTAPE 3/11: Installation des dépendances C++"
    
    info "Installation de nlohmann/json..."
    apt-get install -y -qq nlohmann-json3-dev 2>&1 | tee -a "$LOG_FILE" || {
        warning "nlohmann-json3-dev non disponible, utilisation de la version embarquée"
    }
    
    # Vérifier version installée
    if dpkg -s nlohmann-json3-dev &>/dev/null; then
        local json_version=$(dpkg -s nlohmann-json3-dev | grep '^Version:' | awk '{print $2}')
        info "nlohmann-json version: $json_version"
    fi
    
    success "Bibliothèques C++ installées"
}

# ============================================================================
# ÉTAPE 4: CONFIGURATION ALSA TEMPS RÉEL
# ============================================================================

configure_alsa() {
    log "🎵 ÉTAPE 4/11: Configuration ALSA pour temps réel"
    
    # Configuration ALSA globale
    if [ ! -f /etc/asound.conf ]; then
        info "Création de /etc/asound.conf..."
        cat > /etc/asound.conf << 'EOF'
# MidiMind ALSA Configuration
# Optimized for low-latency MIDI

pcm.!default {
    type hw
    card 0
}

ctl.!default {
    type hw
    card 0
}
EOF
        success "Configuration ALSA créée"
    else
        info "Configuration ALSA existante conservée"
    fi
    
    # Règles udev pour MIDI
    if [ ! -f /etc/udev/rules.d/99-midi.rules ]; then
        info "Création des règles udev MIDI..."
        cat > /etc/udev/rules.d/99-midi.rules << 'EOF'
# MIDI devices realtime priority
KERNEL=="midi*", MODE="0666"
SUBSYSTEM=="sound", GROUP="audio", MODE="0666"
EOF
        udevadm control --reload-rules
        success "Règles udev MIDI créées"
    else
        info "Règles udev MIDI existantes conservées"
    fi
}

# ============================================================================
# ÉTAPE 5: CONFIGURATION PERMISSIONS
# ============================================================================

configure_permissions() {
    log "🔑 ÉTAPE 5/11: Configuration des permissions utilisateur"
    
    # Ajouter l'utilisateur aux groupes nécessaires
    info "Ajout de l'utilisateur $REAL_USER aux groupes audio, dialout, plugdev, bluetooth..."
    
    usermod -a -G audio "$REAL_USER" 2>&1 | tee -a "$LOG_FILE" || warning "Échec ajout groupe audio"
    usermod -a -G dialout "$REAL_USER" 2>&1 | tee -a "$LOG_FILE" || warning "Échec ajout groupe dialout"
    usermod -a -G plugdev "$REAL_USER" 2>&1 | tee -a "$LOG_FILE" || warning "Échec ajout groupe plugdev"
    usermod -a -G bluetooth "$REAL_USER" 2>&1 | tee -a "$LOG_FILE" || warning "Échec ajout groupe bluetooth (normal si non disponible)"
    
    success "Permissions utilisateur configurées"
    info "Redémarrage requis pour appliquer les groupes"
}

# ============================================================================
# ÉTAPE 6: OPTIMISATIONS SYSTÈME
# ============================================================================

configure_system_optimizations() {
    log "⚡ ÉTAPE 6/11: Optimisations système pour temps réel"
    
    # Limites temps réel
    if [ ! -f /etc/security/limits.d/audio.conf ]; then
        info "Configuration des limites temps réel..."
        cat > /etc/security/limits.d/audio.conf << EOF
@audio   -  rtprio     95
@audio   -  memlock    unlimited
$REAL_USER   -  rtprio     95
$REAL_USER   -  memlock    unlimited
EOF
        success "Limites temps réel configurées"
    else
        info "Limites temps réel existantes conservées"
    fi
    
    # Swappiness
    if ! grep -q "vm.swappiness" /etc/sysctl.conf; then
        info "Configuration swappiness..."
        echo "vm.swappiness=10" >> /etc/sysctl.conf
        sysctl -p &>/dev/null
        success "Swappiness configurée"
    fi
}

# ============================================================================
# ÉTAPE 7: CRÉATION RÉPERTOIRES
# ============================================================================

create_directories() {
    log "📁 ÉTAPE 7/11: Création des répertoires système"
    
    info "Création de la structure de répertoires..."
    
    # Répertoires principaux
    mkdir -p "$INSTALL_DIR"/{bin,lib,data/migrations,logs,presets,sessions}
    mkdir -p /etc/midimind
    mkdir -p "$WEB_DIR"
    mkdir -p "$USER_DIR"/{presets,sessions,exports}
    
    success "Structure de répertoires créée"
    
    # Permissions
    chown -R "$REAL_USER:$REAL_USER" "$USER_DIR"
    chmod 755 "$INSTALL_DIR"
    chmod 755 /etc/midimind
    
    success "Permissions configurées"
}

# ============================================================================
# ÉTAPE 8: COMPILATION BACKEND
# ============================================================================

compile_backend() {
    log "🔨 ÉTAPE 8/11: Compilation du backend"
    
    cd "$BACKEND_DIR"
    
    # Nettoyage
    if [ -d "$BUILD_DIR" ]; then
        info "Nettoyage du build précédent..."
        rm -rf "$BUILD_DIR"
    fi
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    info "Configuration CMake..."
    cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | tee -a "$LOG_FILE" || error "Échec configuration CMake"
    
    info "Compilation en cours (sur $NPROC cœurs)..."
    make -j$NPROC 2>&1 | tee -a "$LOG_FILE" || error "Échec compilation"
    
    # Vérification binaire
    if [ ! -f "$BUILD_DIR/midimind" ]; then
        error "Binaire midimind non généré"
    fi
    
    success "Backend compilé avec succès"
    
    # Installation binaire
    info "Installation du binaire..."
    cp "$BUILD_DIR/midimind" "$INSTALL_DIR/bin/" || error "Échec copie binaire"
    chmod +x "$INSTALL_DIR/bin/midimind"
    success "Binaire installé: $INSTALL_DIR/bin/midimind"
    
    # Copie des migrations SQL
    if [ -d "$BACKEND_DIR/data/migrations" ]; then
        info "Copie des migrations SQL..."
        cp -r "$BACKEND_DIR/data/migrations/"* "$INSTALL_DIR/data/migrations/" 2>/dev/null || true
        local copied_count=$(ls -1 "$INSTALL_DIR/data/migrations/"*.sql 2>/dev/null | wc -l)
        if [ $copied_count -gt 0 ]; then
            success "Migrations SQL copiées: $copied_count fichiers"
        else
            info "Aucune migration SQL à copier"
        fi
    fi
    
    # Création config.json avec structure COMPLÈTE v4.1.0
    info "Création de /etc/midimind/config.json..."
    cat > /etc/midimind/config.json << 'EOF'
{
  "api": {
    "host": "0.0.0.0",
    "port": 8080,
    "log_level": "info"
  },
  "database": {
    "path": "/opt/midimind/data/midimind.db",
    "migrations_path": "/opt/midimind/data/migrations"
  },
  "paths": {
    "presets": "/opt/midimind/presets",
    "sessions": "/opt/midimind/sessions",
    "logs": "/opt/midimind/logs",
    "exports": "/home/USER/.midimind/exports",
    "user_dir": "/home/USER/.midimind"
  },
  "midi": {
    "buffer_size": 1024,
    "enable_sysex": true,
    "enable_active_sensing": false,
    "virtual_ports": true
  },
  "latency": {
    "enable_compensation": true,
    "manual_adjustment_ms": 0
  },
  "system": {
    "enable_monitoring": true,
    "log_midi_events": false,
    "max_connections": 10
  }
}
EOF
    
    # Remplacer USER par le vrai nom d'utilisateur
    sed -i "s|/home/USER|/home/$REAL_USER|g" /etc/midimind/config.json
    
    chmod 644 /etc/midimind/config.json
    success "Configuration créée: /etc/midimind/config.json"
}

# ============================================================================
# ÉTAPE 9: INSTALLATION FRONTEND
# ============================================================================

install_frontend() {
    log "🌐 ÉTAPE 9/11: Installation du frontend"
    
    if [ ! -d "$FRONTEND_DIR" ]; then
        error "Frontend introuvable: $FRONTEND_DIR"
    fi
    
    info "Copie des fichiers frontend..."
    rm -rf "$WEB_DIR"/*
    cp -r "$FRONTEND_DIR"/* "$WEB_DIR/" || error "Échec copie frontend"
    
    # Permissions
    chown -R www-data:www-data "$WEB_DIR"
    find "$WEB_DIR" -type f -exec chmod 644 {} \;
    find "$WEB_DIR" -type d -exec chmod 755 {} \;
    
    success "Frontend installé: $WEB_DIR"
}

# ============================================================================
# ÉTAPE 10: CONFIGURATION NGINX
# ============================================================================

configure_nginx() {
    log "🌐 ÉTAPE 10/11: Configuration Nginx"
    
    info "Création de la configuration Nginx..."
    cat > /etc/nginx/sites-available/midimind << 'EOF'
server {
    listen 8000;
    server_name _;

    root /var/www/midimind;
    index index.html;

    access_log /var/log/nginx/midimind_access.log;
    error_log /var/log/nginx/midimind_error.log;

    location / {
        try_files $uri $uri/ /index.html;
    }

    location /api/ {
        proxy_pass http://localhost:8080/;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
EOF
    
    # Activer le site
    ln -sf /etc/nginx/sites-available/midimind /etc/nginx/sites-enabled/midimind
    
    # Tester la configuration
    if nginx -t 2>&1 | tee -a "$LOG_FILE"; then
        success "Configuration Nginx valide"
    else
        error "Configuration Nginx invalide"
    fi
    
    # Redémarrer Nginx
    systemctl restart nginx || error "Échec redémarrage Nginx"
    systemctl enable nginx
    
    success "Nginx configuré et redémarré"
}

# ============================================================================
# ÉTAPE 11: SERVICE SYSTEMD
# ============================================================================

configure_systemd_service() {
    log "🚀 ÉTAPE 11/11: Configuration du service systemd"
    
    info "Création du service systemd..."
    cat > /etc/systemd/system/midimind.service << EOF
[Unit]
Description=MidiMind - Professional MIDI Orchestration System
After=network.target sound.target

[Service]
Type=simple
User=$REAL_USER
WorkingDirectory=$INSTALL_DIR
ExecStart=$INSTALL_DIR/bin/midimind --config /etc/midimind/config.json
Restart=always
RestartSec=3

# Permissions
SupplementaryGroups=audio dialout plugdev

# Sécurité
NoNewPrivileges=true
PrivateTmp=true

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=midimind

[Install]
WantedBy=multi-user.target
EOF
    
    success "Service systemd créé"
    
    # Recharger systemd
    systemctl daemon-reload
    
    # Activer le service
    systemctl enable midimind.service 2>&1 | tee -a "$LOG_FILE" || error "Échec activation service"
    success "Service activé au démarrage"
}

# ============================================================================
# TEST DÉMARRAGE BACKEND
# ============================================================================

test_backend_startup() {
    log "🧪 Test de démarrage du backend..."
    
    info "Démarrage du service..."
    systemctl start midimind.service 2>&1 | tee -a "$LOG_FILE"
    
    # Attendre que le service démarre
    sleep 3
    
    # Vérifier le statut
    if systemctl is-active --quiet midimind.service; then
        success "Service démarré avec succès"
    else
        error "Le service n'a pas démarré correctement\n  Vérifiez les logs: sudo journalctl -u midimind -n 50"
    fi
    
    # Vérifier que le port 8080 est ouvert
    if netstat -tuln | grep -q ":8080"; then
        success "Port 8080 ouvert (WebSocket actif)"
    else
        warning "Port 8080 non ouvert - le WebSocket peut ne pas fonctionner"
        info "Logs du service:"
        journalctl -u midimind -n 20 --no-pager | tee -a "$LOG_FILE"
    fi
    
    # Tester la connexion
    if timeout 2 bash -c "echo > /dev/tcp/localhost/8080" 2>/dev/null; then
        success "Backend accessible sur le port 8080"
    else
        warning "Backend ne répond pas sur le port 8080"
    fi
}

# ============================================================================
# VÉRIFICATION FINALE
# ============================================================================

verify_installation() {
    log "✅ Vérification finale de l'installation..."
    
    echo ""
    echo -e "${CYAN}${BOLD}🔍 Vérifications finales:${NC}"
    echo ""
    
    # Vérifier binaire
    if [ -x "$INSTALL_DIR/bin/midimind" ]; then
        success "Binaire exécutable: $INSTALL_DIR/bin/midimind"
    else
        error "Binaire midimind non exécutable"
    fi
    
    # Vérifier config
    if [ -f /etc/midimind/config.json ]; then
        success "Configuration: /etc/midimind/config.json"
    else
        error "Fichier config.json manquant"
    fi
    
    # Vérifier DB
    if [ -f "$INSTALL_DIR/data/midimind.db" ]; then
        local table_count=$(sqlite3 "$INSTALL_DIR/data/midimind.db" "SELECT COUNT(*) FROM sqlite_master WHERE type='table';" 2>/dev/null || echo "0")
        if [ "$table_count" -ge 5 ]; then
            success "Base de données: $table_count tables"
        else
            warning "Base de données: seulement $table_count tables (attendu: ≥5)"
        fi
    else
        error "Base de données manquante"
    fi
    
    # Vérifier ALSA
    if aconnect -l &>/dev/null; then
        success "ALSA Sequencer accessible"
    else
        warning "ALSA Sequencer non accessible"
    fi
    
    # Vérifier USB
    if lsusb &>/dev/null; then
        success "USB fonctionnel"
    else
        warning "USB non accessible"
    fi
    
    # Vérifier Bluetooth
    if bluetoothctl --version &>/dev/null; then
        success "Bluetooth disponible"
    else
        warning "Bluetooth non disponible"
    fi
    
    # Vérifier service
    if systemctl is-active --quiet midimind.service; then
        success "Service midimind actif"
    else
        warning "Service midimind non actif"
    fi
    
    # Vérifier Nginx
    if systemctl is-active --quiet nginx; then
        success "Service Nginx actif"
    else
        warning "Service Nginx non actif"
    fi
    
    echo ""
    success "✅ Vérification terminée"
}

# ============================================================================
# INFORMATIONS FINALES
# ============================================================================

print_final_info() {
    local ip=$(hostname -I | awk '{print $1}')
    
    echo ""
    echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                                                              ║${NC}"
    echo -e "${GREEN}║          ✅ INSTALLATION TERMINÉE AVEC SUCCÈS ✅            ║${NC}"
    echo -e "${GREEN}║                                                              ║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    echo -e "${CYAN}📂 Configuration installée:${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Backend:           ${GREEN}$INSTALL_DIR/bin/midimind${NC}"
    echo -e "  ${BLUE}•${NC} Frontend:          ${GREEN}$WEB_DIR${NC}"
    echo -e "  ${BLUE}•${NC} Configuration:     ${GREEN}/etc/midimind/config.json${NC}"
    echo -e "  ${BLUE}•${NC} Base de données:   ${GREEN}$INSTALL_DIR/data/midimind.db${NC}"
    echo -e "  ${BLUE}•${NC} Migrations:        ${GREEN}$INSTALL_DIR/data/migrations/${NC}"
    echo ""
    
    echo -e "${CYAN}🌐 Accès:${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Interface Web:     ${GREEN}http://$ip:8000${NC}"
    echo -e "  ${BLUE}•${NC} WebSocket API:     ${GREEN}ws://$ip:8080${NC}"
    echo ""
    
    echo -e "${CYAN}🚀 Commandes utiles:${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Status:        ${GREEN}sudo systemctl status midimind${NC}"
    echo -e "  ${BLUE}•${NC} Redémarrer:    ${GREEN}sudo systemctl restart midimind${NC}"
    echo -e "  ${BLUE}•${NC} Arrêter:       ${GREEN}sudo systemctl stop midimind${NC}"
    echo -e "  ${BLUE}•${NC} Logs:          ${GREEN}sudo journalctl -u midimind -f${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Nginx status:  ${GREEN}sudo systemctl status nginx${NC}"
    echo -e "  ${BLUE}•${NC} Nginx logs:    ${GREEN}tail -f /var/log/nginx/midimind_error.log${NC}"
    echo ""
    
    echo -e "${CYAN}🎵 Commandes ALSA:${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Ports MIDI:    ${GREEN}aconnect -l${NC}"
    echo -e "  ${BLUE}•${NC} Sorties MIDI:  ${GREEN}aconnect -o${NC}"
    echo -e "  ${BLUE}•${NC} Devices MIDI:  ${GREEN}amidi -l${NC}"
    echo -e "  ${BLUE}•${NC} Monitor MIDI:  ${GREEN}aseqdump -p 14:0${NC}"
    echo ""
    
    echo -e "${CYAN}🔌 Commandes USB/Bluetooth/Réseau:${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Devices USB:   ${GREEN}lsusb${NC}"
    echo -e "  ${BLUE}•${NC} Bluetooth:     ${GREEN}bluetoothctl${NC}"
    echo -e "  ${BLUE}•${NC} WiFi:          ${GREEN}iwconfig${NC}"
    echo -e "  ${BLUE}•${NC} Réseau:        ${GREEN}ifconfig${NC}"
    echo ""
    
    echo -e "${CYAN}🔍 Vérifications:${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Port backend:  ${GREEN}netstat -tuln | grep 8080${NC}"
    echo -e "  ${BLUE}•${NC} Port frontend: ${GREEN}netstat -tuln | grep 8000${NC}"
    echo -e "  ${BLUE}•${NC} Test backend:  ${GREEN}curl http://localhost:8080${NC}"
    echo -e "  ${BLUE}•${NC} Check DB:      ${GREEN}sqlite3 $INSTALL_DIR/data/midimind.db '.tables'${NC}"
    echo ""
    
    echo -e "${GREEN}✅ Le système est prêt à l'emploi !${NC}"
    echo -e "${GREEN}   Accédez à l'interface: http://$ip:8000${NC}"
    echo ""
    
    echo -e "${YELLOW}⚠  IMPORTANT:${NC}"
    echo -e "${YELLOW}   Redémarrez le système pour appliquer les permissions audio/bluetooth${NC}"
    echo -e "${YELLOW}   Commande: ${GREEN}sudo reboot${NC}"
    echo ""
    
    echo -e "${GREEN}Installation log: $LOG_FILE${NC}"
    echo ""
}

# ============================================================================
# FONCTION MAIN
# ============================================================================

main() {
    print_banner
    
    # Initialisation log
    echo "==================================" > "$LOG_FILE"
    echo "MidiMind Installation v4.1.4 - $(date)" >> "$LOG_FILE"
    echo "==================================" >> "$LOG_FILE"
    log "Installation démarrée: $(date)"
    
    # Détection et vérifications
    detect_system
    check_prerequisites
    
    echo ""
    echo -e "${CYAN}${BOLD}Installation complète (Backend + Frontend + Nginx + ALSA + USB + WiFi + Bluetooth)${NC}"
    echo -e "${CYAN}Cela peut prendre 10-15 minutes sur Raspberry Pi...${NC}"
    echo ""
    
    read -p "$(echo -e ${GREEN}Continuer? [O/n]: ${NC})" response
    if [[ "$response" =~ ^[Nn]$ ]]; then
        echo ""
        echo -e "${RED}Installation annulée.${NC}"
        exit 0
    fi
    
    # Installation étape par étape
    echo ""
    update_system
    echo ""
    install_system_dependencies
    echo ""
    install_cpp_dependencies
    echo ""
    configure_alsa
    echo ""
    configure_permissions
    echo ""
    configure_system_optimizations
    echo ""
    create_directories
    echo ""
    compile_backend
    echo ""
    install_frontend
    echo ""
    configure_nginx
    echo ""
    configure_systemd_service
    echo ""
    test_backend_startup
    echo ""
    verify_installation
    
    # Informations finales
    print_final_info
    
    log "Installation terminée: $(date)"
}

# ============================================================================
# POINT D'ENTRÉE
# ============================================================================

main 2>&1 | tee -a "$LOG_FILE"

# ============================================================================
# FIN DU FICHIER install.sh v4.1.4 - USB + WiFi + Réseau + Bluetooth
# ============================================================================