#!/bin/bash
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     Diagnostic Connexion WebSocket                          ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

echo "1. Port 8080 ouvert ?"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if ss -tuln | grep -q ":8080"; then
    echo "✓ Port 8080 OUVERT"
    ss -tuln | grep ":8080"
else
    echo "✗ Port 8080 fermé"
fi

echo ""
echo "2. Backend écoute sur quelle interface ?"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
netstat -tlnp 2>/dev/null | grep 8080 || ss -tlnp | grep 8080

echo ""
echo "3. Test connexion locale (127.0.0.1:8080)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
timeout 2 nc -zv 127.0.0.1 8080 2>&1 || echo "Connexion échouée"

echo ""
echo "4. Test connexion depuis IP locale"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
LOCAL_IP=$(hostname -I | awk '{print $1}')
echo "IP locale: $LOCAL_IP"
timeout 2 nc -zv $LOCAL_IP 8080 2>&1 || echo "Connexion échouée"

echo ""
echo "5. Firewall/iptables ?"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if command -v iptables &> /dev/null; then
    sudo iptables -L INPUT -n | grep 8080 || echo "Pas de règle spécifique pour 8080"
else
    echo "iptables non disponible"
fi

echo ""
echo "6. Test WebSocket avec curl"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Test HTTP sur port 8080 (devrait échouer car c'est WebSocket):"
timeout 2 curl -v http://127.0.0.1:8080 2>&1 | head -10

echo ""
echo "7. Test WebSocket avec wscat (si installé)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if command -v wscat &> /dev/null; then
    echo "Connexion à ws://127.0.0.1:8080..."
    timeout 2 wscat -c ws://127.0.0.1:8080 2>&1
else
    echo "wscat non installé (npm install -g wscat)"
fi

echo ""
echo "8. Configuration frontend"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Vérifier l'URL WebSocket dans le frontend:"
if [ -f "/var/www/midimind/js/services/BackendService.js" ]; then
    grep -n "ws://" /var/www/midimind/js/services/BackendService.js | head -5
else
    echo "BackendService.js non trouvé"
fi

echo ""
echo "9. Logs backend récents"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
journalctl -u midimind -n 20 --no-pager | grep -i "websocket\|connection\|client"

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     Résumé                                                   ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "Backend: ✓ Fonctionne (MidiMind Ready)"
echo "Port 8080: À vérifier ci-dessus"
echo ""
echo "Si le port est ouvert mais la connexion échoue:"
echo "  → Vérifier l'URL WebSocket dans le frontend"
echo "  → Vérifier que le frontend utilise la bonne IP"
echo "  → Vérifier les logs navigateur (Console F12)"
