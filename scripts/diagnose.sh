#!/bin/bash
# ============================================================================
# Fichier: scripts/diagnose.sh
# Version: 1.0.0
# Date: 2025-10-20
# Projet: MidiMind - Script de Diagnostic
# ============================================================================
#
# Description:
#   Script de diagnostic complet pour identifier les problèmes d'installation
#   et de configuration de MidiMind
#
# Usage:
#   sudo ./diagnose.sh
#
# ============================================================================

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'
BOLD='\033[1m'

# Chemins
INSTALL_DIR="/opt/midimind"
WEB_DIR="/var/www/midimind"
CONFIG_FILE="/etc/midimind/config.json"
DB_FILE="/opt/midimind/data/midimind.db"
MIGRATIONS_DIR="/opt/midimind/data/migrations"

# Compteurs
OK_COUNT=0
WARN_COUNT=0
ERROR_COUNT=0

# ============================================================================
# FONCTIONS UTILITAIRES
# ============================================================================

print_header() {
    echo ""
    echo -e "${CYAN}${BOLD}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}${BOLD}║                                                              ║${NC}"
    echo -e "${CYAN}${BOLD}║           🔍 MidiMind Diagnostic Tool v1.0.0 🔍              ║${NC}"
    echo -e "${CYAN}${BOLD}║                                                              ║${NC}"
    echo -e "${CYAN}${BOLD}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

print_section() {
    echo ""
    echo -e "${BLUE}${BOLD}━━━ $1 ━━━${NC}"
    echo ""
}

check_ok() {
    echo -e "  ${GREEN}✓${NC} $1"
    ((OK_COUNT++))
}

check_warn() {
    echo -e "  ${YELLOW}⚠${NC} $1"
    ((WARN_COUNT++))
}

check_error() {
    echo -e "  ${RED}✗${NC} $1"
    ((ERROR_COUNT++))
}

check_info() {
    echo -e "  ${CYAN}ℹ${NC} $1"
}

# ============================================================================
# VÉRIFICATIONS
# ============================================================================

check_root() {
    if [[ $EUID -ne 0 ]]; then
        echo -e "${RED}Ce script doit être exécuté avec sudo${NC}"
        exit 1
    fi
}

check_system_info() {
    print_section "Informations Système"
    
    check_info "Hostname: $(hostname)"
    check_info "OS: $(cat /etc/os-release | grep PRETTY_NAME | cut -d'"' -f2)"
    check_info "Kernel: $(uname -r)"
    check_info "Architecture: $(uname -m)"
    check_info "CPU cores: $(nproc)"
    check_info "Memory: $(free -h | grep Mem | awk '{print $2}')"
    
    if [ -f /proc/device-tree/model ]; then
        check_info "Raspberry Pi: $(cat /proc/device-tree/model)"
    fi
}

check_directories() {
    print_section "Répertoires"
    
    local dirs=(
        "$INSTALL_DIR"
        "$INSTALL_DIR/bin"
        "$INSTALL_DIR/data"
        "$INSTALL_DIR/data/migrations"
        "$INSTALL_DIR/logs"
        "$WEB_DIR"
        "/var/log/midimind"
        "/etc/midimind"
    )
    
    for dir in "${dirs[@]}"; do
        if [ -d "$dir" ]; then
            local owner=$(stat -c '%U:%G' "$dir")
            local perms=$(stat -c '%a' "$dir")
            check_ok "$dir (owner: $owner, perms: $perms)"
        else
            check_error "$dir - MANQUANT"
        fi
    done
}

check_files() {
    print_section "Fichiers Critiques"
    
    # Binaire backend
    if [ -f "$INSTALL_DIR/bin/midimind" ]; then
        local size=$(du -h "$INSTALL_DIR/bin/midimind" | cut -f1)
        local perms=$(stat -c '%a' "$INSTALL_DIR/bin/midimind")
        check_ok "Backend binary ($size, perms: $perms)"
        
        # Vérifier les capabilities
        if getcap "$INSTALL_DIR/bin/midimind" | grep -q "cap_sys_nice"; then
            check_ok "Real-time capabilities configurées"
        else
            check_warn "Real-time capabilities manquantes"
        fi
    else
        check_error "Backend binary - MANQUANT"
    fi
    
    # Fichier de configuration
    if [ -f "$CONFIG_FILE" ]; then
        local size=$(du -h "$CONFIG_FILE" | cut -f1)
        check_ok "Configuration file ($size)"
        
        # Valider JSON
        if python3 -m json.tool "$CONFIG_FILE" > /dev/null 2>&1; then
            check_ok "Configuration JSON valide"
        else
            check_error "Configuration JSON INVALIDE"
        fi
    else
        check_error "Configuration file - MANQUANT"
    fi
    
    # Base de données
    if [ -f "$DB_FILE" ]; then
        local size=$(du -h "$DB_FILE" | cut -f1)
        local owner=$(stat -c '%U:%G' "$DB_FILE")
        local perms=$(stat -c '%a' "$DB_FILE")
        check_ok "Database file ($size, owner: $owner, perms: $perms)"
        
        # Tester la connexion SQLite
        if sqlite3 "$DB_FILE" "SELECT 1;" > /dev/null 2>&1; then
            check_ok "Database accessible"
            
            # Compter les tables
            local table_count=$(sqlite3 "$DB_FILE" "SELECT COUNT(*) FROM sqlite_master WHERE type='table';" 2>/dev/null)
            check_info "Tables: $table_count"
        else
            check_error "Database inaccessible ou corrompue"
        fi
    else
        check_warn "Database file - MANQUANT (sera créé au premier démarrage)"
    fi
    
    # Fichiers de migration
    if [ -d "$MIGRATIONS_DIR" ]; then
        local migration_count=$(find "$MIGRATIONS_DIR" -name "*.sql" 2>/dev/null | wc -l)
        if [ $migration_count -gt 0 ]; then
            check_ok "Migration files: $migration_count fichiers"
        else
            check_warn "Aucun fichier de migration trouvé"
        fi
    else
        check_error "Répertoire migrations - MANQUANT"
    fi
    
    # Frontend
    if [ -f "$WEB_DIR/index.html" ]; then
        check_ok "Frontend index.html"
    else
        check_error "Frontend index.html - MANQUANT"
    fi
    
    if [ -d "$WEB_DIR/js" ]; then
        local js_count=$(find "$WEB_DIR/js" -name "*.js" 2>/dev/null | wc -l)
        check_ok "Frontend JS files: $js_count fichiers"
    else
        check_error "Frontend JS directory - MANQUANT"
    fi
}

check_permissions() {
    print_section "Permissions et Groupes"
    
    local user="${SUDO_USER:-$USER}"
    
    # Vérifier groupe audio
    if groups "$user" | grep -q "audio"; then
        check_ok "Utilisateur $user dans le groupe audio"
    else
        check_error "Utilisateur $user PAS dans le groupe audio"
    fi
    
    # Vérifier ownership des fichiers
    if [ -f "$INSTALL_DIR/bin/midimind" ]; then
        local owner=$(stat -c '%U' "$INSTALL_DIR/bin/midimind")
        if [ "$owner" == "$user" ] || [ "$owner" == "root" ]; then
            check_ok "Backend binary owner: $owner"
        else
            check_warn "Backend binary owner incorrect: $owner"
        fi
    fi
    
    # Vérifier permissions DB
    if [ -f "$DB_FILE" ]; then
        local owner=$(stat -c '%U:%G' "$DB_FILE")
        local perms=$(stat -c '%a' "$DB_FILE")
        if [ "$perms" == "664" ] || [ "$perms" == "666" ]; then
            check_ok "Database permissions: $perms"
        else
            check_warn "Database permissions: $perms (recommandé: 664)"
        fi
    fi
}

check_services() {
    print_section "Services Systemd"
    
    # Service MidiMind
    if systemctl list-unit-files | grep -q "midimind.service"; then
        check_ok "Service midimind.service installé"
        
        if systemctl is-enabled --quiet midimind.service; then
            check_ok "Service activé au démarrage"
        else
            check_warn "Service NON activé au démarrage"
        fi
        
        if systemctl is-active --quiet midimind.service; then
            check_ok "Service en cours d'exécution"
            
            # Uptime
            local uptime=$(systemctl show midimind.service --property=ActiveEnterTimestamp --value)
            check_info "Démarré: $uptime"
            
            # PID
            local pid=$(systemctl show midimind.service --property=MainPID --value)
            if [ "$pid" != "0" ]; then
                check_info "PID: $pid"
                
                # CPU et Mémoire
                local cpu=$(ps -p "$pid" -o %cpu --no-headers)
                local mem=$(ps -p "$pid" -o %mem --no-headers)
                check_info "CPU: ${cpu}% | Memory: ${mem}%"
            fi
        else
            check_error "Service NON actif"
            
            # Montrer les dernières erreurs
            echo ""
            check_error "Dernières erreurs du service:"
            journalctl -u midimind.service -n 10 --no-pager | sed 's/^/    /'
        fi
    else
        check_error "Service midimind.service NON installé"
    fi
    
    # Service Nginx
    if systemctl is-active --quiet nginx; then
        check_ok "Nginx en cours d'exécution"
    else
        check_error "Nginx NON actif"
    fi
}

check_network() {
    print_section "Réseau et Ports"
    
    # Port 8080 (Backend WebSocket)
    if netstat -tuln 2>/dev/null | grep -q ":8080"; then
        local pid=$(netstat -tulnp 2>/dev/null | grep ":8080" | awk '{print $7}' | cut -d'/' -f1)
        check_ok "Port 8080 ouvert (PID: $pid)"
        
        # Test de connexion
        if timeout 2 bash -c "echo > /dev/tcp/localhost/8080" 2>/dev/null; then
            check_ok "Backend répond sur le port 8080"
        else
            check_warn "Backend ne répond pas sur le port 8080"
        fi
    else
        check_error "Port 8080 NON ouvert"
    fi
    
    # Port 8000 (Nginx Frontend)
    if netstat -tuln 2>/dev/null | grep -q ":8000"; then
        check_ok "Port 8000 ouvert (Nginx)"
        
        # Test HTTP
        if curl -s -o /dev/null -w "%{http_code}" http://localhost:8000 | grep -q "200"; then
            check_ok "Frontend accessible via HTTP"
        else
            check_warn "Frontend ne répond pas correctement"
        fi
    else
        check_error "Port 8000 NON ouvert"
    fi
    
    # Adresse IP
    local ip=$(hostname -I | awk '{print $1}')
    if [ -n "$ip" ]; then
        check_info "Adresse IP: $ip"
        check_info "Interface web: http://$ip:8000"
        check_info "WebSocket API: ws://$ip:8080"
    fi
}

check_dependencies() {
    print_section "Dépendances"
    
    local deps=(
        "cmake:CMake"
        "g++:G++"
        "make:Make"
        "sqlite3:SQLite3"
        "nginx:Nginx"
        "alsa-utils:ALSA Utils"
    )
    
    for dep in "${deps[@]}"; do
        local cmd=$(echo "$dep" | cut -d':' -f1)
        local name=$(echo "$dep" | cut -d':' -f2)
        
        if command -v "$cmd" &> /dev/null; then
            local version=$(eval "$cmd --version 2>&1 | head -n1 || echo 'installed'")
            check_ok "$name: $version"
        else
            check_error "$name - NON installé"
        fi
    done
    
    # Bibliothèques C++
    local libs=(
        "nlohmann/json"
        "websocketpp"
        "boost"
    )
    
    for lib in "${libs[@]}"; do
        if ldconfig -p | grep -qi "$lib"; then
            check_ok "Library $lib installée"
        else
            check_warn "Library $lib - statut inconnu"
        fi
    done
}

check_logs() {
    print_section "Logs Récents"
    
    # Logs Backend
    if [ -f "/var/log/midimind/backend.log" ]; then
        local size=$(du -h /var/log/midimind/backend.log | cut -f1)
        check_ok "Backend log file ($size)"
        
        echo ""
        echo -e "${CYAN}Dernières lignes du backend log:${NC}"
        tail -n 10 /var/log/midimind/backend.log 2>/dev/null | sed 's/^/    /'
    else
        check_warn "Backend log file manquant"
    fi
    
    # Logs systemd
    echo ""
    echo -e "${CYAN}Dernières lignes systemd (midimind):${NC}"
    journalctl -u midimind.service -n 10 --no-pager 2>/dev/null | sed 's/^/    /' || echo "    Aucun log disponible"
    
    # Logs Nginx
    echo ""
    echo -e "${CYAN}Dernières erreurs Nginx:${NC}"
    if [ -f "/var/log/nginx/midimind_error.log" ]; then
        tail -n 5 /var/log/nginx/midimind_error.log 2>/dev/null | sed 's/^/    /' || echo "    Aucune erreur"
    else
        echo "    Log file non trouvé"
    fi
}

check_config_content() {
    print_section "Configuration"
    
    if [ -f "$CONFIG_FILE" ]; then
        echo -e "${CYAN}Contenu de la configuration:${NC}"
        python3 -m json.tool "$CONFIG_FILE" 2>/dev/null | sed 's/^/    /' || cat "$CONFIG_FILE" | sed 's/^/    /'
    else
        check_error "Configuration file manquant"
    fi
}

# ============================================================================
# RAPPORT FINAL
# ============================================================================

print_summary() {
    echo ""
    echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
    echo -e "${BOLD}📊 RÉSUMÉ DU DIAGNOSTIC${NC}"
    echo ""
    echo -e "  ${GREEN}✓ Checks OK:       $OK_COUNT${NC}"
    echo -e "  ${YELLOW}⚠ Warnings:        $WARN_COUNT${NC}"
    echo -e "  ${RED}✗ Errors:          $ERROR_COUNT${NC}"
    echo ""
    
    if [ $ERROR_COUNT -eq 0 ] && [ $WARN_COUNT -eq 0 ]; then
        echo -e "${GREEN}${BOLD}🎉 TOUT EST BON ! Le système fonctionne correctement.${NC}"
    elif [ $ERROR_COUNT -eq 0 ]; then
        echo -e "${YELLOW}${BOLD}⚠️  Quelques warnings, mais le système devrait fonctionner.${NC}"
    else
        echo -e "${RED}${BOLD}❌ Problèmes détectés ! Vérifiez les erreurs ci-dessus.${NC}"
        echo ""
        echo -e "${CYAN}Solutions recommandées:${NC}"
        echo -e "  1. Relancez l'installation: ${GREEN}sudo ./install_fixed.sh${NC}"
        echo -e "  2. Vérifiez les logs: ${GREEN}sudo journalctl -u midimind -n 50${NC}"
        echo -e "  3. Redémarrez le service: ${GREEN}sudo systemctl restart midimind${NC}"
    fi
    
    echo ""
    echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
}

# ============================================================================
# MAIN
# ============================================================================

main() {
    check_root
    print_header
    
    check_system_info
    check_directories
    check_files
    check_permissions
    check_dependencies
    check_services
    check_network
    check_logs
    check_config_content
    
    print_summary
}

main "$@"

# ============================================================================
# FIN DU FICHIER diagnose.sh
# ============================================================================