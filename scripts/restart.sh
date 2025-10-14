#!/bin/bash
# ============================================================================
# Fichier: restart.sh
# Version: v1.0.0
# Date: 2025-10-14
# Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
# Description:
#   Script pour redémarrer midiMind
#   - Arrêt puis redémarrage automatique
#   - Vérifications entre les étapes
#   - Support systemd et mode direct
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

# ============================================================================
# VARIABLES
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICE_NAME="midimind"

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
# BANNIÈRE
# ============================================================================

print_banner() {
    echo -e "${MAGENTA}"
    cat << "EOF"
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║                  🎹 midiMind - Restart 🎹                    ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
}

# ============================================================================
# DÉTECTION MODE
# ============================================================================

detect_mode() {
    if command -v systemctl &> /dev/null; then
        if systemctl list-unit-files | grep -q "$SERVICE_NAME.service"; then
            return 0  # systemd
        fi
    fi
    return 1  # direct
}

# ============================================================================
# REDÉMARRAGE VIA SYSTEMD
# ============================================================================

restart_systemd() {
    log "Redémarrage via systemd..."
    
    # Vérifier les permissions
    if [[ $EUID -ne 0 ]]; then
        error "Vous devez être root pour redémarrer le service systemd\nUtilisez: sudo ./restart.sh"
    fi
    
    # Redémarrer
    systemctl restart "$SERVICE_NAME"
    
    # Attendre un peu
    sleep 2
    
    # Vérifier
    if systemctl is-active --quiet "$SERVICE_NAME"; then
        success "Service redémarré avec succès"
        
        echo ""
        systemctl status "$SERVICE_NAME" --no-pager -l
        
        echo ""
        info "Pour voir les logs:"
        echo -e "  ${BLUE}journalctl -u $SERVICE_NAME -f${NC}"
        
    else
        error "Échec du redémarrage\nVérifiez: journalctl -u $SERVICE_NAME -n 50"
    fi
}

# ============================================================================
# REDÉMARRAGE DIRECT
# ============================================================================

restart_direct() {
    log "Redémarrage en mode direct..."
    
    # Arrêter d'abord
    if [ -f "$SCRIPT_DIR/stop.sh" ]; then
        info "Étape 1/2: Arrêt..."
        "$SCRIPT_DIR/stop.sh" --force
    else
        error "Script stop.sh introuvable dans $SCRIPT_DIR"
    fi
    
    # Attendre un peu
    echo ""
    info "Attente de l'arrêt complet..."
    sleep 3
    
    # Redémarrer
    echo ""
    if [ -f "$SCRIPT_DIR/start.sh" ]; then
        info "Étape 2/2: Redémarrage..."
        "$SCRIPT_DIR/start.sh"
    else
        error "Script start.sh introuvable dans $SCRIPT_DIR"
    fi
}

# ============================================================================
# VÉRIFIER SI DÉMARRÉ
# ============================================================================

check_if_running() {
    if detect_mode; then
        # systemd
        systemctl is-active --quiet "$SERVICE_NAME"
    else
        # direct
        pgrep -f "midimind" > /dev/null 2>&1
    fi
}

# ============================================================================
# MAIN
# ============================================================================

main() {
    print_banner
    
    # Vérifier si MidiMind est démarré
    if ! check_if_running; then
        warning "MidiMind n'est pas démarré"
        info "Démarrage initial..."
        
        if [ -f "$SCRIPT_DIR/start.sh" ]; then
            "$SCRIPT_DIR/start.sh"
        else
            error "Script start.sh introuvable"
        fi
        
        exit 0
    fi
    
    echo ""
    
    # Redémarrer selon le mode
    if detect_mode; then
        restart_systemd
    else
        restart_direct
    fi
    
    echo ""
    success "Redémarrage terminé !"
    echo ""
}

# ============================================================================
# GESTION DES ARGUMENTS
# ============================================================================

case "$1" in
    -h|--help)
        echo "Usage: $0 [-h|--help]"
        echo ""
        echo "Description:"
        echo "  Redémarre midiMind (arrêt puis démarrage)"
        echo ""
        echo "Options:"
        echo "  -h, --help     Afficher cette aide"
        echo ""
        echo "Notes:"
        echo "  • Utilise systemd si disponible"
        echo "  • Sinon redémarre en mode direct"
        echo "  • Nécessite sudo pour systemd"
        exit 0
        ;;
esac

# ============================================================================
# POINT D'ENTRÉE
# ============================================================================

main "$@"

# ============================================================================
# FIN DU FICHIER restart.sh v1.0.0
# ============================================================================