#!/bin/bash
# ============================================================================
# Fichier: install.sh
# Version: v3.0.3
# Date: 2025-10-10
# Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
# Description:
#   Script d'installation automatique complète pour Raspberry Pi
#   - Installation des dépendances système
#   - Compilation du backend C++
#   - Configuration du frontend
#   - Configuration des services système
#   - Optimisations temps réel
# ============================================================================

set -e  # Arrêter sur erreur

# ============================================================================
# COULEURS POUR L'AFFICHAGE
# ============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# ============================================================================
# VARIABLES GLOBALES
# ============================================================================

INSTALL_DIR="/opt/midimind"
WEB_DIR="/var/www/midimind"
USER_DIR="$HOME/midimind"
LOG_FILE="/tmp/midimind_install.log"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Obtenir le nom de l'utilisateur réel (pas root)
REAL_USER="${SUDO_USER:-$USER}"

# ============================================================================
# FONCTIONS UTILITAIRES
# ============================================================================

log() {
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1" | tee -a "$LOG_FILE"
}

error() {
    echo -e "${RED}[ERREUR]${NC} $1" | tee -a "$LOG_FILE"
    exit 1
}

warning() {
    echo -e "${YELLOW}[ATTENTION]${NC} $1" | tee -a "$LOG_FILE"
}

info() {
    echo -e "${CYAN}[INFO]${NC} $1" | tee -a "$LOG_FILE"
}

success() {
    echo -e "${GREEN}[✓]${NC} $1" | tee -a "$LOG_FILE"
}

# Fonction pour afficher une barre de progression
progress() {
    local current=$1
    local total=$2
    local width=50
    local percent=$((current * 100 / total))
    local filled=$((width * current / total))
    local empty=$((width - filled))
    
    printf "\r["
    printf "%${filled}s" | tr ' ' '█'
    printf "%${empty}s" | tr ' ' '░'
    printf "] %3d%% " $percent
}

# ============================================================================
# DÉTECTION SYSTÈME
# ============================================================================

detect_system() {
    log "Détection du système..."
    
    # Détection Raspberry Pi
    if [ -f /proc/device-tree/model ]; then
        RPI_MODEL=$(cat /proc/device-tree/model)
        info "Raspberry Pi détecté: $RPI_MODEL"
    else
        RPI_MODEL="Generic Linux"
        info "Système Linux générique détecté"
    fi
    
    # Détection architecture
    ARCH=$(uname -m)
    info "Architecture: $ARCH"
    
    # Détection OS
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS_NAME=$NAME
        OS_VERSION=$VERSION_ID
        info "OS: $OS_NAME $OS_VERSION"
    fi
}

# ============================================================================
# VÉRIFICATION DES PRÉREQUIS
# ============================================================================

check_prerequisites() {
    log "Vérification des prérequis..."
    
    # Vérifier root
    if [[ $EUID -ne 0 ]]; then
        error "Ce script doit être exécuté avec sudo"
    fi
    
    # Vérifier connexion internet
    if ! ping -c 1 8.8.8.8 &> /dev/null; then
        error "Pas de connexion internet. Installation impossible."
    fi
    success "Connexion internet OK"
    
    # Vérifier espace disque (au moins 2GB)
    AVAILABLE_SPACE=$(df / | tail -1 | awk '{print $4}')
    if [ $AVAILABLE_SPACE -lt 2097152 ]; then
        error "Espace disque insuffisant. Minimum 2GB requis."
    fi
    success "Espace disque suffisant"
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
║              🎹 midiMind v3.0 Installation 🎹               ║
║                                                              ║
║          Système d'Orchestration MIDI Professionnel         ║
║                  pour Raspberry Pi                           ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
    echo ""
}

# ============================================================================
# ÉTAPE 1: MISE À JOUR SYSTÈME
# ============================================================================

update_system() {
    log "ÉTAPE 1/10: Mise à jour du système"
    
    info "Mise à jour de la liste des paquets..."
    apt-get update 2>&1 | tee -a "$LOG_FILE" || error "Échec de apt-get update"
    
    info "Mise à niveau des paquets installés..."
    apt-get upgrade -y 2>&1 | tee -a "$LOG_FILE" || warning "Certains paquets n'ont pas pu être mis à jour"
    
    success "Système mis à jour"
}

# ============================================================================
# ÉTAPE 2: INSTALLATION DÉPENDANCES SYSTÈME
# ============================================================================

