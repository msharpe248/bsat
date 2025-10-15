// DPLL Search Tree Visualizer using D3.js

class DPLLVisualizer {
    constructor(container) {
        this.container = container;
        this.nodes = [];
        this.links = [];
        this.nodeIdCounter = 0;
        this.currentPath = [];
        this.activeNodeIds = new Set();  // Track active path nodes
        this.zoom = 1.0;
        this.nodeRadius = 25;  // Larger nodes

        this.initializeSVG();
        this.setupZoomControls();
    }

    initializeSVG() {
        // Clear container
        this.container.innerHTML = '';

        // Use container's actual size
        const containerWidth = this.container.clientWidth || 800;
        const containerHeight = this.container.clientHeight || 600;

        // Start with container size, will expand dynamically
        this.baseWidth = containerWidth;
        this.baseHeight = containerHeight;
        this.width = containerWidth;
        this.height = containerHeight;

        this.svg = d3.select(this.container)
            .append('svg')
            .attr('width', this.width)
            .attr('height', this.height)
            .style('display', 'block')
            .style('min-width', '100%')
            .style('min-height', '100%');

        // Create main group for zoom/pan
        this.mainGroup = this.svg.append('g').attr('class', 'main-group');

        // Create groups for links, backtrack markers, and nodes
        this.linkGroup = this.mainGroup.append('g').attr('class', 'links');
        this.markerGroup = this.mainGroup.append('g').attr('class', 'backtrack-markers');
        this.nodeGroup = this.mainGroup.append('g').attr('class', 'nodes');
    }

    setupZoomControls() {
        // Get zoom control buttons
        const zoomInBtn = document.getElementById('zoomInBtn');
        const zoomOutBtn = document.getElementById('zoomOutBtn');
        const zoomResetBtn = document.getElementById('zoomResetBtn');
        const zoomLevel = document.getElementById('zoomLevel');

        if (zoomInBtn) {
            zoomInBtn.addEventListener('click', () => {
                this.zoom = Math.min(this.zoom * 1.2, 3.0);
                this.applyZoom();
                if (zoomLevel) zoomLevel.textContent = `${Math.round(this.zoom * 100)}%`;
            });
        }

        if (zoomOutBtn) {
            zoomOutBtn.addEventListener('click', () => {
                this.zoom = Math.max(this.zoom / 1.2, 0.3);
                this.applyZoom();
                if (zoomLevel) zoomLevel.textContent = `${Math.round(this.zoom * 100)}%`;
            });
        }

        if (zoomResetBtn) {
            zoomResetBtn.addEventListener('click', () => {
                this.zoom = 1.0;
                this.applyZoom();
                if (zoomLevel) zoomLevel.textContent = '100%';
            });
        }
    }

    applyZoom() {
        this.mainGroup.attr('transform', `scale(${this.zoom})`);
        // Update SVG size based on zoom
        const newWidth = this.width * this.zoom;
        const newHeight = this.height * this.zoom;
        this.svg.attr('width', newWidth).attr('height', newHeight);
    }

    update(state) {
        const action = state.action;
        const data = state.data;

        switch (action) {
            case 'start':
                this.handleStart(data);
                break;
            case 'branch':
                this.handleBranch(data);
                break;
            case 'unit_propagation':
                this.handleUnitProp(data);
                break;
            case 'pure_literal':
                this.handlePureLiteral(data);
                break;
            case 'conflict':
                this.handleConflict(data);
                break;
            case 'backtrack':
                this.handleBacktrack(data);
                break;
            case 'solution_found':
                this.handleSolution(data);
                break;
        }

        this.render();
    }

    handleStart(data) {
        // Create root node
        const nodeId = this.addNode({
            id: this.nodeIdCounter++,
            type: 'root',
            label: 'Start',
            depth: 0,
            parent: null
        });
        this.activeNodeIds.add(nodeId);
        this.currentPath.push(nodeId);
    }

    handleBranch(data) {
        const parentId = this.currentPath[this.currentPath.length - 1] || 0;
        const nodeId = this.addNode({
            id: this.nodeIdCounter++,
            type: 'decision',
            label: `${data.variable}=${data.value}`,
            depth: data.depth,
            parent: parentId
        });

        this.addLink({ source: parentId, target: nodeId, type: 'decision' });
        this.activeNodeIds.add(nodeId);
        this.currentPath.push(nodeId);
    }

    handleUnitProp(data) {
        const parentId = this.currentPath[this.currentPath.length - 1];
        if (parentId === undefined) return;

        const nodeId = this.addNode({
            id: this.nodeIdCounter++,
            type: 'propagation',
            label: `${data.variable}=${data.value} (unit)`,
            depth: data.depth,
            parent: parentId
        });

        this.addLink({ source: parentId, target: nodeId, type: 'propagation' });
        this.activeNodeIds.add(nodeId);
        this.currentPath[this.currentPath.length - 1] = nodeId;
    }

    handlePureLiteral(data) {
        const parentId = this.currentPath[this.currentPath.length - 1];
        if (parentId === undefined) return;

        const nodeId = this.addNode({
            id: this.nodeIdCounter++,
            type: 'propagation',
            label: `${data.variable}=${data.value} (pure)`,
            depth: data.depth,
            parent: parentId
        });

        this.addLink({ source: parentId, target: nodeId, type: 'propagation' });
        this.activeNodeIds.add(nodeId);
        this.currentPath[this.currentPath.length - 1] = nodeId;
    }

