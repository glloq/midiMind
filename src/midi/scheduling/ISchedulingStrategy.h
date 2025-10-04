// ============================================================================
// Fichier: src/midi/scheduling/ISchedulingStrategy.h
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Interface pour les stratégies de scheduling des messages MIDI.
//   Permet de définir différentes politiques de traitement des messages
//   (FIFO, priorité, round-robin, etc.).
//
// Design Pattern:
//   - Strategy Pattern (comportement interchangeable)
//   - Interface abstraite (pure virtual)
//
// Stratégies disponibles:
//   - FIFOScheduler : First In First Out (défaut, simple)
//   - PriorityQueueScheduler : File de priorité (messages temps-réel prioritaires)
//   - RoundRobinScheduler : Équitable entre les canaux
//   - DeadlineScheduler : Basé sur les timestamps
//
// Auteur: midiMind Team
// Date: 2025-10-02
// Version: 3.0.0
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <memory>              // Pour std::unique_ptr
#include <vector>              // Pour std::vector
#include "../MidiMessage.h"    // Pour MidiMessage

namespace midiMind {

// ============================================================================
// INTERFACE: ISchedulingStrategy
// ============================================================================

/**
 * @interface ISchedulingStrategy
 * @brief Interface pour les stratégies de scheduling des messages MIDI
 * 
 * Cette interface définit le contrat que toutes les stratégies de scheduling
 * doivent respecter. Le MidiRouter utilise cette interface pour décider
 * dans quel ordre traiter les messages MIDI en attente.
 * 
 * @details
 * Le pattern Strategy permet de:
 * - Changer dynamiquement l'algorithme de scheduling
 * - Tester facilement différentes stratégies
 * - Isoler la logique de scheduling du routeur
 * - Faciliter l'ajout de nouvelles stratégies
 * 
 * Cycle d'utilisation:
 * 1. Messages ajoutés via push()
 * 2. Messages récupérés via pop() selon la stratégie
 * 3. Taille consultée via size()
 * 4. Queue vidée via clear()
 * 
 * @note Thread-safety : Les implémentations doivent être thread-safe
 * 
 * @example Utilisation dans MidiRouter:
 * @code
 * auto strategy = std::make_unique<PriorityQueueScheduler>();
 * router->setSchedulingStrategy(std::move(strategy));
 * @endcode
 * 
 * @example Implémentation d'une stratégie simple (FIFO):
 * @code
 * class FIFOScheduler : public ISchedulingStrategy {
 * public:
 *     void push(const MidiMessage& message) override {
 *         std::lock_guard<std::mutex> lock(mutex_);
 *         queue_.push_back(message);
 *     }
 *     
 *     bool pop(MidiMessage& message) override {
 *         std::lock_guard<std::mutex> lock(mutex_);
 *         if (queue_.empty()) return false;
 *         message = queue_.front();
 *         queue_.erase(queue_.begin());
 *         return true;
 *     }
 *     
 *     size_t size() const override {
 *         std::lock_guard<std::mutex> lock(mutex_);
 *         return queue_.size();
 *     }
 *     
 *     bool empty() const override {
 *         return size() == 0;
 *     }
 *     
 *     void clear() override {
 *         std::lock_guard<std::mutex> lock(mutex_);
 *         queue_.clear();
 *     }
 *     
 * private:
 *     std::vector<MidiMessage> queue_;
 *     mutable std::mutex mutex_;
 * };
 * @endcode
 */
class ISchedulingStrategy {
public:
    // ========================================================================
    // DESTRUCTEUR VIRTUEL
    // ========================================================================
    
    /**
     * @brief Destructeur virtuel (requis pour interface)
     * 
     * Permet la destruction polymorphique correcte via pointeur de base.
     */
    virtual ~ISchedulingStrategy() = default;
    
    // ========================================================================
    // MÉTHODES VIRTUELLES PURES (OBLIGATOIRES)
    // ========================================================================
    
    /**
     * @brief Ajoute un message à la queue
     * 
     * Insère le message dans la structure de données selon la stratégie.
     * Pour FIFO : ajout à la fin
     * Pour Priority : insertion selon priorité
     * Pour RoundRobin : ajout dans la queue du canal
     * 
     * @param message Message MIDI à ajouter
     * 
     * @note Thread-safe : doit pouvoir être appelé depuis plusieurs threads
     * @note Ne doit pas bloquer longtemps (éviter allocations lourdes)
     * 
     * @example
     * @code
     * auto msg = MidiMessage::noteOn(0, 60, 100);
     * strategy->push(msg);
     * @endcode
     */
    virtual void push(const MidiMessage& message) = 0;
    
