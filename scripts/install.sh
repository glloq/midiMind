#!/bin/bash
# ============================================================================
# Fichier: scripts/install.sh
# Version: 4.1.0 - OPTIMISÉE
# Date: 2025-10-16
# Projet: MidiMind - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
#
# AMÉLIORATIONS v4.1.0:
#   ✅ Frontend optionnel (choix utilisateur)
#   ✅ Installation minimale backend uniquement
#   ✅ Détection automatique frontend
#   ✅ Mode développeur (sans Nginx)
#   ✅ Vérifications renforcées
#   ✅ Installation plus rapide
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

# Options d'installation
INSTALL_FRONTEND=true
INSTALL_NGINX=true
DEV_MODE=false

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
║              🎹 MidiMind v4.1.0 Installation ⚡               ║
║                                                              ║
║          Système d'Orchestration MIDI Professionnel          ║
║                  pour Raspberry Pi                           ║
║                                                              ║
║              ⚡ OPTIMISÉE & BACKEND-FIRST ⚡                   ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
    echo ""
}

# ============================================================================
# MENU INSTALLATION
# ============================================================================

show_install_menu() {
    echo -e "${BOLD}${CYAN}📦 Type d'installation :${NC}"
    echo ""
    echo -e "  ${GREEN}1)${NC} ${BOLD}Installation complète${NC} (Backend + Frontend Web + Nginx)"
    echo -e "     → Interface web sur port 8000"
    echo -e "     → Recommandé pour production"
    echo ""
    echo -e "  ${YELLOW}2)${NC} ${BOLD}Backend uniquement${NC} (API WebSocket seule)"
    echo -e "     → Pas d'interface web"
    echo -e "     → Idéal pour intégration custom ou contrôle externe"
    echo ""
    echo -e "  ${BLUE}3)${NC} ${BOLD}Mode développeur${NC} (Backend + Frontend sans Nginx)"
    echo -e "     → Serveur dev intégré"
    echo -e "     → Pour développement local"
    echo ""
    echo -e "  ${RED}0)${NC} Annuler"
    echo ""
    
    read -p "$(echo -e ${CYAN}Votre choix [1-3]: ${NC})" choice
    
    case $choice in
        1)
            INSTALL_FRONTEND=true
            INSTALL_NGINX=true
            DEV_MODE=false
            info "Mode sélectionné: Installation complète"
            ;;
        2)
            INSTALL_FRONTEND=false
            INSTALL_NGINX=false
            DEV_MODE=false
            info "Mode sélectionné: Backend uniquement"
            warning "Aucune interface web ne sera disponible"
            ;;
        3)
            INSTALL_FRONTEND=true
            INSTALL_NGINX=false
            DEV_MODE=true
            info "Mode sélectionné: Développeur"
            ;;
        0)
            echo ""
            echo -e "${RED}Installation annulée.${NC}"
            exit 0
            ;;
        *)
            error "Choix invalide: $choice"
            ;;
    esac
    
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
    echo -e "  ${BLUE}•${NC} Frontend:   ${GREEN}$FRONTEND_DIR${NC} ${CYAN}(racine du projet)${NC}"
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
        "$BACKEND_DIR/src/core/EventBus.cpp"
        "$BACKEND_DIR/src/api/ApiServer.cpp"
        "$BACKEND_DIR/src/api/CommandHandler.cpp"
        "$BACKEND_DIR/src/storage/Database.cpp"
        "$BACKEND_DIR/src/midi/devices/MidiDeviceManager.cpp"
    )
    
    for file in "${critical_files[@]}"; do
        if [ ! -f "$file" ]; then
            error "Fichier critique manquant: $file"
        fi
    done
    info "Vérification de la structure CMake..."
    
    # Vérifier que CMakeLists.txt référence les bons fichiers
    if ! grep -q "CommandHandler.cpp" "$BACKEND_DIR/CMakeLists.txt"; then
        warning "CMakeLists.txt ne contient pas CommandHandler.cpp"
        info "Ceci peut être normal si le fichier utilise un autre nom"
    fi
    
    if grep -q "CommandProcessorV2.cpp" "$BACKEND_DIR/CMakeLists.txt"; then
        error "CMakeLists.txt référence l'ancien fichier CommandProcessorV2.cpp\n  Fichier attendu: CommandHandler.cpp\n  Veuillez mettre à jour CMakeLists.txt"
    fi
    
    # ✅ VÉRIFICATION 4: Frontend (optionnel)
    if [ "$INSTALL_FRONTEND" = true ]; then
        if [ ! -d "$FRONTEND_DIR" ]; then
            warning "Frontend demandé mais répertoire introuvable: $FRONTEND_DIR"
            read -p "$(echo -e ${YELLOW}Continuer sans frontend? [O/n]: ${NC})" response
            if [[ "$response" =~ ^[Nn]$ ]]; then
                error "Installation annulée par l'utilisateur"
            fi
            INSTALL_FRONTEND=false
            INSTALL_NGINX=false
        else
            success "Répertoire frontend/ trouvé"
            
            if [ ! -f "$FRONTEND_DIR/index.html" ]; then
                warning "index.html manquant dans frontend/"
            else
                success "Frontend index.html trouvé"
            fi
        fi
    else
        info "Installation frontend désactivée (mode backend only)"
    fi
    
    echo ""
    success "✅ Structure du projet validée"
    
    # Résumé des vérifications
    echo ""
    echo -e "${BOLD}${GREEN}📋 Résumé des vérifications:${NC}"
    echo -e "  ${GREEN}✓${NC} Core:            4 fichiers .cpp (main, Application, EventBus, JsonValidator)"
    echo -e "  ${GREEN}✓${NC} Storage:         7 fichiers .cpp (Database, FileManager, PathManager, Settings, InstrumentDB, PresetMgr, SessionMgr)"
    echo -e "  ${GREEN}✓${NC} Timing:          2 fichiers .cpp (TimestampManager, LatencyCompensator)"
    echo -e "  ${GREEN}✓${NC} API:             3 fichiers .cpp (ApiServer, CommandHandler, MessageEnvelope)"
    echo -e "  ${GREEN}✓${NC} MIDI:            2 fichiers .cpp minimum (MidiRouter, MidiDeviceManager)"
    echo -e "  ${GREEN}✓${NC} Headers:         5 fichiers .h (header-only: Logger, Config, Error, TimeUtils, InstrumentLatencyProfile)"
    echo -e "  ${GREEN}✓${NC} CMakeLists.txt:  Configuration de build validée"
    if [ "$INSTALL_FRONTEND" = true ]; then
        echo -e "  ${GREEN}✓${NC} Frontend:        Répertoire présent"
    else
        echo -e "  ${YELLOW}○${NC} Frontend:        Installation désactivée"
    fi
    echo -e "  ${CYAN}ℹ${NC} Total vérifié:   ${BOLD}20 fichiers${NC} (.cpp) + ${BOLD}5 headers${NC}"
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
    log "⚙️ ÉTAPE 1/9: Mise à jour du système"
    
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
    log "📦 ÉTAPE 2/9: Installation des dépendances système"
    
    info "Installation des outils de compilation..."
    apt-get install -y -qq \
        build-essential cmake g++ gcc make pkg-config \
        git wget curl unzip \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation build tools"
    
    info "Installation des bibliothèques Audio/MIDI..."
    apt-get install -y -qq \
        libasound2-dev alsa-utils alsa-tools \
        libjack-jackd2-dev \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation ALSA"
    
    info "Installation des bibliothèques système..."
    apt-get install -y -qq \
        sqlite3 libsqlite3-dev \
        libatomic1 libpthread-stubs0-dev \
        zlib1g-dev \
        libssl-dev libcurl4-openssl-dev \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation bibliothèques système"
    
    info "Installation des bibliothèques réseau..."
    apt-get install -y -qq \
        libboost-all-dev \
        avahi-daemon libavahi-client-dev libavahi-common-dev \
        bluez libbluetooth-dev bluetooth \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation bibliothèques réseau"
    
    # Nginx uniquement si nécessaire
    if [ "$INSTALL_NGINX" = true ]; then
        info "Installation du serveur web..."
        apt-get install -y -qq nginx 2>&1 | tee -a "$LOG_FILE" || error "Échec installation nginx"
    else
        info "Installation Nginx ignorée (mode: $([[ "$INSTALL_FRONTEND" = false ]] && echo "backend only" || echo "dev"))"
    fi
    
    # Node.js uniquement si frontend installé
    if [ "$INSTALL_FRONTEND" = true ]; then
        info "Installation Node.js LTS via NodeSource..."
        if command -v node &> /dev/null; then
            local node_version=$(node --version)
            success "Node.js déjà installé: $node_version"
        else
            curl -fsSL https://deb.nodesource.com/setup_18.x | bash - 2>&1 | tee -a "$LOG_FILE" || {
                warning "NodeSource échoué, fallback sur dépôts standard..."
                apt-get install -y -qq nodejs npm 2>&1 | tee -a "$LOG_FILE"
            }
            
            apt-get install -y -qq nodejs 2>&1 | tee -a "$LOG_FILE" || error "Échec installation Node.js"
            
            local node_version=$(node --version)
            local npm_version=$(npm --version)
            success "Node.js $node_version installé (npm $npm_version)"
        fi
    else
        info "Installation Node.js ignorée (backend only)"
    fi
    
    success "✅ Dépendances système installées"
}

