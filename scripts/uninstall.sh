#!/bin/bash
# ============================================================================
# Fichier: uninstall.sh
# Version: v1.0.0
# Date: 2025-10-14
# Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
# Description:
#   Script de désinstallation complète de midiMind
#   - Arrêt et suppression du service
#   - Suppression des binaires et bibliothèques
#   - Nettoyage configuration système
#   - Options pour conserver données utilisateur
#   - Désinstallation sécurisée avec confirmations
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

INSTALL_DIR="/opt/midimind"
WEB_DIR="/var/www/midimind"
USER_DIR="$HOME/midimind"
LOG_DIR="/var/log/midimind"
CONFIG_DIR="/etc/midimind"
SERVICE_FILE="/etc/systemd/system/midimind.service"
NGINX_CONFIG="/etc/nginx/sites-available/midimind"
NGINX_ENABLED="/etc/nginx/sites-enabled/midimind"

UNINSTALL_LOG="/tmp/midimind_uninstall.log"

# Options de désinstallation
KEEP_DATA=false
KEEP_LOGS=false
KEEP_CONFIG=false
FORCE_MODE=false
INTERACTIVE=true

# Obtenir le nom de l'utilisateur réel
REAL_USER="${SUDO_USER:-$USER}"

# ============================================================================
# FONCTIONS UTILITAIRES
# ============================================================================

log() {
    echo -e "${GREEN}[$(date +'%H:%M:%S')]${NC} $1" | tee -a "$UNINSTALL_LOG"
}

error() {
    echo -e "${RED}[ERREUR]${NC} $1" | tee -a "$UNINSTALL_LOG" >&2
    exit 1
}

warning() {
    echo -e "${YELLOW}[ATTENTION]${NC} $1" | tee -a "$UNINSTALL_LOG"
}

info() {
    echo -e "${CYAN}[INFO]${NC} $1" | tee -a "$UNINSTALL_LOG"
}

success() {
    echo -e "${GREEN}[✓]${NC} $1" | tee -a "$UNINSTALL_LOG"
}

# ============================================================================
# BANNIÈRE
# ============================================================================

print_banner() {
    clear
    echo -e "${RED}"
    cat << "EOF"
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║              🗑️  midiMind - Uninstall 🗑️                     ║
║                                                              ║
║            Désinstallation Complète du Système              ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
}

# ============================================================================
# AIDE
# ============================================================================

print_help() {
    cat << EOF
Usage: sudo ./uninstall.sh [OPTIONS]

Description:
  Désinstalle complètement midiMind du système

Options:
  -h, --help              Afficher cette aide
  -f, --force             Mode forcé (sans confirmations)
  -d, --keep-data         Conserver les données utilisateur
  -l, --keep-logs         Conserver les fichiers de log
  -c, --keep-config       Conserver la configuration
  -a, --keep-all          Conserver données + logs + config

Exemples:
  sudo ./uninstall.sh                    # Désinstallation complète interactive
  sudo ./uninstall.sh --keep-data        # Conserver les données utilisateur
  sudo ./uninstall.sh --force            # Désinstallation sans confirmation
  sudo ./uninstall.sh --keep-all --force # Conserver tout, sans confirmation

Éléments supprimés par défaut:
  • Service systemd
  • Binaire (/opt/midimind)
  • Frontend (/var/www/midimind)
  • Configuration Nginx
  • Configuration système (/etc/midimind)
  • Logs (/var/log/midimind)
  • Données utilisateur (~/midimind)
  • Optimisations système

Éléments CONSERVÉS:
  • Paquets système (cmake, alsa, etc.)
  • Groupes utilisateur (audio, etc.)
  • Dépendances C++ (nlohmann/json, websocketpp)

EOF
}

# ============================================================================
# VÉRIFICATIONS PRÉALABLES
# ============================================================================

check_prerequisites() {
    log "Vérification des prérequis..."
    
    # Vérifier root
    if [[ $EUID -ne 0 ]]; then
        error "Ce script doit être exécuté avec sudo"
    fi
    success "Permissions root OK"
    
    # Vérifier si midiMind est installé
    if [ ! -f "$INSTALL_DIR/bin/midimind" ] && [ ! -f "$SERVICE_FILE" ]; then
        warning "midiMind ne semble pas être installé"
        echo ""
        echo -e "${YELLOW}Éléments introuvables:${NC}"
        echo "  • Binaire: $INSTALL_DIR/bin/midimind"
        echo "  • Service: $SERVICE_FILE"
        echo ""
        read -p "Continuer la désinstallation quand même ? (o/N) " -r
        if [[ ! $REPLY =~ ^[oOyY]$ ]]; then
            info "Désinstallation annulée"
            exit 0
        fi
    else
        success "Installation midiMind détectée"
    fi
}

