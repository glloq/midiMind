#!/bin/bash
# ============================================================================
# Diagnostic approfondi - Crash dans Config::load()
# ============================================================================

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     Diagnostic Config Crash                                  ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

CONFIG_FILE="/etc/midimind/config.json"

echo "1. Vérification fichier de configuration"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if [ -f "$CONFIG_FILE" ]; then
    echo "✓ Fichier existe"
    ls -lh "$CONFIG_FILE"
    echo ""
    
    echo "Permissions:"
    stat -c "Mode: %a (%A) Owner: %U:%G" "$CONFIG_FILE"
    echo ""
    
    echo "Type de fichier:"
    file "$CONFIG_FILE"
    echo ""
    
    echo "Encodage:"
    file -i "$CONFIG_FILE"
    echo ""
    
    echo "Vérification BOM UTF-8:"
    hexdump -C "$CONFIG_FILE" | head -1
    echo ""
else
    echo "✗ Fichier manquant"
    exit 1
fi

echo "2. Validation JSON"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if command -v jq &> /dev/null; then
    echo "Test avec jq:"
    if jq empty "$CONFIG_FILE" 2>&1; then
        echo "✓ JSON valide (jq)"
    else
        echo "✗ JSON invalide"
    fi
else
    echo "jq non installé, utilisation de python3"
fi

echo ""
echo "Test avec python3:"
python3 << 'PYEOF'
import json
import sys

try:
    with open('/etc/midimind/config.json', 'r') as f:
        config = json.load(f)
    print("✓ JSON valide (python)")
    print(f"  Clés principales: {list(config.keys())}")
except Exception as e:
    print(f"✗ Erreur: {e}")
    sys.exit(1)
PYEOF

echo ""
echo "3. Version nlohmann-json"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
dpkg -l | grep nlohmann
echo ""

if [ -f "/usr/include/nlohmann/json.hpp" ]; then
    echo "✓ Header trouvé: /usr/include/nlohmann/json.hpp"
    grep "NLOHMANN_JSON_VERSION" /usr/include/nlohmann/json.hpp | head -3
else
    echo "✗ Header non trouvé"
fi

echo ""
echo "4. Compilation test"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

cat > /tmp/test_json.cpp << 'CPPEOF'
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

int main() {
    try {
        std::ifstream file("/etc/midimind/config.json");
        if (!file.is_open()) {
            std::cerr << "Cannot open file" << std::endl;
            return 1;
        }
        
        json config;
        file >> config;
        file.close();
        
        std::cout << "✓ Parsing OK" << std::endl;
        std::cout << "Keys: " << config.size() << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Exception: " << e.what() << std::endl;
        return 1;
    }
}
CPPEOF

echo "Compilation du test..."
g++ -std=c++17 /tmp/test_json.cpp -o /tmp/test_json 2>&1
if [ $? -eq 0 ]; then
    echo "✓ Compilation OK"
    echo ""
    echo "Exécution:"
    /tmp/test_json
    TEST_RESULT=$?
    echo ""
    if [ $TEST_RESULT -eq 0 ]; then
        echo "✓ Test JSON OK - Le problème n'est PAS le parsing JSON"
    else
        echo "✗ Test JSON échoue - Problème de parsing confirmé"
    fi
else
    echo "✗ Compilation échouée"
fi

echo ""
echo "5. Logs backend détaillés"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Dernières 30 lignes:"
journalctl -u midimind.service -n 30 --no-pager

echo ""
echo "6. Recherche segfault/crash"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
journalctl -u midimind.service | grep -i "segfault\|signal\|core\|crash\|fault" | tail -10

echo ""
echo "7. Vérification binary backend"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
BACKEND="/opt/midimind/bin/midimind"
if [ -f "$BACKEND" ]; then
    echo "✓ Binary existe"
    ls -lh "$BACKEND"
    echo ""
    echo "Dépendances nlohmann:"
    ldd "$BACKEND" | grep -i json || echo "  (statiquement linké ou non trouvé)"
    echo ""
    echo "Symboles nlohmann (sample):"
    nm "$BACKEND" | grep -i nlohmann | head -5 || echo "  (aucun symbole visible)"
else
    echo "✗ Binary manquant"
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     Fin du diagnostic                                        ║"
echo "╚══════════════════════════════════════════════════════════════╝"
