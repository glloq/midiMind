#!/bin/bash
# ============================================================================
# Fichier: setup-desktop.sh
# Version: v1.0.0
# Date: 2025-10-14
# Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
# ============================================================================
# Description:
#   Configure l'interface bureau pour midiMind :
#   - Raccourci bureau (icône cliquable)
#   - Lancement automatique au démarrage (avec détection écran)
#   - Icône personnalisée
#   - Mode kiosque optionnel
# ============================================================================

set -e

# ============================================================================
# COULEURS
# ============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# ============================================================================
# VARIABLES
# ============================================================================

DESKTOP_DIR="$HOME/Desktop"
AUTOSTART_DIR="$HOME/.config/autostart"
ICONS_DIR="$HOME/.local/share/icons"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DESKTOP_FILE="midimind.desktop"
AUTOSTART_FILE="midimind-autostart.desktop"
ICON_FILE="midimind-icon.svg"
LAUNCHER_SCRIPT="$HOME/.local/bin/midimind-launcher.sh"

# URL du frontend
FRONTEND_URL="http://localhost:8000"

# Options
KIOSK_MODE=false
AUTO_START=true
DESKTOP_ICON=true

# ============================================================================
# FONCTIONS
# ============================================================================

log() {
    echo -e "${GREEN}[✓]${NC} $1"
}

info() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

warning() {
    echo -e "${YELLOW}[ATTENTION]${NC} $1"
}

error() {
    echo -e "${RED}[ERREUR]${NC} $1" >&2
    exit 1
}

# ============================================================================
# BANNIÈRE
# ============================================================================

print_banner() {
    clear
    echo -e "${CYAN}"
    cat << "EOF"
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║         🖥️  midiMind - Configuration Bureau 🖥️               ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
}

# ============================================================================
# VÉRIFICATIONS
# ============================================================================

