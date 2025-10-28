#!/bin/bash
# ============================================================================
# Script: update_and_build.sh
# Version: v2.0 - MISE À JOUR COMPLÈTE
# Description: Met à jour TOUT le projet depuis GitHub et redéploie
# Repo: https://github.com/glloq/midiMind
# ============================================================================
# CORRECTIONS v2.0:
# ✅ Backend + Frontend + Migrations SQL + Service systemd
# ✅ Correction automatique des permissions
# ✅ Vérification post-installation
# ✅ Utilise make install pour tout installer
# ============================================================================

set -e

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
NC='\033[0m'

# Variables
REPO_DIR="$(pwd)"
BACKEND_DIR="$REPO_DIR/backend"
FRONTEND_DIR="$REPO_DIR/frontend"
BUILD_DIR="$BACKEND_DIR/build"
INSTALL_DIR="/opt/midimind"
WEB_DIR="/var/www/midimind"
REAL_USER="${SUDO_USER:-$USER}"

# Fonctions
log() { echo -e "${BLUE}[$(date '+%H:%M:%S')]${NC} $1"; }
success() { echo -e "${GREEN}✓${NC} $1"; }
error() { echo -e "${RED}✗ ERREUR:${NC} $1"; exit 1; }
warning() { echo -e "${YELLOW}⚠${NC} $1"; }
info() { echo -e "${MAGENTA}ℹ${NC} $1"; }

