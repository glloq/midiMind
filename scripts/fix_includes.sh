#!/bin/bash
# ============================================================================
# Script de Correction Automatique des Includes
# Version: 1.0.0
# ============================================================================

set -e

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

PROJECT_ROOT="/midiMind"
BACKUP_DIR="$PROJECT_ROOT/backup_headers_$(date +%Y%m%d_%H%M%S)"

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║      🔧 Correction Automatique des Includes 🔧             ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# ============================================================================
# BACKUP
# ============================================================================

echo -e "${YELLOW}📦 Création du backup...${NC}"
mkdir -p "$BACKUP_DIR"
cp -r "$PROJECT_ROOT/backend" "$BACKUP_DIR/"
echo -e "${GREEN}✓${NC} Backup créé: $BACKUP_DIR"
echo ""

# ============================================================================
# 1. AJOUTER #pragma once À TOUS LES HEADERS
# ============================================================================

echo -e "${YELLOW}🛡️  1. Ajout de #pragma once aux headers${NC}"
echo ""

FIXED_COUNT=0

while IFS= read -r -d '' header; do
    filename=$(basename "$header")
    
    # Vérifier si le fichier a déjà une protection
    if ! grep -q "#pragma once" "$header" && ! grep -q "#ifndef" "$header"; then
        # Créer fichier temporaire
        temp_file=$(mktemp)
        
        # Ajouter #pragma once en première ligne
        echo "#pragma once" > "$temp_file"
        echo "" >> "$temp_file"
        cat "$header" >> "$temp_file"
        
        # Remplacer le fichier original
        mv "$temp_file" "$header"
        
        echo -e "${GREEN}✓${NC} Ajouté à: $filename"
        ((FIXED_COUNT++))
    fi
    
done < <(find "$PROJECT_ROOT/backend" -name "*.h" -type f -print0)

echo ""
echo -e "${GREEN}$FIXED_COUNT${NC} fichiers corrigés"
echo ""

# ============================================================================
# 2. ANALYSER ET SUGGÉRER FORWARD DECLARATIONS
# ============================================================================

echo -e "${YELLOW}🔍 2. Analyse des candidats pour forward declarations${NC}"
echo ""

# Créer fichier de suggestions
SUGGESTIONS_FILE="$PROJECT_ROOT/forward_declarations_suggestions.txt"
echo "# Suggestions Forward Declarations" > "$SUGGESTIONS_FILE"
echo "# Généré le: $(date)" >> "$SUGGESTIONS_FILE"
echo "" >> "$SUGGESTIONS_FILE"

# Patterns communs qui pourraient utiliser forward declarations
PATTERNS=(
    "class.*\*"
    "class.*&"
    "std::unique_ptr"
    "std::shared_ptr"
)

SUGGESTION_COUNT=0

while IFS= read -r -d '' header; do
    filename=$(basename "$header")
    suggestions=""
    
    # Chercher les patterns
    for pattern in "${PATTERNS[@]}"; do
        matches=$(grep -n "$pattern" "$header" 2>/dev/null | head -5 || true)
        if [ -n "$matches" ]; then
            suggestions="$suggestions\n$matches"
        fi
    done
    
    if [ -n "$suggestions" ]; then
        echo "=== $filename ===" >> "$SUGGESTIONS_FILE"
        echo -e "$suggestions" >> "$SUGGESTIONS_FILE"
        echo "" >> "$SUGGESTIONS_FILE"
        ((SUGGESTION_COUNT++))
    fi
    
done < <(find "$PROJECT_ROOT/backend" -name "*.h" -type f -print0)

echo -e "${CYAN}Suggestions écrites dans:${NC} $SUGGESTIONS_FILE"
echo -e "${CYAN}Fichiers avec suggestions:${NC} $SUGGESTION_COUNT"
echo ""

# ============================================================================
# 3. CRÉER TEMPLATE DE FORWARD DECLARATIONS
# ============================================================================

echo -e "${YELLOW}📝 3. Création de templates forward declarations${NC}"
echo ""

