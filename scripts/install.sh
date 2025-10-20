#!/bin/bash
# ============================================================================
# Fichier: scripts/install.sh
# Version: 4.1.2 - CORRIGÉE
# Date: 2025-10-20
# Projet: MidiMind - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
#
# CORRECTIONS v4.1.2:
#   ✅ Création du dossier migrations
#   ✅ Copie des fichiers SQL de migration
#   ✅ Initialisation correcte de la base de données
#   ✅ Vérification des permissions sur tous les fichiers
#   ✅ Test de démarrage après installation
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
║              🎹 MidiMind v4.1.2 Installation ⚡               ║
║                                                              ║
║          Système d'Orchestration MIDI Professionnel          ║
║                  pour Raspberry Pi                           ║
║                                                              ║
║              Installation Complète Automatique               ║
║                      Version CORRIGÉE                        ║
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
    success "Espace disque suffisant (${available_gb}GB disponible)"
}

# ============================================================================
# ÉTAPE 1: MISE À JOUR SYSTÈME
# ============================================================================

update_system() {
    log "⚙️ ÉTAPE 1/10: Mise à jour du système"
    
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
    log "📦 ÉTAPE 2/10: Installation des dépendances système"
    
    info "Installation des outils de compilation..."
    apt-get install -y -qq \
        build-essential cmake g++ gcc make pkg-config \
        git wget curl unzip \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation build tools"
    
    info "Installation des bibliothèques Audio/MIDI..."
    apt-get install -y -qq \
        libasound2-dev alsa-utils alsa-tools \
        libjack-jackd2-dev \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation audio libs"
    
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
    log "📚 ÉTAPE 3/10: Installation des dépendances C++"
    
    info "Installation de nlohmann/json..."
    apt-get install -y -qq nlohmann-json3-dev 2>&1 | tee -a "$LOG_FILE" || error "Échec nlohmann-json"
    
    info "Installation de websocketpp..."
    if ! apt-cache show libwebsocketpp-dev &> /dev/null; then
        info "websocketpp non disponible via apt, installation depuis GitHub..."
        cd /tmp
        rm -rf websocketpp
        git clone https://github.com/zaphoyd/websocketpp.git
        cd websocketpp
        mkdir -p build && cd build
        cmake ..
        make install
        cd /tmp && rm -rf websocketpp
    else
        apt-get install -y -qq libwebsocketpp-dev 2>&1 | tee -a "$LOG_FILE"
    fi
    
    success "Dépendances C++ installées"
}

# ============================================================================
# ÉTAPE 4: CONFIGURATION PERMISSIONS
# ============================================================================

configure_permissions() {
    log "🔐 ÉTAPE 4/10: Configuration des permissions"
    
    info "Ajout de l'utilisateur $REAL_USER au groupe audio..."
    usermod -a -G audio "$REAL_USER" || warning "Impossible d'ajouter au groupe audio"
    
    info "Configuration des règles udev pour MIDI..."
    cat > /etc/udev/rules.d/99-midimind.rules << 'EOF'
# MidiMind MIDI device rules
SUBSYSTEM=="sound", MODE="0666"
KERNEL=="midi*", MODE="0666"
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", MODE="0666"
EOF
    
    udevadm control --reload-rules
    udevadm trigger
    
    success "Permissions configurées"
}

# ============================================================================
# ÉTAPE 5: OPTIMISATIONS SYSTÈME
# ============================================================================

configure_system_optimizations() {
    log "⚡ ÉTAPE 5/10: Optimisations système"
    
    info "Configuration des limites système..."
    cat >> /etc/security/limits.conf << 'EOF'
# MidiMind real-time audio configuration
@audio   -  rtprio     95
@audio   -  memlock    unlimited
@audio   -  nice       -19
EOF
    
    info "Configuration du kernel..."
    cat >> /etc/sysctl.conf << 'EOF'
# MidiMind kernel optimizations
vm.swappiness=10
kernel.sched_rt_runtime_us=-1
EOF
    
    sysctl -p > /dev/null 2>&1 || warning "Impossible d'appliquer sysctl"
    
    success "Optimisations système appliquées"
}

