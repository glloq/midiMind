<!--++controllers/DebugController.js++-->
        // ===== DEBUG CONTROLLER =====
        class DebugController extends BaseController {
            constructor(eventBus, models, views, notifications, debugConsole) {
                super(eventBus, models, views, notifications, debugConsole);
            }

            bindEvents() {
                this.eventBus.on('debug:toggled', (data) => {
                    this.logDebug('system', `Debug panel ${data.active ? 'ouvert' : 'fermé'}`);
                });
            }

            toggle() {
                this.debugConsole.toggle();
            }

            toggleFilter(filter) {
                this.debugConsole.toggleFilter(filter);
            }
        }