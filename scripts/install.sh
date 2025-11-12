#!/bin/bash
# ============================================================================
# Fichier: scripts/install.sh
# Version: 4.1.5 - FIX répertoires data manquants (uploads, playlists, etc.)
# Date: 2025-11-12
# Projet: MidiMind - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
#
# CORRECTIONS v4.1.5:
#   ✅ FIX: Création répertoires data manquants (uploads, playlists, sessions, recordings)
#   ✅ FIX: Création dans /home/pi/MidiMind ET /opt/midimind pour compatibilité
#   ✅ FIX: Ajout config.json avec chemins data corrects
#
# CORRECTIONS v4.1.4:
#   ✅ Support USB: libusb-1.0-0-dev, usbutils
#   ✅ Support WiFi: wpasupplicant, wireless-tools, iw
#   ✅ Support Réseau: net-tools, ifupdown
#   ✅ Support Bluetooth: bluez, bluez-tools, libbluetooth-dev, pi-bluetooth
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
DATA_DIR="/home/$REAL_USER/MidiMind"

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
║              🎹 MidiMind v4.1.5 Installation ⚡               ║
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
    log "📋 Vérification des prérequis..."
    
    # Vérifier root
    if [ "$EUID" -ne 0 ]; then
        error "Ce script doit être exécuté avec sudo"
    fi
    success "Permissions root validées"
    
    # Vérifier user réel
    if [ -z "$REAL_USER" ] || [ "$REAL_USER" = "root" ]; then
        error "Impossible de déterminer l'utilisateur réel"
    fi
    success "Utilisateur: $REAL_USER"
    
    # Vérifier connexion internet
    if ping -c 1 8.8.8.8 &>/dev/null; then
        success "Connexion internet active"
    else
        warning "Connexion internet non détectée (certains packages pourraient échouer)"
    fi
}

# ============================================================================
# ÉTAPE 1: MISE À JOUR SYSTÈME
# ============================================================================

update_system() {
    log "🔄 ÉTAPE 1/11: Mise à jour du système"
    
    info "Mise à jour des dépôts..."
    apt-get update -qq 2>&1 | tee -a "$LOG_FILE" || warning "Échec mise à jour dépôts"
    
    success "Système mis à jour"
}

# ============================================================================
# ÉTAPE 2: DÉPENDANCES SYSTÈME
# ============================================================================

install_system_dependencies() {
    log "📦 ÉTAPE 2/11: Installation des dépendances système"
    
    info "Installation des packages système..."
    
    # Packages essentiels
    apt-get install -y -qq \
        build-essential \
        cmake \
        git \
        pkg-config \
        sqlite3 \
        libsqlite3-dev \
        nginx \
        curl \
        wget 2>&1 | tee -a "$LOG_FILE" || error "Échec installation packages de base"
    
    success "Packages de base installés"
    
    # ALSA
    info "Installation ALSA..."
    apt-get install -y -qq \
        libasound2-dev \
        alsa-utils \
        alsa-tools 2>&1 | tee -a "$LOG_FILE" || error "Échec installation ALSA"
    
    success "ALSA installé"
    
    # USB Support
    info "Installation support USB..."
    apt-get install -y -qq \
        libusb-1.0-0-dev \
        usbutils 2>&1 | tee -a "$LOG_FILE" || error "Échec installation USB"
    
    success "Support USB installé"
    
    # WiFi Support
    info "Installation support WiFi..."
    apt-get install -y -qq \
        wpasupplicant \
        wireless-tools \
        iw 2>&1 | tee -a "$LOG_FILE" || error "Échec installation WiFi"
    
    success "Support WiFi installé"
    
    # Network Support
    info "Installation support réseau..."
    apt-get install -y -qq \
        net-tools \
        ifupdown 2>&1 | tee -a "$LOG_FILE" || error "Échec installation réseau"
    
    success "Support réseau installé"
    
    # Bluetooth Support
    info "Installation support Bluetooth..."
    apt-get install -y -qq \
        bluez \
        bluez-tools \
        libbluetooth-dev \
        pi-bluetooth 2>&1 | tee -a "$LOG_FILE" || warning "Bluetooth partiellement installé"
    
    success "Support Bluetooth installé"
}

