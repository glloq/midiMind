// ============================================================================
// Fichier: frontend/js/ui/NotificationManager.js
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   Gestionnaire centralisé de toutes les notifications de l'application.
//   Interface unifiée pour notifications, alertes, confirmations.
//
// Fonctionnalités:
//   - Notifications toast (success, error, warning, info)
//   - Alertes modales (alert, confirm, prompt)
//   - Badges de notifications (compteurs)
//   - Notifications système (si permission)
//   - Queue prioritaire
//   - Grouping similaires (anti-spam)
//   - Historique notifications
//   - Préférences utilisateur (son, vibration)
//
// Architecture:
//   NotificationManager (classe singleton)
//   - Utilise Notifications (utils/) pour toasts
//   - Utilise Modal (views/components/) pour alertes
//   - Notification API pour système
//
// Auteur: MidiMind Team
// ============================================================================
/**
 * @class NotificationManager
 * @description Gestionnaire de notifications toast
 */
class NotificationManager {
    constructor() {
        this.container = null;
        this.notifications = [];
        this.maxNotifications = 5;
        this.defaultDuration = 3000; // ms
        
        this.init();
    }

    /**
     * Initialise le gestionnaire
     */
    init() {
        // Créer le container de notifications
        this.container = document.createElement('div');
        this.container.className = 'notification-container';
        document.body.appendChild(this.container);
    }

    /**
     * Affiche une notification
     * @param {string} message - Message à afficher
     * @param {string} type - Type: success, error, warning, info
     * @param {number} duration - Durée en ms (0 = pas d'auto-dismiss)
     * @param {object} options - Options additionnelles
     */
    show(message, type = 'info', duration = null, options = {}) {
        // Limiter le nombre de notifications
        if (this.notifications.length >= this.maxNotifications) {
            this.dismissOldest();
        }

        // Créer l'élément de notification
        const notification = this.createNotification(message, type, options);
        
        // Ajouter au container
        this.container.appendChild(notification.element);
        this.notifications.push(notification);

        // Animation d'entrée
        setTimeout(() => {
            notification.element.classList.add('show');
        }, 10);

        // Auto-dismiss si durée définie
        const dismissDuration = duration !== null ? duration : this.defaultDuration;
        if (dismissDuration > 0) {
            notification.timer = setTimeout(() => {
                this.dismiss(notification.id);
            }, dismissDuration);
        }

        return notification.id;
    }

    /**
     * Crée l'élément de notification
     */
    createNotification(message, type, options) {
        const id = `notif_${Date.now()}_${Math.random()}`;
        
        const element = document.createElement('div');
        element.className = `notification notification-${type}`;
        element.dataset.id = id;

        // Icône selon le type
        const icon = this.getIcon(type);
        
        // Action button (optionnel)
        const actionButton = options.action ? `
            <button class="notification-action" onclick="${options.action.handler}">
                ${options.action.label}
            </button>
        ` : '';

        element.innerHTML = `
            <div class="notification-icon">${icon}</div>
            <div class="notification-content">
                <div class="notification-message">${this.escapeHtml(message)}</div>
                ${options.details ? `<div class="notification-details">${this.escapeHtml(options.details)}</div>` : ''}
            </div>
            ${actionButton}
            <button class="notification-close" onclick="notificationManager.dismiss('${id}')">
                ×
            </button>
        `;

        return {
            id: id,
            element: element,
            timer: null
        };
    }

    /**
     * Obtient l'icône selon le type
     */
    getIcon(type) {
        const icons = {
            success: '✓',
            error: '✕',
            warning: '⚠',
            info: 'ℹ'
        };
        return icons[type] || icons.info;
    }

    /**
     * Dismiss une notification
     */
    dismiss(id) {
        const notification = this.notifications.find(n => n.id === id);
        if (!notification) return;

        // Annuler le timer
        if (notification.timer) {
            clearTimeout(notification.timer);
        }

        // Animation de sortie
        notification.element.classList.remove('show');
        notification.element.classList.add('hide');

        // Retirer du DOM après l'animation
        setTimeout(() => {
            if (notification.element.parentNode) {
                notification.element.parentNode.removeChild(notification.element);
            }
            
            // Retirer du tableau
            const index = this.notifications.findIndex(n => n.id === id);
            if (index !== -1) {
                this.notifications.splice(index, 1);
            }
        }, 300); // Durée de l'animation
    }

    /**
     * Dismiss la plus ancienne notification
     */
    dismissOldest() {
        if (this.notifications.length > 0) {
            this.dismiss(this.notifications[0].id);
        }
    }

    /**
     * Dismiss toutes les notifications
     */
    dismissAll() {
        // Copier le tableau car dismiss modifie this.notifications
        const notifs = [...this.notifications];
        notifs.forEach(n => this.dismiss(n.id));
    }

    /**
     * Raccourcis pour les différents types
     */
    success(message, duration = null, options = {}) {
        return this.show(message, 'success', duration, options);
    }

    error(message, duration = 5000, options = {}) {
        return this.show(message, 'error', duration, options);
    }

    warning(message, duration = 4000, options = {}) {
        return this.show(message, 'warning', duration, options);
    }

    info(message, duration = null, options = {}) {
        return this.show(message, 'info', duration, options);
    }

    /**
     * Échappe le HTML pour éviter XSS
     */
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    /**
     * Nettoie les ressources
     */
    destroy() {
        this.dismissAll();
        
        if (this.container && this.container.parentNode) {
            this.container.parentNode.removeChild(this.container);
        }
        
        this.container = null;
        this.notifications = [];
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = NotificationManager;
}

// Instance globale
if (typeof window !== 'undefined') {
    window.NotificationManager = NotificationManager;
}
window.NotificationManager = NotificationManager;
// ============================================================================
// FIN DU FICHIER NotificationManager.js
// ============================================================================
