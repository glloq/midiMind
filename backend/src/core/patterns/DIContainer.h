// ============================================================================
// Fichier: src/core/patterns/DIContainer.h
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Conteneur d'injection de dépendances (Dependency Injection Container).
//   Permet d'enregistrer et de résoudre des dépendances de manière centralisée,
//   facilitant le découplage et les tests.
//
// Fonctionnalités:
//   - Enregistrement de singletons
//   - Résolution de dépendances par type
//   - Thread-safe
//   - Vérification d'existence
//   - Clear pour réinitialisation
//
// Design Pattern:
//   - Dependency Injection (IoC - Inversion of Control)
//   - Service Locator Pattern
//   - Singleton (pour le conteneur lui-même)
//
// Auteur: midiMind Team
// Date: 2025-10-02
// Version: 3.0.0
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES SYSTÈME
// ============================================================================
#include <memory>        // Pour std::shared_ptr
#include <unordered_map> // Pour std::unordered_map
#include <typeindex>     // Pour std::type_index
#include <mutex>         // Pour std::mutex (thread-safety)
#include <stdexcept>     // Pour std::runtime_error
#include <string>        // Pour std::string

namespace midiMind {

// ============================================================================
// CLASSE: DIContainer
// ============================================================================

/**
 * @class DIContainer
 * @brief Conteneur d'injection de dépendances
 * 
 * Ce conteneur permet d'enregistrer des instances de classes (typiquement
 * des singletons) et de les résoudre par la suite par leur type. Facilite
 * le découplage entre les modules et simplifie les tests.
 * 
 * @details
 * Le conteneur stocke des std::shared_ptr<void> avec leur type_index comme clé.
 * Lors de la résolution, le type est vérifié et un cast est effectué.
 * 
 * Pattern utilisé: Service Locator combiné avec Dependency Injection.
 * 
 * Avantages:
 * - Découplage: les classes n'ont pas besoin de connaître comment créer
 *   leurs dépendances
 * - Centralisation: toutes les dépendances sont gérées en un seul endroit
 * - Testabilité: facile de remplacer une dépendance par un mock pour les tests
 * - Cycle de vie: le conteneur gère la durée de vie des objets
 * 
 * @note Thread-safe : peut être utilisé depuis n'importe quel thread
 * @note Singleton : une seule instance pour toute l'application
 * 
 * @example Enregistrement et résolution:
 * @code
 * // Dans Application::Application()
 * auto& di = DIContainer::instance();
 * 
 * // Enregistrer les singletons
 * di.registerSingleton<MidiDeviceManager>(deviceManager_);
 * di.registerSingleton<MidiRouter>(router_);
 * di.registerSingleton<ApiServer>(apiServer_);
 * 
 * // Ailleurs dans le code, résoudre les dépendances
 * auto deviceMgr = DIContainer::instance().resolve<MidiDeviceManager>();
 * auto router = DIContainer::instance().resolve<MidiRouter>();
 * @endcode
 * 
 * @example Vérifier l'existence:
 * @code
 * if (DIContainer::instance().has<MidiPlayer>()) {
 *     auto player = DIContainer::instance().resolve<MidiPlayer>();
 *     player->play();
 * }
 * @endcode
 */
class DIContainer {
public:
    // ========================================================================
    // SINGLETON - ACCÈS À L'INSTANCE
    // ========================================================================
    
    /**
     * @brief Récupère l'instance unique du conteneur (Singleton)
     * 
     * @return DIContainer& Référence à l'instance unique
     * 
     * @note Thread-safe depuis C++11 (Meyer's Singleton)
     */
    static DIContainer& instance() {
        static DIContainer instance;
        return instance;
    }
    
    // ========================================================================
    // DÉSACTIVATION COPIE ET ASSIGNATION
    // ========================================================================
    
    DIContainer(const DIContainer&) = delete;
    DIContainer& operator=(const DIContainer&) = delete;
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - ENREGISTREMENT
    // ========================================================================
    
    /**
     * @brief Enregistre un singleton dans le conteneur
     * 
     * Stocke un shared_ptr vers l'instance, accessible par son type.
     * Si une instance du même type existe déjà, elle est remplacée.
     * 
     * @tparam T Type de l'objet à enregistrer
     * @param instance shared_ptr vers l'instance
     * 
     * @note Thread-safe
     * @note Remplace silencieusement une instance existante du même type
     * 
     * @example
     * @code
     * auto deviceMgr = std::make_shared<MidiDeviceManager>();
     * DIContainer::instance().registerSingleton<MidiDeviceManager>(deviceMgr);
     * @endcode
     */
    template<typename T>
    void registerSingleton(std::shared_ptr<T> instance) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Obtenir le type_index pour utiliser comme clé
        std::type_index typeIndex(typeid(T));
        