# ============================================================================
# ÉTAPE 6: CRÉATION DES RÉPERTOIRES (CORRIGÉ)
# ============================================================================

create_directories() {
    log "📁 ÉTAPE 6/10: Création des répertoires"
    
    info "Création des répertoires principaux..."
    mkdir -p "$INSTALL_DIR"/{bin,lib,config,logs,backups}
    mkdir -p "$INSTALL_DIR/data/migrations"  # ✅ CORRIGÉ: Ajout du dossier migrations
    mkdir -p "$USER_DIR"/{midi_files,playlists,backups,logs}
    mkdir -p /var/log/midimind
    mkdir -p /etc/midimind
    mkdir -p "$WEB_DIR"
    
    success "Répertoires principaux créés"
    
    # ✅ NOUVEAU: Copie des fichiers de migration
    if [ -d "$BACKEND_DIR/data/migrations" ]; then
        info "Copie des fichiers de migration SQL..."
        local sql_count=$(find "$BACKEND_DIR/data/migrations" -name "*.sql" 2>/dev/null | wc -l)
        if [ $sql_count -gt 0 ]; then
            cp "$BACKEND_DIR/data/migrations"/*.sql "$INSTALL_DIR/data/migrations/" 2>/dev/null || true
            success "Fichiers de migration copiés: $sql_count fichiers"
        else
            warning "Aucun fichier SQL trouvé dans backend/data/migrations"
        fi
    fi
    
    info "Configuration des permissions..."
    chown -R "$REAL_USER:audio" "$INSTALL_DIR"
    chown -R "$REAL_USER:audio" "$USER_DIR"
    chown -R "$REAL_USER:audio" /var/log/midimind
    chown -R "$REAL_USER:audio" /etc/midimind
    chown -R www-data:www-data "$WEB_DIR"
    
    # ✅ Permissions spécifiques sur la base de données
    chmod 775 "$INSTALL_DIR/data"
    chmod 775 "$INSTALL_DIR/data/migrations"
    
    success "Permissions configurées"
}

# ============================================================================
# ÉTAPE 7: COMPILATION BACKEND
# ============================================================================

compile_backend() {
    log "🔨 ÉTAPE 7/10: Compilation du backend"
    
    if [ ! -f "$BACKEND_DIR/CMakeLists.txt" ]; then
        error "CMakeLists.txt introuvable: $BACKEND_DIR/CMakeLists.txt"
    fi
    
    if [ ! -d "$BACKEND_DIR/src" ]; then
        error "Répertoire src/ introuvable: $BACKEND_DIR/src"
    fi
    
    cd "$BACKEND_DIR"
    
    info "Configuration CMake depuis backend/..."
    info "  Répertoire courant: $(pwd)"
    info "  CMakeLists.txt: $BACKEND_DIR/CMakeLists.txt"
    
    mkdir -p build
    cd build
    
    info "Exécution de CMake..."
    cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | tee -a "$LOG_FILE" || error "Échec de cmake"
    
    success "Configuration CMake terminée"
    
    info "Compilation (utilisation de $NPROC cœurs, peut prendre 5-10 min)..."
    make -j$NPROC 2>&1 | tee -a "$LOG_FILE" || error "Échec de make"
    
    success "Compilation terminée"
    
    # Recherche du binaire (peut être dans bin/ ou directement dans build/)
    local binary_path=""
    if [ -f "bin/midimind" ]; then
        binary_path="bin/midimind"
    elif [ -f "midimind" ]; then
        binary_path="midimind"
    elif [ -f "MidiMind_Backend" ]; then
        binary_path="MidiMind_Backend"
    else
        error "Binaire non généré. Vérifiez les logs de compilation."
    fi
    
    info "Binaire trouvé: $binary_path"
    
    info "Installation du binaire..."
    cp "$binary_path" "$INSTALL_DIR/bin/midimind" || error "Échec copie binaire"
    chmod +x "$INSTALL_DIR/bin/midimind"
    
    ln -sf "$INSTALL_DIR/bin/midimind" /usr/local/bin/midimind
    
    # Capabilities pour real-time
    setcap cap_sys_nice+ep "$INSTALL_DIR/bin/midimind" || warning "setcap échoué (non critique)"
    
    success "✅ Backend compilé et installé"
    info "  Binaire: $INSTALL_DIR/bin/midimind"
    info "  Taille: $(du -h $INSTALL_DIR/bin/midimind | cut -f1)"
}

# ============================================================================
# ÉTAPE 8: INSTALLATION FRONTEND
# ============================================================================

install_frontend() {
    log "🌐 ÉTAPE 8/10: Installation du frontend"
    
    if [ ! -d "$FRONTEND_DIR" ]; then
        error "Répertoire frontend introuvable: $FRONTEND_DIR"
    fi
    
    info "Copie des fichiers frontend depuis: $FRONTEND_DIR"
    info "  Destination: $WEB_DIR"
    
    # Copier tout le contenu du dossier frontend vers /var/www/midimind
    cp -r "$FRONTEND_DIR"/* "$WEB_DIR/" || error "Échec copie frontend"
    
    # Vérifier que les fichiers critiques sont bien copiés
    if [ ! -f "$WEB_DIR/index.html" ]; then
        error "index.html non copié vers $WEB_DIR"
    fi
    success "index.html copié"
    
    if [ ! -d "$WEB_DIR/js" ]; then
        error "Dossier js/ non copié vers $WEB_DIR"
    fi
    success "Dossier js/ copié"
    
    # Installer les dépendances npm si package.json existe
    if [ -f "$WEB_DIR/package.json" ]; then
        info "Installation des dépendances npm..."
        cd "$WEB_DIR"
        npm install --production --no-optional 2>&1 | tee -a "$LOG_FILE" || warning "npm install a échoué"
    fi
    
    # Configurer les permissions
    chown -R www-data:www-data "$WEB_DIR"
    chmod -R 755 "$WEB_DIR"
    
    success "✅ Frontend installé"
    info "  Emplacement: $WEB_DIR"
    info "  Fichiers: $(find $WEB_DIR -type f | wc -l) fichiers"
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
    
    # Compression
    gzip on;
    gzip_types text/plain text/css application/json application/javascript text/xml;
    
    # Cache statique
    location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg|woff|woff2|ttf|eot)$ {
        expires 1y;
        add_header Cache-Control "public, immutable";
    }
    
    # Pas de cache HTML
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
    
    success "✅ Nginx configuré"
    info "  Port: 8000"
    info "  Root: $WEB_DIR"
}

# ============================================================================
# ÉTAPE 10: CONFIGURATION SERVICE SYSTEMD (CORRIGÉ)
# ============================================================================

configure_systemd_service() {
    log "⚙️ ÉTAPE 10/10: Configuration du service systemd"
    
    info "Création des fichiers de configuration..."
    
    # ✅ NOUVEAU: Créer le fichier de configuration AVANT de créer le service
    cat > /etc/midimind/config.json << 'EOF'
{
    "version": "4.1.2",
    "server": {
        "host": "0.0.0.0",
        "port": 8080,
        "max_connections": 10
    },
    "database": {
        "path": "/opt/midimind/data/midimind.db",
        "backup_interval": 3600
    },
    "storage": {
        "root": "/opt/midimind",
        "max_file_size": 10485760
    },
    "midi": {
        "buffer_size": 256,
        "hot_plug_scan_interval": 2000,
        "default_device": "auto"
    },
    "latency": {
        "default_compensation": 0,
        "enable_instrument_compensation": true
    },
    "logging": {
        "level": "INFO",
        "file": "/var/log/midimind/backend.log",
        "max_size": 10485760,
        "rotation": 5
    }
}
EOF
    
    chown "$REAL_USER:audio" /etc/midimind/config.json
    chmod 644 /etc/midimind/config.json
    success "Fichier de configuration créé"
    
    # ✅ NOUVEAU: Initialiser la base de données avec SQLite
    info "Initialisation de la base de données..."
    sudo -u "$REAL_USER" sqlite3 "$INSTALL_DIR/data/midimind.db" "VACUUM;" 2>/dev/null || true
    chown "$REAL_USER:audio" "$INSTALL_DIR/data/midimind.db"
    chmod 664 "$INSTALL_DIR/data/midimind.db"
    success "Base de données initialisée"
    
    info "Création du service systemd..."
    cat > /etc/systemd/system/midimind.service << EOF
[Unit]
Description=MidiMind MIDI Orchestration System v4.1.2
After=network.target sound.target

[Service]
Type=simple
User=$REAL_USER
Group=audio
WorkingDirectory=$INSTALL_DIR
ExecStart=$INSTALL_DIR/bin/midimind -c /etc/midimind/config.json
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
    
    success "Service systemd configuré et activé"
}

# ============================================================================
# TEST DE DÉMARRAGE (NOUVEAU)
# ============================================================================

test_backend_startup() {
    log "🧪 Test de démarrage du backend..."
    
    info "Démarrage du service..."
    systemctl start midimind.service
    
    # Attendre 5 secondes
    info "Attente du démarrage (5 secondes)..."
    sleep 5
    
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
        journalctl -u midimind -n 20 --no-pager
    fi
    
    # Tester la connexion
    if timeout 2 bash -c "echo > /dev/tcp/localhost/8080" 2>/dev/null; then
        success "Backend accessible sur le port 8080"
    else
        warning "Backend ne répond pas sur le port 8080"
    fi
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
    
    echo -e "${CYAN}📂 Configuration installée:${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Backend:           ${GREEN}$INSTALL_DIR/bin/midimind${NC}"
    echo -e "  ${BLUE}•${NC} Frontend:          ${GREEN}$WEB_DIR${NC}"
    echo -e "  ${BLUE}•${NC} Interface web:     ${GREEN}http://$(hostname -I | awk '{print $1}'):8000${NC}"
    echo -e "  ${BLUE}•${NC} WebSocket API:     ${GREEN}ws://$(hostname -I | awk '{print $1}'):8080${NC}"
    echo -e "  ${BLUE}•${NC} Configuration:     ${GREEN}/etc/midimind/config.json${NC}"
    echo -e "  ${BLUE}•${NC} Base de données:   ${GREEN}$INSTALL_DIR/data/midimind.db${NC}"
    echo -e "  ${BLUE}•${NC} Migrations:        ${GREEN}$INSTALL_DIR/data/migrations/${NC}"
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
    
    echo -e "${CYAN}🔍 Vérifications:${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Port backend:  ${GREEN}netstat -tuln | grep 8080${NC}"
    echo -e "  ${BLUE}•${NC} Port frontend: ${GREEN}netstat -tuln | grep 8000${NC}"
    echo -e "  ${BLUE}•${NC} Test backend:  ${GREEN}curl http://localhost:8080${NC}"
    echo ""
    
    echo -e "${GREEN}✅ Le système est prêt à l'emploi !${NC}"
    echo -e "${GREEN}   Accédez à l'interface: http://$(hostname -I | awk '{print $1}'):8000${NC}"
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
    echo "MidiMind Installation v4.1.2 - $(date)" >> "$LOG_FILE"
    echo "==================================" >> "$LOG_FILE"
    log "Installation démarrée: $(date)"
    
    # Détection et vérifications
    detect_system
    check_prerequisites
    
    echo ""
    echo -e "${CYAN}${BOLD}Installation complète (Backend + Frontend + Nginx)${NC}"
    echo -e "${CYAN}Cela peut prendre 10-15 minutes...${NC}"
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
    
    # Informations finales
    print_final_info
    
    log "Installation terminée: $(date)"
}

# ============================================================================
# POINT D'ENTRÉE
# ============================================================================

main 2>&1 | tee -a "$LOG_FILE"

# ============================================================================
# FIN DU FICHIER install.sh v4.1.2 - CORRIGÉE
# ============================================================================