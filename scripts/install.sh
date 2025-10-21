#!/bin/bash
# ============================================================================
# Fichier: scripts/install.sh
# Version: 4.1.3 - COMPLET + ALSA Utils
# Date: 2025-10-21
# Projet: MidiMind - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
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
║              🎹 MidiMind v4.1.3 Installation ⚡               ║
║                                                              ║
║          Système d'Orchestration MIDI Professionnel          ║
║                  pour Raspberry Pi                           ║
║                                                              ║
║              Installation Complète Automatique               ║
║                  Version COMPLÈTE + ALSA                     ║
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
    
    info "Installation de WebSocketpp..."
    apt-get install -y -qq \
        libwebsocketpp-dev \
        2>&1 | tee -a "$LOG_FILE" || error "Échec installation websocketpp"
    
    # ✅ VÉRIFICATION ALSA UTILS
    if command -v aconnect &> /dev/null; then
        success "ALSA Utils installé (aconnect, amidi disponibles)"
    else
        error "ALSA Utils manquant après installation"
    fi
    
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

# MIDI sequencer optimization
defaults.seq.timer_backend seq
defaults.seq.queue_capacity 1024
EOF
        success "Configuration ALSA créée"
    else
        info "Configuration ALSA existante préservée"
    fi
    
    # Règles udev pour accès MIDI temps réel
    if [ ! -f /etc/udev/rules.d/99-midi-rt.rules ]; then
        info "Création des règles udev MIDI..."
        cat > /etc/udev/rules.d/99-midi-rt.rules << EOF
# MidiMind - Real-time MIDI access
KERNEL=="snd_seq", GROUP="audio", MODE="0660"
KERNEL=="midi*", GROUP="audio", MODE="0660"
SUBSYSTEM=="sound", GROUP="audio", MODE="0660"
EOF
        udevadm control --reload-rules
        udevadm trigger
        success "Règles udev MIDI créées"
    else
        info "Règles udev MIDI existantes"
    fi
    
    success "ALSA configuré pour temps réel"
}

# ============================================================================
# ÉTAPE 5: CONFIGURATION PERMISSIONS
# ============================================================================

configure_permissions() {
    log "🔐 ÉTAPE 5/11: Configuration des permissions"
    
    # Ajouter utilisateur au groupe audio
    if ! groups "$REAL_USER" | grep -q audio; then
        usermod -a -G audio "$REAL_USER"
        success "Utilisateur ajouté au groupe audio"
    else
        info "Utilisateur déjà dans le groupe audio"
    fi
    
    # Configurer limites temps réel
    if [ ! -f /etc/security/limits.d/99-midimind.conf ]; then
        cat > /etc/security/limits.d/99-midimind.conf << EOF
# MidiMind - Real-time audio limits
$REAL_USER     -    rtprio    95
$REAL_USER     -    memlock   unlimited
$REAL_USER     -    nice      -19
@audio         -    rtprio    95
@audio         -    memlock   unlimited
@audio         -    nice      -19
EOF
        success "Limites temps réel configurées"
    else
        info "Limites temps réel déjà configurées"
    fi
}

# ============================================================================
# ÉTAPE 6: OPTIMISATIONS SYSTÈME
# ============================================================================

configure_system_optimizations() {
    log "⚡ ÉTAPE 6/11: Configuration des optimisations système"
    
    # Swappiness
    if ! grep -q "vm.swappiness" /etc/sysctl.conf; then
        echo "vm.swappiness=10" >> /etc/sysctl.conf
        sysctl -w vm.swappiness=10 > /dev/null
        success "Swappiness configuré (10)"
    else
        info "Swappiness déjà configuré"
    fi
    
    # CPU governor (performance)
    if command -v cpufreq-set &> /dev/null; then
        for cpu in /sys/devices/system/cpu/cpu[0-9]*; do
            if [ -f "$cpu/cpufreq/scaling_governor" ]; then
                echo performance > "$cpu/cpufreq/scaling_governor" 2>/dev/null || true
            fi
        done
        success "CPU governor configuré (performance)"
    else
        info "cpufreq-utils non disponible (ignoré)"
    fi
    
    success "Optimisations système appliquées"
}

# ============================================================================
# ÉTAPE 7: CRÉATION RÉPERTOIRES
# ============================================================================

create_directories() {
    log "📁 ÉTAPE 7/11: Création de la structure de répertoires"
    
    local directories=(
        "$INSTALL_DIR"
        "$INSTALL_DIR/bin"
        "$INSTALL_DIR/data"
        "$INSTALL_DIR/data/migrations"
        "$INSTALL_DIR/data/backups"
        "$INSTALL_DIR/presets"
        "$WEB_DIR"
        "/etc/midimind"
        "/var/log/midimind"
        "$USER_DIR"
    )
    
    info "Création des répertoires..."
    for dir in "${directories[@]}"; do
        mkdir -p "$dir"
        info "  → $dir"
    done
    
    # Permissions
    info "Configuration des permissions..."
    chown -R "$REAL_USER:audio" "$INSTALL_DIR"
    chown -R "$REAL_USER:audio" "$USER_DIR"
    chown -R "$REAL_USER:audio" "/var/log/midimind"
    chown -R www-data:www-data "$WEB_DIR"
    
    chmod 755 "$INSTALL_DIR"
    chmod 775 "$INSTALL_DIR/data"
    chmod 755 /etc/midimind
    
    success "Structure de répertoires créée"
}

