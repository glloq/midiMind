<!--++controllers/SearchController.js++-->
        // ===== SEARCH CONTROLLER =====
        class SearchController extends BaseController {
            constructor(eventBus, models, views, notifications, debugConsole) {
                super(eventBus, models, views, notifications, debugConsole);
                this.searchResults = [];
                this.currentQuery = '';
            }

            bindEvents() {
                // Écouter les changements dans les modèles pour mettre à jour l'index
                this.eventBus.on('file:added', () => this.updateSearchIndex());
                this.eventBus.on('playlist:added', () => this.updateSearchIndex());
            }

            search(query) {
                this.currentQuery = query.toLowerCase().trim();
                
                if (this.currentQuery.length < 2) {
                    this.searchResults = [];
                    this.eventBus.emit('search:results', { results: [], query: this.currentQuery });
                    return [];
                }
                
                const files = this.getModel('file').get('files');
                const playlists = this.getModel('playlist').get('playlists');
                const instruments = this.getModel('instrument').get('instruments');
                
                this.searchResults = [];
                
                // Rechercher dans les fichiers
                files.forEach(file => {
                    if (this.matchesQuery(file, this.currentQuery)) {
                        this.searchResults.push({
                            type: 'file',
                            item: file,
                            relevance: this.calculateRelevance(file, this.currentQuery)
                        });
                    }
                });
                
                // Rechercher dans les playlists
                playlists.forEach(playlist => {
                    if (this.matchesQuery(playlist, this.currentQuery)) {
                        this.searchResults.push({
                            type: 'playlist',
                            item: playlist,
                            relevance: this.calculateRelevance(playlist, this.currentQuery)
                        });
                    }
                });
                
                // Rechercher dans les instruments
                instruments.forEach(instrument => {
                    if (this.matchesQuery(instrument, this.currentQuery)) {
                        this.searchResults.push({
                            type: 'instrument',
                            item: instrument,
                            relevance: this.calculateRelevance(instrument, this.currentQuery)
                        });
                    }
                });
                
                // Trier par pertinence
                this.searchResults.sort((a, b) => b.relevance - a.relevance);
                
                this.logDebug('system', `Recherche "${query}": ${this.searchResults.length} résultat(s)`);
                this.eventBus.emit('search:results', { results: this.searchResults, query: this.currentQuery });
                
                return this.searchResults;
            }

            matchesQuery(item, query) {
                const searchableText = this.getSearchableText(item).toLowerCase();
                return searchableText.includes(query);
            }

            getSearchableText(item) {
                let text = '';
                
                if (item.name) text += item.name + ' ';
                if (item.description) text += item.description + ' ';
                if (item.manufacturer) text += item.manufacturer + ' ';
                if (item.model) text += item.model + ' ';
                if (item.type) text += item.type + ' ';
                if (item.tracks) {
                    item.tracks.forEach(track => {
                        text += track.name + ' ' + track.instrument + ' ';
                    });
                }
                
                return text;
            }

            calculateRelevance(item, query) {
                const text = this.getSearchableText(item).toLowerCase();
                let relevance = 0;
                
                // Correspondance exacte dans le nom
                if (item.name && item.name.toLowerCase().includes(query)) {
                    relevance += 10;
                }
                
                // Correspondance au début du nom
                if (item.name && item.name.toLowerCase().startsWith(query)) {
                    relevance += 5;
                }
                
                // Correspondance dans la description
                if (item.description && item.description.toLowerCase().includes(query)) {
                    relevance += 3;
                }
                
                // Correspondance générale
                const matches = (text.match(new RegExp(query, 'gi')) || []).length;
                relevance += matches;
                
                return relevance;
            }

            clearSearch() {
                this.searchResults = [];
                this.currentQuery = '';
                this.eventBus.emit('search:cleared', {});
            }

            updateSearchIndex() {
                // Re-lancer la recherche actuelle si nécessaire
                if (this.currentQuery) {
                    this.search(this.currentQuery);
                }
            }
        }