        // Stocker le shared_ptr (cast vers void*)
        instances_[typeIndex] = instance;
    }
    
    /**
     * @brief Enregistre un singleton à partir d'un pointeur brut
     * 
     * Crée un shared_ptr à partir du pointeur brut et l'enregistre.
     * 
     * @tparam T Type de l'objet
     * @param instance Pointeur brut vers l'instance
     * 
     * @warning L'appelant doit s'assurer que l'objet reste valide
     *          ou transférer la propriété au shared_ptr
     * 
     * @note Thread-safe
     * 
     * @example
     * @code
     * MidiRouter* router = new MidiRouter();
     * DIContainer::instance().registerSingleton<MidiRouter>(router);
     * @endcode
     */
    template<typename T>
    void registerSingleton(T* instance) {
        registerSingleton<T>(std::shared_ptr<T>(instance));
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - RÉSOLUTION
    // ========================================================================
    
    /**
     * @brief Résout une dépendance par son type
     * 
     * Récupère l'instance enregistrée pour le type T.
     * 
     * @tparam T Type de l'objet à résoudre
     * @return std::shared_ptr<T> Pointeur vers l'instance
     * 
     * @throws std::runtime_error Si aucune instance de ce type n'est enregistrée
     * 
     * @note Thread-safe
     * 
     * @example
     * @code
     * try {
     *     auto router = DIContainer::instance().resolve<MidiRouter>();
     *     router->start();
     * } catch (const std::runtime_error& e) {
     *     std::cerr << "Dependency not found: " << e.what() << std::endl;
     * }
     * @endcode
     */
    template<typename T>
    std::shared_ptr<T> resolve() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::type_index typeIndex(typeid(T));
        
        auto it = instances_.find(typeIndex);
        if (it == instances_.end()) {
            throw std::runtime_error(
                "DIContainer: No instance registered for type: " + 
                std::string(typeid(T).name())
            );
        }
        
        // Cast du shared_ptr<void> vers shared_ptr<T>
        return std::static_pointer_cast<T>(it->second);
    }
    
    /**
     * @brief Tente de résoudre une dépendance sans lancer d'exception
     * 
     * Comme resolve() mais retourne nullptr si l'instance n'existe pas,
     * au lieu de lancer une exception.
     * 
     * @tparam T Type de l'objet à résoudre
     * @return std::shared_ptr<T> Pointeur vers l'instance, ou nullptr si non trouvée
     * 
     * @note Thread-safe
     * 
     * @example
     * @code
     * auto player = DIContainer::instance().tryResolve<MidiPlayer>();
     * if (player) {
     *     player->play();
     * } else {
     *     std::cerr << "Player not available" << std::endl;
     * }
     * @endcode
     */
    template<typename T>
    std::shared_ptr<T> tryResolve() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::type_index typeIndex(typeid(T));
        
        auto it = instances_.find(typeIndex);
        if (it == instances_.end()) {
            return nullptr;
        }
        
        return std::static_pointer_cast<T>(it->second);
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - VÉRIFICATION ET GESTION
    // ========================================================================
    