# ============================================================================
# CONFIRMATION UTILISATEUR
# ============================================================================

confirm_uninstall() {
    if [ "$FORCE_MODE" = true ]; then
        return 0
    fi
    
    echo ""
    echo -e "${BOLD}${RED}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BOLD}${RED}║                    ⚠️  ATTENTION ⚠️                         ║${NC}"
    echo -e "${BOLD}${RED}╚════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${YELLOW}Cette opération va désinstaller midiMind de votre système.${NC}"
    echo ""
    echo -e "${CYAN}Éléments qui seront supprimés:${NC}"
    
    echo "  ${RED}✗${NC} Service systemd"
    echo "  ${RED}✗${NC} Binaire et bibliothèques ($INSTALL_DIR)"
    echo "  ${RED}✗${NC} Interface web ($WEB_DIR)"
    echo "  ${RED}✗${NC} Configuration Nginx"
    
    if [ "$KEEP_CONFIG" = false ]; then
        echo "  ${RED}✗${NC} Configuration système ($CONFIG_DIR)"
    else
        echo "  ${GREEN}✓${NC} Configuration système (CONSERVÉE)"
    fi
    
    if [ "$KEEP_LOGS" = false ]; then
        echo "  ${RED}✗${NC} Fichiers de log ($LOG_DIR)"
    else
        echo "  ${GREEN}✓${NC} Fichiers de log (CONSERVÉS)"
    fi
    
    if [ "$KEEP_DATA" = false ]; then
        echo "  ${RED}✗${NC} Données utilisateur ($USER_DIR)"
    else
        echo "  ${GREEN}✓${NC} Données utilisateur (CONSERVÉES)"
    fi
    
    echo ""
    echo -e "${CYAN}Éléments qui seront CONSERVÉS:${NC}"
    echo "  ${GREEN}✓${NC} Paquets système (cmake, gcc, alsa, etc.)"
    echo "  ${GREEN}✓${NC} Dépendances C++ (nlohmann/json, websocketpp)"
    echo "  ${GREEN}✓${NC} Configuration réseau"
    echo ""
    
    echo -e "${BOLD}${YELLOW}Êtes-vous ABSOLUMENT sûr de vouloir continuer ? (oui/non)${NC}"
    read -r response
    
    if [ "$response" != "oui" ] && [ "$response" != "OUI" ]; then
        info "Désinstallation annulée"
        exit 0
    fi
    
    echo ""
    log "Confirmation reçue, début de la désinstallation..."
}

# ============================================================================
# ARRÊT DU SERVICE
# ============================================================================

stop_service() {
    log "ÉTAPE 1/10: Arrêt du service..."
    
    # Vérifier si le service existe
    if systemctl list-unit-files | grep -q "midimind.service"; then
        
        # Arrêter le service s'il est actif
        if systemctl is-active --quiet midimind; then
            info "Arrêt du service en cours..."
            systemctl stop midimind || warning "Échec de l'arrêt du service"
            sleep 2
        fi
        
        # Désactiver le service
        if systemctl is-enabled --quiet midimind 2>/dev/null; then
            info "Désactivation du démarrage automatique..."
            systemctl disable midimind || warning "Échec de la désactivation"
        fi
        
        success "Service arrêté et désactivé"
    else
        info "Service systemd non trouvé (déjà supprimé ou mode direct)"
    fi
    
    # Vérifier et tuer les processus restants
    if pgrep -f "midimind" > /dev/null; then
        warning "Processus midiMind encore actif, arrêt forcé..."
        pkill -TERM -f "midimind"
        sleep 2
        
        if pgrep -f "midimind" > /dev/null; then
            pkill -KILL -f "midimind"
            sleep 1
        fi
        
        if pgrep -f "midimind" > /dev/null; then
            warning "Impossible d'arrêter tous les processus"
        else
            success "Tous les processus arrêtés"
        fi
    fi
}

# ============================================================================
# SUPPRESSION DU SERVICE SYSTEMD
# ============================================================================

remove_service() {
    log "ÉTAPE 2/10: Suppression du service systemd..."
    
    if [ -f "$SERVICE_FILE" ]; then
        info "Suppression de $SERVICE_FILE..."
        rm -f "$SERVICE_FILE"
        systemctl daemon-reload
        systemctl reset-failed 2>/dev/null || true
        success "Service systemd supprimé"
    else
        info "Fichier service non trouvé (déjà supprimé)"
    fi
}