print_header() {
    echo ""
    echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║   MidiMind Complete Update & Build v2.0               ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

# Vérifier environnement
check_environment() {
    log "🔍 Vérification de l'environnement..."
    
    [ -d ".git" ] || error "Pas un repo git. Exécutez depuis la racine du projet."
    [ -d "backend" ] || error "Dossier backend/ introuvable"
    [ -d "frontend" ] || error "Dossier frontend/ introuvable"
    [ -d "scripts" ] || warning "Dossier scripts/ introuvable"
    
    command -v git >/dev/null 2>&1 || error "git non installé"
    command -v cmake >/dev/null 2>&1 || error "cmake non installé"
    command -v make >/dev/null 2>&1 || error "make non installé"
    
    success "Environnement OK"
}

# Réparer permissions Git
fix_git_permissions() {
    log "🔧 Vérification permissions Git..."
    
    if [ ! -w ".git/FETCH_HEAD" ] 2>/dev/null; then
        warning "Problème de permissions Git"
        sudo chown -R $REAL_USER:$REAL_USER .git/
        sudo chmod -R u+rwX .git/
        success "Permissions réparées"
    fi
}

# Pull depuis GitHub
git_pull_smart() {
    log "📥 Récupération des modifications depuis GitHub..."
    
    CURRENT_BRANCH=$(git branch --show-current)
    [ -z "$CURRENT_BRANCH" ] && error "Impossible de détecter la branche"
    
    info "Branche: $CURRENT_BRANCH"
    
    # Sauvegarder changements locaux
    if ! git diff-index --quiet HEAD -- 2>/dev/null; then
        warning "Changements locaux détectés"
        git status --short
        git stash push -m "Auto-stash $(date +%Y%m%d_%H%M%S)" || error "Échec stash"
        success "Changements sauvegardés"
    fi
    
    # Pull avec retry
    PULL_SUCCESS=0
    for i in {1..3}; do
        info "Tentative $i/3..."
        if git pull origin "$CURRENT_BRANCH" 2>&1; then
            PULL_SUCCESS=1
            break
        else
            [ $i -lt 3 ] && sleep 2
        fi
    done
    
    [ $PULL_SUCCESS -eq 0 ] && error "Échec git pull après 3 tentatives"
    
    success "Modifications récupérées"
    echo -e "${BLUE}Derniers commits:${NC}"
    git log --oneline -3 --color=always
}

# Compiler backend
compile_backend() {
    log "🔨 Compilation du backend..."
    
    # Nettoyer
    [ -d "$BUILD_DIR" ] && rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # CMake
    info "Configuration CMake..."
    cmake .. -DCMAKE_BUILD_TYPE=Release || error "Échec CMake"
    
    # Make
    info "Compilation ($(nproc) cœurs)..."
    make -j$(nproc) || error "Échec compilation"
    
    # Vérifier binaire
    [ -f "bin/midimind" ] || error "Binaire bin/midimind non créé"
    
    BINARY_SIZE=$(du -h bin/midimind | cut -f1)
    success "Compilation terminée ($BINARY_SIZE)"
}

# Arrêter service
stop_service() {
    log "⏸️  Arrêt du service midimind..."
    
    if sudo systemctl is-active --quiet midimind.service 2>/dev/null; then
        sudo systemctl stop midimind.service
        sleep 1
        success "Service arrêté"
    else
        info "Service déjà arrêté"
    fi
}

# Installer backend via make install
install_backend() {
    log "📦 Installation backend (binaire + migrations + service)..."
    
    cd "$BUILD_DIR"
    
    # Sauvegarder ancien binaire
    if [ -f "$INSTALL_DIR/bin/midimind" ]; then
        sudo cp "$INSTALL_DIR/bin/midimind" \
                "$INSTALL_DIR/bin/midimind.backup.$(date +%Y%m%d_%H%M%S)"
        info "Ancien binaire sauvegardé"
    fi
    
    # make install (installe binaire + migrations + service systemd)
    sudo make install 2>&1 | grep -v "^--" || warning "make install a échoué partiellement"
    
    # Corriger permissions (make install crée les fichiers en root)
    sudo chown -R "$REAL_USER:$REAL_USER" "$INSTALL_DIR"
    sudo chmod -R 755 "$INSTALL_DIR"
    
    success "Backend installé"
}

# Copier migrations SQL (sécurité supplémentaire)
update_migrations() {
    log "🗄️  Mise à jour des migrations SQL..."
    
    if [ -d "$BACKEND_DIR/data/migrations" ]; then
        sudo mkdir -p "$INSTALL_DIR/data/migrations"
        sudo cp -f "$BACKEND_DIR/data/migrations/"*.sql "$INSTALL_DIR/data/migrations/" 2>/dev/null || true
        
        SQL_COUNT=$(ls -1 "$INSTALL_DIR/data/migrations/"*.sql 2>/dev/null | wc -l)
        if [ $SQL_COUNT -gt 0 ]; then
            sudo chown -R "$REAL_USER:$REAL_USER" "$INSTALL_DIR/data"
            success "Migrations SQL mises à jour ($SQL_COUNT fichiers)"
        else
            warning "Aucune migration SQL trouvée"
        fi
    fi
}

# Mettre à jour frontend
update_frontend() {
    log "🌐 Mise à jour du frontend..."
    
    if [ ! -d "$FRONTEND_DIR" ]; then
        warning "Frontend introuvable, ignoré"
        return
    fi
    
    # Sauvegarder ancien frontend (optionnel)
    if [ -d "$WEB_DIR" ]; then
        BACKUP_DIR="/tmp/midimind_frontend_backup_$(date +%Y%m%d_%H%M%S)"
        sudo cp -r "$WEB_DIR" "$BACKUP_DIR" 2>/dev/null || true
    fi
    
    # Copier nouveau frontend
    sudo mkdir -p "$WEB_DIR"
    sudo rm -rf "$WEB_DIR"/*
    sudo cp -r "$FRONTEND_DIR"/* "$WEB_DIR/" || error "Échec copie frontend"
    
    # Permissions
    sudo chown -R www-data:www-data "$WEB_DIR"
    sudo find "$WEB_DIR" -type f -exec chmod 644 {} \;
    sudo find "$WEB_DIR" -type d -exec chmod 755 {} \;
    
    FILE_COUNT=$(find "$WEB_DIR" -type f 2>/dev/null | wc -l)
    success "Frontend mis à jour ($FILE_COUNT fichiers)"
}

# Supprimer ancienne base de données si migrations modifiées
check_and_reset_database() {
    log "🔍 Vérification de la base de données..."
    
    # Si les migrations SQL ont été modifiées dans ce commit
    if git diff HEAD@{1} HEAD --name-only 2>/dev/null | grep -q "migrations/.*\.sql"; then
        warning "Migrations SQL modifiées détectées"
        
        if [ -f "$INSTALL_DIR/data/midimind.db" ]; then
            # Sauvegarder l'ancienne DB
            BACKUP_DB="$INSTALL_DIR/data/midimind.db.backup.$(date +%Y%m%d_%H%M%S)"
            sudo cp "$INSTALL_DIR/data/midimind.db" "$BACKUP_DB"
            info "DB sauvegardée: $BACKUP_DB"
            
            # Supprimer pour forcer réinitialisation
            sudo rm "$INSTALL_DIR/data/midimind.db"
            success "Ancienne DB supprimée (sera recréée au démarrage)"
        fi
    else
        info "Migrations SQL inchangées, DB conservée"
    fi
}

# Recharger et démarrer services
start_services() {
    log "🚀 Redémarrage des services..."
    
    # Reload systemd
    sudo systemctl daemon-reload
    
    # Redémarrer midimind
    sudo systemctl start midimind.service || error "Échec démarrage midimind"
    sleep 3
    
    if sudo systemctl is-active --quiet midimind.service; then
        success "Service midimind actif"
        
        # Vérifier port 8080
        if sudo netstat -tuln 2>/dev/null | grep -q ":8080"; then
            success "Port 8080 ouvert"
        else
            warning "Port 8080 non ouvert - vérifier logs"
        fi
    else
        error "Service midimind non actif - voir: sudo journalctl -u midimind -n 50"
    fi
    
    # Vérifier/recharger Nginx
    if sudo systemctl is-active --quiet nginx 2>/dev/null; then
        sudo nginx -t 2>&1 | grep -q "successful" && sudo systemctl reload nginx
        success "Nginx rechargé"
    fi
}

# Vérification post-installation
verify_installation() {
    log "✅ Vérification post-installation..."
    
    ERRORS=0
    
    # Binaire
    [ -f "$INSTALL_DIR/bin/midimind" ] && success "Binaire OK" || { warning "Binaire manquant"; ((ERRORS++)); }
    
    # Migrations
    SQL_COUNT=$(ls -1 "$INSTALL_DIR/data/migrations/"*.sql 2>/dev/null | wc -l)
    [ $SQL_COUNT -ge 2 ] && success "Migrations OK ($SQL_COUNT)" || { warning "Migrations manquantes"; ((ERRORS++)); }
    
    # Base de données
    if [ -f "$INSTALL_DIR/data/midimind.db" ]; then
        TABLE_COUNT=$(sqlite3 "$INSTALL_DIR/data/midimind.db" "SELECT COUNT(*) FROM sqlite_master WHERE type='table';" 2>/dev/null || echo "0")
        [ "$TABLE_COUNT" -ge 5 ] && success "Base de données OK ($TABLE_COUNT tables)" || { warning "DB incomplète ($TABLE_COUNT tables)"; ((ERRORS++)); }
    else
        warning "Base de données pas encore créée"
    fi
    
    # Service
    sudo systemctl is-active --quiet midimind.service && success "Service actif" || { warning "Service non actif"; ((ERRORS++)); }
    
    # Frontend
    [ -f "$WEB_DIR/index.html" ] && success "Frontend OK" || { warning "Frontend manquant"; ((ERRORS++)); }
    
    # Permissions
    OWNER=$(stat -c '%U' "$INSTALL_DIR" 2>/dev/null)
    [ "$OWNER" = "$REAL_USER" ] && success "Permissions OK" || { warning "Permissions incorrectes (owner: $OWNER)"; ((ERRORS++)); }
    
    if [ $ERRORS -gt 0 ]; then
        warning "Vérification terminée avec $ERRORS avertissement(s)"
        info "Logs: sudo journalctl -u midimind -n 50"
    fi
}

# Résumé final
show_summary() {
    local IP=$(hostname -I | awk '{print $1}')
    
    echo ""
    echo -e "${GREEN}╔════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║         ✅ MISE À JOUR COMPLÈTE TERMINÉE ✅            ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${BLUE}Informations:${NC}"
    echo -e "  • Branche:    ${GREEN}$(git branch --show-current)${NC}"
    echo -e "  • Commit:     ${GREEN}$(git log --oneline -1 | cut -d' ' -f1)${NC}"
    echo -e "  • Backend:    ${GREEN}$INSTALL_DIR/bin/midimind${NC}"
    echo -e "  • Frontend:   ${GREEN}$WEB_DIR${NC}"
    echo -e "  • Interface:  ${GREEN}http://$IP:8000${NC}"
    echo ""
    echo -e "${BLUE}Commandes utiles:${NC}"
    echo -e "  • Status:     ${GREEN}sudo systemctl status midimind${NC}"
    echo -e "  • Logs:       ${GREEN}sudo journalctl -u midimind -f${NC}"
    echo -e "  • Restart:    ${GREEN}sudo systemctl restart midimind${NC}"
    echo -e "  • DB tables:  ${GREEN}sqlite3 $INSTALL_DIR/data/midimind.db '.tables'${NC}"
    echo ""
}

# ============================================================================
# MAIN
# ============================================================================

main() {
    print_header
    
    check_environment
    fix_git_permissions
    git_pull_smart
    compile_backend
    stop_service
    install_backend
    update_migrations
    update_frontend
    check_and_reset_database
    start_services
    verify_installation
    show_summary
}

main