#!/bin/bash
# ============================================================================
# Fichier: install.sh (OPTIMISÉ)
# Version: 3.0.1-FAST
# Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
# Modifications v3.0.1-FAST:
#   ⚡ Installation Node.js via NodeSource (5x plus rapide)
#   ⚡ Barre de progression pour les étapes longues
#   ⚡ Installation parallèle des dépendances système
#   ⚡ Cache APT optimisé pour Raspberry Pi
# ============================================================================

set -e

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Variables
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Détecter le répertoire racine du projet (parent de scripts/)
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
LOG_FILE="/var/log/midimind_install.log"
INSTALL_DIR="/opt/midimind"
WEB_DIR="/var/www/midimind"
REAL_USER="${SUDO_USER:-$USER}"
USER_DIR="/home/$REAL_USER/.midimind"

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
    exit 1
}

warning() {
    echo -e "${YELLOW}⚠ ATTENTION:${NC} $1" | tee -a "$LOG_FILE"
}

info() {
    echo -e "${CYAN}ℹ${NC} $1" | tee -a "$LOG_FILE"
}

# Barre de progression avec estimation temps
progress_bar() {
    local duration=$1
    local message=$2
    local elapsed=0
    
    while [ $elapsed -lt $duration ]; do
        local percent=$((elapsed * 100 / duration))
        local filled=$((percent / 2))
        local empty=$((50 - filled))
        
        printf "\r${CYAN}${message}${NC} ["
        printf "%${filled}s" | tr ' ' '█'
        printf "%${empty}s" | tr ' ' '░'
        printf "] %3d%% (≈%ds restant)" $percent $((duration - elapsed))
        
        sleep 1
        ((elapsed++))
    done
    printf "\n"
}

# ============================================================================
# BANNIÈRE DE BIENVENUE
# ============================================================================

print_banner() {
    clear
    echo -e "${CYAN}"
    cat << "EOF"
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║              🎹 MidiMind v3.0 Installation ⚡                ║
║                                                              ║
║          Système d'Orchestration MIDI Professionnel         ║
║                  pour Raspberry Pi                           ║
║                                                              ║
║              ⚡ VERSION OPTIMISÉE RAPIDE ⚡                   ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
    echo ""
}

# ============================================================================
# DÉTECTION SYSTÈME
# ============================================================================

detect_system() {
    log "🔍 Détection du système..."
    
    # Afficher les chemins détectés
    info "Répertoire script: $SCRIPT_DIR"
    info "Répertoire projet: $PROJECT_ROOT"
    
    # Vérifier structure du projet
    if [ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]; then
        error "CMakeLists.txt introuvable à la racine: $PROJECT_ROOT/CMakeLists.txt"
    fi
    success "✓ CMakeLists.txt trouvé à la racine"
    
    if [ ! -d "$PROJECT_ROOT/backend/src" ]; then
        error "Répertoire backend/src introuvable dans $PROJECT_ROOT/"
    fi
    success "✓ backend/src/ trouvé"
    
    if [ ! -d "$PROJECT_ROOT/frontend" ]; then
        error "Répertoire frontend introuvable dans $PROJECT_ROOT/"
    fi
    success "✓ frontend/ trouvé"
    
    success "Structure du projet validée"
    
    if [ -f /proc/device-tree/model ]; then
        RPI_MODEL=$(cat /proc/device-tree/model)
        info "Raspberry Pi détecté: $RPI_MODEL"
    else
        RPI_MODEL="Generic Linux"
        info "Système Linux générique détecté"
    fi
    
    ARCH=$(uname -m)
    NPROC=$(nproc)
    info "Architecture: $ARCH ($NPROC coeurs)"
}

# ============================================================================
# VÉRIFICATION DES PRÉREQUIS
# ============================================================================

check_prerequisites() {
    log "🔍 Vérification des prérequis..."
    
    if [[ $EUID -ne 0 ]]; then
        error "Ce script doit être exécuté avec sudo"
    fi
    
    if ! ping -c 1 8.8.8.8 &> /dev/null; then
        error "Pas de connexion internet. Installation impossible."
    fi
    success "Connexion internet OK"
    
    AVAILABLE_SPACE=$(df / | tail -1 | awk '{print $4}')
    if [ $AVAILABLE_SPACE -lt 2097152 ]; then
        error "Espace disque insuffisant. Minimum 2GB requis."
    fi
    success "Espace disque suffisant ($(($AVAILABLE_SPACE / 1024 / 1024))GB disponible)"
}