    /**
     * @brief Vérifie si une instance du type T est enregistrée
     * 
     * @tparam T Type à vérifier
     * @return true Si une instance est enregistrée
     * @return false Si aucune instance n'est enregistrée
     * 
     * @note Thread-safe
     * 
     * @example
     * @code
     * if (DIContainer::instance().has<MidiPlayer>()) {
     *     auto player = DIContainer::instance().resolve<MidiPlayer>();
     * } else {
     *     std::cerr << "Player not initialized" << std::endl;
     * }
     * @endcode
     */
    template<typename T>
    bool has() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::type_index typeIndex(typeid(T));
        return instances_.find(typeIndex) != instances_.end();
    }
    
    /**
     * @brief Supprime une instance enregistrée
     * 
     * Retire l'instance du conteneur. Le shared_ptr sera détruit
     * si c'était la dernière référence.
     * 
     * @tparam T Type de l'instance à supprimer
     * @return true Si l'instance a été supprimée
     * @return false Si aucune instance de ce type n'était enregistrée
     * 
     * @note Thread-safe
     * 
     * @example
     * @code
     * DIContainer::instance().remove<MidiPlayer>();
     * @endcode
     */
    template<typename T>
    bool remove() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::type_index typeIndex(typeid(T));
        
        auto it = instances_.find(typeIndex);
        if (it == instances_.end()) {
            return false;
        }
        
        instances_.erase(it);
        return true;
    }
    
    /**
     * @brief Efface toutes les instances enregistrées
     * 
     * Supprime toutes les références du conteneur. Les objets seront
     * détruits si le conteneur détenait la dernière référence.
     * 
     * @note Thread-safe
     * 
     * @example
     * @code
     * // Avant de réinitialiser l'application
     * DIContainer::instance().clear();
     * @endcode
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        instances_.clear();
    }
    
    /**
     * @brief Récupère le nombre d'instances enregistrées
     * 
     * @return size_t Nombre d'instances dans le conteneur
     * 
     * @note Thread-safe
     */
    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return instances_.size();
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - DEBUG / INTROSPECTION
    // ========================================================================
    
    /**
     * @brief Affiche toutes les instances enregistrées (debug)
     * 
     * Affiche les types enregistrés dans la console pour debug.
     * 
     * @note Thread-safe
     * @note Utilisé uniquement pour debug
     * 
     * @example
     * @code
     * #ifdef DEBUG
     *     DIContainer::instance().printRegistered();
     * #endif
     * @endcode
     */
    void printRegistered() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::cout << "DIContainer - Registered instances (" << instances_.size() << "):\n";
        for (const auto& pair : instances_) {
            std::cout << "  - " << pair.first.name() << "\n";
        }
        std::cout << std::endl;
    }

private:
    // ========================================================================
    // CONSTRUCTEUR PRIVÉ (SINGLETON)
    // ========================================================================
    
    /**
     * @brief Constructeur privé (Singleton)
     */
    DIContainer() = default;
    
    /**
     * @brief Destructeur
     * 
     * Appelle clear() pour libérer toutes les références.
     */
    ~DIContainer() {
        clear();
    }
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /**
     * @brief Mutex pour thread-safety
     * 
     * Protège l'accès concurrent à instances_.
     * Mutable pour permettre le lock dans les méthodes const.
     */
    mutable std::mutex mutex_;
    
    /**
     * @brief Map des instances enregistrées
     * 
     * Clé: std::type_index (identifiant unique du type)
     * Valeur: std::shared_ptr<void> (pointeur générique vers l'instance)
     * 
     * Le cast vers le type concret est effectué lors de resolve().
     */
    std::unordered_map<std::type_index, std::shared_ptr<void>> instances_;
};

} // namespace midiMind

// ============================================================================
// EXEMPLES D'UTILISATION AVANCÉS
// ============================================================================

/**
 * @example Utilisation typique dans Application
 * @code
 * // Dans Application::Application()
 * void Application::setupDependencyInjection() {
 *     auto& di = DIContainer::instance();
 *     
 *     // Enregistrer tous les singletons
 *     di.registerSingleton<Config>(Config::instance());
 *     di.registerSingleton<Logger>(Logger::instance());
 *     di.registerSingleton<MidiDeviceManager>(deviceManager_);
 *     di.registerSingleton<MidiRouter>(router_);
 *     di.registerSingleton<MidiPlayer>(player_);
 *     di.registerSingleton<ApiServer>(apiServer_);
 * }
 * 
 * // Dans un autre module
 * class MyCommand : public ICommand {
 *     json execute() override {
 *         // Résoudre les dépendances
 *         auto router = DIContainer::instance().resolve<MidiRouter>();
 *         auto player = DIContainer::instance().resolve<MidiPlayer>();
 *         
 *         // Utiliser les dépendances
 *         router->routeMessage(...);
 *         player->play();
 *         
 *         return jsonSuccess();
 *     }
 * };
 * @endcode
 */

/**
 * @example Tests unitaires avec mocks
 * @code
 * class MockMidiRouter : public MidiRouter {
 *     // Implementation mock...
 * };
 * 
 * TEST(MyCommandTest, Execute) {
 *     // Créer un mock
 *     auto mockRouter = std::make_shared<MockMidiRouter>();
 *     
 *     // Enregistrer le mock dans le DI Container
 *     DIContainer::instance().registerSingleton<MidiRouter>(mockRouter);
 *     
 *     // Tester la commande (qui utilisera le mock)
 *     MyCommand cmd;
 *     auto result = cmd.execute();
 *     
 *     // Vérifier le résultat
 *     EXPECT_TRUE(result["success"]);
 *     
 *     // Cleanup
 *     DIContainer::instance().clear();
 * }
 * @endcode
 */

// ============================================================================
// FIN DU FICHIER DIContainer.h
// ============================================================================
