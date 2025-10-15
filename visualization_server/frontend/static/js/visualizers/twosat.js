// 2SAT Implication Graph Visualizer using D3.js

class TwoSATVisualizer {
    constructor(container) {
        this.container = container;
        this.nodes = [];
        this.links = [];
        this.sccs = [];

        this.initializeSVG();
    }

    initializeSVG() {
        this.container.innerHTML = '';

        const width = this.container.clientWidth || 800;
        const height = 600;

        this.svg = d3.select(this.container)
            .append('svg')
            .attr('width', '100%')
            .attr('height', height)
            .attr('viewBox', `0 0 ${width} ${height}`);

        // Define arrow marker
        this.svg.append('defs').append('marker')
            .attr('id', 'arrowhead')
            .attr('viewBox', '-0 -5 10 10')
            .attr('refX', 25)
            .attr('refY', 0)
            .attr('orient', 'auto')
            .attr('markerWidth', 6)
            .attr('markerHeight', 6)
            .append('svg:path')
            .attr('d', 'M 0,-5 L 10,0 L 0,5')
            .attr('fill', '#999');

        this.linkGroup = this.svg.append('g').attr('class', 'links');
        this.nodeGroup = this.svg.append('g').attr('class', 'nodes');
        this.labelGroup = this.svg.append('g').attr('class', 'labels');

        this.width = width;
        this.height = height;
    }

    update(state) {
        const action = state.action;
        const data = state.data;

        switch (action) {
            case 'implication_graph':
                this.handleGraph(data);
                break;
            case 'sccs_found':
                this.handleSCCs(data);
                break;
            case 'conflict':
                this.handleConflict(data);
                break;
        }
    }

    handleGraph(data) {
        const graph = data.graph;

        // Create nodes with initial positions near center
        const centerX = this.width / 2;
        const centerY = this.height / 2;
        const radius = Math.min(this.width, this.height) / 4;

        this.nodes = graph.nodes.map((nodeId, index) => {
            // Arrange in a circle initially
            const angle = (index / graph.nodes.length) * 2 * Math.PI;
            return {
                id: nodeId,
                label: nodeId,
                x: centerX + radius * Math.cos(angle),
                y: centerY + radius * Math.sin(angle)
            };
        });

        // Create links
        this.links = graph.edges.map(edge => ({
            source: edge.from,
            target: edge.to
        }));

        this.renderGraph();
    }

    handleSCCs(data) {
        this.sccs = data.sccs;
        this.renderGraph();
    }

    handleConflict(data) {
        // Highlight conflicting variables
        const conflicts = data.conflicts || [];
        this.nodes.forEach(node => {
            const varName = node.id.replace(/^~/, '');
            if (conflicts.includes(varName)) {
                node.conflict = true;
            }
        });
        this.renderGraph();
    }

    renderGraph() {
        // Add padding to keep nodes in viewport
        const padding = 50;
        const boundedWidth = this.width - padding * 2;
        const boundedHeight = this.height - padding * 2;

        // Create force simulation with bounds
        const simulation = d3.forceSimulation(this.nodes)
            .force('link', d3.forceLink(this.links).id(d => d.id).distance(120))
            .force('charge', d3.forceManyBody().strength(-500))
            .force('center', d3.forceCenter(this.width / 2, this.height / 2))
            .force('collision', d3.forceCollide().radius(40))
            .force('x', d3.forceX(this.width / 2).strength(0.1))
            .force('y', d3.forceY(this.height / 2).strength(0.1));

        // Assign SCC colors
        const sccColors = d3.scaleOrdinal(d3.schemeCategory10);
        const nodeToSCC = new Map();

        this.sccs.forEach((scc, index) => {
            scc.forEach(nodeId => {
                nodeToSCC.set(nodeId, index);
            });
        });

        // Draw links
        const link = this.linkGroup
            .selectAll('line')
            .data(this.links)
            .join('line')
            .attr('class', 'graph-link implication')
            .attr('marker-end', 'url(#arrowhead)');

        // Draw nodes
        const node = this.nodeGroup
            .selectAll('circle')
            .data(this.nodes)
            .join('circle')
            .attr('class', 'graph-node')
            .attr('r', 15)
            .attr('fill', d => {
                if (d.conflict) return '#ef4444';
                const sccIndex = nodeToSCC.get(d.id);
                return sccIndex !== undefined ? sccColors(sccIndex) : '#94a3b8';
            })
            .call(d3.drag()
                .on('start', dragstarted)
                .on('drag', dragged)
                .on('end', dragended));

        // Draw labels
        const label = this.labelGroup
            .selectAll('text')
            .data(this.nodes)
            .join('text')
            .attr('text-anchor', 'middle')
            .attr('dy', 4)
            .attr('font-size', '12px')
            .attr('fill', d => d.conflict ? 'white' : '#1e293b')
            .text(d => d.label);

        // Update positions on simulation tick with bounds
        simulation.on('tick', () => {
            // Constrain nodes to viewport
            this.nodes.forEach(d => {
                d.x = Math.max(padding, Math.min(this.width - padding, d.x));
                d.y = Math.max(padding, Math.min(this.height - padding, d.y));
            });

            link
                .attr('x1', d => d.source.x)
                .attr('y1', d => d.source.y)
                .attr('x2', d => d.target.x)
                .attr('y2', d => d.target.y);

            node
                .attr('cx', d => d.x)
                .attr('cy', d => d.y);

            label
                .attr('x', d => d.x)
                .attr('y', d => d.y);
        });

        // Drag functions
        function dragstarted(event, d) {
            if (!event.active) simulation.alphaTarget(0.3).restart();
            d.fx = d.x;
            d.fy = d.y;
        }

        function dragged(event, d) {
            d.fx = event.x;
            d.fy = event.y;
        }

        function dragended(event, d) {
            if (!event.active) simulation.alphaTarget(0);
            d.fx = null;
            d.fy = null;
        }
    }
}
