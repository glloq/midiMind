#!/bin/bash
# ============================================================================
# Fichier: stop.sh
# Version: v1.0.0
# Date: 2025-10-14
# Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
# Description:
#   Script pour arrêter midiMind
#   - Via systemd si disponible
#   - Ou arrêt direct du processus
#   - Arrêt propre avec timeout
#   - Nettoyage des ressources
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
NC='\033[0m'

# ============================================================================
# VARIABLES
# ============================================================================

INSTALL_DIR="/opt/midimind"
BINARY="$INSTALL_DIR/bin/midimind"
SERVICE_NAME="midimind"
PID_FILE="/var/run/midimind.pid"
PID_FILE_ALT="/tmp/midimind.pid"

# Timeouts
GRACEFUL_TIMEOUT=10  # Secondes pour arrêt gracieux
FORCE_TIMEOUT=5      # Secondes avant SIGKILL

# Mode de démarrage (auto-détecté)
USE_SYSTEMD=false
DIRECT_MODE=false

# ============================================================================
# FONCTIONS
# ============================================================================

log() {
    echo -e "${GREEN}[$(date +'%H:%M:%S')]${NC} $1"
}

error() {
    echo -e "${RED}[ERREUR]${NC} $1" >&2
    exit 1
}

warning() {
    echo -e "${YELLOW}[ATTENTION]${NC} $1"
}

info() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

# ============================================================================
# DÉTECTION MODE
# ============================================================================

detect_mode() {
    # Vérifier si systemd est disponible
    if command -v systemctl &> /dev/null; then
        if systemctl list-unit-files | grep -q "$SERVICE_NAME.service"; then
            USE_SYSTEMD=true
            info "Mode: systemd détecté"
        else
            DIRECT_MODE=true
        fi
    else
        DIRECT_MODE=true
        info "Mode: Arrêt direct"
    fi
}

# ============================================================================
# ARRÊT VIA SYSTEMD
# ============================================================================

stop_systemd() {
    log "Arrêt via systemd..."
    
    # Vérifier les permissions
    if [[ $EUID -ne 0 ]]; then
        error "Vous devez être root pour arrêter le service systemd\nUtilisez: sudo ./stop.sh"
    fi
    
    # Vérifier si le service est actif
    if ! systemctl is-active --quiet "$SERVICE_NAME"; then
        warning "Le service n'est pas démarré"
        exit 0
    fi
    
    # Arrêter le service
    info "Envoi de la commande d'arrêt..."
    systemctl stop "$SERVICE_NAME"
    
    # Attendre et vérifier
    local count=0
    while systemctl is-active --quiet "$SERVICE_NAME" && [ $count -lt $GRACEFUL_TIMEOUT ]; do
        echo -n "."
        sleep 1
        ((count++))
    done
    echo ""
    
    # Vérifier résultat
    if systemctl is-active --quiet "$SERVICE_NAME"; then
        warning "Le service ne s'est pas arrêté dans le délai imparti"
        info "Forçage de l'arrêt..."
        systemctl kill "$SERVICE_NAME"
        sleep 2
    fi
    
    if ! systemctl is-active --quiet "$SERVICE_NAME"; then
        success "Service arrêté avec succès"
        
        # Afficher les derniers logs
        echo ""
        info "Dernières lignes de log:"
        journalctl -u "$SERVICE_NAME" -n 10 --no-pager
    else
        error "Impossible d'arrêter le service\nVérifiez: journalctl -u $SERVICE_NAME -n 50"
    fi
}

# ============================================================================
# ARRÊT DIRECT
# ============================================================================

stop_direct() {
    log "Arrêt en mode direct..."
    
    # Chercher le PID file
    local pid_file=""
    if [ -f "$PID_FILE" ]; then
        pid_file="$PID_FILE"
    elif [ -f "$PID_FILE_ALT" ]; then
        pid_file="$PID_FILE_ALT"
    fi
    
    # Si pas de PID file, chercher le processus
    if [ -z "$pid_file" ]; then
        warning "Fichier PID non trouvé, recherche du processus..."
        
        PID=$(pgrep -f "^$BINARY" | head -1)
        
        if [ -z "$PID" ]; then
            warning "MidiMind n'est pas en cours d'exécution"
            exit 0
        fi
        
        info "Processus trouvé: PID $PID"
    else
        PID=$(cat "$pid_file")
        info "PID lu depuis $pid_file: $PID"
    fi
    
    # Vérifier que le processus existe
    if ! ps -p "$PID" > /dev/null 2>&1; then
        warning "Le processus PID $PID n'existe pas"
        
        # Nettoyer le PID file
        if [ -n "$pid_file" ]; then
            rm -f "$pid_file"
            info "Fichier PID nettoyé"
        fi
        
        exit 0
    fi
    
    # Arrêt gracieux (SIGTERM)
    info "Envoi de SIGTERM au processus $PID..."
    kill -TERM "$PID" 2>/dev/null || true
    
    # Attendre l'arrêt gracieux
    local count=0
    while ps -p "$PID" > /dev/null 2>&1 && [ $count -lt $GRACEFUL_TIMEOUT ]; do
        echo -n "."
        sleep 1
        ((count++))
    done
    echo ""
    
    # Vérifier si le processus est arrêté
    if ps -p "$PID" > /dev/null 2>&1; then
        warning "Le processus ne s'est pas arrêté gracieusement"
        info "Forçage de l'arrêt (SIGKILL)..."
        
        kill -KILL "$PID" 2>/dev/null || true
        
        # Attendre un peu
        count=0
        while ps -p "$PID" > /dev/null 2>&1 && [ $count -lt $FORCE_TIMEOUT ]; do
            echo -n "."
            sleep 1
            ((count++))
        done
        echo ""
    fi
    
    # Vérification finale
    if ps -p "$PID" > /dev/null 2>&1; then
        error "Impossible d'arrêter le processus $PID\nEssayez: sudo kill -9 $PID"
    else
        success "Processus arrêté (PID: $PID)"
    fi
    
    # Nettoyer le PID file
    if [ -n "$pid_file" ]; then
        rm -f "$pid_file"
        success "Fichier PID nettoyé"
    fi
}

