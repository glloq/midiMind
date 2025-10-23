#!/bin/bash
# ============================================================================
# Script: sync-frontend.sh
# Version: v1.3 - FIX EDITOR FOLDER SYNC
# Description: Synchronise le frontend depuis le repo vers www après git pull
# Usage: ./sync-frontend.sh
# ============================================================================
# CORRECTIONS v1.3:
# ✅ AJOUT de la synchronisation du dossier js/editor/ (CRITIQUE)
# ✅ Correction de la structure complète
# ============================================================================

# Couleurs pour les messages
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
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
    echo "║         MidiMind Frontend Sync Script v1.3            ║"
    echo "║              (Editor Folder Fix)                      ║"
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

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
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

fix_git_permissions_complete() {
    print_step "Performing COMPLETE Git permissions repair..."
    
    cd "$REPO_DIR" || exit 1
    
    # Vérifier si le dossier .git existe
    if [ ! -d ".git" ]; then
        print_error "Not a git repository: $REPO_DIR"
        exit 1
    fi
    
    echo -e "${MAGENTA}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${MAGENTA}    RÉPARATION COMPLÈTE DES PERMISSIONS GIT${NC}"
    echo -e "${MAGENTA}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    CURRENT_USER=$(whoami)
    
    # 1. Réparer TOUT le dossier .git récursivement
    print_info "Fixing ownership of entire .git directory..."
    sudo chown -R $CURRENT_USER:$CURRENT_USER .git/
    
    # 2. Réparer les permissions des dossiers
    print_info "Fixing directory permissions..."
    sudo find .git -type d -exec chmod 755 {} \;
    
    # 3. Réparer les permissions des fichiers
    print_info "Fixing file permissions..."
    sudo find .git -type f -exec chmod 644 {} \;
    
    # 4. Fichiers spécifiques qui doivent être exécutables
    print_info "Setting executable permissions for hooks..."
    if [ -d ".git/hooks" ]; then
        sudo chmod -R 755 .git/hooks/
    fi
    
    # 5. Réparer spécifiquement les fichiers de contrôle Git critiques
    print_info "Fixing critical Git control files..."
    CRITICAL_FILES=(
        ".git/HEAD"
        ".git/FETCH_HEAD"
        ".git/ORIG_HEAD"
        ".git/config"
        ".git/description"
        ".git/index"
        ".git/packed-refs"
    )
    
    for file in "${CRITICAL_FILES[@]}"; do
        if [ -f "$file" ]; then
            sudo chown $CURRENT_USER:$CURRENT_USER "$file"
            sudo chmod 644 "$file"
            echo "    ✓ Fixed: $file"
        fi
    done
    
    # 6. Réparer les dossiers critiques
    print_info "Fixing critical Git directories..."
    CRITICAL_DIRS=(
        ".git/objects"
        ".git/refs"
        ".git/refs/heads"
        ".git/refs/remotes"
        ".git/refs/tags"
        ".git/logs"
        ".git/logs/refs"
        ".git/info"
        ".git/hooks"
    )
    
    for dir in "${CRITICAL_DIRS[@]}"; do
        if [ -d "$dir" ]; then
            sudo chown -R $CURRENT_USER:$CURRENT_USER "$dir"
            sudo chmod -R u+rwX "$dir"
            echo "    ✓ Fixed: $dir"
        fi
    done
    
    # 7. Nettoyer les locks s'ils existent
    print_info "Removing stale lock files..."
    sudo rm -f .git/index.lock .git/HEAD.lock .git/refs/heads/*.lock 2>/dev/null
    
    # 8. Vérifier l'état final
    print_step "Verifying Git repository state..."
    
    if git status &>/dev/null; then
        print_success "Git repository is healthy"
    else
        print_warning "Git repository may have issues, attempting repair..."
        git fsck --full 2>&1 | grep -E "(error|warning)" || true
        
        if ! git status &>/dev/null; then
            print_error "Unable to repair git repository"
            print_info "You may need to:"
            echo "  1. Run: cd $REPO_DIR && git reset --hard HEAD"
            echo "  2. Or re-clone the repository"
            exit 1
        fi
    fi
    
    echo -e "${MAGENTA}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    print_success "Git permissions completely repaired"
    echo -e "${MAGENTA}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
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
        print_info "No existing files to backup"
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
    if ! git diff-index --quiet HEAD -- 2>/dev/null; then
        print_warning "Local changes detected"
        
        # Lister les fichiers modifiés
        echo -e "${BLUE}Modified files:${NC}"
        git status --short
        
        print_step "Stashing local changes..."
        if git stash push -m "Auto-stash before sync $(date +%Y%m%d_%H%M%S)"; then
            print_success "Local changes stashed"
        else
            print_error "Failed to stash changes"
            print_info "Attempting to continue anyway..."
        fi
    fi
    
    # Pull avec retry
    PULL_SUCCESS=0
    for i in {1..3}; do
        echo -e "${BLUE}Pull attempt $i/3...${NC}"
        
        if git pull origin "$CURRENT_BRANCH" 2>&1; then
            PULL_SUCCESS=1
            break
        else
            print_warning "Pull attempt $i failed"
            
            if [ $i -lt 3 ]; then
                print_step "Checking permissions again..."
                
                # Vérifier si c'est un problème de permissions
                if [ ! -w ".git/FETCH_HEAD" ] 2>/dev/null; then
                    print_warning "FETCH_HEAD permission issue detected"
                    print_step "Attempting emergency repair..."
                    sudo chown $USER:$USER .git/FETCH_HEAD 2>/dev/null
                    sudo chmod 644 .git/FETCH_HEAD 2>/dev/null
                fi
                
                print_step "Retrying in 2 seconds..."
                sleep 2
            fi
        fi
    done
    
    if [ $PULL_SUCCESS -eq 0 ]; then
        print_error "Git pull failed after 3 attempts"
        print_step "Diagnostics:"
        
        # Diagnostic détaillé
        echo -e "${BLUE}Git status:${NC}"
        git status || true
        
        echo -e "${BLUE}Git remote:${NC}"
        git remote -v || true
        
        echo -e "${BLUE}Network test:${NC}"
        if ping -c 1 github.com &>/dev/null; then
            print_success "Network connection OK"
        else
            print_error "Cannot reach github.com - check your internet connection"
        fi
        
        # Proposer une solution
        echo ""
        print_info "Possible solutions:"
        echo "  1. Run: cd $REPO_DIR && git reset --hard HEAD"
        echo "  2. Run: cd $REPO_DIR && git fetch origin && git reset --hard origin/$CURRENT_BRANCH"
        echo "  3. Re-run this script"
        
        exit 1
    fi
    
    print_success "Git pull successful"
    
    # Afficher les derniers commits
    echo -e "${BLUE}Last 3 commits:${NC}"
    git log --oneline -3 --color=always
    
    # Afficher les fichiers modifiés
    if git diff --name-status HEAD@{1} HEAD &>/dev/null; then
        echo -e "${BLUE}Modified files in this pull:${NC}"
        git diff --name-status HEAD@{1} HEAD | head -n 10
    fi
}

sync_files() {
    print_step "Synchronizing files..."
    
    # Créer la structure de destination si nécessaire
    sudo mkdir -p "$WWW_DEST/js/"{core,models,views,controllers,utils,services,editor}
    sudo mkdir -p "$WWW_DEST/css"
    sudo mkdir -p "$WWW_DEST/styles"
    
    # Compteur de fichiers synchronisés
    SYNC_COUNT=0
    
    # Synchroniser les fichiers JS
    echo "  → Syncing core files..."
    if [ -d "$FRONTEND_SRC/js/core" ]; then
        sudo rsync -av --delete "$FRONTEND_SRC/js/core/" "$WWW_DEST/js/core/" | grep -v "/$" | wc -l
        SYNC_COUNT=$((SYNC_COUNT + $(find "$WWW_DEST/js/core" -type f | wc -l)))
    fi
    
    # ✅ AJOUT CRITIQUE : Synchroniser le dossier editor
    echo "  → Syncing editor files... (CRITICAL)"
    if [ -d "$FRONTEND_SRC/js/editor" ]; then
        sudo rsync -av --delete "$FRONTEND_SRC/js/editor/" "$WWW_DEST/js/editor/" | grep -v "/$" | wc -l
        EDITOR_COUNT=$(find "$WWW_DEST/js/editor" -type f | wc -l)
        SYNC_COUNT=$((SYNC_COUNT + EDITOR_COUNT))
        print_success "Editor files synced: $EDITOR_COUNT files"
    else
        print_warning "Editor folder not found in source!"
    fi
    
    echo "  → Syncing models..."
    if [ -d "$FRONTEND_SRC/js/models" ]; then
        sudo rsync -av --delete "$FRONTEND_SRC/js/models/" "$WWW_DEST/js/models/" > /dev/null
        SYNC_COUNT=$((SYNC_COUNT + $(find "$WWW_DEST/js/models" -type f | wc -l)))
    fi
    
    echo "  → Syncing views..."
    if [ -d "$FRONTEND_SRC/js/views" ]; then
        sudo rsync -av --delete "$FRONTEND_SRC/js/views/" "$WWW_DEST/js/views/" > /dev/null
        SYNC_COUNT=$((SYNC_COUNT + $(find "$WWW_DEST/js/views" -type f | wc -l)))
    fi
    
    echo "  → Syncing controllers..."
    if [ -d "$FRONTEND_SRC/js/controllers" ]; then
        sudo rsync -av --delete "$FRONTEND_SRC/js/controllers/" "$WWW_DEST/js/controllers/" > /dev/null
        SYNC_COUNT=$((SYNC_COUNT + $(find "$WWW_DEST/js/controllers" -type f | wc -l)))
    fi
    
    echo "  → Syncing utils..."
    if [ -d "$FRONTEND_SRC/js/utils" ]; then
        sudo rsync -av --delete "$FRONTEND_SRC/js/utils/" "$WWW_DEST/js/utils/" > /dev/null
        SYNC_COUNT=$((SYNC_COUNT + $(find "$WWW_DEST/js/utils" -type f | wc -l)))
    fi
    
    echo "  → Syncing services..."
    if [ -d "$FRONTEND_SRC/js/services" ]; then
        sudo rsync -av --delete "$FRONTEND_SRC/js/services/" "$WWW_DEST/js/services/" > /dev/null
        SYNC_COUNT=$((SYNC_COUNT + $(find "$WWW_DEST/js/services" -type f | wc -l)))
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
        sudo rsync -av --delete "$FRONTEND_SRC/css/" "$WWW_DEST/css/" > /dev/null
    fi
    
    if [ -d "$FRONTEND_SRC/styles" ]; then
        echo "  → Syncing CSS files (from styles/)..."
        sudo rsync -av --delete "$FRONTEND_SRC/styles/" "$WWW_DEST/styles/" > /dev/null
    fi
    
    # Synchroniser assets si présent
    if [ -d "$FRONTEND_SRC/assets" ]; then
        echo "  → Syncing assets..."
        sudo rsync -av --delete "$FRONTEND_SRC/assets/" "$WWW_DEST/assets/" > /dev/null
    fi
    
    print_success "Files synchronized ($SYNC_COUNT files)"
    
    # Vérification spécifique de RenderEngine.js
    if [ -f "$WWW_DEST/js/editor/core/RenderEngine.js" ]; then
        print_success "RenderEngine.js found in destination ✓"
    else
        print_error "RenderEngine.js NOT FOUND in destination!"
    fi
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
    if [ -d "$WWW_DEST/js/editor" ]; then
        EDITOR_FILES=$(find "$WWW_DEST/js/editor" -type f -name "*.js" | wc -l)
        echo "  • Editor JS: $EDITOR_FILES files"
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
        "$WWW_DEST/js/editor/core/RenderEngine.js"
    )
    
    MISSING_FILES=0
    for file in "${ESSENTIAL_FILES[@]}"; do
        if [ ! -f "$file" ]; then
            print_warning "Missing essential file: $file"
            MISSING_FILES=$((MISSING_FILES + 1))
        else
            echo "  ✓ Found: $(basename $file)"
        fi
    done
    
    if [ $MISSING_FILES -gt 0 ]; then
        print_warning "$MISSING_FILES essential file(s) missing"
        print_info "Please check the synchronization manually"
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
    fix_git_permissions_complete    # 🔧 RÉPARATION COMPLÈTE
    create_backup
    git_pull
    sync_files
    set_permissions
    verify_sync
    clean_old_backups
    show_summary
}

# Exécuter
main