# ============================================================================
# ÉTAPE 3: DÉPENDANCES C++
# ============================================================================

install_cpp_dependencies() {
    log "🔧 ÉTAPE 3/11: Installation des dépendances C++"
    
    info "Installation Boost..."
    apt-get install -y -qq \
        libboost-all-dev 2>&1 | tee -a "$LOG_FILE" || error "Échec installation Boost"
    
    success "Boost installé"
    
    info "Installation WebSocket++..."
    apt-get install -y -qq \
        libwebsocketpp-dev 2>&1 | tee -a "$LOG_FILE" || error "Échec installation WebSocket++"
    
    success "WebSocket++ installé"
    
    info "Installation nlohmann-json..."
    apt-get install -y -qq \
        nlohmann-json3-dev 2>&1 | tee -a "$LOG_FILE" || error "Échec installation nlohmann-json"
    
    success "nlohmann-json installé"
    
    info "Installation GIO (D-Bus)..."
    apt-get install -y -qq \
        libglib2.0-dev 2>&1 | tee -a "$LOG_FILE" || error "Échec installation GIO"
    
    success "GIO installé"
}

# ============================================================================
# ÉTAPE 4: CONFIGURATION ALSA
# ============================================================================

configure_alsa() {
    log "🎵 ÉTAPE 4/11: Configuration ALSA"
    
    if [ ! -f /etc/asound.conf ]; then
        info "Création de /etc/asound.conf..."
        cat > /etc/asound.conf << 'EOF'
# ALSA Configuration for MidiMind
# Real-time MIDI processing

pcm.!default {
    type hw
    card 0
}

ctl.!default {
    type hw
    card 0
}

# MIDI Sequencer
seq.default {
    type hw
}
EOF
        success "Configuration ALSA créée"
    else
        info "Configuration ALSA existante conservée"
    fi
    
    # Règles udev pour MIDI
    if [ ! -f /etc/udev/rules.d/99-midi.rules ]; then
        info "Configuration des règles udev MIDI..."
        cat > /etc/udev/rules.d/99-midi.rules << EOF
# MIDI devices - Real-time priority
KERNEL=="midi[0-9]*", GROUP="audio", MODE="0660"
KERNEL=="seq", GROUP="audio", MODE="0660"
SUBSYSTEM=="sound", GROUP="audio", MODE="0660"
EOF
        udevadm control --reload-rules &>/dev/null
        success "Règles udev MIDI configurées"
    else
        info "Règles udev MIDI existantes conservées"
    fi
}

# ============================================================================
# ÉTAPE 5: PERMISSIONS UTILISATEUR
# ============================================================================

configure_permissions() {
    log "🔐 ÉTAPE 5/11: Configuration des permissions"
    
    info "Ajout de $REAL_USER aux groupes audio, bluetooth, dialout..."
    usermod -a -G audio "$REAL_USER" 2>/dev/null || warning "Groupe audio non ajouté"
    usermod -a -G bluetooth "$REAL_USER" 2>/dev/null || warning "Groupe bluetooth non ajouté"
    usermod -a -G dialout "$REAL_USER" 2>/dev/null || warning "Groupe dialout non ajouté"
    
    success "Permissions utilisateur configurées"
}

# ============================================================================
# ÉTAPE 6: OPTIMISATIONS SYSTÈME
# ============================================================================