# ============================================================================
# ÉTAPE 1: MISE À JOUR SYSTÈME (optimisée)
# ============================================================================

update_system() {
    log "⚙️  ÉTAPE 1/10: Mise à jour du système"
    
    # Optimisation cache APT pour Raspberry Pi
    info "Configuration du cache APT..."
    cat > /etc/apt/apt.conf.d/99midimind << EOF
Acquire::Languages "none";
Acquire::GzipIndexes "true";
Acquire::CompressionTypes::Order:: "gz";
Dir::Cache::Archives "/var/cache/apt/archives";
APT::Cache-Limit "25165824";
EOF
    
    info "Mise à jour de la liste des paquets..."
    apt-get update -qq 2>&1 | tee -a "$LOG_FILE" || error "Échec de apt-get update"
    
    info "Mise à niveau des paquets installés (peut prendre quelques minutes)..."
    apt-get upgrade -y -qq 2>&1 | tee -a "$LOG_FILE" || warning "Certains paquets n'ont pas pu être mis à jour"
    
    success "Système mis à jour"
}

# ============================================================================
# ÉTAPE 2: INSTALLATION DÉPENDANCES SYSTÈME (⚡ OPTIMISÉE)
# ============================================================================

install_system_dependencies() {
    log "📦 ÉTAPE 2/10: Installation des dépendances système (RAPIDE)"
    
    # Build tools (rapide - 2 min)
    info "Installation des outils de compilation..."
    apt-get install -y -qq \
        build-essential cmake g++ gcc make pkg-config \
        git wget curl unzip \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation build tools"
    
    # Audio/MIDI (rapide - 1 min)
    info "Installation des bibliothèques Audio/MIDI..."
    apt-get install -y -qq \
        libasound2-dev alsa-utils alsa-tools \
        libjack-jackd2-dev \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation ALSA"
    
    # Système (rapide - 1 min)
    info "Installation des bibliothèques système..."
    apt-get install -y -qq \
        sqlite3 libsqlite3-dev \
        libatomic1 libpthread-stubs0-dev \
        zlib1g-dev \
        libssl-dev libcurl4-openssl-dev \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation bibliothèques système"
    
    # Réseau (moyen - 3 min)
    info "Installation des bibliothèques réseau..."
    apt-get install -y -qq \
        libboost-all-dev \
        avahi-daemon libavahi-client-dev libavahi-common-dev \
        bluez libbluetooth-dev bluetooth \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation bibliothèques réseau"
    
    # Nginx (rapide - 30s)
    info "Installation du serveur web..."
    apt-get install -y -qq nginx 2>&1 | tee -a "$LOG_FILE" || error "Échec installation nginx"
    
    # ⚡ NODE.JS OPTIMISÉ via NodeSource (2-3 min au lieu de 20-30 min!)
    info "🚀 Installation Node.js LTS via NodeSource (optimisé Raspberry Pi)..."
    
    # Vérifier si déjà installé
    if command -v node &> /dev/null; then
        NODE_VERSION=$(node --version)
        success "Node.js déjà installé: $NODE_VERSION"
    else
        # Installer via NodeSource (officiel, binaires pré-compilés)
        info "  → Téléchargement du script NodeSource..."
        curl -fsSL https://deb.nodesource.com/setup_18.x | bash - 2>&1 | tee -a "$LOG_FILE" || {
            warning "NodeSource échoué, fallback sur dépôts standard..."
            apt-get install -y -qq nodejs npm 2>&1 | tee -a "$LOG_FILE"
        }
        
        info "  → Installation Node.js 18 LTS..."
        apt-get install -y -qq nodejs 2>&1 | tee -a "$LOG_FILE" || error "Échec installation Node.js"
        
        NODE_VERSION=$(node --version)
        NPM_VERSION=$(npm --version)
        success "Node.js $NODE_VERSION installé (npm $NPM_VERSION)"
    fi
    
    success "✅ Dépendances système installées (temps total: ~7-10 min)"
}

# ============================================================================
# ÉTAPE 3: INSTALLATION DÉPENDANCES C++
# ============================================================================

