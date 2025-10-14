#!/bin/bash
# ============================================================================
# Fichier: start.sh
# Version: v1.0.0
# Date: 2025-10-14
# Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
# Description:
#   Script pour démarrer midiMind
#   - Via systemd si disponible
#   - Ou en mode direct pour développement
#   - Vérifications avant démarrage
#   - Logs détaillés
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
LOG_FILE="/var/log/midimind/midimind.log"
PID_FILE="/var/run/midimind.pid"

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
            warning "systemd disponible mais service non installé"
            DIRECT_MODE=true
        fi
    else
        DIRECT_MODE=true
        info "Mode: Démarrage direct (pas de systemd)"
    fi
}

# ============================================================================
# VÉRIFICATIONS PRÉALABLES
# ============================================================================

check_prerequisites() {
    log "Vérification des prérequis..."
    
    # Vérifier si le binaire existe
    if [ ! -f "$BINARY" ]; then
        error "Binaire introuvable: $BINARY\nExécutez d'abord ./install.sh"
    fi
    success "Binaire trouvé"
    
    # Vérifier les permissions
    if [ ! -x "$BINARY" ]; then
        error "Binaire non exécutable: $BINARY"
    fi
    success "Permissions OK"
    
    # Vérifier que le service n'est pas déjà démarré
    if $USE_SYSTEMD; then
        if systemctl is-active --quiet "$SERVICE_NAME"; then
            warning "Le service est déjà démarré"
            info "Utilisez './restart.sh' pour redémarrer"
            exit 0
        fi
    elif $DIRECT_MODE; then
        if [ -f "$PID_FILE" ]; then
            PID=$(cat "$PID_FILE")
            if ps -p "$PID" > /dev/null 2>&1; then
                warning "MidiMind est déjà en cours d'exécution (PID: $PID)"
                info "Utilisez './stop.sh' pour l'arrêter d'abord"
                exit 0
            else
                # PID file existe mais processus mort
                warning "Fichier PID orphelin détecté, nettoyage..."
                rm -f "$PID_FILE"
            fi
        fi
    fi
    
    # Vérifier la configuration
    CONFIG_FILE="/etc/midimind/config.json"
    if [ ! -f "$CONFIG_FILE" ]; then
        warning "Fichier de configuration non trouvé: $CONFIG_FILE"
        warning "MidiMind utilisera la configuration par défaut"
    else
        success "Configuration trouvée"
    fi
    
    # Créer répertoire logs si nécessaire
    LOG_DIR=$(dirname "$LOG_FILE")
    if [ ! -d "$LOG_DIR" ]; then
        mkdir -p "$LOG_DIR"
        chown "${SUDO_USER:-$USER}:audio" "$LOG_DIR" 2>/dev/null || true
    fi
}

# ============================================================================
# DÉMARRAGE VIA SYSTEMD
# ============================================================================

start_systemd() {
    log "Démarrage via systemd..."
    
    # Vérifier les permissions
    if [[ $EUID -ne 0 ]]; then
        error "Vous devez être root pour démarrer le service systemd\nUtilisez: sudo ./start.sh"
    fi
    
    # Démarrer le service
    systemctl start "$SERVICE_NAME"
    
    # Attendre un peu
    sleep 2
    
    # Vérifier que le service a démarré
    if systemctl is-active --quiet "$SERVICE_NAME"; then
        success "Service démarré avec succès"
        
        # Afficher le statut
        echo ""
        systemctl status "$SERVICE_NAME" --no-pager -l
        
        echo ""
        info "Pour voir les logs en temps réel:"
        echo -e "  ${BLUE}journalctl -u $SERVICE_NAME -f${NC}"
        
        echo ""
        info "Pour arrêter le service:"
        echo -e "  ${BLUE}sudo ./stop.sh${NC}"
        
    else
        error "Échec du démarrage du service\nVérifiez les logs: journalctl -u $SERVICE_NAME -n 50"
    fi
}

# ============================================================================
# DÉMARRAGE DIRECT
# ============================================================================

start_direct() {
    log "Démarrage en mode direct..."
    
    # Vérifier si on peut écrire le PID file
    PID_DIR=$(dirname "$PID_FILE")
    if [ ! -w "$PID_DIR" ]; then
        warning "Impossible d'écrire dans $PID_DIR"
        PID_FILE="/tmp/midimind.pid"
        info "Utilisation de $PID_FILE à la place"
    fi
    
    # Lancer en arrière-plan
    info "Lancement de $BINARY"
    
    # Utiliser nohup pour continuer après fermeture du terminal
    nohup "$BINARY" >> "$LOG_FILE" 2>&1 &
    
    PID=$!
    
    # Sauvegarder le PID
    echo $PID > "$PID_FILE"
    
    # Attendre un peu et vérifier
    sleep 2
    
    if ps -p $PID > /dev/null 2>&1; then
        success "MidiMind démarré avec succès (PID: $PID)"
        
        echo ""
        info "Informations:"
        echo -e "  • PID: ${BLUE}$PID${NC}"
        echo -e "  • Logs: ${BLUE}$LOG_FILE${NC}"
        echo -e "  • PID file: ${BLUE}$PID_FILE${NC}"
        
        echo ""
        info "Pour voir les logs en temps réel:"
        echo -e "  ${BLUE}tail -f $LOG_FILE${NC}"
        
        echo ""
        info "Pour arrêter:"
        echo -e "  ${BLUE}./stop.sh${NC}"
        
    else
        rm -f "$PID_FILE"
        error "Échec du démarrage\nVérifiez les logs: $LOG_FILE"
    fi
}

# ============================================================================
# AFFICHAGE INFORMATIONS POST-DÉMARRAGE
# ============================================================================

show_info() {
    echo ""
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║                                                              ║${NC}"
    echo -e "${CYAN}║                    🎹 midiMind Démarré 🎹                    ║${NC}"
    echo -e "${CYAN}║                                                              ║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    # Récupérer l'IP
    IP=$(hostname -I | awk '{print $1}')
    
    echo -e "${CYAN}🌐 Accès Interface Web:${NC}"
    echo -e "   • Local:  ${GREEN}http://localhost:8000${NC}"
    echo -e "   • Réseau: ${GREEN}http://$IP:8000${NC}"
    echo ""
    
    echo -e "${CYAN}🔌 API WebSocket:${NC}"
    echo -e "   • Endpoint: ${GREEN}ws://$IP:8080${NC}"
    echo ""
}

# ============================================================================
# BANNIÈRE
# ============================================================================

print_banner() {
    echo -e "${BLUE}"
    cat << "EOF"
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║                  🎹 midiMind - Start 🎹                      ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
}

# ============================================================================
# MAIN
# ============================================================================

main() {
    print_banner
    
    # Détecter le mode
    detect_mode
    
    # Vérifications
    check_prerequisites
    
    echo ""
    
    # Démarrer selon le mode
    if $USE_SYSTEMD; then
        start_systemd
    elif $DIRECT_MODE; then
        start_direct
    fi
    
    # Afficher infos
    show_info
    
    echo ""
    success "Démarrage terminé !"
    echo ""
}

# ============================================================================
# POINT D'ENTRÉE
# ============================================================================

main "$@"

# ============================================================================
# FIN DU FICHIER start.sh v1.0.0
# ============================================================================