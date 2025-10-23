#!/bin/bash
# ============================================================================
# Script: update_and_build.sh
# Version: v1.1 - AUTO BRANCH DETECTION
# Description: Met à jour le backend depuis GitHub et recompile
# Repo: https://github.com/glloq/midiMind
# Date: 2025-10-23
# ============================================================================
# CORRECTIONS v1.1:
# ✅ Détection automatique de la branche courante
# ✅ Meilleure gestion des erreurs git
# ✅ Support des permissions git
# ✅ Retry automatique sur échec
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
BUILD_DIR="$BACKEND_DIR/build"
INSTALL_DIR="/opt/midimind"

# Fonctions
log() { echo -e "${BLUE}[$(date '+%H:%M:%S')]${NC} $1"; }
success() { echo -e "${GREEN}✓${NC} $1"; }
error() { echo -e "${RED}✗ ERREUR:${NC} $1"; exit 1; }
warning() { echo -e "${YELLOW}⚠${NC} $1"; }
info() { echo -e "${MAGENTA}ℹ${NC} $1"; }

print_header() {
    echo ""
    echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║       MidiMind Backend Update & Build v1.1            ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

# Vérifier qu'on est dans le repo midiMind
check_environment() {
    log "Vérification de l'environnement..."
    
    [ -d ".git" ] || error "Pas un repo git. Exécutez depuis la racine du projet."
    [ -d "backend" ] || error "Dossier backend/ introuvable. Exécutez depuis la racine du repo."
    
    # Vérifier les outils nécessaires
    command -v git >/dev/null 2>&1 || error "git n'est pas installé"
    command -v cmake >/dev/null 2>&1 || error "cmake n'est pas installé"
    command -v make >/dev/null 2>&1 || error "make n'est pas installé"
    
    success "Environnement OK"
}

# Réparer les permissions Git si nécessaire
fix_git_permissions() {
    log "Vérification des permissions Git..."
    
    CURRENT_USER=$(whoami)
    
    # Vérifier si on peut écrire dans .git
    if [ ! -w ".git/FETCH_HEAD" ] 2>/dev/null; then
        warning "Problème de permissions Git détecté"
        info "Réparation des permissions..."
        
        sudo chown -R $CURRENT_USER:$CURRENT_USER .git/
        sudo chmod -R u+rwX .git/
        
        success "Permissions réparées"
    fi
}

# Pull les modifications avec détection automatique de branche
git_pull_smart() {
    log "📥 Récupération des modifications depuis GitHub..."
    
    # Détection automatique de la branche courante
    CURRENT_BRANCH=$(git branch --show-current)
    
    if [ -z "$CURRENT_BRANCH" ]; then
        error "Impossible de détecter la branche courante"
    fi
    
    info "Branche courante: $CURRENT_BRANCH"
    
    # Vérifier s'il y a des changements locaux
    if ! git diff-index --quiet HEAD -- 2>/dev/null; then
        warning "Changements locaux détectés"
        
        echo -e "${BLUE}Fichiers modifiés:${NC}"
        git status --short
        
        log "Sauvegarde des changements locaux..."
        if git stash push -m "Auto-stash before update $(date +%Y%m%d_%H%M%S)"; then
            success "Changements sauvegardés (git stash)"
        else
            error "Impossible de sauvegarder les changements locaux"
        fi
    fi
    
    # Pull avec retry
    PULL_SUCCESS=0
    for i in {1..3}; do
        info "Tentative de pull $i/3..."
        
        if git pull origin "$CURRENT_BRANCH" 2>&1; then
            PULL_SUCCESS=1
            break
        else
            warning "Échec de la tentative $i"
            
            if [ $i -lt 3 ]; then
                # Vérifier les permissions
                if [ ! -w ".git/FETCH_HEAD" ] 2>/dev/null; then
                    warning "Problème de permissions FETCH_HEAD"
                    log "Réparation d'urgence..."
                    sudo chown $USER:$USER .git/FETCH_HEAD 2>/dev/null
                    sudo chmod 644 .git/FETCH_HEAD 2>/dev/null
                fi
                
                log "Nouvelle tentative dans 2 secondes..."
                sleep 2
            fi
        fi
    done
    
    if [ $PULL_SUCCESS -eq 0 ]; then
        error "Échec du git pull après 3 tentatives"
    fi
    
    success "Modifications récupérées"
    
    # Afficher les derniers commits
    echo -e "${BLUE}Derniers commits:${NC}"
    git log --oneline -3 --color=always
}

# Nettoyer le build
clean_build() {
    log "🧹 Nettoyage du dossier build..."
    
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
    
    mkdir -p "$BUILD_DIR"
    success "Build nettoyé"
}

# Compiler avec CMake
compile_backend() {
    log "🔨 Compilation du backend..."
    
    cd "$BUILD_DIR"
    
    # Configuration CMake
    info "Configuration CMake..."
    if ! cmake "$BACKEND_DIR"; then
        error "Échec de la configuration CMake"
    fi
    
    # Compilation
    info "Compilation ($(nproc) cœurs)..."
    if ! make -j$(nproc); then
        error "Échec de la compilation"
    fi
    
    success "Compilation terminée"
    
    # Vérifier que le binaire existe
    if [ ! -f "bin/midimind" ]; then
        error "Le binaire bin/midimind n'a pas été créé"
    fi
    
    # Afficher la taille
    BINARY_SIZE=$(du -h bin/midimind | cut -f1)
    info "Taille du binaire: $BINARY_SIZE"
}

# Arrêter le service
stop_service() {
    log "⏸️  Arrêt du service midimind..."
    
    if sudo systemctl is-active --quiet midimind.service; then
        sudo systemctl stop midimind.service
        sleep 1
        success "Service arrêté"
    else
        info "Service déjà arrêté"
    fi
}

# Installer le nouveau binaire
install_binary() {
    log "📦 Installation du nouveau binaire..."
    
    # Vérifier que le dossier de destination existe
    if [ ! -d "$INSTALL_DIR/bin" ]; then
        sudo mkdir -p "$INSTALL_DIR/bin"
    fi
    
    # Sauvegarder l'ancien binaire
    if [ -f "$INSTALL_DIR/bin/midimind" ]; then
        sudo cp "$INSTALL_DIR/bin/midimind" \
                "$INSTALL_DIR/bin/midimind.backup.$(date +%Y%m%d_%H%M%S)"
        info "Ancien binaire sauvegardé"
    fi
    
    # Copier le nouveau binaire
    sudo cp "$BUILD_DIR/bin/midimind" "$INSTALL_DIR/bin/midimind" || \
        error "Échec de la copie du binaire"
    
    # Définir les permissions
    sudo chown root:root "$INSTALL_DIR/bin/midimind"
    sudo chmod 755 "$INSTALL_DIR/bin/midimind"
    
    success "Binaire installé"
}

# Redémarrer le service
start_service() {
    log "🚀 Redémarrage du service..."
    
    sudo systemctl start midimind.service || error "Échec du démarrage du service"
    
    # Attendre que le service démarre
    sleep 2
    
    # Vérifier le statut
    if sudo systemctl is-active --quiet midimind.service; then
        success "Service midimind actif"
    else
        error "Service midimind non actif - vérifiez les logs"
    fi
}

# Afficher le résumé final
show_summary() {
    echo ""
    echo -e "${GREEN}════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}✅ Mise à jour et compilation terminées${NC}"
    echo -e "${GREEN}════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "${BLUE}Informations:${NC}"
    echo -e "  • Branche:  ${GREEN}$(git branch --show-current)${NC}"
    echo -e "  • Commit:   ${GREEN}$(git log --oneline -1 | cut -d' ' -f1)${NC}"
    echo -e "  • Binaire:  ${GREEN}$INSTALL_DIR/bin/midimind${NC}"
    echo ""
    echo -e "${BLUE}Commandes utiles:${NC}"
    echo -e "  • Status:   ${GREEN}sudo systemctl status midimind${NC}"
    echo -e "  • Logs:     ${GREEN}sudo journalctl -u midimind -f${NC}"
    echo -e "  • Stop:     ${GREEN}sudo systemctl stop midimind${NC}"
    echo -e "  • Restart:  ${GREEN}sudo systemctl restart midimind${NC}"
    echo ""
}

# ============================================================================
# MAIN
# ============================================================================

main() {
    print_header
    
    # Étapes d'exécution
    check_environment
    fix_git_permissions
    git_pull_smart
    clean_build
    compile_backend
    stop_service
    install_binary
    start_service
    show_summary
}

# Exécuter
main