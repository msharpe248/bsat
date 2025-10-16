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
        this.stepHistory = [];  // Track all steps

        // Use container's actual size
        const containerWidth = this.container.clientWidth || 800;
        const containerHeight = this.container.clientHeight || 600;

        // Start with container size, will expand dynamically
        this.baseWidth = containerWidth;
        this.baseHeight = Math.min(containerHeight, 400);  // Reduce to make room for history
        this.width = containerWidth;
        this.height = this.baseHeight;

        this.svg = d3.select(this.container)
            .append('svg')
            .attr('width', this.width)
            .attr('height', this.height)
            .style('display', 'block')
            .style('min-width', '100%');

        // Create main group for zoom/pan
        this.mainGroup = this.svg.append('g').attr('class', 'main-group');

        // Create groups for links, backtrack markers, and nodes
        this.linkGroup = this.mainGroup.append('g').attr('class', 'links');
        this.markerGroup = this.mainGroup.append('g').attr('class', 'backtrack-markers');
        this.nodeGroup = this.mainGroup.append('g').attr('class', 'nodes');

        // Create scrollable history panel below the tree
        this.detailsDiv = d3.select(this.container)
            .append('div')
            .attr('class', 'dpll-details')
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
            .text('Search Steps');
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
            case 'decision_point':
                // Decision point - just track it, don't create a node
                // This is informational and doesn't need visualization
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

        this.addStep({
            type: 'start',
            message: `Starting DPLL search`,
            details: `${data.num_variables} variables, ${data.num_clauses} clauses`
        });
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

        this.addStep({
            type: 'decision',
            message: `Decision: ${data.variable} = ${data.value}`,
            details: `Depth ${data.depth}, trying ${data.branch} branch`,
            assignment: data.assignment || {},
            clauses: data.clauses || [],
            currentVariable: data.variable
        });
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

        this.addStep({
            type: 'unit_propagation',
            message: `Unit propagation: ${data.variable} = ${data.value}`,
            details: `Forced by unit clause: ${data.clause}`,
            assignment: data.assignment || {},
            clauses: data.clauses || [],
            currentVariable: data.variable
        });
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

        this.addStep({
            type: 'pure_literal',
            message: `Pure literal: ${data.variable} = ${data.value}`,
            details: `Variable ${data.variable} only appears in one polarity - can safely assign`,
            assignment: data.assignment || {},
            currentVariable: data.variable
        });
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

        this.addStep({
            type: 'conflict',
            message: `✗ Conflict detected!`,
            details: data.message || 'Empty clause found - this path is unsatisfiable',
            clauses: data.clauses || []
        });
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

        this.addStep({
            type: 'backtrack',
            message: `⬅ Backtracking to depth ${data.depth}`,
            details: data.message || `Trying alternative assignment`,
            assignment: data.assignment || {}
        });
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

        this.addStep({
            type: 'solution',
            message: `✓ Solution found!`,
            details: `Assignment: ${JSON.stringify(data.assignment)}`,
            clauses: data.clauses || []
        });
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

        // Display assignment if available
        if (data.assignment && Object.keys(data.assignment).length > 0) {
            const assignmentDiv = stepCard.append('div')
                .style('margin-top', '8px')
                .style('padding', '6px')
                .style('background', '#f8f9fa')
                .style('border-radius', '3px')
                .style('font-size', '12px');

            assignmentDiv.append('strong').text('Current assignment: ');

            const assignments = Object.entries(data.assignment)
                .map(([key, value]) => {
                    const color = value ? '#28a745' : '#dc3545';
                    return `<span style="color: ${color}; font-weight: bold;">${key}=${value}</span>`;
                })
                .join(', ');

            assignmentDiv.append('span').html(assignments);
        }

        // Display clauses with highlighting
        if (data.clauses && data.clauses.length > 0) {
            stepCard.append('div')
                .style('margin-top', '8px')
                .style('font-size', '13px')
                .style('font-weight', 'bold')
                .style('color', '#495057')
                .text(`Clauses (${data.clauses.length}):`);

            const clauseList = stepCard.append('div')
                .style('margin-top', '4px')
                .style('max-height', '200px')
                .style('overflow-y', 'auto')
                .style('background', '#f8f9fa')
                .style('border-radius', '3px')
                .style('padding', '8px');

            data.clauses.forEach((clause, idx) => {
                const clauseDiv = clauseList.append('div')
                    .style('padding', '4px 0')
                    .style('font-family', 'monospace')
                    .style('font-size', '12px')
                    .style('line-height', '1.6');

                // Render clause with highlighted variables
                const highlightedClause = this.highlightClause(
                    clause,
                    data.assignment || {},
                    data.currentVariable
                );
                clauseDiv.html(highlightedClause);
            });
        }

        // Auto-scroll to bottom
        this.detailsDiv.node().scrollTop = this.detailsDiv.node().scrollHeight;
    }

    highlightClause(clauseStr, assignment, currentVariable) {
        // Parse and highlight variables in a clause
        // Replace variables with colored spans based on their assignment

        let result = clauseStr;

        // Find all variables (including negated ones)
        // Match patterns like: ~x, x, ~var, var
        const variablePattern = /(~?)(\w+)/g;

        // Collect all matches first to avoid replacement conflicts
        const matches = [];
        let match;
        while ((match = variablePattern.exec(clauseStr)) !== null) {
            matches.push({
                full: match[0],
                negation: match[1],
                variable: match[2],
                index: match.index
            });
        }

        // Sort by index in reverse to replace from end to start
        matches.sort((a, b) => b.index - a.index);

        // Replace each match with highlighted version
        matches.forEach(m => {
            const varName = m.variable;
            const isNegated = m.negation === '~';

            // Determine color based on assignment and current variable
            let color = '#495057'; // Default gray
            let backgroundColor = 'transparent';
            let fontWeight = 'normal';

            if (varName === currentVariable) {
                // Current decision variable - highlight in blue
                color = '#0066cc';
                backgroundColor = '#e7f3ff';
                fontWeight = 'bold';
            } else if (assignment.hasOwnProperty(varName)) {
                // Variable is assigned
                const value = assignment[varName];
                const literalValue = isNegated ? !value : value;

                if (literalValue) {
                    // This literal is true - green
                    color = '#28a745';
                    fontWeight = 'bold';
                } else {
                    // This literal is false - red
                    color = '#dc3545';
                    fontWeight = 'bold';
                }
            }

            const highlighted = `<span style="color: ${color}; background-color: ${backgroundColor}; padding: 1px 3px; border-radius: 2px; font-weight: ${fontWeight};">${m.full}</span>`;

            result = result.substring(0, m.index) + highlighted + result.substring(m.index + m.full.length);
        });

        return result;
    }

    getStepColor(type) {
        const colors = {
            'start': '#17a2b8',
            'decision': '#0066cc',
            'unit_propagation': '#28a745',
            'pure_literal': '#20c997',
            'conflict': '#dc3545',
            'backtrack': '#ffc107',
            'solution': '#28a745'
        };
        return colors[type] || '#6c757d';
    }
}
