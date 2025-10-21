#!/bin/bash
# ============================================================================
# Script: update_and_build.sh
# Description: Met à jour le backend depuis GitHub et recompile
# Repo: https://github.com/glloq/midiMind
# Date: 2025-10-21
# ============================================================================

set -e

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

# Vérifier qu'on est dans le repo midiMind
[ -d ".git" ] || error "Pas un repo git. Exécutez depuis la racine du projet."
[ -d "backend" ] || error "Dossier backend/ introuvable. Exécutez depuis la racine du repo."

# 1. Pull les modifications
log "📥 Récupération des modifications depuis GitHub..."
git pull origin main || git pull origin master || error "Échec du git pull"
success "Modifications récupérées"

# 2. Nettoyer le build
log "🧹 Nettoyage du dossier build..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
success "Build nettoyé"

# 3. Compiler avec CMake
log "🔨 Compilation du backend..."
cd "$BUILD_DIR"
cmake "$BACKEND_DIR" || error "Échec de cmake"
make -j$(nproc) || error "Échec de la compilation"
success "Compilation terminée"

# 4. Arrêter le service
log "⏸️  Arrêt du service midimind..."
sudo systemctl stop midimind.service 2>/dev/null || true
success "Service arrêté"

# 5. Copier le binaire
log "📦 Installation du nouveau binaire..."
sudo cp bin/midimind "$INSTALL_DIR/bin/midimind" || error "Échec de la copie"
sudo chown root:root "$INSTALL_DIR/bin/midimind"
sudo chmod 755 "$INSTALL_DIR/bin/midimind"
success "Binaire installé"

# 6. Redémarrer le service
log "🚀 Redémarrage du service..."
sudo systemctl start midimind.service || error "Échec du démarrage du service"
sleep 2

# Vérifier le statut
if sudo systemctl is-active --quiet midimind.service; then
    success "Service midimind actif"
else
    error "Service midimind non actif"
fi

echo ""
echo -e "${GREEN}✅ Mise à jour et compilation terminées${NC}"
echo ""
echo -e "${BLUE}Commandes utiles:${NC}"
echo -e "  • Status:  ${GREEN}sudo systemctl status midimind${NC}"
echo -e "  • Logs:    ${GREEN}sudo journalctl -u midimind -f${NC}"
echo ""