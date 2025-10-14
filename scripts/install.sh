#!/bin/bash
# ============================================================================
# Fichier: scripts/install.sh
# Version: 3.0.4
# Date: 2025-10-14
# ============================================================================
# Description:
#   Script d'installation automatique MidiMind pour Raspberry Pi
#   Adapté pour la structure avec backend/CMakeLists.txt
# ============================================================================

set -e  # Arrêter sur erreur

# ============================================================================
# COULEURS
# ============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# ============================================================================
# VARIABLES GLOBALES
# ============================================================================

# Chemins
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_DIR/backend"
FRONTEND_DIR="$PROJECT_DIR/frontend"

# Destinations installation
INSTALL_DIR="/opt/midimind"
WEB_DIR="/var/www/midimind"
USER_DIR="$HOME/midimind"

# Logs
LOG_FILE="/tmp/midimind_install_$(date +%Y%m%d_%H%M%S).log"

# Utilisateur
REAL_USER="${SUDO_USER:-$USER}"

# ============================================================================
# FONCTIONS UTILITAIRES
# ============================================================================

log() {
    echo -e "${CYAN}[$(date +'%H:%M:%S')]${NC} $1" | tee -a "$LOG_FILE"
}

error() {
    echo -e "${RED}[ERREUR]${NC} $1" | tee -a "$LOG_FILE"
    exit 1
}

warning() {
    echo -e "${YELLOW}[ATTENTION]${NC} $1" | tee -a "$LOG_FILE"
}

info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$LOG_FILE"
}

success() {
    echo -e "${GREEN}[✓]${NC} $1" | tee -a "$LOG_FILE"
}

