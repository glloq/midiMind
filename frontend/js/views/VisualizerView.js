// ============================================================================
// Fichier: frontend/js/views/VisualizerView.js
// Version: v4.0.0 - CONFORMITÃƒÆ’Ã¢â‚¬Â° API
// Date: 2025-11-02
// ============================================================================

class VisualizerView {
    constructor(containerId, eventBus) {
        if (typeof containerId === 'string') {
            this.container = document.getElementById(containerId) || document.querySelector(containerId);
        } else {
            this.container = containerId;
        }
        
        this.eventBus = eventBus;
        this.canvas = null;
        this.ctx = null;
        this.animationId = null;
        
        this.state = {
            activeNotes: new Map(), // note -> {channel, velocity, time}
            mode: 'bars', // 'bars', 'waveform', 'spectrum'
            colorMode: 'channel' // 'channel', 'velocity'
        };
    }
    
    init() {
        if (!this.container) return;
        
        this.render();
        this.setupCanvas();
        this.attachEvents();
        this.startAnimation();
    }
    
    render() {
        this.container.innerHTML = `
            <div class="visualizer-view">
                <div class="visualizer-controls">
                    <select data-action="mode">
                        <option value="bars">Bars</option>
                        <option value="waveform">Waveform</option>
                        <option value="spectrum">Spectrum</option>
                    </select>
                    <select data-action="color">
                        <option value="channel">Par canal</option>
                        <option value="velocity">Par vÃƒÆ’Ã‚Â©locitÃƒÆ’Ã‚Â©</option>
                    </select>
                </div>
                <canvas id="visualizerCanvas"></canvas>
                <div class="visualizer-info">
                    <span class="note-count">0 notes</span>
                    <span class="channel-count">0 canaux</span>
                </div>
            </div>
        `;
    }
    
    setupCanvas() {
        this.canvas = document.getElementById('visualizerCanvas');
        if (!this.canvas) return;
        
        this.ctx = this.canvas.getContext('2d');
        this.resizeCanvas();
        
        window.addEventListener('resize', () => this.resizeCanvas());
    }
    
    resizeCanvas() {
        if (!this.canvas) return;
        const parent = this.canvas.parentElement;
        this.canvas.width = parent.clientWidth;
        this.canvas.height = parent.clientHeight;
    }
    
    attachEvents() {
        this.container.addEventListener('change', (e) => {
            const action = e.target.dataset.action;
            if (action === 'mode') this.state.mode = e.target.value;
            if (action === 'color') this.state.colorMode = e.target.value;
        });
        
        if (!this.eventBus) return;
        
        this.eventBus.on('midi:note_on', (data) => {
            this.state.activeNotes.set(data.note, {
                channel: data.channel || 0,
                velocity: data.velocity || 64,
                time: Date.now()
            });
        });
        
        this.eventBus.on('midi:note_off', (data) => {
            // Laisser fade out
        });
    }
    
    startAnimation() {
        const animate = () => {
            this.draw();
            this.animationId = requestAnimationFrame(animate);
        };
        animate();
    }
    
    draw() {
        if (!this.ctx) return;
        
        const ctx = this.ctx;
        const width = this.canvas.width;
        const height = this.canvas.height;
        
        // Clear
        ctx.fillStyle = 'rgba(20, 20, 30, 0.3)';
        ctx.fillRect(0, 0, width, height);
        
        // Mode-specific rendering
        switch (this.state.mode) {
            case 'bars':
                this.drawBars(ctx, width, height);
                break;
            case 'waveform':
                this.drawWaveform(ctx, width, height);
                break;
            case 'spectrum':
                this.drawSpectrum(ctx, width, height);
                break;
        }
        
        this.updateInfo();
    }
    
    drawBars(ctx, width, height) {
        const now = Date.now();
        const noteHeight = height / 128;
        
        this.state.activeNotes.forEach((data, note) => {
            const age = now - data.time;
            const alpha = Math.max(0, 1 - age / 2000);
            
            if (alpha <= 0) {
                this.state.activeNotes.delete(note);
                return;
            }
            
            const color = this.getColor(data);
            ctx.fillStyle = `${color}${Math.floor(alpha * 255).toString(16).padStart(2, '0')}`;
            
            const y = (127 - note) * noteHeight;
            const barWidth = (data.velocity / 127) * width * 0.8;
            ctx.fillRect(width * 0.1, y, barWidth, noteHeight);
        });
    }
    
    drawWaveform(ctx, width, height) {
        const now = Date.now();
        ctx.strokeStyle = '#4a9eff';
        ctx.lineWidth = 2;
        ctx.beginPath();
        
        let x = 0;
        this.state.activeNotes.forEach((data, note) => {
            const age = now - data.time;
            const y = height / 2 + Math.sin(age * 0.01 + note) * (data.velocity / 127) * height * 0.4;
            
            if (x === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
            
            x += width / Math.max(1, this.state.activeNotes.size);
        });
        
        ctx.stroke();
    }
    
    drawSpectrum(ctx, width, height) {
        const barWidth = width / 128;
        
        this.state.activeNotes.forEach((data, note) => {
            const x = (note / 128) * width;
            const barHeight = (data.velocity / 127) * height * 0.8;
            const color = this.getColor(data);
            
            ctx.fillStyle = color;
            ctx.fillRect(x, height - barHeight, barWidth, barHeight);
        });
    }
    
    getColor(data) {
        if (this.state.colorMode === 'channel') {
            const hue = (data.channel * 30) % 360;
            return `hsl(${hue}, 70%, 60%)`;
        } else {
            const hue = (data.velocity / 127) * 240;
            return `hsl(${hue}, 70%, 60%)`;
        }
    }
    
    updateInfo() {
        const noteCount = this.container.querySelector('.note-count');
        const channelCount = this.container.querySelector('.channel-count');
        
        if (noteCount) noteCount.textContent = `${this.state.activeNotes.size} notes`;
        
        const channels = new Set();
        this.state.activeNotes.forEach(data => channels.add(data.channel));
        if (channelCount) channelCount.textContent = `${channels.size} canaux`;
    }
    
    destroy() {
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
        }
        
        if (this.eventBus) {
            this.eventBus.off('midi:note_on');
            this.eventBus.off('midi:note_off');
        }
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = VisualizerView;
}
if (typeof window !== 'undefined') {
    window.VisualizerView = VisualizerView;
}