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
        this.stepHistory = [];  // Track all steps

        const width = this.container.clientWidth || 800;
        const height = 400;  // Reduced to make room for history

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

        // Create scrollable history panel below the graph
        this.detailsDiv = d3.select(this.container)
            .append('div')
            .attr('class', 'twosat-details')
            .style('margin-top', '20px')
            .style('padding', '15px')
            .style('background', '#f8f9fa')
            .style('border-radius', '5px')
            .style('border', '1px solid #dee2e6')
            .style('min-height', 'calc(100vh - 550px)')
            .style('max-height', 'calc(100vh - 450px)')
            .style('overflow-y', 'auto');

        this.detailsDiv.append('h3')
            .style('margin', '0 0 15px 0')
            .style('color', '#343a40')
            .text('2SAT Solution Steps');
    }

    update(state) {
        const action = state.action;
        const data = state.data;

        switch (action) {
            case 'start':
                this.addStep({
                    type: 'start',
                    message: 'Starting 2SAT solver',
                    details: `${data.clauses.length} clauses to solve`
                });
                break;
            case 'implication_graph':
                this.handleGraph(data);
                break;
            case 'finding_sccs':
                this.addStep({
                    type: 'finding_sccs',
                    message: 'Finding strongly connected components...',
                    details: data.message || "Using Tarjan's algorithm"
                });
                break;
            case 'sccs_found':
                this.handleSCCs(data);
                break;
            case 'no_conflicts':
                this.addStep({
                    type: 'no_conflicts',
                    message: '✓ No conflicts found',
                    details: data.message || 'Formula is satisfiable'
                });
                break;
            case 'solution_constructed':
                this.addStep({
                    type: 'solution',
                    message: '✓ Solution constructed',
                    details: data.method || 'Assigned values based on SCC topological order'
                });
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

        this.addStep({
            type: 'implication_graph',
            message: 'Built implication graph',
            details: `${graph.nodes.length} nodes (literals), ${graph.edges.length} edges (implications)`
        });

        this.renderGraph();
    }

    handleSCCs(data) {
        this.sccs = data.sccs;

        this.addStep({
            type: 'sccs_found',
            message: `Found ${data.num_sccs} strongly connected components`,
            details: `Each SCC shown in a different color. Variables in the same SCC must have the same truth value.`
        });

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

        this.addStep({
            type: 'conflict',
            message: `✗ UNSAT: Conflict detected!`,
            details: data.message || `Variables ${conflicts.join(', ')} have both polarities in the same SCC - formula is unsatisfiable`
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

    addStep(data) {
        this.stepHistory.push(data);

        // Create a compact card for this step
        const stepCard = this.detailsDiv.append('div')
            .attr('class', 'step-card')
            .style('margin-bottom', '10px')
            .style('padding', '10px')
            .style('background', 'white')
            .style('border-radius', '4px')
            .style('border-left', `4px solid ${this.getStepColor(data.type)}`)
            .style('box-shadow', '0 1px 2px rgba(0,0,0,0.1)');

        // Header with step number and message
        stepCard.append('div')
            .style('font-weight', 'bold')
            .style('color', '#495057')
            .style('font-size', '14px')
            .html(`<span style="color: #6c757d; font-weight: normal;">#${this.stepHistory.length}:</span> ${data.message}`);

        // Details
        if (data.details) {
            stepCard.append('div')
                .style('margin-top', '4px')
                .style('font-size', '13px')
                .style('color', '#6c757d')
                .text(data.details);
        }

        // Auto-scroll to bottom
        this.detailsDiv.node().scrollTop = this.detailsDiv.node().scrollHeight;
    }

    getStepColor(type) {
        const colors = {
            'start': '#17a2b8',
            'implication_graph': '#0066cc',
            'finding_sccs': '#6c757d',
            'sccs_found': '#20c997',
            'no_conflicts': '#28a745',
            'solution': '#28a745',
            'conflict': '#dc3545'
        };
        return colors[type] || '#6c757d';
    }
}
