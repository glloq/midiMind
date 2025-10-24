#!/bin/bash
# ============================================================================
# Script: sync-frontend-complete.sh
# Version: v2.0 - STRUCTURE COMPLÈTE NON-PLATE
# Description: Synchronise TOUT le frontend depuis GitHub avec structure complète
# Usage: ./sync-frontend-complete.sh
# ============================================================================

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

# Chemins
REPO_DIR="$HOME/midiMind"
FRONTEND_SRC="$REPO_DIR/frontend"
WWW_DEST="/var/www/midimind"
BACKUP_DIR="$HOME/backups/midimind"

# ============================================================================
# FONCTIONS UTILITAIRES
# ============================================================================

print_header() {
    echo -e "${BLUE}"
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║     MidiMind Frontend Complete Sync v2.0                   ║"
    echo "║     Structure Non-Plate - Tous les fichiers               ║"
    echo "╚════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_step() {
    echo -e "${YELLOW}➜${NC} $1"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

# ============================================================================
# VÉRIFICATIONS
# ============================================================================

check_directories() {
    print_step "Vérification des répertoires..."
    
    if [ ! -d "$REPO_DIR" ]; then
        print_error "Répertoire repo introuvable: $REPO_DIR"
        exit 1
    fi
    
    if [ ! -d "$FRONTEND_SRC" ]; then
        print_error "Source frontend introuvable: $FRONTEND_SRC"
        exit 1
    fi
    
    if [ ! -d "$WWW_DEST" ]; then
        print_warning "Destination introuvable, création..."
        sudo mkdir -p "$WWW_DEST"
        sudo chown -R $USER:www-data "$WWW_DEST"
    fi
    
    print_success "Répertoires OK"
}

# ============================================================================
# GIT PERMISSIONS
# ============================================================================

fix_git_permissions() {
    print_step "Réparation des permissions Git..."
    
    cd "$REPO_DIR" || exit 1
    
    if [ ! -d ".git" ]; then
        print_error "Pas un dépôt Git: $REPO_DIR"
        exit 1
    fi
    
    CURRENT_USER=$(whoami)
    
    # Réparer ownership
    sudo chown -R $CURRENT_USER:$CURRENT_USER .git/
    
    # Permissions directories
    sudo find .git -type d -exec chmod 755 {} \;
    
    # Permissions fichiers
    sudo find .git -type f -exec chmod 644 {} \;
    
    # Hooks exécutables
    if [ -d ".git/hooks" ]; then
        sudo chmod -R 755 .git/hooks/
    fi
    
    # Supprimer locks
    sudo rm -f .git/index.lock .git/HEAD.lock .git/refs/heads/*.lock 2>/dev/null
    
    # Vérifier état
    if git status &>/dev/null; then
        print_success "Permissions Git réparées"
    else
        print_error "Problème avec le dépôt Git"
        exit 1
    fi
}

# ============================================================================
# BACKUP
# ============================================================================

create_backup() {
    print_step "Création du backup..."
    
    TIMESTAMP=$(date +%Y%m%d_%H%M%S)
    BACKUP_PATH="$BACKUP_DIR/backup_$TIMESTAMP"
    
    mkdir -p "$BACKUP_DIR"
    
    if [ -d "$WWW_DEST/js" ] || [ -d "$WWW_DEST/styles" ]; then
        mkdir -p "$BACKUP_PATH"
        [ -d "$WWW_DEST/js" ] && cp -r "$WWW_DEST/js" "$BACKUP_PATH/"
        [ -d "$WWW_DEST/styles" ] && cp -r "$WWW_DEST/styles" "$BACKUP_PATH/"
        [ -f "$WWW_DEST/index.html" ] && cp "$WWW_DEST/index.html" "$BACKUP_PATH/"
        print_success "Backup créé: $BACKUP_PATH"
    else
        print_info "Aucun fichier à sauvegarder"
    fi
}

# ============================================================================
# GIT PULL
# ============================================================================

git_pull() {
    print_step "Pull des dernières modifications GitHub..."
    
    cd "$REPO_DIR" || exit 1
    
    CURRENT_BRANCH=$(git branch --show-current)
    echo -e "${BLUE}Branche: $CURRENT_BRANCH${NC}"
    
    # Stash changements locaux si nécessaire
    if ! git diff-index --quiet HEAD -- 2>/dev/null; then
        print_warning "Changements locaux détectés"
        git status --short
        
        if git stash push -m "Auto-stash $(date +%Y%m%d_%H%M%S)"; then
            print_success "Changements stashés"
        fi
    fi
    
    # Pull avec retry
    PULL_SUCCESS=0
    for i in {1..3}; do
        echo -e "${CYAN}Tentative $i/3...${NC}"
        
        if git pull origin "$CURRENT_BRANCH" 2>&1; then
            PULL_SUCCESS=1
            break
        else
            print_warning "Échec tentative $i"
            
            if [ $i -lt 3 ]; then
                sleep 2
                fix_git_permissions
            fi
        fi
    done
    
    if [ $PULL_SUCCESS -eq 0 ]; then
        print_error "Échec du git pull après 3 tentatives"
        exit 1
    fi
    
    print_success "Git pull réussi"
    
    # Derniers commits
    echo -e "${BLUE}Derniers commits:${NC}"
    git log --oneline -3 --color=always
}

# ============================================================================
# SYNCHRONISATION COMPLÈTE
# ============================================================================

sync_complete_structure() {
    print_step "Synchronisation de la structure complète..."
    
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}   CRÉATION DE LA STRUCTURE COMPLÈTE${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    # Créer TOUTE la structure de dossiers
    print_info "Création des dossiers..."
    
    # Structure JS complète
    sudo mkdir -p "$WWW_DEST/js"/{audio,config,controllers,core,editor,models,monitoring,services,ui,utils,views}
    
    # Sous-dossiers editor
    sudo mkdir -p "$WWW_DEST/js/editor"/{components,core,interaction,renderers,utils}
    
    # Sous-dossier views/components
    sudo mkdir -p "$WWW_DEST/js/views/components"
    
    # Dossier styles
    sudo mkdir -p "$WWW_DEST/styles"
    
    print_success "Structure créée"
    
    # ========================================================================
    # SYNCHRONISATION PAR CATÉGORIE
    # ========================================================================
    
    TOTAL_FILES=0
    
    # index.html
    if [ -f "$FRONTEND_SRC/index.html" ]; then
        echo -e "${CYAN}→${NC} index.html"
        sudo cp "$FRONTEND_SRC/index.html" "$WWW_DEST/"
        ((TOTAL_FILES++))
    fi
    
    # main.js
    if [ -f "$FRONTEND_SRC/js/main.js" ]; then
        echo -e "${CYAN}→${NC} main.js"
        sudo cp "$FRONTEND_SRC/js/main.js" "$WWW_DEST/js/"
        ((TOTAL_FILES++))
    fi
    
    # Audio
    if [ -d "$FRONTEND_SRC/js/audio" ]; then
        echo -e "${CYAN}→${NC} audio/"
        COUNT=$(find "$FRONTEND_SRC/js/audio" -type f | wc -l)
        sudo rsync -a --delete "$FRONTEND_SRC/js/audio/" "$WWW_DEST/js/audio/"
        TOTAL_FILES=$((TOTAL_FILES + COUNT))
        echo "  ✓ $COUNT fichiers"
    fi
    
    # Config
    if [ -d "$FRONTEND_SRC/js/config" ]; then
        echo -e "${CYAN}→${NC} config/"
        COUNT=$(find "$FRONTEND_SRC/js/config" -type f | wc -l)
        sudo rsync -a --delete "$FRONTEND_SRC/js/config/" "$WWW_DEST/js/config/"
        TOTAL_FILES=$((TOTAL_FILES + COUNT))
        echo "  ✓ $COUNT fichiers"
    fi
    
    # Controllers
    if [ -d "$FRONTEND_SRC/js/controllers" ]; then
        echo -e "${CYAN}→${NC} controllers/"
        COUNT=$(find "$FRONTEND_SRC/js/controllers" -type f | wc -l)
        sudo rsync -a --delete "$FRONTEND_SRC/js/controllers/" "$WWW_DEST/js/controllers/"
        TOTAL_FILES=$((TOTAL_FILES + COUNT))
        echo "  ✓ $COUNT fichiers"
    fi
    
    # Core
    if [ -d "$FRONTEND_SRC/js/core" ]; then
        echo -e "${CYAN}→${NC} core/"
        COUNT=$(find "$FRONTEND_SRC/js/core" -type f | wc -l)
        sudo rsync -a --delete "$FRONTEND_SRC/js/core/" "$WWW_DEST/js/core/"
        TOTAL_FILES=$((TOTAL_FILES + COUNT))
        echo "  ✓ $COUNT fichiers"
    fi
    
    # Editor (STRUCTURE COMPLÈTE)
    if [ -d "$FRONTEND_SRC/js/editor" ]; then
        echo -e "${CYAN}→${NC} editor/ (STRUCTURE COMPLÈTE)"
        
        # Sous-dossiers editor
        for subdir in components core interaction renderers utils; do
            if [ -d "$FRONTEND_SRC/js/editor/$subdir" ]; then
                COUNT=$(find "$FRONTEND_SRC/js/editor/$subdir" -type f | wc -l)
                sudo rsync -a --delete "$FRONTEND_SRC/js/editor/$subdir/" "$WWW_DEST/js/editor/$subdir/"
                TOTAL_FILES=$((TOTAL_FILES + COUNT))
                echo "  ✓ editor/$subdir: $COUNT fichiers"
            fi
        done
    fi
    
    # Models
    if [ -d "$FRONTEND_SRC/js/models" ]; then
        echo -e "${CYAN}→${NC} models/"
        COUNT=$(find "$FRONTEND_SRC/js/models" -type f | wc -l)
        sudo rsync -a --delete "$FRONTEND_SRC/js/models/" "$WWW_DEST/js/models/"
        TOTAL_FILES=$((TOTAL_FILES + COUNT))
        echo "  ✓ $COUNT fichiers"
    fi
    
    # Monitoring
    if [ -d "$FRONTEND_SRC/js/monitoring" ]; then
        echo -e "${CYAN}→${NC} monitoring/"
        COUNT=$(find "$FRONTEND_SRC/js/monitoring" -type f | wc -l)
        sudo rsync -a --delete "$FRONTEND_SRC/js/monitoring/" "$WWW_DEST/js/monitoring/"
        TOTAL_FILES=$((TOTAL_FILES + COUNT))
        echo "  ✓ $COUNT fichiers"
    fi
    
    # Services
    if [ -d "$FRONTEND_SRC/js/services" ]; then
        echo -e "${CYAN}→${NC} services/"
        COUNT=$(find "$FRONTEND_SRC/js/services" -type f | wc -l)
        sudo rsync -a --delete "$FRONTEND_SRC/js/services/" "$WWW_DEST/js/services/"
        TOTAL_FILES=$((TOTAL_FILES + COUNT))
        echo "  ✓ $COUNT fichiers"
    fi
    
    # UI
    if [ -d "$FRONTEND_SRC/js/ui" ]; then
        echo -e "${CYAN}→${NC} ui/"
        COUNT=$(find "$FRONTEND_SRC/js/ui" -type f | wc -l)
        sudo rsync -a --delete "$FRONTEND_SRC/js/ui/" "$WWW_DEST/js/ui/"
        TOTAL_FILES=$((TOTAL_FILES + COUNT))
        echo "  ✓ $COUNT fichiers"
    fi
    
    # Utils
    if [ -d "$FRONTEND_SRC/js/utils" ]; then
        echo -e "${CYAN}→${NC} utils/"
        COUNT=$(find "$FRONTEND_SRC/js/utils" -type f | wc -l)
        sudo rsync -a --delete "$FRONTEND_SRC/js/utils/" "$WWW_DEST/js/utils/"
        TOTAL_FILES=$((TOTAL_FILES + COUNT))
        echo "  ✓ $COUNT fichiers"
    fi
    
    # Views (avec components)
    if [ -d "$FRONTEND_SRC/js/views" ]; then
        echo -e "${CYAN}→${NC} views/"
        
        # Fichiers racine views
        COUNT_ROOT=$(find "$FRONTEND_SRC/js/views" -maxdepth 1 -type f | wc -l)
        sudo rsync -a --delete --exclude='components/' "$FRONTEND_SRC/js/views/" "$WWW_DEST/js/views/"
        TOTAL_FILES=$((TOTAL_FILES + COUNT_ROOT))
        echo "  ✓ views racine: $COUNT_ROOT fichiers"
        
        # Sous-dossier components
        if [ -d "$FRONTEND_SRC/js/views/components" ]; then
            COUNT_COMP=$(find "$FRONTEND_SRC/js/views/components" -type f | wc -l)
            sudo rsync -a --delete "$FRONTEND_SRC/js/views/components/" "$WWW_DEST/js/views/components/"
            TOTAL_FILES=$((TOTAL_FILES + COUNT_COMP))
            echo "  ✓ views/components: $COUNT_COMP fichiers"
        fi
    fi
    
    # Styles (tous les CSS)
    if [ -d "$FRONTEND_SRC/styles" ]; then
        echo -e "${CYAN}→${NC} styles/"
        COUNT=$(find "$FRONTEND_SRC/styles" -type f -name "*.css" | wc -l)
        sudo rsync -a --delete "$FRONTEND_SRC/styles/" "$WWW_DEST/styles/"
        TOTAL_FILES=$((TOTAL_FILES + COUNT))
        echo "  ✓ $COUNT fichiers CSS"
    fi
    
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    print_success "Synchronisation complète: $TOTAL_FILES fichiers"
}

# ============================================================================
# PERMISSIONS
# ============================================================================

set_permissions() {
    print_step "Configuration des permissions..."
    
    sudo chown -R $USER:www-data "$WWW_DEST"
    sudo find "$WWW_DEST" -type d -exec chmod 755 {} \;
    sudo find "$WWW_DEST" -type f -exec chmod 644 {} \;
    
    print_success "Permissions configurées"
}

# ============================================================================
# VÉRIFICATION
# ============================================================================

verify_structure() {
    print_step "Vérification de la structure..."
    
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    # Fichiers essentiels
    CRITICAL_FILES=(
        "$WWW_DEST/index.html"
        "$WWW_DEST/js/main.js"
        "$WWW_DEST/js/core/Application.js"
        "$WWW_DEST/js/editor/core/RenderEngine.js"
        "$WWW_DEST/js/editor/renderers/PianoRollRenderer.js"
    )
    
    MISSING=0
    for file in "${CRITICAL_FILES[@]}"; do
        if [ -f "$file" ]; then
            echo "  ✓ $(basename $file)"
        else
            echo "  ✗ MANQUANT: $(basename $file)"
            ((MISSING++))
        fi
    done
    
    if [ $MISSING -gt 0 ]; then
        print_warning "$MISSING fichier(s) critique(s) manquant(s)"
    else
        print_success "Tous les fichiers critiques présents"
    fi
    
    # Statistiques par dossier
    echo ""
    echo "Statistiques:"
    
    if [ -d "$WWW_DEST/js" ]; then
        for dir in audio config controllers core editor models monitoring services ui utils views; do
            if [ -d "$WWW_DEST/js/$dir" ]; then
                COUNT=$(find "$WWW_DEST/js/$dir" -type f -name "*.js" | wc -l)
                printf "  • %-15s %3d fichiers\n" "$dir:" "$COUNT"
            fi
        done
    fi
    
    if [ -d "$WWW_DEST/js/editor" ]; then
        echo ""
        echo "Editor sous-structure:"
        for subdir in components core interaction renderers utils; do
            if [ -d "$WWW_DEST/js/editor/$subdir" ]; then
                COUNT=$(find "$WWW_DEST/js/editor/$subdir" -type f -name "*.js" | wc -l)
                printf "  • editor/%-12s %3d fichiers\n" "$subdir:" "$COUNT"
            fi
        done
    fi
    
    if [ -d "$WWW_DEST/styles" ]; then
        CSS_COUNT=$(find "$WWW_DEST/styles" -type f -name "*.css" | wc -l)
        echo ""
        printf "  • %-15s %3d fichiers\n" "styles:" "$CSS_COUNT"
    fi
    
    # Taille totale
    if [ -d "$WWW_DEST/js" ]; then
        SIZE=$(du -sh "$WWW_DEST/js" | cut -f1)
        echo ""
        echo "  • Taille JS totale: $SIZE"
    fi
    
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

# ============================================================================
# NETTOYAGE
# ============================================================================

clean_old_backups() {
    print_step "Nettoyage des anciens backups (garde les 5 derniers)..."
    
    if [ -d "$BACKUP_DIR" ]; then
        cd "$BACKUP_DIR" || return
        BACKUP_COUNT=$(ls -1 | wc -l)
        
        if [ $BACKUP_COUNT -gt 5 ]; then
            ls -t | tail -n +6 | xargs -r rm -rf
            print_success "Backups nettoyés"
        else
            print_info "Pas besoin de nettoyage ($BACKUP_COUNT backups)"
        fi
    fi
}

# ============================================================================
# RÉSUMÉ
# ============================================================================

show_summary() {
    echo ""
    echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}✓ SYNCHRONISATION COMPLÈTE RÉUSSIE !${NC}"
    echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo "Détails:"
    echo "  • Source      : $FRONTEND_SRC"
    echo "  • Destination : $WWW_DEST"
    if [ -n "$BACKUP_PATH" ]; then
        echo "  • Backup      : $BACKUP_PATH"
    fi
    echo ""
    echo "Prochaines étapes:"
    echo "  1. Ouvrir http://localhost:8000"
    echo "  2. Ctrl+Shift+R pour vider le cache"
    echo "  3. F12 pour vérifier la console"
    echo "  4. Vérifier la connexion backend"
    echo ""
}

# ============================================================================
# MAIN
# ============================================================================

main() {
    print_header
    
    # Vérifier qu'on n'est pas root
    if [ "$EUID" -eq 0 ]; then 
        print_error "Ne pas exécuter en root"
        exit 1
    fi
    
    # Exécution séquentielle
    check_directories
    fix_git_permissions
    create_backup
    git_pull
    sync_complete_structure
    set_permissions
    verify_structure
    clean_old_backups
    show_summary
    
    echo -e "${GREEN}✓ Script terminé avec succès${NC}"
}

# Exécution
main