# ============================================================================
# ÉTAPE 8: COMPILATION BACKEND
# ============================================================================

compile_backend() {
    log "🔨 ÉTAPE 8/11: Compilation du backend"
    
    cd "$BACKEND_DIR"
    
    # Nettoyer build précédent
    if [ -d "$BUILD_DIR" ]; then
        info "Nettoyage du build précédent..."
        rm -rf "$BUILD_DIR"
    fi
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    info "Configuration CMake..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DBUILD_TESTS=OFF \
        2>&1 | tee -a "$LOG_FILE" || error "Échec configuration CMake"
    
    info "Compilation en cours (5-10 minutes sur Raspberry Pi)..."
    info "Utilisation de $NPROC cœurs..."
    if ! make -j$NPROC 2>&1 | tee -a "$LOG_FILE"; then
        error "Échec compilation\n  Vérifiez les logs: cat /var/log/midimind_install.log | tail -100"
    fi
    
    success "Backend compilé avec succès"
    
    # Copier l'exécutable
    info "Installation du binaire..."
    if [ ! -f "$BUILD_DIR/bin/midimind" ]; then
        error "Binaire midimind non trouvé dans $BUILD_DIR/bin/"
    fi
    
    cp "$BUILD_DIR/bin/midimind" "$INSTALL_DIR/bin/"
    chmod 755 "$INSTALL_DIR/bin/midimind"
    chown "$REAL_USER:audio" "$INSTALL_DIR/bin/midimind"
    success "Binaire installé: $INSTALL_DIR/bin/midimind"
    
    # ✅ COPIE DES FICHIERS SQL DE MIGRATION
    info "Copie des fichiers de migration SQL..."
    if [ -d "$BACKEND_DIR/data/migrations" ]; then
        local sql_files=$(find "$BACKEND_DIR/data/migrations" -name "*.sql" 2>/dev/null)
        if [ -n "$sql_files" ]; then
            cp "$BACKEND_DIR/data/migrations"/*.sql "$INSTALL_DIR/data/migrations/" 2>/dev/null || true
            local copied_count=$(find "$INSTALL_DIR/data/migrations" -name "*.sql" 2>/dev/null | wc -l)
            if [ $copied_count -gt 0 ]; then
                success "Fichiers SQL copiés: $copied_count fichiers"
                for sql_file in $(find "$INSTALL_DIR/data/migrations" -name "*.sql" | sort); do
                    info "  → $(basename $sql_file)"
                done
            else
                warning "Aucun fichier SQL copié"
            fi
        else
            warning "Aucun fichier SQL trouvé dans backend/data/migrations/"
        fi
    else
        warning "Dossier backend/data/migrations/ introuvable"
    fi
    
    chown -R "$REAL_USER:audio" "$INSTALL_DIR/data/migrations"
    chmod 644 "$INSTALL_DIR/data/migrations"/*.sql 2>/dev/null || true
    
    success "Backend installé avec succès"
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
    cp -r "$FRONTEND_DIR"/* "$WEB_DIR/"
    
    # Compter fichiers copiés
    local file_count=$(find "$WEB_DIR" -type f | wc -l)
    info "Fichiers copiés: $file_count"
    
    chown -R www-data:www-data "$WEB_DIR"
    chmod -R 755 "$WEB_DIR"
    
    success "Frontend installé: $WEB_DIR"
}

# ============================================================================
# ÉTAPE 10: CONFIGURATION NGINX
# ============================================================================

configure_nginx() {
    log "🌐 ÉTAPE 10/11: Configuration de Nginx"
    
    info "Création de la configuration Nginx..."
    cat > /etc/nginx/sites-available/midimind << 'EOF'
server {
    listen 8000;
    server_name _;
    
    root /var/www/midimind;
    index index.html;
    
    # Logs
    access_log /var/log/nginx/midimind_access.log;
    error_log /var/log/nginx/midimind_error.log;
    
    # Frontend
    location / {
        try_files $uri $uri/ /index.html;
    }
    
    # WebSocket proxy vers backend
    location /ws {
        proxy_pass http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_read_timeout 86400;
    }
    
    # API REST
    location /api {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }
    
    # Cache statique
    location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg)$ {
        expires 1y;
        add_header Cache-Control "public, immutable";
    }
}
EOF
    
    # Activer le site
    info "Activation du site..."
    ln -sf /etc/nginx/sites-available/midimind /etc/nginx/sites-enabled/
    
    # Désactiver site par défaut
    info "Désactivation du site par défaut..."
    rm -f /etc/nginx/sites-enabled/default
    
    # Tester configuration
    info "Test de la configuration Nginx..."
    nginx -t 2>&1 | tee -a "$LOG_FILE" || error "Configuration Nginx invalide"
    
    # Redémarrer Nginx
    info "Redémarrage de Nginx..."
    systemctl restart nginx
    systemctl enable nginx
    
    success "Nginx configuré et démarré"
}

# ============================================================================
# ÉTAPE 11: CONFIGURATION SYSTEMD
# ============================================================================

configure_systemd_service() {
    log "⚙️  ÉTAPE 11/11: Configuration du service systemd"
    
    # ✅ CONFIG.JSON COMPLET - Conforme à Config.h v4.1.0
    info "Création du fichier de configuration complet..."
    cat > /etc/midimind/config.json << EOF
{
    "application": {
        "version": "4.1.0",
        "name": "MidiMind",
        "data_dir": "$INSTALL_DIR/data",
        "log_dir": "/var/log/midimind"
    },
    "midi": {
        "buffer_size": 256,
        "sample_rate": 44100,
        "max_devices": 32,
        "alsa_client_name": "MidiMind"
    },
    "api": {
        "port": 8080,
        "host": "0.0.0.0",
        "max_connections": 10,
        "timeout_ms": 30000
    },
    "timing": {
        "latency_compensation": true,
        "auto_calibration": true,
        "calibration_duration_ms": 5000,
        "calibration_iterations": 100,
        "max_jitter_ms": 5.0,
        "default_compensation": 0,
        "enable_instrument_compensation": true
    },
    "storage": {
        "database_path": "$INSTALL_DIR/data/midimind.db",
        "auto_backup": true,
        "backup_interval_hours": 24,
        "max_backups": 7,
        "root": "$INSTALL_DIR",
        "max_file_size": 10485760
    },
    "logging": {
        "level": "info",
        "file_enabled": true,
        "console_enabled": true,
        "max_file_size_mb": 10,
        "max_backups": 5,
        "file": "/var/log/midimind/backend.log",
        "rotation": 5
    }
}
EOF
    
    chown "$REAL_USER:audio" /etc/midimind/config.json
    chmod 644 /etc/midimind/config.json
    success "Fichier de configuration créé: /etc/midimind/config.json"
    
    # Initialiser la base de données
    info "Initialisation de la base de données..."
    touch "$INSTALL_DIR/data/midimind.db"
    sudo -u "$REAL_USER" sqlite3 "$INSTALL_DIR/data/midimind.db" "VACUUM;" 2>/dev/null || true
    
    # ✅ APPLIQUER LES MIGRATIONS SQL
    local sql_files=$(find "$INSTALL_DIR/data/migrations" -name "*.sql" 2>/dev/null | sort)
    if [ -n "$sql_files" ]; then
        info "Application des migrations SQL..."
        for sql_file in $sql_files; do
            local basename=$(basename "$sql_file")
            info "  → Application de $basename..."
            sudo -u "$REAL_USER" sqlite3 "$INSTALL_DIR/data/midimind.db" < "$sql_file" 2>&1 | grep -v "RAISE" | tee -a "$LOG_FILE" || true
        done
        success "Migrations SQL appliquées"
    else
        warning "Aucune migration SQL à appliquer"
    fi
    
    # Vérifier la base de données
    info "Vérification de la base de données..."
    local table_count=$(sudo -u "$REAL_USER" sqlite3 "$INSTALL_DIR/data/midimind.db" "SELECT COUNT(*) FROM sqlite_master WHERE type='table';" 2>/dev/null || echo "0")
    if [ "$table_count" -gt 0 ]; then
        success "Base de données initialisée ($table_count tables créées)"
    else
        warning "Aucune table créée dans la base de données"
    fi
    
    chown "$REAL_USER:audio" "$INSTALL_DIR/data/midimind.db"
    chmod 664 "$INSTALL_DIR/data/midimind.db"
    success "Base de données configurée"
    
    info "Création du service systemd..."
    cat > /etc/systemd/system/midimind.service << EOF
[Unit]
Description=MidiMind MIDI Orchestration System v4.1.3
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
# TEST DE DÉMARRAGE
# ============================================================================

test_backend_startup() {
    log "🧪 Test de démarrage du backend..."
    
    info "Démarrage du service..."
    systemctl start midimind.service
    
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
    echo -e "${YELLOW}   Redémarrez le système pour appliquer les permissions audio${NC}"
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
    echo "MidiMind Installation v4.1.3 - $(date)" >> "$LOG_FILE"
    echo "==================================" >> "$LOG_FILE"
    log "Installation démarrée: $(date)"
    
    # Détection et vérifications
    detect_system
    check_prerequisites
    
    echo ""
    echo -e "${CYAN}${BOLD}Installation complète (Backend + Frontend + Nginx + ALSA)${NC}"
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
# FIN DU FICHIER install.sh v4.1.3 - COMPLET + ALSA Utils
# ============================================================================