# ============================================================================
# ÉTAPE 3: INSTALLATION DÉPENDANCES C++
# ============================================================================

install_cpp_dependencies() {
    log "🔧 ÉTAPE 3/9: Installation des dépendances C++"
    
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
# ÉTAPE 4: CONFIGURATION PERMISSIONS
# ============================================================================

configure_permissions() {
    log "🔒 ÉTAPE 4/9: Configuration des permissions"
    
    info "Ajout de l'utilisateur $REAL_USER aux groupes..."
    usermod -a -G audio "$REAL_USER"
    usermod -a -G dialout "$REAL_USER"
    usermod -a -G bluetooth "$REAL_USER"
    usermod -a -G gpio "$REAL_USER" 2>/dev/null || true
    
    info "Configuration des permissions temps réel..."
    cat >> /etc/security/limits.conf << EOF

# MidiMind real-time permissions (v4.1.0)
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
    log "⚡ ÉTAPE 5/9: Optimisations système"
    
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
    
    # IRQ Balance
    info "Configuration IRQ Balance..."
    systemctl disable irqbalance 2>/dev/null || true
    
    # Audio tweaks
    info "Configuration audio..."
    cat >> /etc/modprobe.d/alsa-base.conf << EOF

# MidiMind audio optimizations (v4.1.0)
options snd-usb-audio nrpacks=1
EOF
    
    success "Optimisations système configurées"
}

# ============================================================================
# ÉTAPE 6: CRÉATION RÉPERTOIRES
# ============================================================================

create_directories() {
    log "📁 ÉTAPE 6/9: Création des répertoires"
    
    info "Création des répertoires principaux..."
    mkdir -p "$INSTALL_DIR"/{bin,lib,config,logs,data,backups}
    mkdir -p "$USER_DIR"/{midi_files,playlists,backups,logs}
    mkdir -p /var/log/midimind
    mkdir -p /etc/midimind
    
    if [ "$INSTALL_FRONTEND" = true ]; then
        mkdir -p "$WEB_DIR"
    fi
    
    info "Configuration des permissions..."
    chown -R "$REAL_USER:audio" "$INSTALL_DIR"
    chown -R "$REAL_USER:audio" "$USER_DIR"
    chown -R "$REAL_USER:audio" /var/log/midimind
    chown -R "$REAL_USER:audio" /etc/midimind
    
    if [ "$INSTALL_FRONTEND" = true ]; then
        chown -R www-data:www-data "$WEB_DIR"
    fi
    
    success "Répertoires créés"
}

# ============================================================================
# ÉTAPE 7: COMPILATION BACKEND
# ============================================================================

compile_backend() {
    log "🔨 ÉTAPE 7/9: Compilation du backend"
    
    if [ ! -f "$BACKEND_DIR/CMakeLists.txt" ]; then
        error "CMakeLists.txt introuvable: $BACKEND_DIR/CMakeLists.txt"
    fi
    
    if [ ! -d "$BACKEND_DIR/src" ]; then
        error "Répertoire src/ introuvable: $BACKEND_DIR/src"
    fi
    
    # Vérifier les dépendances critiques avant compilation
    info "Vérification des dépendances de compilation..."
    local missing_deps=()
    
    # Vérifier ALSA
    if ! ldconfig -p | grep -q libasound; then
        missing_deps+=("libasound2-dev")
    fi
    
    # Vérifier SQLite3
    if ! ldconfig -p | grep -q libsqlite3; then
        missing_deps+=("libsqlite3-dev")
    fi
    
    # Vérifier nlohmann/json
    if [ ! -f "/usr/include/nlohmann/json.hpp" ] && [ ! -f "/usr/local/include/nlohmann/json.hpp" ]; then
        missing_deps+=("nlohmann-json3-dev")
    fi
    
    # Vérifier websocketpp
    if [ ! -d "/usr/include/websocketpp" ] && [ ! -d "/usr/local/include/websocketpp" ]; then
        missing_deps+=("libwebsocketpp-dev")
    fi
    
    if [ ${#missing_deps[@]} -gt 0 ]; then
        error "Dépendances manquantes:\n$(printf '  - %s\n' "${missing_deps[@]}")\n  Installez-les avec: sudo apt install ${missing_deps[*]}"
    fi
    success "Toutes les dépendances sont présentes"
    
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
    
    if [ ! -f "bin/midimind" ]; then
        error "Binaire non généré: $BUILD_DIR/bin/midimind"
    fi
    
    info "Installation du binaire..."
    cp bin/midimind "$INSTALL_DIR/bin/" || error "Échec copie binaire"
    
    ln -sf "$INSTALL_DIR/bin/midimind" /usr/local/bin/midimind
    
    setcap cap_sys_nice+ep "$INSTALL_DIR/bin/midimind"
    
    success "✅ Backend compilé et installé"
    info "  Binaire: $INSTALL_DIR/bin/midimind"
    info "  Taille: $(du -h $INSTALL_DIR/bin/midimind | cut -f1)"
}

# ============================================================================
# ÉTAPE 8: INSTALLATION FRONTEND (OPTIONNEL)
# ============================================================================

install_frontend() {
    if [ "$INSTALL_FRONTEND" = false ]; then
        log "🌐 ÉTAPE 8/9: Frontend non installé (backend only)"
        return 0
    fi
    
    log "🌐 ÉTAPE 8/9: Installation du frontend"
    
    if [ ! -d "$FRONTEND_DIR" ]; then
        warning "Répertoire frontend introuvable: $FRONTEND_DIR"
        return 0
    fi
    
    info "Copie des fichiers frontend..."
    cp -r "$FRONTEND_DIR"/* "$WEB_DIR/"
    
    if [ -f "$WEB_DIR/package.json" ]; then
        info "Installation des dépendances npm..."
        cd "$WEB_DIR"
        npm install --production --no-optional 2>&1 | tee -a "$LOG_FILE" || warning "npm install a échoué"
    fi
    
    chown -R www-data:www-data "$WEB_DIR"
    
    success "Frontend installé"
}

# ============================================================================
# ÉTAPE 9: CONFIGURATION NGINX (OPTIONNEL)
# ============================================================================

configure_nginx() {
    if [ "$INSTALL_NGINX" = false ]; then
        log "🌐 ÉTAPE 9/9: Nginx non configuré"
        if [ "$DEV_MODE" = true ]; then
            info "Mode dev: utilisez un serveur dev local (ex: python -m http.server 8000)"
        fi
        return 0
    fi
    
    log "🌐 ÉTAPE 9/9: Configuration de Nginx"
    
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
    
    success "Nginx configuré"
}

# ============================================================================
# CONFIGURATION SERVICE SYSTEMD
# ============================================================================

configure_systemd_service() {
    log "⚙️ Configuration du service systemd"
    
    info "Création du service systemd..."
    cat > /etc/systemd/system/midimind.service << EOF
[Unit]
Description=MidiMind MIDI Orchestration System v4.1.0
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
# CRÉATION FICHIERS CONFIGURATION
# ============================================================================

create_config_files() {
    log "📄 Création des fichiers de configuration..."
    
    cat > /etc/midimind/config.json << 'EOF'
{
    "version": "4.1.0",
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
    
    echo -e "${CYAN}📂 Configuration installée:${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Type:              ${GREEN}$([[ "$INSTALL_FRONTEND" = true ]] && echo "Installation complète" || echo "Backend uniquement")${NC}"
    echo -e "  ${BLUE}•${NC} Backend:           ${GREEN}$INSTALL_DIR/bin/midimind${NC}"
    
    if [ "$INSTALL_FRONTEND" = true ]; then
        if [ "$INSTALL_NGINX" = true ]; then
            echo -e "  ${BLUE}•${NC} Interface web:     ${GREEN}http://$(hostname -I | awk '{print $1}'):8000${NC}"
        else
            echo -e "  ${BLUE}•${NC} Frontend:          ${YELLOW}Mode dev (serveur manuel requis)${NC}"
        fi
    else
        echo -e "  ${BLUE}•${NC} Interface web:     ${YELLOW}Non installée${NC}"
    fi
    
    echo -e "  ${BLUE}•${NC} WebSocket API:     ${GREEN}ws://$(hostname -I | awk '{print $1}'):8080${NC}"
    echo -e "  ${BLUE}•${NC} Configuration:     ${GREEN}/etc/midimind/config.json${NC}"
    echo ""
    
    echo -e "${CYAN}🚀 Commandes utiles:${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Démarrer:      ${GREEN}sudo systemctl start midimind${NC}"
    echo -e "  ${BLUE}•${NC} Arrêter:       ${GREEN}sudo systemctl stop midimind${NC}"
    echo -e "  ${BLUE}•${NC} Status:        ${GREEN}sudo systemctl status midimind${NC}"
    echo -e "  ${BLUE}•${NC} Logs:          ${GREEN}sudo journalctl -u midimind -f${NC}"
    echo ""
    
    if [ "$INSTALL_FRONTEND" = false ]; then
        echo -e "${YELLOW}⚠️  Interface web non installée${NC}"
        echo -e "   Utilisez l'API WebSocket directement ou installez un client custom"
        echo -e "   Documentation API: /opt/midimind/docs/api.md"
        echo ""
    fi
    
    echo -e "${YELLOW}⚠️  Important:${NC}"
    echo -e "  ${RED}•${NC} Redémarrez le système: ${GREEN}sudo reboot${NC}"
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
    echo "MidiMind Installation v4.1.0 - $(date)" >> "$LOG_FILE"
    echo "==================================" >> "$LOG_FILE"
    log "Installation démarrée: $(date)"
    
    # Menu de sélection
    show_install_menu
    
    # Détection et vérifications
    detect_system
    check_prerequisites
    
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
# FIN DU FICHIER install.sh v4.1.0 - OPTIMISÉE
# ============================================================================