configure_system_optimizations() {
    log "⚡ ÉTAPE 6/11: Optimisations système temps réel"
    
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
# ÉTAPE 7: CRÉATION RÉPERTOIRES (CORRIGÉ v4.1.5)
# ============================================================================

create_directories() {
    log "📁 ÉTAPE 7/11: Création des répertoires système"
    
    info "Création de la structure de répertoires..."
    
    # Répertoires principaux /opt/midimind
    mkdir -p "$INSTALL_DIR"/{bin,lib,logs,presets,sessions}
    mkdir -p "$INSTALL_DIR"/data/{migrations,uploads,midi,playlists,sessions,recordings}
    mkdir -p /etc/midimind
    mkdir -p "$WEB_DIR"
    
    # Répertoires utilisateur ~/.midimind
    mkdir -p "$USER_DIR"/{presets,sessions,exports}
    
    # ✅ FIX v4.1.5: Créer aussi dans /home/pi/MidiMind (chemin par défaut backend)
    mkdir -p "$DATA_DIR"/data/{migrations,uploads,midi,playlists,sessions,recordings}
    
    success "Structure de répertoires créée"
    
    # Permissions
    chown -R "$REAL_USER:$REAL_USER" "$INSTALL_DIR"
    chown -R "$REAL_USER:$REAL_USER" "$USER_DIR"
    chown -R "$REAL_USER:$REAL_USER" "$DATA_DIR"
    chmod -R 755 "$INSTALL_DIR"
    chmod -R 755 "$DATA_DIR"
    chmod 755 /etc/midimind
    
    success "Permissions configurées"
    
    info "Répertoires créés:"
    info "  • /opt/midimind/data/uploads"
    info "  • $DATA_DIR/data/uploads"
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
    if [ ! -f "$BUILD_DIR/bin/midimind" ]; then
        error "Binaire midimind non généré"
    fi
    
    success "Backend compilé avec succès"
    
    # Installation binaire
    info "Installation du binaire..."
    cp "$BUILD_DIR/bin/midimind" "$INSTALL_DIR/bin/" || error "Échec copie binaire"
    chmod +x "$INSTALL_DIR/bin/midimind"
    chown "$REAL_USER:$REAL_USER" "$INSTALL_DIR/bin/midimind"
    success "Binaire installé: $INSTALL_DIR/bin/midimind"
    
    # Copie des migrations SQL
    if [ -d "$BACKEND_DIR/data/migrations" ]; then
        info "Copie des migrations SQL..."
        cp -r "$BACKEND_DIR/data/migrations/"*.sql "$INSTALL_DIR/data/migrations/" 2>/dev/null || true
        cp -r "$BACKEND_DIR/data/migrations/"*.sql "$DATA_DIR/data/migrations/" 2>/dev/null || true
        local copied_count=$(ls -1 "$INSTALL_DIR/data/migrations/"*.sql 2>/dev/null | wc -l)
        if [ $copied_count -gt 0 ]; then
            success "Migrations SQL copiées: $copied_count fichiers"
        else
            info "Aucune migration SQL à copier"
        fi
    fi
    chown -R "$REAL_USER:$REAL_USER" "$INSTALL_DIR/data"
    chown -R "$REAL_USER:$REAL_USER" "$DATA_DIR/data"
    
    # ✅ FIX v4.1.5: Création config.json avec TOUS les chemins data
    info "Création de /etc/midimind/config.json..."
    cat > /etc/midimind/config.json << EOF
{
  "api": {
    "host": "0.0.0.0",
    "port": 8080,
    "log_level": "info"
  },
  "database": {
    "path": "$DATA_DIR/data/midimind.db",
    "migrations_path": "$DATA_DIR/data/migrations"
  },
  "paths": {
    "data_dir": "$DATA_DIR/data",
    "uploads": "$DATA_DIR/data/uploads",
    "playlists": "$DATA_DIR/data/playlists",
    "sessions": "$DATA_DIR/data/sessions",
    "recordings": "$DATA_DIR/data/recordings",
    "presets": "/opt/midimind/presets",
    "logs": "/opt/midimind/logs",
    "exports": "/home/$REAL_USER/.midimind/exports",
    "user_dir": "/home/$REAL_USER/.midimind"
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
    
    chmod 644 /etc/midimind/config.json
    success "Configuration créée: /etc/midimind/config.json"
}

# ============================================================================
# ÉTAPE 9: INSTALLATION FRONTEND
# ============================================================================

install_frontend() {
    log "🌐 ÉTAPE 9/11: Installation du frontend"
    
    info "Copie des fichiers frontend..."
    cp -r "$FRONTEND_DIR"/* "$WEB_DIR/" || error "Échec copie frontend"
    
    chown -R www-data:www-data "$WEB_DIR"
    chmod -R 755 "$WEB_DIR"
    
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
    
    location / {
        try_files $uri $uri/ =404;
    }
    
    location /api/ {
        proxy_pass http://localhost:8080/;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_cache_bypass $http_upgrade;
    }
    
    error_log /var/log/nginx/midimind_error.log;
    access_log /var/log/nginx/midimind_access.log;
}
EOF
    
    # Activer le site
    if [ ! -L /etc/nginx/sites-enabled/midimind ]; then
        ln -s /etc/nginx/sites-available/midimind /etc/nginx/sites-enabled/
    fi
    
    # Tester configuration
    nginx -t 2>&1 | tee -a "$LOG_FILE" || error "Configuration Nginx invalide"
    
    # Redémarrer Nginx
    systemctl restart nginx || error "Échec redémarrage Nginx"
    systemctl enable nginx &>/dev/null
    
    success "Nginx configuré et démarré"
}

# ============================================================================
# ÉTAPE 11: SERVICE SYSTEMD
# ============================================================================

configure_systemd_service() {
    log "⚙️  ÉTAPE 11/11: Configuration du service systemd"
    
    info "Création du service midimind.service..."
    cat > /etc/systemd/system/midimind.service << EOF
[Unit]
Description=MidiMind - MIDI Orchestration System
After=network.target sound.target

[Service]
Type=simple
User=$REAL_USER
Group=$REAL_USER
WorkingDirectory=$INSTALL_DIR
ExecStart=$INSTALL_DIR/bin/midimind --config /etc/midimind/config.json
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

# Permissions temps réel
LimitRTPRIO=95
LimitMEMLOCK=infinity

[Install]
WantedBy=multi-user.target
EOF
    
    chmod 644 /etc/systemd/system/midimind.service
    
    systemctl daemon-reload
    systemctl enable midimind.service &>/dev/null
    
    success "Service systemd configuré"
}

# ============================================================================
# TEST DÉMARRAGE BACKEND
# ============================================================================

test_backend_startup() {
    log "🧪 Test de démarrage du backend..."
    
    info "Démarrage du service midimind..."
    systemctl start midimind.service || error "Échec démarrage service"
    
    sleep 3
    
    if systemctl is-active --quiet midimind.service; then
        success "Service midimind démarré avec succès"
    else
        error "Service midimind n'a pas démarré correctement"
    fi
    
    # Vérifier que le port 8080 est ouvert
    sleep 2
    if netstat -tuln 2>/dev/null | grep -q ":8080"; then
        success "Backend écoute sur le port 8080"
    else
        warning "Port 8080 non détecté (peut prendre quelques secondes)"
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
    
    # Vérifier répertoires data
    if [ -d "$DATA_DIR/data/uploads" ]; then
        success "Répertoire uploads: $DATA_DIR/data/uploads"
    else
        warning "Répertoire uploads manquant"
    fi
    
    # Vérifier DB
    if [ -f "$DATA_DIR/data/midimind.db" ]; then
        local table_count=$(sqlite3 "$DATA_DIR/data/midimind.db" "SELECT COUNT(*) FROM sqlite_master WHERE type='table';" 2>/dev/null || echo "0")
        if [ "$table_count" -ge 5 ]; then
            success "Base de données: $table_count tables"
        else
            warning "Base de données: seulement $table_count tables (attendu: ≥5)"
        fi
    else
        warning "Base de données en attente de création au premier démarrage"
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
    echo -e "  ${BLUE}•${NC} Base de données:   ${GREEN}$DATA_DIR/data/midimind.db${NC}"
    echo -e "  ${BLUE}•${NC} Répertoire data:   ${GREEN}$DATA_DIR/data/${NC}"
    echo -e "  ${BLUE}•${NC} Uploads:           ${GREEN}$DATA_DIR/data/uploads/${NC}"
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
    echo -e "  ${BLUE}•${NC} Check DB:      ${GREEN}sqlite3 $DATA_DIR/data/midimind.db '.tables'${NC}"
    echo -e "  ${BLUE}•${NC} Check uploads: ${GREEN}ls -la $DATA_DIR/data/uploads/${NC}"
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
    echo "MidiMind Installation v4.1.5 - $(date)" >> "$LOG_FILE"
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
# FIN DU FICHIER install.sh v4.1.5
# ============================================================================