    /**
     * @brief Récupère et retire le prochain message selon la stratégie
     * 
     * Extrait le message le plus prioritaire selon l'algorithme:
     * - FIFO : premier arrivé
     * - Priority : message avec plus haute priorité
     * - RoundRobin : prochain canal dans le cycle
     * - Deadline : message avec timestamp le plus proche
     * 
     * @param message Référence où stocker le message (modifiée si retour true)
     * @return true Si un message a été récupéré
     * @return false Si la queue est vide
     * 
     * @note Thread-safe : doit pouvoir être appelé depuis plusieurs threads
     * @note Si retourne false, message n'est pas modifié
     * 
     * @example
     * @code
     * MidiMessage msg;
     * if (strategy->pop(msg)) {
     *     // Traiter le message
     *     router->processMessage(msg);
     * }
     * @endcode
     */
    virtual bool pop(MidiMessage& message) = 0;
    
    /**
     * @brief Récupère le nombre de messages en attente
     * 
     * @return size_t Nombre de messages dans la queue
     * 
     * @note Thread-safe
     * @note Peut être coûteux selon l'implémentation (O(1) recommandé)
     * 
     * @example
     * @code
     * if (strategy->size() > 1000) {
     *     Logger::warn("MidiRouter", "Message queue is getting large");
     * }
     * @endcode
     */
    virtual size_t size() const = 0;
    
    /**
     * @brief Vérifie si la queue est vide
     * 
     * @return true Si la queue est vide (size() == 0)
     * @return false Si la queue contient des messages
     * 
     * @note Thread-safe
     * @note Implémentation par défaut fournie (basée sur size())
     * @note Peut être overridée pour optimisation
     * 
     * @example
     * @code
     * while (!strategy->empty()) {
     *     MidiMessage msg;
     *     strategy->pop(msg);
     *     // Traiter msg
     * }
     * @endcode
     */
    virtual bool empty() const {
        return size() == 0;
    }
    
    /**
     * @brief Vide complètement la queue
     * 
     * Supprime tous les messages en attente. Utilisé lors de l'arrêt
     * du routeur ou en cas de reset.
     * 
     * @note Thread-safe
     * @note Après clear(), size() doit retourner 0
     * 
     * @example
     * @code
     * // Arrêt du routeur - vider la queue
     * strategy->clear();
     * @endcode
     */
    virtual void clear() = 0;
    
    // ========================================================================
    // MÉTHODES VIRTUELLES OPTIONNELLES
    // ========================================================================
    
    /**
     * @brief Récupère le nom de la stratégie
     * 
     * Utilisé pour logging et debug. Permet d'identifier quelle
     * stratégie est actuellement active.
     * 
     * @return std::string Nom de la stratégie
     * 
     * @note Implémentation par défaut retourne "Unknown"
     * 
     * @example
     * @code
     * Logger::info("MidiRouter", "Using scheduler: " + strategy->getName());
     * @endcode
     */
    virtual std::string getName() const {
        return "Unknown";
    }
    
    /**
     * @brief Récupère une description de la stratégie
     * 
     * Description plus détaillée du comportement de la stratégie.
     * Utilisée pour documentation et aide utilisateur.
     * 
     * @return std::string Description
     * 
     * @note Implémentation par défaut retourne une string vide
     * 
     * @example
     * @code
     * std::cout << "Current scheduler: " << strategy->getName() << "\n";
     * std::cout << strategy->getDescription() << "\n";
     * @endcode
     */
    virtual std::string getDescription() const {
        return "";
    }
    
    /**
     * @brief Récupère des statistiques sur la stratégie
     * 
     * Retourne des métriques spécifiques à la stratégie:
     * - Nombre de messages traités
     * - Latence moyenne
     * - Nombre de messages droppés
     * - Statistiques par priorité/canal
     * 
     * @return std::string Statistiques formatées
     * 
     * @note Implémentation par défaut retourne une string vide
     * @note Les implémentations peuvent retourner du JSON ou du texte
     * 
     * @example
     * @code
     * std::cout << strategy->getStatistics() << std::endl;
     * @endcode
     */
    virtual std::string getStatistics() const {
        return "";
    }
    
