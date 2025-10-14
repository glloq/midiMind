#!/bin/bash
# ============================================================================
# Fichier: status.sh
# Version: v1.0.0
# Date: 2025-10-14
# Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
# Description:
#   Script pour afficher le statut de midiMind
#   - État du service (systemd ou processus)
#   - Utilisation ressources (CPU, RAM)
#   - Ports réseau ouverts
#   - Dernières lignes de log
#   - Informations système
# ============================================================================

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
# VARIABLES
# ============================================================================

SERVICE_NAME="midimind"
BINARY="/opt/midimind/bin/midimind"
PID_FILE="/var/run/midimind.pid"
PID_FILE_ALT="/tmp/midimind.pid"
LOG_FILE="/var/log/midimind/midimind.log"
CONFIG_FILE="/etc/midimind/config.json"

# ============================================================================
# FONCTIONS
# ============================================================================

print_section() {
    echo ""
    echo -e "${BOLD}${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD}${CYAN}$1${NC}"
    echo -e "${BOLD}${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

print_item() {
    echo -e "  ${BLUE}•${NC} $1: ${GREEN}$2${NC}"
}

print_item_warn() {
    echo -e "  ${BLUE}•${NC} $1: ${YELLOW}$2${NC}"
}

print_item_error() {
    echo -e "  ${BLUE}•${NC} $1: ${RED}$2${NC}"
}

# ============================================================================
# BANNIÈRE
# ============================================================================

print_banner() {
    echo -e "${CYAN}"
    cat << "EOF"
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║                  🎹 midiMind - Status 🎹                     ║
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
            echo "systemd"
            return
        fi
    fi
    echo "direct"
}

# ============================================================================
# STATUT SERVICE SYSTEMD
# ============================================================================

show_systemd_status() {
    print_section "📊 STATUT SERVICE (systemd)"
    
    if systemctl is-active --quiet "$SERVICE_NAME"; then
        print_item "État" "Actif ✓"
        
        # Uptime
        local start_time=$(systemctl show "$SERVICE_NAME" --property=ActiveEnterTimestamp --value)
        if [ -n "$start_time" ]; then
            print_item "Démarré le" "$start_time"
        fi
        
        # PID
        local main_pid=$(systemctl show "$SERVICE_NAME" --property=MainPID --value)
        if [ "$main_pid" != "0" ]; then
            print_item "PID" "$main_pid"
        fi
        
    elif systemctl is-enabled --quiet "$SERVICE_NAME"; then
        print_item_warn "État" "Inactif (mais activé au démarrage)"
    else
        print_item_error "État" "Inactif"
    fi
    
    # Auto-start
    if systemctl is-enabled --quiet "$SERVICE_NAME" 2>/dev/null; then
        print_item "Démarrage auto" "Activé ✓"
    else
        print_item_warn "Démarrage auto" "Désactivé"
    fi
}

# ============================================================================
# STATUT PROCESSUS DIRECT
# ============================================================================

show_direct_status() {
    print_section "📊 STATUT PROCESSUS"
    
    # Chercher PID
    local pid=""
    if [ -f "$PID_FILE" ]; then
        pid=$(cat "$PID_FILE")
    elif [ -f "$PID_FILE_ALT" ]; then
        pid=$(cat "$PID_FILE_ALT")
    else
        pid=$(pgrep -f "^$BINARY" | head -1)
    fi
    
    if [ -n "$pid" ] && ps -p "$pid" > /dev/null 2>&1; then
        print_item "État" "En cours d'exécution ✓"
        print_item "PID" "$pid"
        
        # Uptime
        local start_time=$(ps -p "$pid" -o lstart= 2>/dev/null)
        if [ -n "$start_time" ]; then
            print_item "Démarré le" "$start_time"
        fi
        
    else
        print_item_error "État" "Non démarré"
    fi
}

# ============================================================================
# RESSOURCES
# ============================================================================

show_resources() {
    print_section "💾 UTILISATION RESSOURCES"
    
    # Trouver le PID
    local pid=""
    local mode=$(detect_mode)
    
    if [ "$mode" = "systemd" ]; then
        pid=$(systemctl show "$SERVICE_NAME" --property=MainPID --value)
    else
        if [ -f "$PID_FILE" ]; then
            pid=$(cat "$PID_FILE")
        elif [ -f "$PID_FILE_ALT" ]; then
            pid=$(cat "$PID_FILE_ALT")
        else
            pid=$(pgrep -f "^$BINARY" | head -1)
        fi
    fi
    
    if [ -n "$pid" ] && [ "$pid" != "0" ] && ps -p "$pid" > /dev/null 2>&1; then
        # CPU
        local cpu=$(ps -p "$pid" -o %cpu= 2>/dev/null | xargs)
        if [ -n "$cpu" ]; then
            print_item "CPU" "${cpu}%"
        fi
        
        # RAM
        local mem=$(ps -p "$pid" -o %mem= 2>/dev/null | xargs)
        local rss=$(ps -p "$pid" -o rss= 2>/dev/null | xargs)
        if [ -n "$mem" ] && [ -n "$rss" ]; then
            local mem_mb=$((rss / 1024))
            print_item "Mémoire" "${mem}% (${mem_mb} MB)"
        fi
        
        # Threads
        local threads=$(ps -p "$pid" -o nlwp= 2>/dev/null | xargs)
        if [ -n "$threads" ]; then
            print_item "Threads" "$threads"
        fi
        
        # Fichiers ouverts
        if command -v lsof &> /dev/null; then
            local open_files=$(lsof -p "$pid" 2>/dev/null | wc -l)
            print_item "Fichiers ouverts" "$open_files"
        fi
        
    else
        print_item_error "Ressources" "Non disponible (processus non démarré)"
    fi
}

# ============================================================================
# RÉSEAU
# ============================================================================

show_network() {
    print_section "🌐 RÉSEAU"
    
    # Récupérer IP
    local ip=$(hostname -I | awk '{print $1}')
    print_item "Adresse IP" "$ip"
    
    # Vérifier ports
    local ws_port=8080
    local web_port=8000
    
    # Port WebSocket (API)
    if netstat -tuln 2>/dev/null | grep -q ":$ws_port "; then
        print_item "Port WebSocket" "$ws_port ✓ (ouvert)"
        print_item "API Endpoint" "ws://$ip:$ws_port"
    else
        print_item_warn "Port WebSocket" "$ws_port (fermé)"
    fi
    
    # Port Web (Nginx)
    if netstat -tuln 2>/dev/null | grep -q ":$web_port "; then
        print_item "Port HTTP" "$web_port ✓ (ouvert)"
        print_item "Interface Web" "http://$ip:$web_port"
    else
        print_item_warn "Port HTTP" "$web_port (fermé)"
    fi
}

# ============================================================================
# CONFIGURATION
# ============================================================================

show_config() {
    print_section "⚙️  CONFIGURATION"
    
    if [ -f "$CONFIG_FILE" ]; then
        print_item "Fichier config" "$CONFIG_FILE ✓"
        
        # Extraire infos si jq disponible
        if command -v jq &> /dev/null; then
            local version=$(jq -r '.application.version // "N/A"' "$CONFIG_FILE" 2>/dev/null)
            local data_dir=$(jq -r '.application.data_directory // "N/A"' "$CONFIG_FILE" 2>/dev/null)
            local log_level=$(jq -r '.logger.level // "N/A"' "$CONFIG_FILE" 2>/dev/null)
            
            print_item "Version" "$version"
            print_item "Répertoire données" "$data_dir"
            print_item "Niveau log" "$log_level"
        fi
    else
        print_item_warn "Fichier config" "Non trouvé (utilise valeurs par défaut)"
    fi
}

# ============================================================================
# LOGS RÉCENTS
# ============================================================================

show_logs() {
    print_section "📝 LOGS RÉCENTS"
    
    local mode=$(detect_mode)
    
    if [ "$mode" = "systemd" ]; then
        # Logs systemd
        echo ""
        journalctl -u "$SERVICE_NAME" -n 10 --no-pager 2>/dev/null || {
            print_item_warn "Logs" "Non disponibles"
        }
    else
        # Logs fichier
        if [ -f "$LOG_FILE" ]; then
            echo ""
            tail -n 10 "$LOG_FILE" 2>/dev/null || {
                print_item_warn "Logs" "Impossible de lire $LOG_FILE"
            }
        else
            print_item_warn "Logs" "Fichier log non trouvé: $LOG_FILE"
        fi
    fi
}

# ============================================================================
# SYSTÈME
# ============================================================================

show_system() {
    print_section "🖥️  INFORMATIONS SYSTÈME"
    
    # Raspberry Pi model
    if [ -f /proc/device-tree/model ]; then
        local rpi_model=$(cat /proc/device-tree/model 2>/dev/null)
        print_item "Modèle" "$rpi_model"
    fi
    
    # OS
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        print_item "OS" "$PRETTY_NAME"
    fi
    
    # Kernel
    local kernel=$(uname -r)
    print_item "Kernel" "$kernel"
    
    # Uptime système
    local uptime=$(uptime -p 2>/dev/null || uptime)
    print_item "Uptime" "$uptime"
    
    # CPU temperature (Raspberry Pi)
    if [ -f /sys/class/thermal/thermal_zone0/temp ]; then
        local temp=$(cat /sys/class/thermal/thermal_zone0/temp)
        local temp_c=$((temp / 1000))
        
        if [ $temp_c -lt 70 ]; then
            print_item "Température CPU" "${temp_c}°C ✓"
        elif [ $temp_c -lt 80 ]; then
            print_item_warn "Température CPU" "${temp_c}°C (attention)"
        else
            print_item_error "Température CPU" "${temp_c}°C (critique!)"
        fi
    fi
}

# ============================================================================
# COMMANDES RAPIDES
# ============================================================================

show_commands() {
    print_section "🚀 COMMANDES RAPIDES"
    
    echo -e "  ${BLUE}•${NC} Démarrer:   ${GREEN}./start.sh${NC}"
    echo -e "  ${BLUE}•${NC} Arrêter:    ${GREEN}./stop.sh${NC}"
    echo -e "  ${BLUE}•${NC} Redémarrer: ${GREEN}./restart.sh${NC}"
    echo -e "  ${BLUE}•${NC} Logs live:  ${GREEN}journalctl -u midimind -f${NC} (systemd)"
    echo -e "  ${BLUE}•${NC}              ${GREEN}tail -f $LOG_FILE${NC} (direct)"
}

# ============================================================================
# MAIN
# ============================================================================

main() {
    print_banner
    
    local mode=$(detect_mode)
    
    # Statut principal
    if [ "$mode" = "systemd" ]; then
        show_systemd_status
    else
        show_direct_status
    fi
    
    # Ressources
    show_resources
    
    # Réseau
    show_network
    
    # Configuration
    show_config
    
    # Système
    show_system
    
    # Logs récents
    show_logs
    
    # Commandes
    show_commands
    
    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
}

# ============================================================================
# GESTION DES ARGUMENTS
# ============================================================================

case "$1" in
    -h|--help)
        echo "Usage: $0 [-h|--help] [-w|--watch]"
        echo ""
        echo "Description:"
        echo "  Affiche le statut complet de midiMind"
        echo ""
        echo "Options:"
        echo "  -h, --help     Afficher cette aide"
        echo "  -w, --watch    Mode surveillance (rafraîchissement auto)"
        echo ""
        exit 0
        ;;
    -w|--watch)
        # Mode watch
        while true; do
            clear
            main
            echo -e "${CYAN}Rafraîchissement dans 3 secondes... (Ctrl+C pour quitter)${NC}"
            sleep 3
        done
        ;;
esac

# ============================================================================
# POINT D'ENTRÉE
# ============================================================================

main

# ============================================================================
# FIN DU FICHIER status.sh v1.0.0
# ============================================================================