    handleConflict(data) {
        const parentId = this.currentPath[this.currentPath.length - 1];
        if (parentId === undefined) return;

        const nodeId = this.addNode({
            id: this.nodeIdCounter++,
            type: 'conflict',
            label: '✗ Conflict',
            depth: data.depth,
            parent: parentId
        });

        this.addLink({ source: parentId, target: nodeId, type: 'conflict' });
    }

    handleBacktrack(data) {
        // Mark edges as backtracked before popping
        while (this.currentPath.length > data.depth) {
            const nodeId = this.currentPath[this.currentPath.length - 1];

            // Find all links leading to this node and mark as backtracked
            this.links.forEach(link => {
                if (link.target === nodeId) {
                    link.backtracked = true;
                }
            });

            // Mark node as inactive
            this.activeNodeIds.delete(nodeId);

            this.currentPath.pop();
        }
    }

    handleSolution(data) {
        const parentId = this.currentPath[this.currentPath.length - 1];
        if (parentId === undefined) return;

        const nodeId = this.addNode({
            id: this.nodeIdCounter++,
            type: 'solution',
            label: '✓ SAT',
            depth: data.depth,
            parent: parentId
        });

        this.addLink({ source: parentId, target: nodeId, type: 'solution' });
        this.activeNodeIds.add(nodeId);
    }

    addNode(node) {
        this.nodes.push(node);
        return node.id;
    }

    addLink(link) {
        this.links.push(link);
    }

    render() {
        // Layout nodes in a tree structure
        const nodeMap = new Map();
        this.nodes.forEach(n => nodeMap.set(n.id, n));

        // Calculate positions with better spacing
        const maxDepth = Math.max(...this.nodes.map(n => n.depth), 0);
        const levelWidth = 200;  // Fixed width between levels
        const verticalSpacing = 100;  // Fixed vertical spacing
        const levelHeights = new Map();

        this.nodes.forEach(node => {
            const level = node.depth;
            if (!levelHeights.has(level)) {
                levelHeights.set(level, 0);
            }
            const count = levelHeights.get(level);
            levelHeights.set(level, count + 1);

            node.x = 100 + level * levelWidth;
            node.y = 100 + count * verticalSpacing;
        });

        // Calculate required dimensions based on actual node positions
        if (this.nodes.length > 0) {
            const maxX = Math.max(...this.nodes.map(n => n.x)) + 150;
            const maxY = Math.max(...this.nodes.map(n => n.y)) + 150;

            // Expand SVG only if content exceeds current size
            const newWidth = Math.max(this.baseWidth, maxX);
            const newHeight = Math.max(this.baseHeight, maxY);

            if (newWidth !== this.width || newHeight !== this.height) {
                this.width = newWidth;
                this.height = newHeight;
                this.svg
                    .attr('width', this.width * this.zoom)
                    .attr('height', this.height * this.zoom);
            }
        }

        // Draw links
        const linkSelection = this.linkGroup
            .selectAll('line')
            .data(this.links, (d, i) => i);

        linkSelection
            .enter()
            .append('line')
            .merge(linkSelection)
            .attr('class', d => {
                const classes = ['link', d.type];
                if (d.backtracked) classes.push('backtracked');
                else if (this.activeNodeIds.has(d.target)) classes.push('active');
                return classes.join(' ');
            })
            .attr('x1', d => nodeMap.get(d.source).x)
            .attr('y1', d => nodeMap.get(d.source).y)
            .attr('x2', d => nodeMap.get(d.target).x)
            .attr('y2', d => nodeMap.get(d.target).y);

        linkSelection.exit().remove();

        // Draw backtrack markers (X or scissors) on backtracked links
        const markerSelection = this.markerGroup
            .selectAll('text')
            .data(this.links.filter(d => d.backtracked), (d, i) => `${d.source}-${d.target}`);

        markerSelection
            .enter()
            .append('text')
            .merge(markerSelection)
            .attr('class', 'backtrack-marker')
            .attr('x', d => (nodeMap.get(d.source).x + nodeMap.get(d.target).x) / 2)
            .attr('y', d => (nodeMap.get(d.source).y + nodeMap.get(d.target).y) / 2)
            .attr('text-anchor', 'middle')
            .attr('dy', 5)
            .text('✂');  // Scissors symbol

        markerSelection.exit().remove();

        // Draw nodes
        const nodeSelection = this.nodeGroup
            .selectAll('g')
            .data(this.nodes, d => d.id);

        const nodeEnter = nodeSelection
            .enter()
            .append('g')
            .attr('transform', d => `translate(${d.x}, ${d.y})`);

        nodeEnter.append('circle')
            .attr('r', this.nodeRadius);

        nodeEnter.append('text')
            .attr('dy', -this.nodeRadius - 10)
            .attr('text-anchor', 'middle')
            .style('font-size', '14px')
            .text(d => d.label);

        nodeSelection
            .merge(nodeEnter)
            .attr('class', d => {
                const classes = ['node', d.type];
                if (!this.activeNodeIds.has(d.id) && d.type !== 'root') {
                    classes.push('inactive');
                }
                return classes.join(' ');
            })
            .attr('transform', d => `translate(${d.x}, ${d.y})`);

        nodeSelection.exit().remove();
    }
}