# ============================================================================
# SUPPRESSION DU BINAIRE
# ============================================================================

remove_binary() {
    log "ÉTAPE 3/10: Suppression du binaire..."
    
    # Supprimer le répertoire d'installation
    if [ -d "$INSTALL_DIR" ]; then
        info "Suppression de $INSTALL_DIR..."
        rm -rf "$INSTALL_DIR"
        success "Répertoire d'installation supprimé"
    else
        info "Répertoire d'installation non trouvé"
    fi
    
    # Supprimer le lien symbolique
    if [ -L "/usr/local/bin/midimind" ]; then
        info "Suppression du lien symbolique..."
        rm -f "/usr/local/bin/midimind"
        success "Lien symbolique supprimé"
    fi
}

# ============================================================================
# SUPPRESSION DU FRONTEND
# ============================================================================

remove_frontend() {
    log "ÉTAPE 4/10: Suppression du frontend..."
    
    # Supprimer la configuration Nginx
    if [ -L "$NGINX_ENABLED" ]; then
        info "Désactivation du site Nginx..."
        rm -f "$NGINX_ENABLED"
    fi
    
    if [ -f "$NGINX_CONFIG" ]; then
        info "Suppression de la configuration Nginx..."
        rm -f "$NGINX_CONFIG"
    fi
    
    # Recharger Nginx si actif
    if systemctl is-active --quiet nginx; then
        info "Rechargement de Nginx..."
        systemctl reload nginx || warning "Échec du rechargement de Nginx"
        success "Configuration Nginx supprimée et rechargée"
    else
        success "Configuration Nginx supprimée"
    fi
    
    # Supprimer le répertoire web
    if [ -d "$WEB_DIR" ]; then
        info "Suppression de $WEB_DIR..."
        rm -rf "$WEB_DIR"
        success "Répertoire web supprimé"
    else
        info "Répertoire web non trouvé"
    fi
}

# ============================================================================
# SUPPRESSION DE LA CONFIGURATION
# ============================================================================

remove_config() {
    if [ "$KEEP_CONFIG" = true ]; then
        log "ÉTAPE 5/10: Conservation de la configuration (option --keep-config)"
        info "Configuration conservée dans: $CONFIG_DIR"
        return
    fi
    
    log "ÉTAPE 5/10: Suppression de la configuration..."
    
    if [ -d "$CONFIG_DIR" ]; then
        info "Suppression de $CONFIG_DIR..."
        rm -rf "$CONFIG_DIR"
        success "Configuration supprimée"
    else
        info "Répertoire de configuration non trouvé"
    fi
}

# ============================================================================
# SUPPRESSION DES LOGS
# ============================================================================

remove_logs() {
    if [ "$KEEP_LOGS" = true ]; then
        log "ÉTAPE 6/10: Conservation des logs (option --keep-logs)"
        info "Logs conservés dans: $LOG_DIR"
        return
    fi
    
    log "ÉTAPE 6/10: Suppression des logs..."
    
    if [ -d "$LOG_DIR" ]; then
        info "Suppression de $LOG_DIR..."
        rm -rf "$LOG_DIR"
        success "Logs supprimés"
    else
        info "Répertoire de logs non trouvé"
    fi
}

# ============================================================================
# SUPPRESSION DES DONNÉES UTILISATEUR
# ============================================================================

remove_user_data() {
    if [ "$KEEP_DATA" = true ]; then
        log "ÉTAPE 7/10: Conservation des données utilisateur (option --keep-data)"
        info "Données conservées dans: $USER_DIR"
        return
    fi
    
    log "ÉTAPE 7/10: Suppression des données utilisateur..."
    
    # Chemin absolu pour l'utilisateur réel
    local real_user_dir="/home/$REAL_USER/midimind"
    
    if [ -d "$real_user_dir" ]; then
        # Confirmation supplémentaire pour les données utilisateur
        if [ "$FORCE_MODE" = false ]; then
            echo ""
            echo -e "${YELLOW}⚠️  Les données utilisateur contiennent vos fichiers MIDI et playlists.${NC}"
            echo -e "${YELLOW}   Répertoire: $real_user_dir${NC}"
            echo ""
            read -p "Supprimer définitivement ces données ? (oui/non) " -r
            
            if [ "$REPLY" != "oui" ] && [ "$REPLY" != "OUI" ]; then
                info "Données utilisateur conservées"
                return
            fi
        fi
        
        info "Suppression de $real_user_dir..."
        rm -rf "$real_user_dir"
        success "Données utilisateur supprimées"
    else
        info "Répertoire de données non trouvé"
    fi
}