check_environment() {
    info "Vérification de l'environnement..."
    
    # Vérifier qu'on est sur un système avec GUI
    if [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; then
        warning "Aucun serveur graphique détecté"
        warning "Ce script est conçu pour un Raspberry Pi avec écran"
        echo ""
        read -p "Continuer quand même ? (o/N) " -r
        if [[ ! $REPLY =~ ^[oOyY]$ ]]; then
            info "Configuration annulée"
            exit 0
        fi
    fi
    
    # Vérifier que MidiMind est installé
    if [ ! -f "/opt/midimind/bin/midimind" ] && [ ! -f "/etc/systemd/system/midimind.service" ]; then
        error "MidiMind n'est pas installé. Exécutez d'abord ./install.sh"
    fi
    
    log "Environnement OK"
}

# ============================================================================
# CRÉATION RÉPERTOIRES
# ============================================================================

create_directories() {
    info "Création des répertoires..."
    
    mkdir -p "$DESKTOP_DIR"
    mkdir -p "$AUTOSTART_DIR"
    mkdir -p "$ICONS_DIR"
    mkdir -p "$(dirname "$LAUNCHER_SCRIPT")"
    
    log "Répertoires créés"
}

# ============================================================================
# CRÉATION ICÔNE SVG
# ============================================================================

create_icon() {
    info "Création de l'icône midiMind..."
    
    cat > "$ICONS_DIR/$ICON_FILE" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<svg width="128" height="128" viewBox="0 0 128 128" xmlns="http://www.w3.org/2000/svg">
  <!-- Background -->
  <rect width="128" height="128" rx="16" fill="#1a1a2e"/>
  
  <!-- Piano keys -->
  <g transform="translate(24, 40)">
    <!-- White keys -->
    <rect x="0" y="0" width="12" height="48" fill="#ffffff" stroke="#333" stroke-width="1"/>
    <rect x="12" y="0" width="12" height="48" fill="#ffffff" stroke="#333" stroke-width="1"/>
    <rect x="24" y="0" width="12" height="48" fill="#ffffff" stroke="#333" stroke-width="1"/>
    <rect x="36" y="0" width="12" height="48" fill="#ffffff" stroke="#333" stroke-width="1"/>
    <rect x="48" y="0" width="12" height="48" fill="#ffffff" stroke="#333" stroke-width="1"/>
    <rect x="60" y="0" width="12" height="48" fill="#ffffff" stroke="#333" stroke-width="1"/>
    <rect x="72" y="0" width="12" height="48" fill="#ffffff" stroke="#333" stroke-width="1"/>
    
    <!-- Black keys -->
    <rect x="8" y="0" width="8" height="30" fill="#000000"/>
    <rect x="20" y="0" width="8" height="30" fill="#000000"/>
    <rect x="44" y="0" width="8" height="30" fill="#000000"/>
    <rect x="56" y="0" width="8" height="30" fill="#000000"/>
    <rect x="68" y="0" width="8" height="30" fill="#000000"/>
  </g>
  
  <!-- Sound waves -->
  <path d="M 15 20 Q 25 15, 35 20 T 55 20" stroke="#00d4ff" stroke-width="2" fill="none" opacity="0.6"/>
  <path d="M 73 20 Q 83 15, 93 20 T 113 20" stroke="#00d4ff" stroke-width="2" fill="none" opacity="0.6"/>
  
  <!-- Text -->
  <text x="64" y="105" font-family="Arial, sans-serif" font-size="16" font-weight="bold" fill="#00d4ff" text-anchor="middle">midiMind</text>
</svg>
EOF
    
    log "Icône créée : $ICONS_DIR/$ICON_FILE"
}

# ============================================================================
# CRÉATION SCRIPT LANCEUR
# ============================================================================

create_launcher_script() {
    info "Création du script lanceur..."
    
    cat > "$LAUNCHER_SCRIPT" << 'EOF'
#!/bin/bash
# ============================================================================
# Fichier: ~/.local/bin/midimind-launcher.sh
# Version: v1.0.0
# Description: Lance le frontend midiMind dans le navigateur
# ============================================================================

# URL du frontend
FRONTEND_URL="http://localhost:8000"

# Mode kiosque (plein écran sans barres)
KIOSK_MODE=false

# Attendre que le service soit démarré
MAX_WAIT=30
count=0

echo "Attente du démarrage de midiMind..."

while ! curl -s "$FRONTEND_URL" > /dev/null 2>&1; do
    sleep 1
    ((count++))
    if [ $count -ge $MAX_WAIT ]; then
        zenity --error --text="Impossible de se connecter à midiMind.\n\nVérifiez que le service est démarré:\nsudo systemctl status midimind" --width=400 2>/dev/null || \
        notify-send "midiMind" "Erreur: Service non accessible" 2>/dev/null || \
        echo "Erreur: midiMind non accessible après ${MAX_WAIT}s"
        exit 1
    fi
done

echo "midiMind accessible, lancement du navigateur..."

# Déterminer le navigateur disponible
if command -v chromium-browser &> /dev/null; then
    BROWSER="chromium-browser"
elif command -v chromium &> /dev/null; then
    BROWSER="chromium"
elif command -v firefox &> /dev/null; then
    BROWSER="firefox"
elif command -v x-www-browser &> /dev/null; then
    BROWSER="x-www-browser"
else
    zenity --error --text="Aucun navigateur trouvé.\n\nInstallez Chromium ou Firefox." --width=400 2>/dev/null || \
    notify-send "midiMind" "Erreur: Aucun navigateur trouvé" 2>/dev/null || \
    echo "Erreur: Aucun navigateur trouvé"
    exit 1
fi

# Options selon le mode
if [ "$KIOSK_MODE" = true ]; then
    # Mode kiosque (plein écran)
    if [ "$BROWSER" = "chromium-browser" ] || [ "$BROWSER" = "chromium" ]; then
        $BROWSER --kiosk --app="$FRONTEND_URL" &
    elif [ "$BROWSER" = "firefox" ]; then
        $BROWSER --kiosk "$FRONTEND_URL" &
    else
        $BROWSER "$FRONTEND_URL" &
    fi
else
    # Mode normal
    if [ "$BROWSER" = "chromium-browser" ] || [ "$BROWSER" = "chromium" ]; then
        $BROWSER --app="$FRONTEND_URL" --window-size=1280,800 &
    else
        $BROWSER "$FRONTEND_URL" &
    fi
fi

echo "Navigateur lancé avec succès"
EOF
    
    chmod +x "$LAUNCHER_SCRIPT"
    
    log "Script lanceur créé : $LAUNCHER_SCRIPT"
}

# ============================================================================
# CRÉATION RACCOURCI BUREAU
# ============================================================================

create_desktop_shortcut() {
    if [ "$DESKTOP_ICON" = false ]; then
        info "Raccourci bureau désactivé (option)"
        return
    fi
    
    info "Création du raccourci bureau..."
    
    cat > "$DESKTOP_DIR/$DESKTOP_FILE" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=midiMind
Comment=Interface Web midiMind - Orchestration MIDI
Exec=$LAUNCHER_SCRIPT
Icon=$ICONS_DIR/$ICON_FILE
Terminal=false
Categories=AudioVideo;Audio;MIDI;
Keywords=MIDI;Music;Audio;Orchestration;
StartupNotify=true
EOF
    
    # Rendre exécutable
    chmod +x "$DESKTOP_DIR/$DESKTOP_FILE"
    
    # Marquer comme trusted (Raspbian/Ubuntu)
    gio set "$DESKTOP_DIR/$DESKTOP_FILE" metadata::trusted true 2>/dev/null || true
    
    log "Raccourci bureau créé : $DESKTOP_DIR/$DESKTOP_FILE"
}

# ============================================================================
# CRÉATION AUTOSTART
# ============================================================================

create_autostart() {
    if [ "$AUTO_START" = false ]; then
        info "Lancement automatique désactivé (option)"
        return
    fi
    
    info "Création du lancement automatique..."
    
    # Script wrapper pour détection écran
    cat > "$HOME/.local/bin/midimind-autostart.sh" << 'EOF'
#!/bin/bash
# ============================================================================
# Fichier: ~/.local/bin/midimind-autostart.sh
# Version: v1.0.0
# Description: Lance midiMind seulement si un écran est connecté
# ============================================================================

# Attendre que l'environnement graphique soit prêt
sleep 5

# Vérifier qu'un écran est connecté
if [ -n "$DISPLAY" ] || [ -n "$WAYLAND_DISPLAY" ]; then
    # Vérifier avec xrandr si disponible
    if command -v xrandr &> /dev/null; then
        # Compter les écrans connectés
        SCREEN_COUNT=$(xrandr --query | grep -c " connected")
        
        if [ "$SCREEN_COUNT" -eq 0 ]; then
            echo "Aucun écran connecté, pas de lancement auto"
            exit 0
        fi
        
        echo "Écran(s) détecté(s): $SCREEN_COUNT"
    fi
    
    # Lancer le frontend
    echo "Lancement du frontend midiMind..."
    $HOME/.local/bin/midimind-launcher.sh
else
    echo "Pas d'environnement graphique, mode headless"
fi
EOF
    
    chmod +x "$HOME/.local/bin/midimind-autostart.sh"
    
    # Créer le fichier autostart
    cat > "$AUTOSTART_DIR/$AUTOSTART_FILE" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=midiMind AutoStart
Comment=Lance automatiquement l'interface midiMind au démarrage
Exec=$HOME/.local/bin/midimind-autostart.sh
Icon=$ICONS_DIR/$ICON_FILE
Terminal=false
Categories=AudioVideo;Audio;MIDI;
X-GNOME-Autostart-enabled=true
Hidden=false
NoDisplay=false
EOF
    
    log "Lancement automatique configuré : $AUTOSTART_DIR/$AUTOSTART_FILE"
}

# ============================================================================
# OPTIONS INTERACTIVES
# ============================================================================

ask_options() {
    echo ""
    echo -e "${CYAN}Configuration des options :${NC}"
    echo ""
    
    # Mode kiosque
    read -p "Activer le mode kiosque (plein écran) ? (o/N) " -r
    if [[ $REPLY =~ ^[oOyY]$ ]]; then
        KIOSK_MODE=true
        # Modifier le script lanceur
        sed -i 's/KIOSK_MODE=false/KIOSK_MODE=true/' "$LAUNCHER_SCRIPT"
        info "Mode kiosque activé"
    else
        info "Mode fenêtre normal"
    fi
    
    echo ""
    
    # Autostart
    read -p "Activer le lancement automatique au démarrage ? (O/n) " -r
    if [[ $REPLY =~ ^[nN]$ ]]; then
        AUTO_START=false
        info "Lancement automatique désactivé"
    else
        info "Lancement automatique activé"
    fi
    
    echo ""
}

# ============================================================================
# RÉSUMÉ
# ============================================================================

print_summary() {
    echo ""
    echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                                                              ║${NC}"
    echo -e "${GREEN}║         ✓ Configuration bureau terminée avec succès !        ║${NC}"
    echo -e "${GREEN}║                                                              ║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    echo -e "${CYAN}📋 Résumé de la configuration :${NC}"
    echo ""
    
    if [ "$DESKTOP_ICON" = true ]; then
        echo -e "  ${GREEN}✓${NC} Raccourci bureau créé"
        echo -e "    ${BLUE}→${NC} Double-cliquez sur l'icône 'midiMind' sur le bureau"
    fi
    
    if [ "$AUTO_START" = true ]; then
        echo -e "  ${GREEN}✓${NC} Lancement automatique activé"
        echo -e "    ${BLUE}→${NC} S'ouvre automatiquement au démarrage (si écran connecté)"
    fi
    
    echo -e "  ${GREEN}✓${NC} Icône personnalisée installée"
    
    if [ "$KIOSK_MODE" = true ]; then
        echo -e "  ${GREEN}✓${NC} Mode kiosque activé (plein écran)"
    else
        echo -e "  ${GREEN}✓${NC} Mode fenêtre normal"
    fi
    
    echo ""
    echo -e "${CYAN}🖥️  Utilisation :${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Cliquez sur l'icône bureau pour lancer manuellement"
    echo -e "  ${BLUE}•${NC} Ou redémarrez pour le lancement automatique"
    echo -e "  ${BLUE}•${NC} URL accessible : ${GREEN}$FRONTEND_URL${NC}"
    echo ""
    
    echo -e "${CYAN}📝 Fichiers créés :${NC}"
    echo ""
    echo -e "  ${BLUE}•${NC} Icône : $ICONS_DIR/$ICON_FILE"
    echo -e "  ${BLUE}•${NC} Lanceur : $LAUNCHER_SCRIPT"
    [ "$DESKTOP_ICON" = true ] && echo -e "  ${BLUE}•${NC} Bureau : $DESKTOP_DIR/$DESKTOP_FILE"
    [ "$AUTO_START" = true ] && echo -e "  ${BLUE}•${NC} Autostart : $AUTOSTART_DIR/$AUTOSTART_FILE"
    echo ""
    
    echo -e "${YELLOW}ℹ️  Remarques :${NC}"
    echo ""
    echo -e "  • Le navigateur s'ouvre après le démarrage de midiMind"
    echo -e "  • Délai maximum d'attente : 30 secondes"
    echo -e "  • Détection automatique d'écran en mode headless"
    echo ""
    
    if [ "$AUTO_START" = true ]; then
        echo -e "${CYAN}Pour désactiver le lancement automatique :${NC}"
        echo -e "  ${BLUE}rm $AUTOSTART_DIR/$AUTOSTART_FILE${NC}"
        echo ""
    fi
}

# ============================================================================
# MAIN
# ============================================================================

main() {
    print_banner
    
    check_environment
    echo ""
    
    ask_options
    
    create_directories
    create_icon
    create_launcher_script
    create_desktop_shortcut
    create_autostart
    
    print_summary
    
    log "Configuration terminée !"
    echo ""
}

# ============================================================================
# GESTION DES ARGUMENTS
# ============================================================================

case "$1" in
    -h|--help)
        echo "Usage: $0 [OPTIONS]"
        echo ""
        echo "Configure l'interface bureau pour midiMind"
        echo ""
        echo "Options:"
        echo "  -h, --help          Afficher cette aide"
        echo "  --no-autostart      Ne pas créer le lancement automatique"
        echo "  --no-desktop-icon   Ne pas créer le raccourci bureau"
        echo "  --kiosk             Activer le mode kiosque (plein écran)"
        echo ""
        exit 0
        ;;
    --no-autostart)
        AUTO_START=false
        ;;
    --no-desktop-icon)
        DESKTOP_ICON=false
        ;;
    --kiosk)
        KIOSK_MODE=true
        ;;
esac

# ============================================================================
# POINT D'ENTRÉE
# ============================================================================

main

# ============================================================================
# FIN DU FICHIER setup-desktop.sh v1.0.0
# ============================================================================