print_banner() {
    echo ""
    echo -e "${BLUE}╔═══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║${NC}  $1"
    echo -e "${BLUE}╚═══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

# ============================================================================
# VÉRIFICATION ROOT
# ============================================================================

check_root() {
    if [ "$EUID" -ne 0 ]; then
        error "Ce script doit être exécuté avec sudo"
    fi
    
    if [ -z "$REAL_USER" ] || [ "$REAL_USER" = "root" ]; then
        error "Impossible de déterminer l'utilisateur réel. N'exécutez pas directement en root."
    fi
    
    success "Exécution en tant que root (utilisateur: $REAL_USER)"
}

# ============================================================================
# VÉRIFICATION STRUCTURE PROJET
# ============================================================================

check_project_structure() {
    print_banner "Vérification de la structure du projet"
    
    log "Répertoire du script: $SCRIPT_DIR"
    log "Répertoire du projet: $PROJECT_DIR"
    log "Répertoire backend: $BACKEND_DIR"
    
    # Vérifier backend/
    if [ ! -d "$BACKEND_DIR" ]; then
        error "Répertoire backend/ introuvable: $BACKEND_DIR"
    fi
    success "Répertoire backend trouvé"
    
    # Vérifier CMakeLists.txt dans backend/
    if [ ! -f "$BACKEND_DIR/CMakeLists.txt" ]; then
        error "CMakeLists.txt introuvable dans: $BACKEND_DIR"
    fi
    success "CMakeLists.txt trouvé dans backend/"
    
    # Vérifier src/core/Application.cpp
    if [ ! -f "$BACKEND_DIR/src/core/Application.cpp" ]; then
        error "Application.cpp introuvable: $BACKEND_DIR/src/core/Application.cpp"
    fi
    success "Application.cpp trouvé"
    
    # Vérifier frontend/ (optionnel)
    if [ -d "$FRONTEND_DIR" ]; then
        success "Répertoire frontend trouvé"
    else
        warning "Répertoire frontend introuvable (installation backend uniquement)"
    fi
}

# ============================================================================
# ÉTAPE 1: VÉRIFICATION PRÉREQUIS
# ============================================================================

check_prerequisites() {
    print_banner "Vérification des prérequis"
    
    # OS
    if [ ! -f /etc/debian_version ]; then
        error "Ce script nécessite une distribution basée sur Debian"
    fi
    success "Distribution Debian détectée"
    
    # Architecture
    ARCH=$(uname -m)
    log "Architecture: $ARCH"
    
    if [[ "$ARCH" != "armv7l" && "$ARCH" != "aarch64" && "$ARCH" != "x86_64" ]]; then
        warning "Architecture non testée: $ARCH"
    else
        success "Architecture supportée: $ARCH"
    fi
    
    # Espace disque (minimum 500MB)
    AVAILABLE=$(df / | tail -1 | awk '{print $4}')
    if [ "$AVAILABLE" -lt 500000 ]; then
        error "Espace disque insuffisant (< 500MB)"
    fi
    success "Espace disque suffisant"
    
    # Mémoire (minimum 512MB)
    MEMORY=$(free -m | awk '/^Mem:/{print $2}')
    if [ "$MEMORY" -lt 512 ]; then
        warning "Mémoire faible (< 512MB)"
    else
        success "Mémoire suffisante"
    fi
}

# ============================================================================
# ÉTAPE 2: MISE À JOUR SYSTÈME
# ============================================================================

update_system() {
    print_banner "Mise à jour du système"
    
    info "Mise à jour des paquets..."
    apt-get update 2>&1 | tee -a "$LOG_FILE" || error "Échec apt-get update"
    success "Liste des paquets mise à jour"
    
    info "Mise à jour des paquets installés..."
    apt-get upgrade -y 2>&1 | tee -a "$LOG_FILE" || warning "Certains paquets n'ont pas pu être mis à jour"
    success "Paquets mis à jour"
}

# ============================================================================
# ÉTAPE 3: INSTALLATION DÉPENDANCES SYSTÈME
# ============================================================================

install_system_dependencies() {
    print_banner "Installation des dépendances système"
    
    info "Installation des outils de build..."
    apt-get install -y \
        build-essential \
        cmake \
        git \
        pkg-config \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation outils de build"
    success "Outils de build installés"
    
    info "Installation des bibliothèques MIDI et audio..."
    apt-get install -y \
        libasound2-dev \
        libjack-jackd2-dev \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation bibliothèques MIDI/audio"
    success "Bibliothèques MIDI/audio installées"
    
    info "Installation des bibliothèques système..."
    apt-get install -y \
        sqlite3 \
        libsqlite3-dev \
        libssl-dev \
        libcurl4-openssl-dev \
        zlib1g-dev \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation bibliothèques système"
    success "Bibliothèques système installées"
    
    info "Installation des bibliothèques réseau..."
    apt-get install -y \
        libboost-all-dev \
        avahi-daemon \
        libavahi-client-dev \
        libavahi-common-dev \
        2>&1 | tee -a "$LOG_FILE" || warning "Certaines bibliothèques réseau n'ont pas pu être installées"
    success "Bibliothèques réseau installées"
    
    info "Installation du serveur web..."
    apt-get install -y \
        nginx \
        2>&1 | tee -a "$LOG_FILE" || warning "Nginx non installé"
    success "Serveur web installé"
}

# ============================================================================
# ÉTAPE 4: INSTALLATION DÉPENDANCES C++
# ============================================================================

install_cpp_dependencies() {
    print_banner "Installation des dépendances C++"
    
    # nlohmann/json
    info "Installation de nlohmann/json..."
    if apt-get install -y nlohmann-json3-dev 2>&1 | tee -a "$LOG_FILE"; then
        success "nlohmann/json installé via apt"
    else
        warning "Installation via apt échouée, installation manuelle..."
        mkdir -p /usr/local/include/nlohmann
        wget -q -O /usr/local/include/nlohmann/json.hpp \
            https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp \
            || error "Échec téléchargement nlohmann/json"
        success "nlohmann/json installé manuellement"
    fi
    
    # WebSocketpp (optionnel)
    info "Installation de WebSocketpp..."
    if apt-get install -y libwebsocketpp-dev 2>&1 | tee -a "$LOG_FILE"; then
        success "WebSocketpp installé"
    else
        warning "WebSocketpp non installé (peut être optionnel)"
    fi
}

# ============================================================================
# ÉTAPE 5: CONFIGURATION PERMISSIONS
# ============================================================================

configure_permissions() {
    print_banner "Configuration des permissions"
    
    info "Ajout de $REAL_USER aux groupes nécessaires..."
    usermod -a -G audio "$REAL_USER" 2>/dev/null || true
    usermod -a -G dialout "$REAL_USER" 2>/dev/null || true
    usermod -a -G bluetooth "$REAL_USER" 2>/dev/null || true
    usermod -a -G gpio "$REAL_USER" 2>/dev/null || true
    success "Groupes configurés"
    
    info "Configuration des limites temps réel..."
    cat > /etc/security/limits.d/99-midimind.conf << EOF
# MidiMind real-time permissions
$REAL_USER    -    rtprio    95
$REAL_USER    -    memlock   unlimited
$REAL_USER    -    nice      -19
EOF
    success "Limites temps réel configurées"
}

# ============================================================================
# ÉTAPE 6: CRÉATION RÉPERTOIRES
# ============================================================================

create_directories() {
    print_banner "Création de la structure de répertoires"
    
    info "Création des répertoires principaux..."
    mkdir -p "$INSTALL_DIR"/{bin,lib,config,logs,data,backups}
    mkdir -p "$USER_DIR"/{midi_files,playlists,backups,logs}
    mkdir -p /var/log/midimind
    mkdir -p /etc/midimind
    success "Répertoires créés"
    
    info "Configuration des permissions..."
    chown -R "$REAL_USER:audio" "$INSTALL_DIR"
    chown -R "$REAL_USER:audio" "$USER_DIR"
    chown -R "$REAL_USER:audio" /var/log/midimind
    chown -R "$REAL_USER:audio" /etc/midimind
    success "Permissions configurées"
}

# ============================================================================
# ÉTAPE 7: COMPILATION BACKEND (ADAPTÉ POUR backend/)
# ============================================================================

compile_backend() {
    print_banner "Compilation du backend"
    
    # Vérifier que backend/ existe
    if [ ! -d "$BACKEND_DIR" ]; then
        error "Répertoire backend introuvable: $BACKEND_DIR"
    fi
    
    # Vérifier que CMakeLists.txt est dans backend/
    if [ ! -f "$BACKEND_DIR/CMakeLists.txt" ]; then
        error "CMakeLists.txt introuvable dans: $BACKEND_DIR"
    fi
    
    info "Répertoire backend: $BACKEND_DIR"
    
    # Se déplacer dans backend/
    cd "$BACKEND_DIR" || error "Impossible d'accéder à $BACKEND_DIR"
    log "Working directory: $(pwd)"
    
    # Nettoyer build précédent
    if [ -d "build" ]; then
        info "Nettoyage du build précédent..."
        rm -rf build
    fi
    
    # Créer répertoire build
    info "Création du répertoire build..."
    mkdir -p build
    cd build || error "Impossible de créer build/"
    
    # Configuration CMake
    info "Configuration CMake..."
    log "Exécution: cmake .. -DCMAKE_BUILD_TYPE=Release"
    
    if cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | tee -a "$LOG_FILE"; then
        success "Configuration CMake réussie"
    else
        error "Échec de la configuration CMake"
    fi
    
    # Compilation
    info "Compilation (cela peut prendre plusieurs minutes)..."
    NPROC=$(nproc)
    log "Compilation avec $NPROC jobs"
    
    START_TIME=$(date +%s)
    
    if make -j"$NPROC" 2>&1 | tee -a "$LOG_FILE"; then
        END_TIME=$(date +%s)
        DURATION=$((END_TIME - START_TIME))
        success "Compilation réussie en ${DURATION}s"
    else
        error "Échec de la compilation"
    fi
    
    # Vérifier binaire
    if [ ! -f "midimind" ]; then
        error "Binaire midimind non trouvé après compilation"
    fi
    
    BINARY_SIZE=$(du -h midimind | cut -f1)
    success "Binaire créé: $BINARY_SIZE"
    
    # Installation du binaire
    info "Installation du binaire..."
    cp midimind "$INSTALL_DIR/bin/" || error "Échec copie binaire"
    success "Binaire installé dans $INSTALL_DIR/bin/"
    
    # Lien symbolique
    info "Création du lien symbolique..."
    ln -sf "$INSTALL_DIR/bin/midimind" /usr/local/bin/midimind
    success "Lien symbolique créé"
    
    # Capabilities pour temps réel
    info "Configuration des capabilities..."
    setcap cap_sys_nice+ep "$INSTALL_DIR/bin/midimind" || warning "Échec setcap"
    success "Capabilities configurées"
}

# ============================================================================
# ÉTAPE 8: INSTALLATION FRONTEND
# ============================================================================

install_frontend() {
    print_banner "Installation du frontend"
    
    if [ ! -d "$FRONTEND_DIR" ]; then
        warning "Répertoire frontend introuvable, installation ignorée"
        return 0
    fi
    
    info "Copie des fichiers frontend..."
    mkdir -p "$WEB_DIR"
    cp -r "$FRONTEND_DIR"/* "$WEB_DIR/" || warning "Échec copie frontend"
    
    chown -R www-data:www-data "$WEB_DIR"
    success "Frontend installé"
}

# ============================================================================
# ÉTAPE 9: CONFIGURATION NGINX
# ============================================================================

configure_nginx() {
    print_banner "Configuration Nginx"
    
    if ! command -v nginx &> /dev/null; then
        warning "Nginx non installé, configuration ignorée"
        return 0
    fi
    
    info "Création de la configuration Nginx..."
    cat > /etc/nginx/sites-available/midimind << 'EOF'
server {
    listen 80;
    server_name localhost;
    
    root /var/www/midimind;
    index index.html;
    
    location / {
        try_files $uri $uri/ /index.html;
    }
    
    location /api {
        proxy_pass http://localhost:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
    }
    
    location /ws {
        proxy_pass http://localhost:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
EOF
    
    # Activer le site
    ln -sf /etc/nginx/sites-available/midimind /etc/nginx/sites-enabled/
    rm -f /etc/nginx/sites-enabled/default
    
    # Tester configuration
    if nginx -t 2>&1 | tee -a "$LOG_FILE"; then
        systemctl restart nginx
        success "Nginx configuré et redémarré"
    else
        error "Configuration Nginx invalide"
    fi
}

# ============================================================================
# ÉTAPE 10: SERVICE SYSTEMD
# ============================================================================

configure_systemd_service() {
    print_banner "Configuration du service systemd"
    
    info "Création du service midimind..."
    cat > /etc/systemd/system/midimind.service << EOF
[Unit]
Description=MidiMind MIDI Orchestration System
After=network.target sound.target

[Service]
Type=simple
User=$REAL_USER
WorkingDirectory=$INSTALL_DIR
ExecStart=$INSTALL_DIR/bin/midimind
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

# Real-time scheduling
Nice=-19
LimitRTPRIO=95
LimitMEMLOCK=infinity

[Install]
WantedBy=multi-user.target
EOF
    
    # Recharger systemd
    systemctl daemon-reload
    success "Service systemd créé"
    
    info "Activation du service au démarrage..."
    systemctl enable midimind.service
    success "Service activé au démarrage"
}

# ============================================================================
# ÉTAPE 11: FICHIERS DE CONFIGURATION
# ============================================================================

create_config_files() {
    print_banner "Création des fichiers de configuration"
    
    # Config principal
    if [ ! -f "$INSTALL_DIR/config/config.json" ]; then
        info "Création de config.json..."
        cat > "$INSTALL_DIR/config/config.json" << EOF
{
    "system": {
        "name": "MidiMind",
        "version": "3.0.0",
        "log_level": "info"
    },
    "api": {
        "port": 8080,
        "host": "0.0.0.0"
    },
    "database": {
        "path": "$USER_DIR/midimind.db"
    },
    "midi": {
        "default_device": "auto",
        "buffer_size": 256
    },
    "paths": {
        "midi_files": "$USER_DIR/midi_files",
        "logs": "/var/log/midimind"
    }
}
EOF
        chown "$REAL_USER:audio" "$INSTALL_DIR/config/config.json"
        success "config.json créé"
    else
        info "config.json existe déjà"
    fi
}

# ============================================================================
# INFORMATIONS FINALES
# ============================================================================

print_final_info() {
    print_banner "Installation terminée!"
    
    echo ""
    echo -e "${GREEN}✓ MidiMind a été installé avec succès!${NC}"
    echo ""
    echo "Informations importantes:"
    echo ""
    echo "  • Binaire: $INSTALL_DIR/bin/midimind"
    echo "  • Configuration: $INSTALL_DIR/config/config.json"
    echo "  • Données utilisateur: $USER_DIR"
    echo "  • Logs: /var/log/midimind"
    echo ""
    echo "Commandes utiles:"
    echo ""
    echo "  Démarrer le service:"
    echo "    sudo systemctl start midimind"
    echo ""
    echo "  Arrêter le service:"
    echo "    sudo systemctl stop midimind"
    echo ""
    echo "  Voir le statut:"
    echo "    sudo systemctl status midimind"
    echo ""
    echo "  Voir les logs:"
    echo "    sudo journalctl -u midimind -f"
    echo ""
    echo "  Interface web:"
    echo "    http://$(hostname -I | awk '{print $1}')"
    echo ""
    echo -e "${YELLOW}Note: Vous devez vous reconnecter ou redémarrer pour que"
    echo -e "      les permissions de groupe prennent effet.${NC}"
    echo ""
}

# ============================================================================
# FONCTION PRINCIPALE
# ============================================================================

main() {
    print_banner "Installation MidiMind v3.0"
    
    log "Début de l'installation: $(date)"
    log "Log file: $LOG_FILE"
    
    # Exécution des étapes
    check_root
    check_project_structure
    check_prerequisites
    update_system
    install_system_dependencies
    install_cpp_dependencies
    configure_permissions
    create_directories
    compile_backend
    install_frontend
    configure_nginx
    configure_systemd_service
    create_config_files
    
    # Informations finales
    print_final_info
    
    log "Installation terminée: $(date)"
    
    echo ""
    echo -e "${GREEN}Logs complets disponibles dans: $LOG_FILE${NC}"
    echo ""
}

# ============================================================================
# POINT D'ENTRÉE
# ============================================================================

# Vérifier arguments
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "Usage: sudo ./install.sh"
    echo ""
    echo "Script d'installation automatique MidiMind v3.0"
    echo ""
    echo "Ce script doit être exécuté avec sudo depuis le répertoire scripts/"
    echo ""
    exit 0
fi

# Démarrer installation
main 2>&1 | tee -a "$LOG_FILE"

# ============================================================================
# FIN
# ============================================================================

exit 0
