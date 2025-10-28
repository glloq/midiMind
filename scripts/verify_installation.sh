#!/bin/bash
# ============================================================================
# Script: verify_installation.sh
# Description: Vérification COMPLÈTE de l'installation MidiMind
# ============================================================================

set +e  # Ne pas arrêter sur erreur pour tout tester

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

ERRORS=0
WARNINGS=0

check() {
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓${NC} $1"
        return 0
    else
        echo -e "${RED}✗${NC} $1"
        ((ERRORS++))
        return 1
    fi
}

warn() {
    echo -e "${YELLOW}⚠${NC} $1"
    ((WARNINGS++))
}

info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

section() {
    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

# ============================================================================
# VÉRIFICATIONS
# ============================================================================

section "1. STRUCTURE DE FICHIERS"

# Backend
[ -d "backend/src" ] && check "backend/src/" || check "backend/src/ (MANQUANT)"
[ -f "backend/CMakeLists.txt" ] && check "backend/CMakeLists.txt" || check "backend/CMakeLists.txt (MANQUANT)"
[ -f "backend/src/main.cpp" ] && check "backend/src/main.cpp" || check "backend/src/main.cpp (MANQUANT)"
[ -d "backend/data/migrations" ] && check "backend/data/migrations/" || check "backend/data/migrations/ (MANQUANT)"

# Frontend
[ -d "frontend" ] && check "frontend/" || check "frontend/ (MANQUANT)"
[ -f "frontend/index.html" ] && check "frontend/index.html" || check "frontend/index.html (MANQUANT)"
[ -d "frontend/js" ] && check "frontend/js/" || check "frontend/js/ (MANQUANT)"

# Scripts
[ -d "scripts" ] && check "scripts/" || check "scripts/ (MANQUANT)"
[ -f "scripts/install.sh" ] && check "scripts/install.sh" || check "scripts/install.sh (MANQUANT)"
[ -f "scripts/midimind.service" ] && check "scripts/midimind.service" || check "scripts/midimind.service (MANQUANT)"

section "2. RÉPERTOIRES SYSTÈME"

[ -d "/opt/midimind" ] && check "/opt/midimind/" || check "/opt/midimind/ (MANQUANT)"
[ -d "/opt/midimind/bin" ] && check "/opt/midimind/bin/" || check "/opt/midimind/bin/ (MANQUANT)"
[ -d "/opt/midimind/data" ] && check "/opt/midimind/data/" || check "/opt/midimind/data/ (MANQUANT)"
[ -d "/opt/midimind/data/migrations" ] && check "/opt/midimind/data/migrations/" || check "/opt/midimind/data/migrations/ (MANQUANT)"
[ -d "/opt/midimind/logs" ] && check "/opt/midimind/logs/" || check "/opt/midimind/logs/ (MANQUANT)"
[ -d "/opt/midimind/presets" ] && check "/opt/midimind/presets/" || check "/opt/midimind/presets/ (MANQUANT)"
[ -d "/opt/midimind/sessions" ] && check "/opt/midimind/sessions/" || check "/opt/midimind/sessions/ (MANQUANT)"

[ -d "/etc/midimind" ] && check "/etc/midimind/" || check "/etc/midimind/ (MANQUANT)"
[ -d "/var/www/midimind" ] && check "/var/www/midimind/" || check "/var/www/midimind/ (MANQUANT)"

REAL_USER="${SUDO_USER:-$USER}"
[ -d "/home/$REAL_USER/.midimind" ] && check "/home/$REAL_USER/.midimind/" || check "/home/$REAL_USER/.midimind/ (MANQUANT)"

section "3. FICHIERS CRITIQUES"

[ -f "/opt/midimind/bin/midimind" ] && check "Binaire midimind" || check "Binaire midimind (MANQUANT)"
if [ -f "/opt/midimind/bin/midimind" ]; then
    [ -x "/opt/midimind/bin/midimind" ] && check "Binaire exécutable" || check "Binaire NON exécutable"
fi

[ -f "/etc/midimind/config.json" ] && check "config.json" || check "config.json (MANQUANT)"
[ -f "/etc/systemd/system/midimind.service" ] && check "midimind.service" || check "midimind.service (MANQUANT)"

# Migrations SQL
SQL_COUNT=$(find /opt/midimind/data/migrations -name "*.sql" 2>/dev/null | wc -l)
if [ $SQL_COUNT -ge 2 ]; then
    check "Migrations SQL ($SQL_COUNT fichiers)"
else
    check "Migrations SQL (SEULEMENT $SQL_COUNT fichiers)"
fi

# Base de données
[ -f "/opt/midimind/data/midimind.db" ] && check "Base de données" || warn "Base de données pas encore créée"

section "4. PERMISSIONS"

# /opt/midimind
OWNER=$(stat -c '%U' /opt/midimind 2>/dev/null)
if [ "$OWNER" = "$REAL_USER" ]; then
    check "/opt/midimind appartient à $REAL_USER"
else
    check "/opt/midimind appartient à $OWNER (attendu: $REAL_USER)"
fi

# /opt/midimind/data
if [ -d "/opt/midimind/data" ]; then
    OWNER=$(stat -c '%U' /opt/midimind/data 2>/dev/null)
    if [ "$OWNER" = "$REAL_USER" ]; then
        check "/opt/midimind/data appartient à $REAL_USER"
    else
        check "/opt/midimind/data appartient à $OWNER (attendu: $REAL_USER)"
    fi
    
    # Test écriture
    if sudo -u "$REAL_USER" touch /opt/midimind/data/.test 2>/dev/null; then
        sudo -u "$REAL_USER" rm /opt/midimind/data/.test
        check "Écriture possible dans /opt/midimind/data"
    else
        check "Écriture IMPOSSIBLE dans /opt/midimind/data"
    fi
fi

section "5. CONFIGURATION"

if [ -f "/etc/midimind/config.json" ]; then
    # Vérifier JSON valide
    if python3 -m json.tool /etc/midimind/config.json > /dev/null 2>&1; then
        check "config.json est un JSON valide"
    else
        check "config.json a des ERREURS de syntaxe"
    fi
    
    # Vérifier sections
    grep -q '"api"' /etc/midimind/config.json && check "Section 'api'" || check "Section 'api' MANQUANTE"
    grep -q '"database"' /etc/midimind/config.json && check "Section 'database'" || check "Section 'database' MANQUANTE"
    grep -q '"paths"' /etc/midimind/config.json && check "Section 'paths'" || check "Section 'paths' MANQUANTE"
    grep -q '"midi"' /etc/midimind/config.json && check "Section 'midi'" || check "Section 'midi' MANQUANTE"
    grep -q '"latency"' /etc/midimind/config.json && check "Section 'latency'" || check "Section 'latency' MANQUANTE"
    grep -q '"system"' /etc/midimind/config.json && check "Section 'system'" || check "Section 'system' MANQUANTE"
fi

section "6. DÉPENDANCES SYSTÈME"

command -v cmake >/dev/null 2>&1 && check "CMake installé" || check "CMake NON installé"
command -v g++ >/dev/null 2>&1 && check "G++ installé" || check "G++ NON installé"
command -v git >/dev/null 2>&1 && check "Git installé" || check "Git NON installé"
command -v aconnect >/dev/null 2>&1 && check "ALSA Utils installé" || check "ALSA Utils NON installé"
command -v sqlite3 >/dev/null 2>&1 && check "SQLite3 installé" || check "SQLite3 NON installé"
command -v nginx >/dev/null 2>&1 && check "Nginx installé" || check "Nginx NON installé"
command -v bluetoothctl >/dev/null 2>&1 && check "Bluetooth installé" || warn "Bluetooth non installé"
command -v lsusb >/dev/null 2>&1 && check "USB Utils installé" || warn "USB Utils non installé"

# Bibliothèques C++
ldconfig -p | grep -q libasound && check "ALSA library" || check "ALSA library MANQUANTE"
ldconfig -p | grep -q libsqlite3 && check "SQLite3 library" || check "SQLite3 library MANQUANTE"
ldconfig -p | grep -q libglib && check "GLib library" || check "GLib library MANQUANTE"

section "7. SERVICE SYSTEMD"

systemctl list-unit-files | grep -q midimind && check "Service midimind enregistré" || check "Service midimind NON enregistré"

if systemctl is-enabled --quiet midimind.service 2>/dev/null; then
    check "Service activé au démarrage"
else
    check "Service NON activé au démarrage"
fi

if systemctl is-active --quiet midimind.service 2>/dev/null; then
    check "Service midimind actif"
    
    # Test port 8080
    if netstat -tuln 2>/dev/null | grep -q ":8080"; then
        check "Port 8080 ouvert"
    else
        check "Port 8080 NON ouvert"
    fi
    
    # Test connexion
    if timeout 2 bash -c "echo > /dev/tcp/localhost/8080" 2>/dev/null; then
        check "Backend répond sur port 8080"
    else
        check "Backend NE répond PAS sur port 8080"
    fi
else
    warn "Service midimind non actif"
    info "Logs: sudo journalctl -u midimind -n 20"
fi

section "8. NGINX"

if systemctl is-active --quiet nginx 2>/dev/null; then
    check "Nginx actif"
    
    # Test port 8000
    if netstat -tuln 2>/dev/null | grep -q ":8000"; then
        check "Port 8000 ouvert"
    else
        check "Port 8000 NON ouvert"
    fi
    
    # Test site
    [ -f "/etc/nginx/sites-available/midimind" ] && check "Site Nginx configuré" || warn "Site Nginx non configuré"
    [ -L "/etc/nginx/sites-enabled/midimind" ] && check "Site Nginx activé" || warn "Site Nginx non activé"
else
    warn "Nginx non actif"
fi

section "9. FRONTEND"

if [ -d "/var/www/midimind" ]; then
    FILE_COUNT=$(find /var/www/midimind -type f 2>/dev/null | wc -l)
    if [ $FILE_COUNT -gt 5 ]; then
        check "Frontend déployé ($FILE_COUNT fichiers)"
    else
        check "Frontend incomplet ($FILE_COUNT fichiers)"
    fi
    
    [ -f "/var/www/midimind/index.html" ] && check "index.html" || check "index.html MANQUANT"
    [ -d "/var/www/midimind/js" ] && check "Dossier js/" || check "Dossier js/ MANQUANT"
    [ -d "/var/www/midimind/css" ] && check "Dossier css/" || warn "Dossier css/ manquant"
fi

section "10. TESTS FONCTIONNELS"

# Test base de données
if [ -f "/opt/midimind/data/midimind.db" ]; then
    TABLE_COUNT=$(sqlite3 /opt/midimind/data/midimind.db "SELECT COUNT(*) FROM sqlite_master WHERE type='table';" 2>/dev/null)
    if [ "$TABLE_COUNT" -ge 5 ]; then
        check "Base de données initialisée ($TABLE_COUNT tables)"
    else
        warn "Base de données partiellement initialisée ($TABLE_COUNT tables)"
    fi
fi

# Test ALSA
if aconnect -l >/dev/null 2>&1; then
    check "ALSA Sequencer accessible"
else
    warn "ALSA Sequencer non accessible"
fi

# Test interface web
IP=$(hostname -I | awk '{print $1}')
if curl -s -o /dev/null -w "%{http_code}" "http://localhost:8000" 2>/dev/null | grep -q "200\|301\|302"; then
    check "Interface web accessible"
    info "URL: http://$IP:8000"
else
    warn "Interface web non accessible"
fi

# ============================================================================
# RÉSUMÉ
# ============================================================================

section "RÉSUMÉ"

echo ""
if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}╔════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║         ✅ INSTALLATION PARFAITE - AUCUN PROBLÈME      ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════╝${NC}"
elif [ $ERRORS -eq 0 ]; then
    echo -e "${YELLOW}╔════════════════════════════════════════════════════════╗${NC}"
    echo -e "${YELLOW}║    ⚠️  INSTALLATION OK avec $WARNINGS avertissement(s)     ║${NC}"
    echo -e "${YELLOW}╚════════════════════════════════════════════════════════╝${NC}"
else
    echo -e "${RED}╔════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║      ❌ PROBLÈMES DÉTECTÉS: $ERRORS erreur(s)              ║${NC}"
    echo -e "${RED}╚════════════════════════════════════════════════════════╝${NC}"
fi

echo ""
echo -e "${BLUE}Statistiques:${NC}"
echo -e "  • Erreurs:        ${RED}$ERRORS${NC}"
echo -e "  • Avertissements: ${YELLOW}$WARNINGS${NC}"
echo ""

if [ $ERRORS -gt 0 ]; then
    echo -e "${YELLOW}Actions recommandées:${NC}"
    echo -e "  1. Vérifier les logs: ${GREEN}sudo journalctl -u midimind -n 50${NC}"
    echo -e "  2. Relancer install: ${GREEN}sudo ./scripts/install.sh${NC}"
    echo -e "  3. Corriger permissions: ${GREEN}sudo ./scripts/fix_permissions.sh${NC}"
    echo ""
    exit 1
fi

exit 0