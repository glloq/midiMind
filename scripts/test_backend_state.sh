#!/bin/bash
# ============================================================================
# Test : Le backend est-il bloqué ou se termine-t-il ?
# ============================================================================

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     Test Backend - Bloqué ou Terminé ?                      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

echo "1. Redémarrage du service"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
sudo systemctl restart midimind
echo "✓ Service redémarré"
echo ""

echo "2. Attente 2 secondes..."
sleep 2
echo ""

echo "3. État du processus"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if pgrep -f "/opt/midimind/bin/midimind" > /dev/null; then
    PID=$(pgrep -f "/opt/midimind/bin/midimind")
    echo "✓ Processus trouvé - PID: $PID"
    echo ""
    
    echo "4. Informations du processus"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    ps -p $PID -o pid,ppid,state,cmd
    echo ""
    
    STATE=$(ps -p $PID -o state=)
    case $STATE in
        "S")
            echo "État: S (Sleeping/Interruptible) - Processus en attente"
            echo "→ Le backend est BLOQUÉ en attente de quelque chose"
            ;;
        "R")
            echo "État: R (Running) - Processus actif"
            echo "→ Le backend tourne normalement"
            ;;
        "D")
            echo "État: D (Uninterruptible sleep) - Attente I/O"
            echo "→ Le backend attend une opération I/O"
            ;;
        "Z")
            echo "État: Z (Zombie) - Processus mort"
            echo "→ Le backend a crashé"
            ;;
        *)
            echo "État: $STATE"
            ;;
    esac
    echo ""
    
    echo "5. Threads du processus"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    ps -T -p $PID | head -10
    echo ""
    
    echo "6. Fichiers ouverts"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    sudo lsof -p $PID 2>/dev/null | head -20
    echo ""
    
    echo "7. Stack trace (où est bloqué le processus)"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Utilisation de gdb pour voir la stack..."
    sudo gdb -batch -ex "attach $PID" -ex "thread apply all bt" -ex "detach" -ex "quit" 2>&1 | head -50
    echo ""
    
    echo "8. Syscalls en cours (strace 1 seconde)"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    timeout 1 sudo strace -p $PID 2>&1 | head -20
    echo ""
    
else
    echo "✗ Processus NON trouvé"
    echo ""
    echo "Le backend se TERMINE immédiatement après le démarrage"
    echo ""
    
    echo "4. Derniers logs (plus détaillés)"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    journalctl -u midimind -n 50 --no-pager
    echo ""
    
    echo "5. Code de sortie"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    systemctl show midimind -p ExecMainStatus --value
    systemctl show midimind -p Result --value
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     Fin du test                                              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
