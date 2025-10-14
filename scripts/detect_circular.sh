#!/bin/bash
# ============================================================================
# Script de Détection des Inclusions Circulaires
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
BACKEND_SRC="$PROJECT_ROOT/backend/src"

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   🔍 Détection Inclusions Circulaires MidiMind 🔍          ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# ============================================================================
# 1. VÉRIFIER INCLUDE GUARDS
# ============================================================================

echo -e "${YELLOW}📋 1. Vérification Include Guards${NC}"
echo ""

MISSING_GUARDS=0
TOTAL_HEADERS=0

while IFS= read -r -d '' header; do
    ((TOTAL_HEADERS++))
    filename=$(basename "$header")
    
    # Vérifier #pragma once OU #ifndef
    if ! grep -q "#pragma once" "$header" && ! grep -q "#ifndef" "$header"; then
        echo -e "${RED}✗${NC} $filename - PAS DE PROTECTION"
        ((MISSING_GUARDS++))
    fi
done < <(find "$PROJECT_ROOT/backend" -name "*.h" -type f -print0)

echo ""
echo -e "Headers analysés: $TOTAL_HEADERS"
echo -e "Sans protection:  ${RED}$MISSING_GUARDS${NC}"
echo ""

# ============================================================================
# 2. DÉTECTER INCLUSIONS MULTIPLES
# ============================================================================

echo -e "${YELLOW}📊 2. Fichiers les Plus Inclus${NC}"
echo ""

# Créer fichier temporaire pour compter
temp_file=$(mktemp)

# Compter les #include de chaque header
while IFS= read -r -d '' header; do
    basename "$header"
done < <(find "$PROJECT_ROOT/backend" -name "*.h" -type f -print0) | sort | uniq -c | sort -rn | head -20 > "$temp_file"

# Afficher top 10
head -10 "$temp_file" | while read count file; do
    if [ "$count" -gt 20 ]; then
        echo -e "${RED}$count${NC}x - $file ${RED}(SUSPECT)${NC}"
    elif [ "$count" -gt 10 ]; then
        echo -e "${YELLOW}$count${NC}x - $file"
    else
        echo -e "${GREEN}$count${NC}x - $file"
    fi
done

rm "$temp_file"
echo ""

# ============================================================================
# 3. ANALYSER DÉPENDANCES DIRECTES
# ============================================================================

echo -e "${YELLOW}🔗 3. Analyse Dépendances Suspectes${NC}"
echo ""

# Fonction pour extraire les includes d'un fichier
get_includes() {
    local file=$1
    grep -h "^#include" "$file" 2>/dev/null | \
        grep -v "^//" | \
        sed 's/#include[[:space:]]*[<"]\(.*\)[>"]/\1/' | \
        grep -v "^<" | \
        sort -u
}

# Analyser chaque header
CIRCULAR_FOUND=0

while IFS= read -r -d '' header; do
    filename=$(basename "$header")
    
    # Extraire les includes
    includes=$(get_includes "$header")
    
    # Pour chaque include, vérifier si celui-ci inclut le fichier original
    while IFS= read -r included; do
        [ -z "$included" ] && continue
        
        # Trouver le chemin complet du fichier inclus
        included_path=$(find "$PROJECT_ROOT/backend" -name "$included" -type f 2>/dev/null | head -1)
        
        if [ -n "$included_path" ]; then
            # Vérifier si le fichier inclus référence le fichier original
            if grep -q "$filename" "$included_path" 2>/dev/null; then
                echo -e "${RED}⚠️  CIRCULAIRE DÉTECTÉE:${NC}"
                echo -e "   $filename → $(basename $included_path)"
                echo -e "   $(basename $included_path) → $filename"
                echo ""
                ((CIRCULAR_FOUND++))
            fi
        fi
    done <<< "$includes"
    
done < <(find "$PROJECT_ROOT/backend" -name "*.h" -type f -print0)

if [ $CIRCULAR_FOUND -eq 0 ]; then
    echo -e "${GREEN}✓ Aucune inclusion circulaire directe détectée${NC}"
else
    echo -e "${RED}✗ $CIRCULAR_FOUND inclusions circulaires directes trouvées${NC}"
fi

echo ""

# ============================================================================
# 4. FICHIERS PROBLÉMATIQUES POTENTIELS
# ============================================================================

echo -e "${YELLOW}🎯 4. Fichiers Potentiellement Problématiques${NC}"
echo ""

# Headers avec beaucoup d'includes
while IFS= read -r -d '' header; do
    filename=$(basename "$header")
    include_count=$(grep -c "^#include" "$header" 2>/dev/null || echo 0)
    
    if [ "$include_count" -gt 15 ]; then
        echo -e "${RED}⚠${NC}  $filename - ${RED}$include_count includes${NC} (beaucoup!)"
    fi
done < <(find "$PROJECT_ROOT/backend" -name "*.h" -type f -print0)

echo ""

# ============================================================================
# 5. RECOMMANDATIONS
# ============================================================================

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║                    RECOMMANDATIONS                           ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

if [ $MISSING_GUARDS -gt 0 ]; then
    echo -e "${YELLOW}1. Ajouter include guards à tous les headers:${NC}"
    echo ""
    echo -e "   Option A: #pragma once (moderne, simple)"
    echo -e "   ${CYAN}#pragma once${NC}"
    echo ""
    echo -e "   Option B: Include guards traditionnels"
    echo -e "   ${CYAN}#ifndef MIDIMIND_FILENAME_H${NC}"
    echo -e "   ${CYAN}#define MIDIMIND_FILENAME_H${NC}"
    echo -e "   ${CYAN}// ... contenu ...${NC}"
    echo -e "   ${CYAN}#endif // MIDIMIND_FILENAME_H${NC}"
    echo ""
fi

if [ $CIRCULAR_FOUND -gt 0 ]; then
    echo -e "${YELLOW}2. Résoudre les inclusions circulaires:${NC}"
    echo ""
    echo -e "   ${CYAN}Forward declarations${NC} au lieu d'includes:"
    echo -e "   ${GREEN}// Au lieu de: #include \"MyClass.h\"${NC}"
    echo -e "   ${GREEN}class MyClass; // Forward declaration${NC}"
    echo ""
fi

echo -e "${YELLOW}3. Restructurer les dépendances:${NC}"
echo ""
echo -e "   - Utiliser forward declarations dans les .h"
echo -e "   - Mettre les #include dans les .cpp"
echo -e "   - Séparer interfaces et implémentations"
echo ""

echo -e "${YELLOW}4. Script de correction automatique disponible${NC}"
echo -e "   Voulez-vous que je génère un script pour corriger automatiquement ?"
echo ""