# ============================================================================
# NETTOYAGE RESSOURCES
# ============================================================================

cleanup_resources() {
    log "Nettoyage des ressources..."
    
    # Supprimer fichiers temporaires
    local temp_files=(
        "/tmp/midimind_*"
        "/var/run/midimind_*"
    )
    
    for pattern in "${temp_files[@]}"; do
        if ls $pattern 2>/dev/null; then
            rm -f $pattern 2>/dev/null || true
        fi
    done
    
    # Vérifier les ports (WebSocket)
    local ws_port=8080
    if netstat -tuln 2>/dev/null | grep -q ":$ws_port "; then
        warning "Le port $ws_port est encore utilisé"
        info "Il se libérera automatiquement dans quelques secondes"
    fi
    
    success "Nettoyage terminé"
}

# ============================================================================
# VÉRIFIER SI DÉMARRÉ
# ============================================================================

check_if_running() {
    if $USE_SYSTEMD; then
        if ! systemctl is-active --quiet "$SERVICE_NAME"; then
            warning "Le service n'est pas démarré"
            return 1
        fi
    elif $DIRECT_MODE; then
        # Chercher le processus
        if ! pgrep -f "^$BINARY" > /dev/null 2>&1; then
            if [ ! -f "$PID_FILE" ] && [ ! -f "$PID_FILE_ALT" ]; then
                warning "MidiMind n'est pas en cours d'exécution"
                return 1
            fi
        fi
    fi
    
    return 0
}

# ============================================================================
# BANNIÈRE
# ============================================================================

print_banner() {
    echo -e "${RED}"
    cat << "EOF"
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║                   🎹 midiMind - Stop 🎹                      ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
}

# ============================================================================
# CONFIRMATION (optionnelle)
# ============================================================================

ask_confirmation() {
    if [ "$1" = "-f" ] || [ "$1" = "--force" ]; then
        return 0
    fi
    
    echo -e "${YELLOW}⚠️  Êtes-vous sûr de vouloir arrêter midiMind ? (o/N)${NC} "
    read -r response
    
    case "$response" in
        [oOyY]|[oO][uU][iI]|[yY][eE][sS])
            return 0
            ;;
        *)
            info "Arrêt annulé"
            exit 0
            ;;
    esac
}

# ============================================================================
# MAIN
# ============================================================================

main() {
    print_banner
    
    # Détecter le mode
    detect_mode
    
    # Vérifier si démarré
    if ! check_if_running; then
        exit 0
    fi
    
    # Demander confirmation (sauf si -f)
    ask_confirmation "$1"
    
    echo ""
    
    # Arrêter selon le mode
    if $USE_SYSTEMD; then
        stop_systemd
    elif $DIRECT_MODE; then
        stop_direct
    fi
    
    echo ""
    
    # Nettoyage
    cleanup_resources
    
    echo ""
    echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                                                              ║${NC}"
    echo -e "${GREEN}║                ✓ midiMind arrêté avec succès                 ║${NC}"
    echo -e "${GREEN}║                                                              ║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    info "Pour redémarrer:"
    echo -e "  ${BLUE}./start.sh${NC}"
    echo ""
}

# ============================================================================
# GESTION DES ARGUMENTS
# ============================================================================

case "$1" in
    -h|--help)
        echo "Usage: $0 [-f|--force] [-h|--help]"
        echo ""
        echo "Options:"
        echo "  -f, --force    Arrêt sans confirmation"
        echo "  -h, --help     Afficher cette aide"
        echo ""
        echo "Exemples:"
        echo "  $0              Arrêt avec confirmation"
        echo "  $0 --force      Arrêt immédiat"
        exit 0
        ;;
esac

# ============================================================================
# POINT D'ENTRÉE
# ============================================================================

main "$@"

# ============================================================================
# FIN DU FICHIER stop.sh v1.0.0
# ============================================================================