# ============================================================================
# NETTOYAGE OPTIMISATIONS SYSTÈME
# ============================================================================

remove_system_optimizations() {
    log "ÉTAPE 8/10: Nettoyage des optimisations système..."
    
    # Supprimer les limites temps réel
    local limits_file="/etc/security/limits.d/95-midimind.conf"
    if [ -f "$limits_file" ]; then
        info "Suppression des limites temps réel..."
        rm -f "$limits_file"
        success "Limites temps réel supprimées"
    fi
    
    # Supprimer le service CPU governor
    if systemctl list-unit-files | grep -q "cpufreq-performance.service"; then
        info "Suppression du service CPU governor..."
        systemctl disable cpufreq-performance.service 2>/dev/null || true
        rm -f /etc/systemd/system/cpufreq-performance.service
        systemctl daemon-reload
        success "Service CPU governor supprimé"
    fi
    
    # Restaurer IRQ balance si disponible
    if command -v irqbalance &> /dev/null; then
        info "Réactivation de IRQ balance..."
        systemctl enable irqbalance 2>/dev/null || true
    fi
    
    # Nettoyer les modifications ALSA
    local alsa_conf="/etc/modprobe.d/alsa-base.conf"
    if [ -f "$alsa_conf" ]; then
        if grep -q "midiMind audio optimizations" "$alsa_conf"; then
            info "Nettoyage des modifications ALSA..."
            sed -i '/# midiMind audio optimizations/,+1d' "$alsa_conf"
            success "Modifications ALSA nettoyées"
        fi
    fi
    
    success "Optimisations système nettoyées"
}

# ============================================================================
# NETTOYAGE FICHIERS TEMPORAIRES
# ============================================================================

cleanup_temp_files() {
    log "ÉTAPE 9/10: Nettoyage des fichiers temporaires..."
    
    # PID files
    rm -f /var/run/midimind.pid 2>/dev/null || true
    rm -f /tmp/midimind.pid 2>/dev/null || true
    
    # Fichiers temporaires
    rm -f /tmp/midimind_* 2>/dev/null || true
    
    # Sockets
    rm -f /tmp/midimind.sock 2>/dev/null || true
    
    success "Fichiers temporaires nettoyés"
}

# ============================================================================
# VÉRIFICATION FINALE
# ============================================================================

verify_uninstall() {
    log "ÉTAPE 10/10: Vérification de la désinstallation..."
    
    local issues=0
    
    # Vérifier service
    if systemctl list-unit-files | grep -q "midimind.service"; then
        warning "Service systemd encore présent"
        ((issues++))
    fi
    
    # Vérifier binaire
    if [ -f "$INSTALL_DIR/bin/midimind" ]; then
        warning "Binaire encore présent"
        ((issues++))
    fi
    
    # Vérifier processus
    if pgrep -f "midimind" > /dev/null; then
        warning "Processus encore actif"
        ((issues++))
    fi
    
    # Vérifier ports
    if netstat -tuln 2>/dev/null | grep -q ":8080 "; then
        warning "Port 8080 encore utilisé"
        ((issues++))
    fi
    
    if [ $issues -eq 0 ]; then
        success "Vérification terminée : désinstallation complète"
    else
        warning "Vérification terminée avec $issues avertissement(s)"
    fi
}

# ============================================================================
# RÉSUMÉ FINAL
# ============================================================================

