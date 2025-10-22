#!/bin/bash
# ============================================================================
# Script: sync-frontend.sh
# Version: v1.1 - FIXED GIT PERMISSIONS
# Description: Synchronise le frontend depuis le repo vers www après git pull
# Usage: ./sync-frontend.sh
# ============================================================================
# CORRECTIONS v1.1:
# ✅ Ajout de la vérification et correction des permissions Git
# ✅ Fix du problème "insufficient permission for adding an object"
# ✅ Détection automatique des problèmes de permissions
# ============================================================================

# Couleurs pour les messages
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Chemins
REPO_DIR="$HOME/midiMind"
FRONTEND_SRC="$REPO_DIR/frontend"
WWW_DEST="/var/www/midimind"
BACKUP_DIR="$HOME/backups/midimind"

# ============================================================================
# FONCTIONS
# ============================================================================

print_header() {
    echo -e "${BLUE}"
    echo "╔════════════════════════════════════════════════════════╗"
    echo "║         MidiMind Frontend Sync Script v1.1            ║"
    echo "║              (Fixed Git Permissions)                  ║"
    echo "╚════════════════════════════════════════════════════════╝"
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

check_directories() {
    print_step "Checking directories..."
    
    if [ ! -d "$REPO_DIR" ]; then
        print_error "Repository directory not found: $REPO_DIR"
        exit 1
    fi
    
    if [ ! -d "$FRONTEND_SRC" ]; then
        print_error "Frontend source not found: $FRONTEND_SRC"
        exit 1
    fi
    
    if [ ! -d "$WWW_DEST" ]; then
        print_error "WWW destination not found: $WWW_DEST"
        print_step "Creating destination directory..."
        sudo mkdir -p "$WWW_DEST"
        sudo chown -R $USER:www-data "$WWW_DEST"
    fi
    
    print_success "All directories OK"
}

fix_git_permissions() {
    print_step "Checking Git repository permissions..."
    
    cd "$REPO_DIR" || exit 1
    
    # Vérifier si le dossier .git existe
    if [ ! -d ".git" ]; then
        print_error "Not a git repository: $REPO_DIR"
        exit 1
    fi
    
    # Vérifier les permissions actuelles
    GIT_OWNER=$(stat -c '%U' .git 2>/dev/null || stat -f '%Su' .git 2>/dev/null)
    CURRENT_USER=$(whoami)
    
    if [ "$GIT_OWNER" != "$CURRENT_USER" ]; then
        print_warning "Git directory owned by '$GIT_OWNER', current user is '$CURRENT_USER'"
        print_step "Fixing git permissions..."
        
        # Corriger les permissions de tout le dépôt
        sudo chown -R $CURRENT_USER:$CURRENT_USER "$REPO_DIR/.git"
        
        # Permissions spécifiques pour les objets Git
        if [ -d ".git/objects" ]; then
            sudo chmod -R u+rwX .git/objects
        fi
        
        # Permissions pour les refs
        if [ -d ".git/refs" ]; then
            sudo chmod -R u+rwX .git/refs
        fi
        
        # Permissions pour les hooks
        if [ -d ".git/hooks" ]; then
            sudo chmod -R u+rwX .git/hooks
        fi
        
        # Fichiers de configuration
        sudo chmod u+rw .git/config .git/HEAD 2>/dev/null
        
        print_success "Git permissions fixed"
    else
        print_success "Git permissions OK"
    fi
    
    # Vérifier l'état du dépôt
    if ! git status &>/dev/null; then
        print_error "Git repository is corrupted"
        print_step "Attempting to repair..."
        git fsck --full 2>&1 | grep -v "^Checking"
        
        if ! git status &>/dev/null; then
            print_error "Unable to repair git repository"
            print_warning "You may need to re-clone the repository"
            exit 1
        fi
    fi
}

create_backup() {
    print_step "Creating backup..."
    
    TIMESTAMP=$(date +%Y%m%d_%H%M%S)
    BACKUP_PATH="$BACKUP_DIR/backup_$TIMESTAMP"
    
    mkdir -p "$BACKUP_DIR"
    
    if [ -d "$WWW_DEST/js" ]; then
        cp -r "$WWW_DEST/js" "$BACKUP_PATH"
        print_success "Backup created: $BACKUP_PATH"
    else
        print_step "No existing files to backup"
    fi
}

git_pull() {
    print_step "Pulling latest changes from GitHub..."
    
    cd "$REPO_DIR" || exit 1
    
    # Sauvegarder la branche actuelle
    CURRENT_BRANCH=$(git branch --show-current)
    
    # Afficher l'état avant le pull
    echo -e "${BLUE}Current branch: $CURRENT_BRANCH${NC}"
    
    # Vérifier s'il y a des changements locaux
    if ! git diff-index --quiet HEAD --; then
        print_warning "Local changes detected"
        print_step "Stashing local changes..."
        git stash push -m "Auto-stash before sync $(date +%Y%m%d_%H%M%S)"
    fi
    
    # Pull avec retry
    PULL_SUCCESS=0
    for i in {1..3}; do
        if git pull origin "$CURRENT_BRANCH"; then
            PULL_SUCCESS=1
            break
        else
            print_warning "Pull attempt $i failed"
            if [ $i -lt 3 ]; then
                print_step "Retrying in 2 seconds..."
                sleep 2
            fi
        fi
    done
    
    if [ $PULL_SUCCESS -eq 0 ]; then
        print_error "Git pull failed after 3 attempts"
        print_step "Checking for network issues..."
        
        if ! ping -c 1 github.com &>/dev/null; then
            print_error "Cannot reach github.com - check your internet connection"
        fi
        
        exit 1
    fi
    
    print_success "Git pull successful"
    
    # Afficher les derniers commits
    echo -e "${BLUE}Last 3 commits:${NC}"
    git log --oneline -3 --color=always
    
    # Afficher les fichiers modifiés
    echo -e "${BLUE}Modified files:${NC}"
    git diff --name-status HEAD@{1} HEAD 2>/dev/null | head -n 10
}

sync_files() {
    print_step "Synchronizing files..."
    
    # Créer la structure de destination si nécessaire
    sudo mkdir -p "$WWW_DEST/js/"{core,models,views,controllers,utils,services}
    sudo mkdir -p "$WWW_DEST/css"
    sudo mkdir -p "$WWW_DEST/styles"
    
    # Synchroniser les fichiers JS
    echo "  → Syncing core files..."
    if [ -d "$FRONTEND_SRC/js/core" ]; then
        sudo rsync -av --delete "$FRONTEND_SRC/js/core/" "$WWW_DEST/js/core/"
    fi
    
    echo "  → Syncing models..."
    if [ -d "$FRONTEND_SRC/js/models" ]; then
        sudo rsync -av --delete "$FRONTEND_SRC/js/models/" "$WWW_DEST/js/models/"
    fi
    
    echo "  → Syncing views..."
    if [ -d "$FRONTEND_SRC/js/views" ]; then
        sudo rsync -av --delete "$FRONTEND_SRC/js/views/" "$WWW_DEST/js/views/"
    fi
    
    echo "  → Syncing controllers..."
    if [ -d "$FRONTEND_SRC/js/controllers" ]; then
        sudo rsync -av --delete "$FRONTEND_SRC/js/controllers/" "$WWW_DEST/js/controllers/"
    fi
    
    echo "  → Syncing utils..."
    if [ -d "$FRONTEND_SRC/js/utils" ]; then
        sudo rsync -av --delete "$FRONTEND_SRC/js/utils/" "$WWW_DEST/js/utils/"
    fi
    
    echo "  → Syncing services..."
    if [ -d "$FRONTEND_SRC/js/services" ]; then
        sudo rsync -av --delete "$FRONTEND_SRC/js/services/" "$WWW_DEST/js/services/"
    fi
    
    # Synchroniser index.html et main.js
    if [ -f "$FRONTEND_SRC/index.html" ]; then
        echo "  → Syncing index.html..."
        sudo cp "$FRONTEND_SRC/index.html" "$WWW_DEST/"
    fi
    
    if [ -f "$FRONTEND_SRC/js/main.js" ]; then
        echo "  → Syncing main.js..."
        sudo cp "$FRONTEND_SRC/js/main.js" "$WWW_DEST/js/"
    fi
    
    # Synchroniser CSS depuis les deux emplacements possibles
    if [ -d "$FRONTEND_SRC/css" ]; then
        echo "  → Syncing CSS files (from css/)..."
        sudo rsync -av --delete "$FRONTEND_SRC/css/" "$WWW_DEST/css/"
    fi
    
    if [ -d "$FRONTEND_SRC/styles" ]; then
        echo "  → Syncing CSS files (from styles/)..."
        sudo rsync -av --delete "$FRONTEND_SRC/styles/" "$WWW_DEST/styles/"
    fi
    
    # Synchroniser assets si présent
    if [ -d "$FRONTEND_SRC/assets" ]; then
        echo "  → Syncing assets..."
        sudo rsync -av --delete "$FRONTEND_SRC/assets/" "$WWW_DEST/assets/"
    fi
    
    print_success "Files synchronized"
}

set_permissions() {
    print_step "Setting permissions..."
    
    sudo chown -R $USER:www-data "$WWW_DEST"
    sudo chmod -R 755 "$WWW_DEST"
    
    # Permissions spécifiques pour les fichiers
    sudo find "$WWW_DEST" -type f -exec chmod 644 {} \;
    sudo find "$WWW_DEST" -type d -exec chmod 755 {} \;
    
    print_success "Permissions set"
}

show_summary() {
    echo ""
    echo -e "${GREEN}════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}✓ Synchronization completed successfully!${NC}"
    echo -e "${GREEN}════════════════════════════════════════════════════════${NC}"
    echo ""
    echo "Summary:"
    echo "  • Source: $FRONTEND_SRC"
    echo "  • Destination: $WWW_DEST"
    echo "  • Backup: $BACKUP_PATH"
    echo ""
    echo "Files synchronized:"
    if [ -d "$WWW_DEST/js" ]; then
        JS_FILES=$(find "$WWW_DEST/js" -type f -name "*.js" | wc -l)
        echo "  • JavaScript: $JS_FILES files"
    fi
    if [ -d "$WWW_DEST/css" ] || [ -d "$WWW_DEST/styles" ]; then
        CSS_FILES=$(find "$WWW_DEST" -type f -name "*.css" | wc -l)
        echo "  • CSS: $CSS_FILES files"
    fi
    echo ""
    echo "Next steps:"
    echo "  1. Open http://localhost:8000 in your browser"
    echo "  2. Press Ctrl+Shift+R to clear cache and reload"
    echo "  3. Check browser console (F12) for any errors"
    echo "  4. Check backend connection status"
    echo ""
}

clean_old_backups() {
    print_step "Cleaning old backups (keeping last 5)..."
    
    if [ -d "$BACKUP_DIR" ]; then
        cd "$BACKUP_DIR" || return
        ls -t | tail -n +6 | xargs -r rm -rf
        print_success "Old backups cleaned"
    fi
}

verify_sync() {
    print_step "Verifying synchronization..."
    
    # Vérifier que les fichiers essentiels sont présents
    ESSENTIAL_FILES=(
        "$WWW_DEST/index.html"
        "$WWW_DEST/js/main.js"
    )
    
    MISSING_FILES=0
    for file in "${ESSENTIAL_FILES[@]}"; do
        if [ ! -f "$file" ]; then
            print_warning "Missing essential file: $file"
            MISSING_FILES=$((MISSING_FILES + 1))
        fi
    done
    
    if [ $MISSING_FILES -gt 0 ]; then
        print_warning "$MISSING_FILES essential file(s) missing"
        print_step "Please check the synchronization manually"
    else
        print_success "All essential files present"
    fi
    
    # Vérifier la taille du dossier js
    if [ -d "$WWW_DEST/js" ]; then
        JS_SIZE=$(du -sh "$WWW_DEST/js" | cut -f1)
        echo "  • JavaScript folder size: $JS_SIZE"
    fi
}

# ============================================================================
# MAIN
# ============================================================================

main() {
    print_header
    
    # Vérifier si on est root
    if [ "$EUID" -eq 0 ]; then 
        print_error "Do not run this script as root"
        exit 1
    fi
    
    # Étapes
    check_directories
    fix_git_permissions      # 🔧 NOUVEAU: Corriger les permissions Git
    create_backup
    git_pull
    sync_files
    set_permissions
    verify_sync              # 🔧 NOUVEAU: Vérifier la synchronisation
    clean_old_backups
    show_summary
}

# Exécuter
main