install_system_dependencies() {
    log "ÉTAPE 2/10: Installation des dépendances système"
    
    # Build tools
    info "Installation des outils de compilation..."
    apt-get install -y \
        build-essential cmake g++ gcc make pkg-config \
        git wget curl unzip \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation build tools"
    
    # Audio/MIDI
    info "Installation des bibliothèques Audio/MIDI..."
    apt-get install -y \
        libasound2-dev alsa-utils alsa-tools \
        libjack-jackd2-dev jackd2 \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation ALSA"
    
    # Système
    info "Installation des bibliothèques système..."
    apt-get install -y \
        sqlite3 libsqlite3-dev \
        libatomic1 libpthread-stubs0-dev \
        zlib1g-dev \
        libssl-dev libcurl4-openssl-dev \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation bibliothèques système"
    
    # Réseau
    info "Installation des bibliothèques réseau..."
    apt-get install -y \
        libboost-all-dev \
        avahi-daemon libavahi-client-dev libavahi-common-dev \
        bluez libbluetooth-dev bluetooth \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation bibliothèques réseau"
    
    # Web
    info "Installation du serveur web..."
    apt-get install -y \
        nginx \
        nodejs npm \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation serveur web"
    
    success "Dépendances système installées"
}

# ============================================================================
# ÉTAPE 3: INSTALLATION DÉPENDANCES C++
# ============================================================================

install_cpp_dependencies() {
    log "ÉTAPE 3/10: Installation des dépendances C++"
    
    # nlohmann/json
    info "Installation de nlohmann/json..."
    if ! dpkg -l | grep -q nlohmann-json3-dev; then
        apt-get install -y nlohmann-json3-dev 2>&1 | tee -a "$LOG_FILE" || {
            warning "Installation via apt échouée, installation manuelle..."
            wget -O /tmp/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
            mkdir -p /usr/local/include/nlohmann
            cp /tmp/json.hpp /usr/local/include/nlohmann/
        }
    fi
    success "nlohmann/json installé"
    
    # WebSocketpp
    info "Installation de WebSocketpp..."
    apt-get install -y libwebsocketpp-dev 2>&1 | tee -a "$LOG_FILE" || {
        warning "Installation via apt échouée, installation manuelle..."
        cd /tmp
        git clone https://github.com/zaphoyd/websocketpp.git
        cd websocketpp
        mkdir -p build && cd build
        cmake ..
        make install
    }
    success "WebSocketpp installé"
    
    success "Dépendances C++ installées"
}

# ============================================================================
# ÉTAPE 4: CONFIGURATION GROUPES ET PERMISSIONS
# ============================================================================

configure_permissions() {
    log "ÉTAPE 4/10: Configuration des permissions"
    
    # Ajouter utilisateur aux groupes nécessaires
    info "Ajout de l'utilisateur $REAL_USER aux groupes..."
    usermod -a -G audio "$REAL_USER"
    usermod -a -G dialout "$REAL_USER"
    usermod -a -G bluetooth "$REAL_USER"
    usermod -a -G gpio "$REAL_USER" 2>/dev/null || true
    
    # Permissions temps réel
    info "Configuration des permissions temps réel..."
    cat >> /etc/security/limits.conf << EOF

# midiMind real-time permissions
@audio   -  rtprio     95
@audio   -  memlock    unlimited
@audio   -  nice       -19
EOF
    
    # Règles udev pour MIDI
    info "Configuration des règles udev..."
    cat > /etc/udev/rules.d/99-midimind.rules << EOF
# midiMind USB MIDI rules
SUBSYSTEM=="usb", MODE="0666", GROUP="audio"
SUBSYSTEM=="sound", MODE="0666", GROUP="audio"
KERNEL=="midi*", MODE="0666", GROUP="audio"
EOF
    
    udevadm control --reload-rules
    udevadm trigger
    
    success "Permissions configurées"
}

# ============================================================================
# ÉTAPE 5: OPTIMISATIONS SYSTÈME
# ============================================================================

