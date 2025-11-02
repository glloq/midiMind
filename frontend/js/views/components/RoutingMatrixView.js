// ============================================================================
// Fichier: frontend/js/views/components/RoutingMatrixView.js
// Version: v4.0.0
// ============================================================================

class RoutingMatrixView {
    constructor(container, eventBus) {
        this.container = container;
        this.eventBus = eventBus;
        this.state = {
            sources: [],
            destinations: [],
            routes: []
        };
    }
    
    render() {
        if (!this.container) return;
        
        const { sources, destinations, routes } = this.state;
        
        if (sources.length === 0 || destinations.length === 0) {
            this.container.innerHTML = '<div class="matrix-empty">Aucun device connecté</div>';
            return;
        }
        
        let html = '<table class="routing-matrix"><thead><tr><th></th>';
        destinations.forEach(d => {
            html += `<th>${d.name}</th>`;
        });
        html += '</tr></thead><tbody>';
        
        sources.forEach(src => {
            html += `<tr><th>${src.name}</th>`;
            destinations.forEach(dst => {
                const route = routes.find(r => r.source_id === src.id && r.destination_id === dst.id);
                const active = route && route.enabled !== false;
                html += `<td class="matrix-cell ${route ? 'connected' : ''} ${!active ? 'disabled' : ''}"
                            data-source="${src.id}" data-dest="${dst.id}">
                            ${route ? (active ? '✓' : '✕') : ''}
                         </td>`;
            });
            html += '</tr>';
        });
        
        html += '</tbody></table>';
        this.container.innerHTML = html;
        
        this.attachEvents();
    }
    
    attachEvents() {
        if (!this.container) return;
        
        this.container.querySelectorAll('.matrix-cell').forEach(cell => {
            cell.addEventListener('click', () => {
                const src = cell.dataset.source;
                const dst = cell.dataset.dest;
                const hasRoute = cell.classList.contains('connected');
                
                if (this.eventBus) {
                    if (hasRoute) {
                        this.eventBus.emit('routing:remove_route_requested', {
                            source_id: src,
                            destination_id: dst
                        });
                    } else {
                        this.eventBus.emit('routing:add_route_requested', {
                            source_id: src,
                            destination_id: dst
                        });
                    }
                }
            });
        });
    }
    
    update(sources, destinations, routes) {
        this.state.sources = sources;
        this.state.destinations = destinations;
        this.state.routes = routes;
        this.render();
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = RoutingMatrixView;
}
if (typeof window !== 'undefined') {
    window.RoutingMatrixView = RoutingMatrixView;
}