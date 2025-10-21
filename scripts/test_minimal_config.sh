#!/bin/bash
# ============================================================================
# Test de Config::load() isolé
# ============================================================================

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     Test Config::load() Minimal                              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

cd /tmp

# Copier le test
cat > test_config.cpp << 'CPPEOF'
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main() {
    std::cout << "=== Test parsing JSON minimal ===" << std::endl;
    
    try {
        std::cout << "1. Opening file..." << std::endl;
        std::ifstream file("/etc/midimind/config.json");
        if (!file.is_open()) {
            std::cerr << "Cannot open file" << std::endl;
            return 1;
        }
        
        std::cout << "2. Parsing JSON..." << std::endl;
        json config;
        file >> config;
        file.close();
        
        std::cout << "3. Accessing api.port..." << std::endl;
        int port = config["api"]["port"];
        std::cout << "   Port: " << port << std::endl;
        
        std::cout << "4. Creating defaults JSON..." << std::endl;
        json defaults = json::parse(R"({
            "api": {
                "port": 8080
            }
        })");
        
        std::cout << "5. Merging..." << std::endl;
        for (auto it = config.begin(); it != config.end(); ++it) {
            defaults[it.key()] = it.value();
        }
        
        std::cout << "=== SUCCESS ===" << std::endl;
        return 0;
        
    } catch (const json::exception& e) {
        std::cerr << "JSON exception: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 2;
    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
        return 3;
    }
}
CPPEOF

echo "Compilation..."
g++ -std=c++17 test_config.cpp -o test_config 2>&1

if [ $? -eq 0 ]; then
    echo "✓ Compilation OK"
    echo ""
    echo "Exécution:"
    ./test_config
    EXIT_CODE=$?
    echo ""
    echo "Exit code: $EXIT_CODE"
else
    echo "✗ Compilation échouée"
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     Fin du test                                              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