    /**
     * @brief Configure un paramètre de la stratégie
     * 
     * Permet de modifier dynamiquement le comportement de la stratégie.
     * Les paramètres disponibles dépendent de l'implémentation.
     * 
     * Exemples de paramètres:
     * - "max_queue_size" : Taille maximale de la queue
     * - "priority_boost" : Boost de priorité pour messages temps-réel
     * - "fairness_factor" : Facteur d'équité pour RoundRobin
     * 
     * @param key Nom du paramètre
     * @param value Valeur (string, sera parsée par l'implémentation)
     * @return true Si le paramètre a été appliqué
     * @return false Si le paramètre est inconnu ou invalide
     * 
     * @note Implémentation par défaut ne fait rien et retourne false
     * @note Thread-safe si implémenté
     * 
     * @example
     * @code
     * strategy->setParameter("max_queue_size", "1000");
     * strategy->setParameter("priority_boost", "2.0");
     * @endcode
     */
    virtual bool setParameter(const std::string& key, const std::string& value) {
        // Implémentation par défaut : paramètres non supportés
        return false;
    }
    
    /**
     * @brief Vérifie si la stratégie supporte les priorités
     * 
     * Indique si la stratégie tient compte des priorités des messages.
     * Utilisé pour optimisations (pas besoin de calculer priorité si non supporté).
     * 
     * @return true Si les priorités sont supportées
     * @return false Si tous les messages sont traités égaux
     * 
     * @note Implémentation par défaut retourne false
     * 
     * @example
     * @code
     * if (strategy->supportsPriority()) {
     *     // Calculer et assigner priorités
     *     msg.priority = calculatePriority(msg);
     * }
     * @endcode
     */
    virtual bool supportsPriority() const {
        return false;
    }
    
    /**
     * @brief Réinitialise les statistiques
     * 
     * Remet à zéro les compteurs de statistiques sans vider la queue.
     * 
     * @note Implémentation par défaut ne fait rien
     * @note Thread-safe si implémenté
     */
    virtual void resetStatistics() {
        // Implémentation par défaut : rien à faire
    }
};

} // namespace midiMind

// ============================================================================
// NOTES D'IMPLÉMENTATION
// ============================================================================

/**
 * @section impl_notes Notes pour l'Implémentation
 * 
 * @subsection thread_safety Thread Safety
 * Toutes les implémentations de ISchedulingStrategy DOIVENT être thread-safe
 * car le MidiRouter peut recevoir des messages depuis plusieurs sources
 * simultanément (player, API, devices).
 * 
 * Recommandations:
 * - Utiliser std::mutex pour protéger les structures de données
 * - Préférer std::lock_guard pour éviter les oublis de unlock
 * - Minimiser la durée des locks (pas d'I/O dans les sections critiques)
 * - Considérer std::atomic pour les compteurs simples
 * 
 * @subsection performance Performance
 * Les méthodes push() et pop() sont dans le chemin critique du routage MIDI.
 * Elles sont appelées potentiellement des milliers de fois par seconde.
 * 
 * Recommandations:
 * - Viser O(log n) ou mieux pour push/pop
 * - Éviter les allocations mémoire dans push/pop si possible
 * - Utiliser des structures de données optimisées (std::priority_queue, etc.)
 * - Profiler les performances sur Raspberry Pi
 * 
 * @subsection priority Calcul de Priorité
 * Si votre stratégie supporte les priorités, voici les recommandations:
 * 
 * Priorités suggérées (plus haut = plus prioritaire):
 * - 10: Messages temps-réel (Clock, Start, Stop)
 * - 8:  Note On/Off (timing critique)
 * - 5:  Control Change (moins critique)
 * - 3:  Program Change
 * - 1:  SysEx (peut attendre)
 * 
 * @subsection memory Gestion Mémoire
 * Attention à la croissance illimitée de la queue si le traitement
 * est plus lent que l'arrivée des messages.
 * 
 * Stratégies:
 * - Implémenter une taille maximum (drop les nouveaux si pleine)
 * - Drop les anciens messages si queue pleine (comportement FIFO)
 * - Logger des warnings si la queue devient trop grande
 */

// ============================================================================
// FIN DU FICHIER ISchedulingStrategy.h
// ============================================================================
