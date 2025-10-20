#!/bin/bash
# ============================================================================
# Script: sync-frontend.sh
# Description: Synchronise le frontend depuis le repo vers www après git pull
# Usage: ./sync-frontend.sh
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
    echo "║         MidiMind Frontend Sync Script v1.0            ║"
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
    
    # Pull
    if git pull origin "$CURRENT_BRANCH"; then
        print_success "Git pull successful"
        
        # Afficher les derniers commits
        echo -e "${BLUE}Last 3 commits:${NC}"
        git log --oneline -3
    else
        print_error "Git pull failed"
        exit 1
    fi
}

sync_files() {
    print_step "Synchronizing files..."
    
    # Créer la structure de destination si nécessaire
    sudo mkdir -p "$WWW_DEST/js/"{core,models,views,controllers,utils,services}
    
    # Synchroniser les fichiers JS
    echo "  → Syncing core files..."
    sudo rsync -av --delete "$FRONTEND_SRC/js/core/" "$WWW_DEST/js/core/"
    
    echo "  → Syncing models..."
    sudo rsync -av --delete "$FRONTEND_SRC/js/models/" "$WWW_DEST/js/models/"
    
    echo "  → Syncing views..."
    sudo rsync -av --delete "$FRONTEND_SRC/js/views/" "$WWW_DEST/js/views/"
    
    echo "  → Syncing controllers..."
    sudo rsync -av --delete "$FRONTEND_SRC/js/controllers/" "$WWW_DEST/js/controllers/"
    
    echo "  → Syncing utils..."
    sudo rsync -av --delete "$FRONTEND_SRC/js/utils/" "$WWW_DEST/js/utils/"
    
    echo "  → Syncing services..."
    sudo rsync -av --delete "$FRONTEND_SRC/js/services/" "$WWW_DEST/js/services/"
    
    # Synchroniser index.html et main.js
    if [ -f "$FRONTEND_SRC/index.html" ]; then
        echo "  → Syncing index.html..."
        sudo cp "$FRONTEND_SRC/index.html" "$WWW_DEST/"
    fi
    
    if [ -f "$FRONTEND_SRC/js/main.js" ]; then
        echo "  → Syncing main.js..."
        sudo cp "$FRONTEND_SRC/js/main.js" "$WWW_DEST/js/"
    fi
    
    # Synchroniser CSS si présent
    if [ -d "$FRONTEND_SRC/css" ]; then
        echo "  → Syncing CSS files..."
        sudo rsync -av --delete "$FRONTEND_SRC/css/" "$WWW_DEST/css/"
    fi
    
    print_success "Files synchronized"
}

set_permissions() {
    print_step "Setting permissions..."
    
    sudo chown -R $USER:www-data "$WWW_DEST"
    sudo chmod -R 755 "$WWW_DEST"
    
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
    echo "Next steps:"
    echo "  1. Open http://localhost:8000 in your browser"
    echo "  2. Press Ctrl+Shift+R to clear cache"
    echo "  3. Check console for errors"
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
    create_backup
    git_pull
    sync_files
    set_permissions
    clean_old_backups
    show_summary
}

# Exécuter
main