print_summary() {
    echo ""
    echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                                                              ║${NC}"
    echo -e "${GREEN}║        ✓ Désinstallation terminée avec succès !              ║${NC}"
    echo -e "${GREEN}║                                                              ║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    echo -e "${CYAN}📋 Résumé:${NC}"
    echo "  ${GREEN}✓${NC} Service systemd supprimé"
    echo "  ${GREEN}✓${NC} Binaire supprimé"
    echo "  ${GREEN}✓${NC} Frontend supprimé"
    echo "  ${GREEN}✓${NC} Configuration Nginx supprimée"
    
    if [ "$KEEP_CONFIG" = true ]; then
        echo "  ${BLUE}→${NC} Configuration conservée: $CONFIG_DIR"
    else
        echo "  ${GREEN}✓${NC} Configuration supprimée"
    fi
    
    if [ "$KEEP_LOGS" = true ]; then
        echo "  ${BLUE}→${NC} Logs conservés: $LOG_DIR"
    else
        echo "  ${GREEN}✓${NC} Logs supprimés"
    fi
    
    if [ "$KEEP_DATA" = true ]; then
        echo "  ${BLUE}→${NC} Données conservées: /home/$REAL_USER/midimind"
    else
        echo "  ${GREEN}✓${NC} Données utilisateur supprimées"
    fi
    
    echo "  ${GREEN}✓${NC} Optimisations système nettoyées"
    echo ""
    
    echo -e "${CYAN}📝 Éléments conservés:${NC}"
    echo "  • Paquets système (cmake, gcc, alsa, etc.)"
    echo "  • Bibliothèques C++ (nlohmann/json, websocketpp)"
    echo "  • Configuration réseau et groupes utilisateur"
    echo ""
    
    if [ "$KEEP_DATA" = true ] || [ "$KEEP_LOGS" = true ] || [ "$KEEP_CONFIG" = true ]; then
        echo -e "${YELLOW}💾 Données conservées:${NC}"
        [ "$KEEP_CONFIG" = true ] && echo "  • Configuration: $CONFIG_DIR"
        [ "$KEEP_LOGS" = true ] && echo "  • Logs: $LOG_DIR"
        [ "$KEEP_DATA" = true ] && echo "  • Données utilisateur: /home/$REAL_USER/midimind"
        echo ""
        echo -e "${YELLOW}Pour supprimer manuellement ces données:${NC}"
        [ "$KEEP_CONFIG" = true ] && echo "  sudo rm -rf $CONFIG_DIR"
        [ "$KEEP_LOGS" = true ] && echo "  sudo rm -rf $LOG_DIR"
        [ "$KEEP_DATA" = true ] && echo "  rm -rf /home/$REAL_USER/midimind"
        echo ""
    fi
    
    echo -e "${CYAN}ℹ️  Informations:${NC}"
    echo "  • Log de désinstallation: $UNINSTALL_LOG"
    echo "  • Pour réinstaller: sudo ./install.sh"
    echo ""
    
    if [ "$KEEP_DATA" = false ]; then
        echo -e "${YELLOW}⚠️  Remarque:${NC}"
        echo "  Les données utilisateur ont été supprimées définitivement."
        echo "  Assurez-vous d'avoir sauvegardé vos fichiers MIDI si nécessaire."
        echo ""
    fi
}

# ============================================================================
# GESTION DES ARGUMENTS
# ============================================================================

parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                print_help
                exit 0
                ;;
            -f|--force)
                FORCE_MODE=true
                shift
                ;;
            -d|--keep-data)
                KEEP_DATA=true
                shift
                ;;
            -l|--keep-logs)
                KEEP_LOGS=true
                shift
                ;;
            -c|--keep-config)
                KEEP_CONFIG=true
                shift
                ;;
            -a|--keep-all)
                KEEP_DATA=true
                KEEP_LOGS=true
                KEEP_CONFIG=true
                shift
                ;;
            *)
                error "Option inconnue: $1\nUtilisez --help pour l'aide"
                ;;
        esac
    done
}

# ============================================================================
# FONCTION PRINCIPALE
# ============================================================================

main() {
    # Bannière
    print_banner
    
    # Parser les arguments
    parse_arguments "$@"
    
    # Initialiser le log
    echo "=== Désinstallation de midiMind - $(date) ===" > "$UNINSTALL_LOG"
    
    # Vérifications
    check_prerequisites
    
    # Confirmation
    confirm_uninstall
    
    echo ""
    
    # Désinstallation séquentielle
    stop_service
    echo ""
    
    remove_service
    echo ""
    
    remove_binary
    echo ""
    
    remove_frontend
    echo ""
    
    remove_config
    echo ""
    
    remove_logs
    echo ""
    
    remove_user_data
    echo ""
    
    remove_system_optimizations
    echo ""
    
    cleanup_temp_files
    echo ""
    
    verify_uninstall
    
    # Résumé
    print_summary
    
    # Log final
    echo "=== Désinstallation terminée - $(date) ===" >> "$UNINSTALL_LOG"
    
    success "Désinstallation terminée !"
    echo ""
}

# ============================================================================
# POINT D'ENTRÉE
# ============================================================================

main "$@"

# ============================================================================
# FIN DU FICHIER uninstall.sh v1.0.0
# ============================================================================