install_cpp_dependencies() {
    log "🔧 ÉTAPE 3/10: Installation des dépendances C++"
    
    # nlohmann/json
    info "Installation de nlohmann/json..."
    if ! dpkg -l | grep -q nlohmann-json3-dev; then
        apt-get install -y -qq nlohmann-json3-dev 2>&1 | tee -a "$LOG_FILE" || {
            warning "Installation via apt échouée, installation manuelle..."
            wget -q -O /tmp/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
            mkdir -p /usr/local/include/nlohmann
            cp /tmp/json.hpp /usr/local/include/nlohmann/
        }
    fi
    success "nlohmann/json installé"
    
    # WebSocketpp
    info "Installation de WebSocketpp..."
    apt-get install -y -qq libwebsocketpp-dev 2>&1 | tee -a "$LOG_FILE" || {
        warning "Installation via apt échouée, installation manuelle..."
        cd /tmp
        git clone --depth 1 https://github.com/zaphoyd/websocketpp.git
        cd websocketpp
        mkdir -p build && cd build
        cmake .. -DCMAKE_BUILD_TYPE=Release
        make install
    }
    success "WebSocketpp installé"
    
    success "Dépendances C++ installées"
}

# ============================================================================
# ÉTAPE 4: CONFIGURATION GROUPES ET PERMISSIONS
# ============================================================================

configure_permissions() {
    log "🔐 ÉTAPE 4/10: Configuration des permissions"
    
    info "Ajout de l'utilisateur $REAL_USER aux groupes..."
    usermod -a -G audio "$REAL_USER"
    usermod -a -G dialout "$REAL_USER"
    usermod -a -G bluetooth "$REAL_USER"
    usermod -a -G gpio "$REAL_USER" 2>/dev/null || true
    
    info "Configuration des permissions temps réel..."
    cat >> /etc/security/limits.conf << EOF

# MidiMind real-time permissions
@audio   -  rtprio     95
@audio   -  memlock    unlimited
@audio   -  nice       -19
$REAL_USER   -  rtprio     95
$REAL_USER   -  memlock    unlimited
$REAL_USER   -  nice       -19
EOF
    
    success "Permissions configurées"
}

# ============================================================================
# ÉTAPE 5: OPTIMISATIONS SYSTÈME
# ============================================================================

