#!/bin/bash
# ============================================================================
# Fichier: scripts/build.sh
# Version: 3.0.1
# Date: 2025-10-14
# ============================================================================
# Description: Script de compilation MidiMind
# Usage: ./scripts/build.sh [options]
# ============================================================================

set -e  # Arrêter sur erreur

# ============================================================================
# CONFIGURATION
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_DIR/backend"
BUILD_DIR="$BACKEND_DIR/build"

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Options par défaut
BUILD_TYPE="Release"
CLEAN=false
JOBS=$(nproc)
VERBOSE=false

# ============================================================================
# FONCTIONS UTILITAIRES
# ============================================================================

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
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

print_header() {
    echo ""
    echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
    echo ""
}

usage() {
    cat << EOF
Usage: $0 [options]

Options:
    -t, --type TYPE       Build type: Debug|Release (défaut: Release)
    -c, --clean           Nettoyer avant compilation
    -j, --jobs N          Nombre de jobs parallèles (défaut: $(nproc))
    -v, --verbose         Mode verbeux
    --tests               Compiler avec les tests
    --help                Afficher cette aide

Exemples:
    $0                    # Compilation Release
    $0 -t Debug           # Compilation Debug
    $0 -c                 # Nettoyer et compiler
    $0 --tests            # Compiler avec tests
    $0 -t Debug -c -v     # Debug, clean, verbeux

EOF
    exit 0
}

# ============================================================================
# PARSE ARGUMENTS
# ============================================================================

BUILD_TESTS="OFF"

while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        --tests)
            BUILD_TESTS="ON"
            shift
            ;;
        --help)
            usage
            ;;
        *)
            print_error "Option inconnue: $1"
            usage
            ;;
    esac
done

# ============================================================================
# VÉRIFICATIONS
# ============================================================================

print_header "Vérification de l'environnement"

# Vérifier que le répertoire backend existe
if [ ! -d "$BACKEND_DIR" ]; then
    print_error "Répertoire backend introuvable: $BACKEND_DIR"
    exit 1
fi
print_success "Répertoire backend trouvé"

# Vérifier CMake
if ! command -v cmake &> /dev/null; then
    print_error "CMake non trouvé. Installation: sudo apt install cmake"
    exit 1
fi
CMAKE_VERSION=$(cmake --version | head -n1)
print_success "CMake trouvé: $CMAKE_VERSION"

# Vérifier g++
if ! command -v g++ &> /dev/null; then
    print_error "g++ non trouvé. Installation: sudo apt install g++"
    exit 1
fi
GCC_VERSION=$(g++ --version | head -n1)
print_success "g++ trouvé: $GCC_VERSION"

# Vérifier CMakeLists.txt
if [ ! -f "$BACKEND_DIR/CMakeLists.txt" ]; then
    print_error "CMakeLists.txt introuvable dans: $BACKEND_DIR"
    exit 1
fi
print_success "CMakeLists.txt trouvé"

# Vérifier Application.cpp
if [ ! -f "$BACKEND_DIR/src/core/Application.cpp" ]; then
    print_error "src/core/Application.cpp introuvable dans: $BACKEND_DIR"
    print_error "Structure attendue: backend/src/core/Application.cpp"
    exit 1
fi
print_success "Application.cpp trouvé"

# ============================================================================
# NETTOYAGE (si demandé)
# ============================================================================

if [ "$CLEAN" = true ]; then
    print_header "Nettoyage"
    
    if [ -d "$BUILD_DIR" ]; then
        print_info "Suppression de $BUILD_DIR..."
        rm -rf "$BUILD_DIR"
        print_success "Répertoire build supprimé"
    else
        print_info "Rien à nettoyer"
    fi
fi

# ============================================================================
# CRÉATION RÉPERTOIRE BUILD
# ============================================================================

print_header "Préparation"

if [ ! -d "$BUILD_DIR" ]; then
    print_info "Création du répertoire build..."
    mkdir -p "$BUILD_DIR"
    print_success "Répertoire build créé"
else
    print_info "Répertoire build existe déjà"
fi

# ============================================================================
# CONFIGURATION CMAKE
# ============================================================================

print_header "Configuration CMake"

# Se déplacer dans le répertoire backend
cd "$BACKEND_DIR"
print_info "Working directory: $(pwd)"

# Se déplacer dans build
cd build

print_info "Configuration:"
print_info "  - Type de build: $BUILD_TYPE"
print_info "  - Tests: $BUILD_TESTS"
print_info "  - Jobs: $JOBS"

# Options CMake
CMAKE_OPTS=(
    "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
    "-DBUILD_TESTS=$BUILD_TESTS"
)

if [ "$VERBOSE" = true ]; then
    CMAKE_OPTS+=("-DCMAKE_VERBOSE_MAKEFILE=ON")
fi

# Lancer CMake
print_info "Exécution de cmake..."
if cmake "${CMAKE_OPTS[@]}" .. ; then
    print_success "Configuration CMake réussie"
else
    print_error "Échec de la configuration CMake"
    exit 1
fi

# ============================================================================
# COMPILATION
# ============================================================================

print_header "Compilation"

print_info "Compilation avec $JOBS jobs..."

START_TIME=$(date +%s)

if [ "$VERBOSE" = true ]; then
    make -j"$JOBS" VERBOSE=1
else
    make -j"$JOBS"
fi

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

print_success "Compilation réussie en ${DURATION}s"

# ============================================================================
# VÉRIFICATION BINAIRE
# ============================================================================

print_header "Vérification"

BINARY="$BUILD_DIR/midimind"

if [ -f "$BINARY" ]; then
    SIZE=$(du -h "$BINARY" | cut -f1)
    print_success "Binaire créé: $BINARY ($SIZE)"
    
    # Afficher infos
    print_info "Informations binaire:"
    file "$BINARY" | sed 's/^/  /'
    
else
    print_error "Binaire non trouvé: $BINARY"
    exit 1
fi

# ============================================================================
# RÉSUMÉ
# ============================================================================

print_header "Résumé"

echo -e "${GREEN}✓${NC} Compilation terminée avec succès"
echo ""
echo "  Binaire: $BINARY"
echo "  Taille: $SIZE"
echo "  Type: $BUILD_TYPE"
echo "  Durée: ${DURATION}s"
echo ""
echo "Pour lancer l'application:"
echo "  cd $BUILD_DIR"
echo "  ./midimind"
echo ""

# ============================================================================
# FIN
# ============================================================================

exit 0