TEMPLATE_FILE="$PROJECT_ROOT/forward_declarations_template.h"

cat > "$TEMPLATE_FILE" << 'EOF'
#pragma once
// ============================================================================
// Forward Declarations - MidiMind
// ============================================================================
// Ce fichier contient les forward declarations communes pour éviter
// les inclusions circulaires
// ============================================================================

// Core
namespace midiMind {
    class Application;
    class Logger;
    class ErrorManager;
    class EventBus;
}

// MIDI Core
namespace midiMind {
    class MidiMessage;
    class MidiRouter;
    class MidiDevice;
    class MidiDeviceManager;
}

// MIDI Processing
namespace midiMind {
    class NoteProcessor;
    class ControlProcessor;
    class ChainProcessor;
}

// Routing
namespace midiMind {
    class ChannelMapper;
    class TransformationEngine;
    class RoutingTable;
}

// Storage
namespace midiMind {
    class Database;
    class FileManager;
    class PresetManager;
    class SessionManager;
}

// Network
namespace midiMind {
    class NetworkManager;
    class WifiManager;
    class BluetoothManager;
}

// API
namespace midiMind {
    class ApiServer;
    class CommandProcessor;
}

// ============================================================================
// USAGE:
// ============================================================================
// Dans vos headers, au lieu de:
//   #include "MidiRouter.h"
//
// Utilisez:
//   #include "ForwardDeclarations.h"
//   // et dans le .cpp:
//   #include "MidiRouter.h"
// ============================================================================
EOF

echo -e "${GREEN}✓${NC} Template créé: $TEMPLATE_FILE"
echo ""

# ============================================================================
# 4. DÉTECTER INCLUSIONS CIRCULAIRES RESTANTES
# ============================================================================

echo -e "${YELLOW}🔄 4. Vérification inclusions circulaires${NC}"
echo ""

# Utiliser g++ pour préprocesser et détecter les erreurs
TEST_FILE=$(mktemp --suffix=.cpp)
cat > "$TEST_FILE" << 'EOF'
#include "../backend/src/core/Application.h"
int main() { return 0; }
EOF

cd "$PROJECT_ROOT"

if g++ -I"$PROJECT_ROOT/backend/src" -fsyntax-only "$TEST_FILE" 2>&1 | grep -i "nested.*depth"; then
    echo -e "${RED}⚠️  Inclusions circulaires détectées${NC}"
    echo ""
    echo -e "${YELLOW}Fichiers problématiques:${NC}"
    g++ -I"$PROJECT_ROOT/backend/src" -H -fsyntax-only "$TEST_FILE" 2>&1 | head -30
else
    echo -e "${GREEN}✓${NC} Aucune inclusion circulaire détectée"
fi

rm "$TEST_FILE"
echo ""

# ============================================================================
# RÉSUMÉ
# ============================================================================

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║                        RÉSUMÉ                                 ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${GREEN}✓${NC} Backup créé:               $BACKUP_DIR"
echo -e "${GREEN}✓${NC} Headers protégés:          $FIXED_COUNT fichiers"
echo -e "${GREEN}✓${NC} Suggestions générées:      $SUGGESTIONS_FILE"
echo -e "${GREEN}✓${NC} Template créé:             $TEMPLATE_FILE"
echo ""
echo -e "${YELLOW}Prochaines étapes:${NC}"
echo ""
echo -e "1. Vérifier les suggestions dans: $SUGGESTIONS_FILE"
echo -e "2. Copier le template ForwardDeclarations.h dans backend/src/"
echo -e "3. Remplacer les #include par forward declarations quand possible"
echo -e "4. Relancer cmake et make"
echo ""
echo -e "${CYAN}Commandes:${NC}"
echo -e "  cd $PROJECT_ROOT"
echo -e "  rm -rf build && mkdir build && cd build"
echo -e "  cmake .. -DCMAKE_BUILD_TYPE=Release"
echo -e "  make -j\$(nproc)"
echo ""