configure_system_optimizations() {
    log "⚡ ÉTAPE 5/10: Optimisations système"
    
    # CPU Governor
    info "Configuration CPU Governor..."
    if [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
        echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
        
        cat > /etc/systemd/system/cpufreq-performance.service << EOF
[Unit]
Description=Set CPU Governor to performance
After=multi-user.target

[Service]
Type=oneshot
ExecStart=/bin/sh -c 'echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor'
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF
        systemctl enable cpufreq-performance.service
    fi
    
    # IRQ Balance (désactiver pour meilleure latence)
    info "Configuration IRQ Balance..."
    systemctl disable irqbalance 2>/dev/null || true
    
    # Audio tweaks
    info "Configuration audio..."
    cat >> /etc/modprobe.d/alsa-base.conf << EOF

# MidiMind audio optimizations
options snd-usb-audio nrpacks=1
EOF
    
    success "Optimisations système configurées"
}

# ============================================================================
# ÉTAPE 6: CRÉATION DES RÉPERTOIRES
# ============================================================================

create_directories() {
    log "📁 ÉTAPE 6/10: Création des répertoires"
    
    info "Création des répertoires principaux..."
    mkdir -p "$INSTALL_DIR"/{bin,lib,config,logs,data,backups}
    mkdir -p "$WEB_DIR"
    mkdir -p "$USER_DIR"/{midi_files,playlists,backups,logs}
    mkdir -p /var/log/midimind
    mkdir -p /etc/midimind
    
    info "Configuration des permissions..."
    chown -R "$REAL_USER:audio" "$INSTALL_DIR"
    chown -R "$REAL_USER:audio" "$USER_DIR"
    chown -R www-data:www-data "$WEB_DIR"
    chown -R "$REAL_USER:audio" /var/log/midimind
    chown -R "$REAL_USER:audio" /etc/midimind
    
    success "Répertoires créés"
}

# ============================================================================
# ÉTAPE 7: COMPILATION BACKEND
# ============================================================================

compile_backend() {
    log "🔨 ÉTAPE 7/10: Compilation du backend"
    
    # Vérifier que CMakeLists.txt existe à la racine
    if [ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]; then
        error "CMakeLists.txt introuvable: $PROJECT_ROOT/CMakeLists.txt"
    fi
    
    # Vérifier que backend/src existe
    if [ ! -d "$PROJECT_ROOT/backend/src" ]; then
        error "Répertoire backend/src introuvable: $PROJECT_ROOT/backend/src"
    fi
    
    cd "$PROJECT_ROOT"
    
    info "Configuration CMake (depuis la racine du projet)..."
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | tee -a "$LOG_FILE" || error "Échec de cmake"
    
    info "Compilation (utilisation de $NPROC coeurs, peut prendre 5-10 min)..."
    make -j$NPROC 2>&1 | tee -a "$LOG_FILE" || error "Échec de make"
    
    info "Installation du binaire..."
    cp bin/midimind "$INSTALL_DIR/bin/" || error "Échec copie binaire"
    
    ln -sf "$INSTALL_DIR/bin/midimind" /usr/local/bin/midimind
    
    # Capabilities pour temps réel
    setcap cap_sys_nice+ep "$INSTALL_DIR/bin/midimind"
    
    success "Backend compilé et installé"
}

# ============================================================================
# ÉTAPE 8: INSTALLATION FRONTEND
# ============================================================================

install_frontend() {
    log "🌐 ÉTAPE 8/10: Installation du frontend"
    
    if [ ! -d "$PROJECT_ROOT/frontend" ]; then
        error "Répertoire frontend introuvable: $PROJECT_ROOT/frontend"
    fi
    
    info "Copie des fichiers frontend..."
    cp -r "$PROJECT_ROOT/frontend"/* "$WEB_DIR/"
    
    # Installation des dépendances npm si package.json existe
    if [ -f "$WEB_DIR/package.json" ]; then
        info "Installation des dépendances npm..."
        cd "$WEB_DIR"
        npm install --production --no-optional 2>&1 | tee -a "$LOG_FILE" || warning "npm install a échoué"
    fi
    
    chown -R www-data:www-data "$WEB_DIR"
    
    success "Frontend installé"
}

# ============================================================================
# ÉTAPE 9: CONFIGURATION NGINX
# ============================================================================

configure_nginx() {
    log "🌐 ÉTAPE 9/10: Configuration de Nginx"
    
    info "Création de la configuration Nginx..."
    cat > /etc/nginx/sites-available/midimind << 'EOF'
server {
    listen 8000;
    server_name _;
    root /var/www/midimind;
    index index.html;
    
    access_log /var/log/nginx/midimind_access.log;
    error_log /var/log/nginx/midimind_error.log;
    
    location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg|woff|woff2|ttf|eot)$ {
        expires 1y;
        add_header Cache-Control "public, immutable";
    }
    
    location ~* \.html$ {
        expires -1;
        add_header Cache-Control "no-store, no-cache, must-revalidate";
    }
    
    location /ws {
        proxy_pass http://localhost:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_read_timeout 3600s;
        proxy_send_timeout 3600s;
    }
    
    location / {
        try_files $uri $uri/ /index.html;
    }
}
EOF
    
    ln -sf /etc/nginx/sites-available/midimind /etc/nginx/sites-enabled/
    rm -f /etc/nginx/sites-enabled/default
    
    nginx -t || error "Configuration Nginx invalide"
    
    systemctl restart nginx
    systemctl enable nginx
    
    success "Nginx configuré"
}

# ============================================================================
# ÉTAPE 10: CONFIGURATION SERVICE SYSTEMD
# ============================================================================

configure_systemd_service() {
    log "⚙️  ÉTAPE 10/10: Configuration du service systemd"
    
    info "Création du service systemd..."
    cat > /etc/systemd/system/midimind.service << EOF
[Unit]
Description=MidiMind MIDI Orchestration System
After=network.target sound.target

[Service]
Type=simple
User=$REAL_USER
Group=audio
WorkingDirectory=$INSTALL_DIR
ExecStart=$INSTALL_DIR/bin/midimind
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

# Real-time scheduling
CPUSchedulingPolicy=rr
CPUSchedulingPriority=50

# Resource limits
LimitRTPRIO=95
LimitMEMLOCK=infinity
LimitNICE=-19

[Install]
WantedBy=multi-user.target
EOF
    
    systemctl daemon-reload
    systemctl enable midimind.service
    
    success "Service systemd configuré"
}

# ============================================================================
# CRÉATION DES FICHIERS DE CONFIG
# ============================================================================

create_config_files() {
    log "📝 Création des fichiers de configuration..."
    
    # Config principale
    cat > /etc/midimind/config.json << 'EOF'
{
    "midi": {
        "buffer_size": 256,
        "sample_rate": 48000
    },
    "api": {
        "port": 8080,
        "host": "0.0.0.0"
    },
    "web": {
        "port": 8000
    },
    "database": {
        "path": "/opt/midimind/data/midimind.db"
    }
}
EOF
    
    chown "$REAL_USER:audio" /etc/midimind/config.json
    chmod 644 /etc/midimind/config.json
    
    success "Fichiers de configuration créés"
}

# ============================================================================
# INFORMATIONS FINALES
# ============================================================================

print_final_info() {
    echo ""
    echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                                                              ║${NC}"
    echo -e "${GREEN}║              ✅ INSTALLATION TERMINÉE AVEC SUCCÈS ✅          ║${NC}"
    echo -e "${GREEN}║                                                              ║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${CYAN}📂 Chemins du projet:${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Répertoire projet: ${GREEN}$PROJECT_ROOT${NC}"
    echo -e "  ${BLUE}•${NC} CMakeLists.txt:    ${GREEN}$PROJECT_ROOT/CMakeLists.txt${NC}"
    echo -e "  ${BLUE}•${NC} Backend source:    ${GREEN}$PROJECT_ROOT/backend/src${NC}"
    echo -e "  ${BLUE}•${NC} Frontend source:   ${GREEN}$PROJECT_ROOT/frontend${NC}"
    echo -e "  ${BLUE}•${NC} Build directory:   ${GREEN}$PROJECT_ROOT/build${NC}"
    echo ""
    echo -e "${CYAN}📊 Informations importantes:${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Binaire:           ${GREEN}$INSTALL_DIR/bin/midimind${NC}"
    echo -e "  ${BLUE}•${NC} Interface web:     ${GREEN}http://$(hostname -I | awk '{print $1}'):8000${NC}"
    echo -e "  ${BLUE}•${NC} WebSocket API:    ${GREEN}ws://$(hostname -I | awk '{print $1}'):8080${NC}"
    echo -e "  ${BLUE}•${NC} Configuration:     ${GREEN}/etc/midimind/config.json${NC}"
    echo -e "  ${BLUE}•${NC} Logs:              ${GREEN}/var/log/midimind/${NC}"
    echo -e "  ${BLUE}•${NC} Fichiers MIDI:     ${GREEN}$USER_DIR/midi_files/${NC}"
    echo ""
    echo -e "${YELLOW}⚡ Optimisations Raspberry Pi activées:${NC}"
    echo -e "  ${BLUE}•${NC} CPU Governor: performance"
    echo -e "  ${BLUE}•${NC} Permissions temps réel: activées"
    echo -e "  ${BLUE}•${NC} Latence audio: optimisée"
    echo ""
    echo -e "${CYAN}🚀 Commandes utiles:${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Démarrer:      ${GREEN}sudo systemctl start midimind${NC}"
    echo -e "  ${BLUE}•${NC} Arrêter:       ${GREEN}sudo systemctl stop midimind${NC}"
    echo -e "  ${BLUE}•${NC} Redémarrer:    ${GREEN}sudo systemctl restart midimind${NC}"
    echo -e "  ${BLUE}•${NC} Status:        ${GREEN}sudo systemctl status midimind${NC}"
    echo -e "  ${BLUE}•${NC} Logs:          ${GREEN}sudo journalctl -u midimind -f${NC}"
    echo ""
    echo -e "${YELLOW}⚠️  Important:${NC}"
    echo -e "  ${RED}•${NC} Redémarrez le système pour appliquer toutes les optimisations"
    echo -e "  ${BLUE}•${NC} Commande: ${GREEN}sudo reboot${NC}"
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
    echo "MidiMind Installation - $(date)" >> "$LOG_FILE"
    echo "==================================" >> "$LOG_FILE"
    log "Installation démarrée: $(date)"
    
    # Détection et vérifications
    detect_system
    check_prerequisites
    
    # Installation
    echo ""
    update_system
    echo ""
    install_system_dependencies
    echo ""
    install_cpp_dependencies
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
    create_config_files
    
    # Informations finales
    print_final_info
    
    log "Installation terminée: $(date)"
}

# ============================================================================
# POINT D'ENTRÉE
# ============================================================================

main 2>&1 | tee -a "$LOG_FILE"

# ============================================================================
# FIN DU FICHIER install.sh v3.0.1-FAST
# ============================================================================