configure_system_optimizations() {
    log "ÉTAPE 5/10: Configuration des optimisations système"
    
    # Swappiness
    info "Configuration du swappiness..."
    echo "vm.swappiness=10" >> /etc/sysctl.conf
    sysctl -w vm.swappiness=10
    
    # CPU Governor
    info "Configuration du CPU governor..."
    if [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
        echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
        
        # Rendre permanent
        cat > /etc/systemd/system/cpufreq-performance.service << EOF
[Unit]
Description=Set CPU governor to performance
After=multi-user.target

[Service]
Type=oneshot
ExecStart=/bin/bash -c 'echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor'

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

# midiMind audio optimizations
options snd-usb-audio nrpacks=1
EOF
    
    success "Optimisations système configurées"
}

# ============================================================================
# ÉTAPE 6: CRÉATION DES RÉPERTOIRES
# ============================================================================

create_directories() {
    log "ÉTAPE 6/10: Création des répertoires"
    
    # Répertoires principaux
    info "Création des répertoires principaux..."
    mkdir -p "$INSTALL_DIR"/{bin,lib,config,logs,data,backups}
    mkdir -p "$WEB_DIR"
    mkdir -p "$USER_DIR"/{midi_files,playlists,backups,logs}
    mkdir -p /var/log/midimind
    mkdir -p /etc/midimind
    
    # Permissions
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
    log "ÉTAPE 7/10: Compilation du backend"
    
    if [ ! -d "$SCRIPT_DIR/backend" ]; then
        error "Répertoire backend introuvable"
    fi
    
    cd "$SCRIPT_DIR/backend"
    
    info "Configuration CMake..."
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | tee -a "$LOG_FILE" || error "Échec de cmake"
    
    info "Compilation (cela peut prendre plusieurs minutes)..."
    NPROC=$(nproc)
    make -j$NPROC 2>&1 | tee -a "$LOG_FILE" || error "Échec de make"
    
    info "Installation du binaire..."
    cp midimind "$INSTALL_DIR/bin/" || error "Échec copie binaire"
    
    # Créer lien symbolique
    ln -sf "$INSTALL_DIR/bin/midimind" /usr/local/bin/midimind
    
    # Capabilities pour temps réel
    setcap cap_sys_nice+ep "$INSTALL_DIR/bin/midimind"
    
    success "Backend compilé et installé"
}

# ============================================================================
# ÉTAPE 8: INSTALLATION FRONTEND
# ============================================================================

install_frontend() {
    log "ÉTAPE 8/10: Installation du frontend"
    
    if [ ! -d "$SCRIPT_DIR/frontend" ]; then
        error "Répertoire frontend introuvable"
    fi
    
    info "Copie des fichiers frontend..."
    cp -r "$SCRIPT_DIR/frontend"/* "$WEB_DIR/"
    
    # Installation des dépendances npm si package.json existe
    if [ -f "$WEB_DIR/package.json" ]; then
        info "Installation des dépendances npm..."
        cd "$WEB_DIR"
        npm install --production 2>&1 | tee -a "$LOG_FILE" || warning "npm install a échoué"
    fi
    
    # Permissions
    chown -R www-data:www-data "$WEB_DIR"
    
    success "Frontend installé"
}

# ============================================================================
# ÉTAPE 9: CONFIGURATION NGINX
# ============================================================================

configure_nginx() {
    log "ÉTAPE 9/10: Configuration de Nginx"
    
    info "Création de la configuration Nginx..."
    cat > /etc/nginx/sites-available/midimind << 'EOF'
server {
    listen 8000;
    server_name _;
    root /var/www/midimind;
    index index.html;
    
    access_log /var/log/nginx/midimind_access.log;
    error_log /var/log/nginx/midimind_error.log;
    
    # Cache statique
    location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg|woff|woff2|ttf|eot)$ {
        expires 1y;
        add_header Cache-Control "public, immutable";
    }
    
    # Pas de cache pour HTML
    location ~* \.html$ {
        expires -1;
        add_header Cache-Control "no-store, no-cache, must-revalidate";
    }
    
    # WebSocket proxy
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
    
    # Fallback
    location / {
        try_files $uri $uri/ /index.html;
    }
}
EOF
    
    # Activer le site
    ln -sf /etc/nginx/sites-available/midimind /etc/nginx/sites-enabled/
    
    # Désactiver site par défaut
    rm -f /etc/nginx/sites-enabled/default
    
    # Test configuration
    nginx -t || error "Configuration Nginx invalide"
    
    # Redémarrer Nginx
    systemctl restart nginx
    systemctl enable nginx
    
    success "Nginx configuré"
}

# ============================================================================
# ÉTAPE 10: CONFIGURATION SERVICE SYSTEMD
# ============================================================================

configure_systemd_service() {
    log "ÉTAPE 10/10: Configuration du service systemd"
    
    info "Création du service midimind..."
    cat > /etc/systemd/system/midimind.service << EOF
[Unit]
Description=midiMind MIDI Orchestration System
After=network.target sound.target alsa-restore.service

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

# Permissions temps réel
LimitRTPRIO=95
LimitMEMLOCK=infinity
Nice=-19

# Sécurité
PrivateTmp=yes
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=$INSTALL_DIR $USER_DIR /var/log/midimind /tmp

[Install]
WantedBy=multi-user.target
EOF
    
    # Recharger systemd
    systemctl daemon-reload
    
    # Activer le service
    systemctl enable midimind.service
    
    success "Service systemd configuré"
}

# ============================================================================
# CONFIGURATION FICHIERS
# ============================================================================

create_config_files() {
    log "Création des fichiers de configuration..."
    
    # Config principal
    cat > /etc/midimind/config.json << 'EOF'
{
    "application": {
        "name": "midiMind",
        "version": "3.0.0",
        "environment": "production"
    },
    "midi": {
        "buffer_size": 256,
        "sample_rate": 48000,
        "latency_ms": 10
    },
    "api": {
        "port": 8080,
        "host": "0.0.0.0",
        "cors_enabled": true
    },
    "network": {
        "wifi_enabled": true,
        "bluetooth_enabled": true,
        "hotspot_enabled": false
    },
    "database": {
        "path": "/opt/midimind/data/midimind.db"
    },
    "logging": {
        "level": "info",
        "console": true,
        "file": true,
        "file_path": "/var/log/midimind/midimind.log"
    }
}
EOF
    
    chown "$REAL_USER:audio" /etc/midimind/config.json
    chmod 644 /etc/midimind/config.json
    
    success "Fichiers de configuration créés"
}

# ============================================================================
# AFFICHAGE INFORMATIONS FINALES
# ============================================================================

print_final_info() {
    echo ""
    echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                                                              ║${NC}"
    echo -e "${GREEN}║            ✓ Installation terminée avec succès !             ║${NC}"
    echo -e "${GREEN}║                                                              ║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${CYAN}📋 Informations système:${NC}"
    echo -e "   • Modèle: $RPI_MODEL"
    echo -e "   • Architecture: $ARCH"
    echo -e "   • OS: $OS_NAME $OS_VERSION"
    echo ""
    echo -e "${CYAN}📂 Chemins d'installation:${NC}"
    echo -e "   • Backend: $INSTALL_DIR"
    echo -e "   • Frontend: $WEB_DIR"
    echo -e "   • Données utilisateur: $USER_DIR"
    echo -e "   • Configuration: /etc/midimind"
    echo -e "   • Logs: /var/log/midimind"
    echo ""
    echo -e "${CYAN}🚀 Commandes utiles:${NC}"
    echo -e "   • Démarrer: ${GREEN}sudo systemctl start midimind${NC}"
    echo -e "   • Arrêter: ${GREEN}sudo systemctl stop midimind${NC}"
    echo -e "   • Statut: ${GREEN}sudo systemctl status midimind${NC}"
    echo -e "   • Logs: ${GREEN}journalctl -u midimind -f${NC}"
    echo ""
    echo -e "${CYAN}🌐 Accès interface web:${NC}"
    echo -e "   • Local: ${GREEN}http://localhost:8000${NC}"
    echo -e "   • Réseau: ${GREEN}http://$(hostname -I | awk '{print $1}'):8000${NC}"
    echo ""
    echo -e "${YELLOW}⚠️  IMPORTANT:${NC}"
    echo -e "   • Redémarrez le système pour appliquer toutes les modifications"
    echo -e "   • Commande: ${GREEN}sudo reboot${NC}"
    echo ""
    echo -e "${CYAN}📚 Documentation:${NC}"
    echo -e "   • README: $INSTALL_DIR/README.md"
    echo -e "   • Wiki: https://github.com/midimind/midimind/wiki"
    echo ""
}

# ============================================================================
# FONCTION PRINCIPALE
# ============================================================================

main() {
    # Bannière
    print_banner
    
    # Détection système
    detect_system
    
    # Vérifications
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
    
    # Log final
    log "Installation terminée: $(date)"
}

# ============================================================================
# POINT D'ENTRÉE
# ============================================================================

# Démarrer avec redirection vers log
main 2>&1 | tee -a "$LOG_FILE"

# ============================================================================
# FIN DU FICHIER install.sh
# ============================================================================