// Davis-Putnam Clause Growth Visualizer using D3.js

class DavisPutnamVisualizer {
    constructor(container) {
        this.container = container;
        this.clauseCounts = [];
        this.variablesEliminated = [];

        this.initializeSVG();
    }

    initializeSVG() {
        this.container.innerHTML = '';
        this.stepHistory = [];  // Track all steps

        const width = this.container.clientWidth || 800;
        const height = 500;

        this.svg = d3.select(this.container)
            .append('svg')
            .attr('width', '100%')
            .attr('height', height)
            .attr('viewBox', `0 0 ${width} ${height}`);

        this.width = width;
        this.height = height;
        this.margin = { top: 40, right: 40, bottom: 60, left: 60 };

        this.chartWidth = this.width - this.margin.left - this.margin.right;
        this.chartHeight = this.height - this.margin.top - this.margin.bottom;

        // Create chart group
        this.chartGroup = this.svg.append('g')
            .attr('transform', `translate(${this.margin.left}, ${this.margin.top})`);

        // Title
        this.svg.append('text')
            .attr('x', this.width / 2)
            .attr('y', 20)
            .attr('text-anchor', 'middle')
            .attr('font-size', '18px')
            .attr('font-weight', 'bold')
            .text('Davis-Putnam Clause Growth');

        // Create scrollable history panel below the chart
        this.detailsDiv = d3.select(this.container)
            .append('div')
            .attr('class', 'davis-putnam-details')
            .style('margin-top', '20px')
            .style('padding', '15px')
            .style('background', '#f8f9fa')
            .style('border-radius', '5px')
            .style('border', '1px solid #dee2e6')
            .style('min-height', 'calc(100vh - 650px)')
            .style('max-height', 'calc(100vh - 550px)')
            .style('overflow-y', 'auto');

        this.detailsDiv.append('h3')
            .style('margin', '0 0 15px 0')
            .style('color', '#343a40')
            .text('Solution Steps');
    }

    update(state) {
        const action = state.action;
        const data = state.data;

        switch (action) {
            case 'start':
                this.handleStart(data);
                break;
            case 'eliminate_variable':
                this.handleEliminate(data);
                break;
            case 'after_resolution':
                this.handleAfterResolution(data);
                break;
            case 'clause_growth':
                this.handleGrowthComplete(data);
                break;
        }
    }

    handleStart(data) {
        this.clauseCounts = [data.initial_clauses];
        this.variablesEliminated = ['start'];
        this.stepHistory = [];  // Reset history
        this.render();
        this.addStep({
            step: 'Initial Formula',
            clauses: data.clauses || [],
            message: `Starting with ${data.initial_clauses} clauses`,
            type: 'start'
        });
    }

    handleEliminate(data) {
        this.variablesEliminated.push(data.variable);
        this.clauseCounts.push(data.current_clause_count);

        // Update info text
        this.showEliminationInfo(data);
        this.render();

        // Add to history
        if (data.method === 'resolution') {
            this.addStep({
                step: `Eliminating variable: ${data.variable}`,
                method: 'Resolution',
                message: data.message,
                positiveClauses: data.pos_clause_list || [],
                negativeClauses: data.neg_clause_list || [],
                expected: data.expected_new_clauses,
                type: 'elimination'
            });
        } else {
            this.addStep({
                step: `Eliminating variable: ${data.variable}`,
                method: data.method === 'unit_propagation' ? 'Unit Propagation' : 'Pure Literal',
                message: data.message,
                clauses: data.clauses || [],
                type: 'elimination'
            });
        }
    }

    handleAfterResolution(data) {
        this.addStep({
            step: `After resolving ${data.variable}`,
            clauses: data.clauses || [],
            message: `Now have ${data.clause_count} clauses`,
            type: 'after_resolution'
        });
    }

    handleGrowthComplete(data) {
        this.clauseCounts = data.clause_counts;
        this.variablesEliminated = ['start', ...data.variables_eliminated];
        this.render();

        // Show growth statistics
        this.showGrowthStats(data);
    }

    showEliminationInfo(data) {
        // Remove old info
        this.chartGroup.selectAll('.elimination-info').remove();

        // Add new info
        const info = this.chartGroup.append('g')
            .attr('class', 'elimination-info')
            .attr('transform', `translate(10, ${this.chartHeight + 20})`);

        info.append('text')
            .attr('font-size', '14px')
            .text(`Eliminating ${data.variable}: ${data.positive_clauses} × ${data.negative_clauses} = ${data.expected_new_clauses} new clauses`);
    }

    showGrowthStats(data) {
        this.chartGroup.selectAll('.growth-stats').remove();

        const stats = this.chartGroup.append('g')
            .attr('class', 'growth-stats')
            .attr('transform', `translate(${this.chartWidth - 200}, 20)`);

        stats.append('text')
            .attr('font-size', '14px')
            .attr('font-weight', 'bold')
            .text('Growth Statistics:');

        stats.append('text')
            .attr('y', 20)
            .attr('font-size', '12px')
            .text(`Initial: ${data.clause_counts[0]} clauses`);

        stats.append('text')
            .attr('y', 35)
            .attr('font-size', '12px')
            .text(`Max: ${data.max_clauses} clauses`);

        stats.append('text')
            .attr('y', 50)
            .attr('font-size', '12px')
            .text(`Growth: ${(data.growth_factor * 100).toFixed(0)}%`);
    }

    addStep(data) {
        this.stepHistory.push(data);

        // Create a card for this step
        const stepCard = this.detailsDiv.append('div')
            .attr('class', 'step-card')
            .style('margin-bottom', '15px')
            .style('padding', '12px')
            .style('background', 'white')
            .style('border-radius', '5px')
            .style('border-left', `4px solid ${this.getStepColor(data.type)}`)
            .style('box-shadow', '0 1px 3px rgba(0,0,0,0.1)');

        // Step number and title
        const header = stepCard.append('div')
            .style('display', 'flex')
            .style('justify-content', 'space-between')
            .style('align-items', 'center')
            .style('margin-bottom', '8px');

        header.append('h4')
            .style('margin', '0')
            .style('color', '#495057')
            .style('font-size', '16px')
            .html(`<span style="color: #6c757d; font-weight: normal;">Step ${this.stepHistory.length}:</span> ${data.step}`);

        // Message
        if (data.message) {
            stepCard.append('p')
                .style('margin', '5px 0')
                .style('font-weight', 'bold')
                .style('color', '#007bff')
                .text(data.message);
        }

        // Method badge
        if (data.method) {
            stepCard.append('span')
                .style('display', 'inline-block')
                .style('padding', '3px 8px')
                .style('margin', '5px 0')
                .style('background', '#e7f3ff')
                .style('color', '#0066cc')
                .style('border-radius', '3px')
                .style('font-size', '12px')
                .style('font-weight', 'bold')
                .text(data.method);
        }

        // Resolution details
        if (data.positiveClauses && data.negativeClauses) {
            const resDiv = stepCard.append('div')
                .style('margin-top', '10px');

            resDiv.append('p')
                .style('margin', '5px 0')
                .style('font-size', '14px')
                .html(`<strong>Clauses with ${data.step.split(': ')[1] || 'variable'}:</strong>`);

            const posList = resDiv.append('ul')
                .style('margin', '5px 0 10px 15px')
                .style('padding', '0')
                .style('list-style', 'none');

            data.positiveClauses.forEach(clause => {
                posList.append('li')
                    .style('padding', '4px 8px')
                    .style('margin', '2px 0')
                    .style('background', '#d4edda')
                    .style('color', '#155724')
                    .style('border-radius', '3px')
                    .style('font-family', 'monospace')
                    .style('font-size', '13px')
                    .text('✓ ' + clause);
            });

            resDiv.append('p')
                .style('margin', '10px 0 5px 0')
                .style('font-size', '14px')
                .html(`<strong>Clauses with ¬${data.step.split(': ')[1] || 'variable'}:</strong>`);

            const negList = resDiv.append('ul')
                .style('margin', '5px 0 10px 15px')
                .style('padding', '0')
                .style('list-style', 'none');

            data.negativeClauses.forEach(clause => {
                negList.append('li')
                    .style('padding', '4px 8px')
                    .style('margin', '2px 0')
                    .style('background', '#f8d7da')
                    .style('color', '#721c24')
                    .style('border-radius', '3px')
                    .style('font-family', 'monospace')
                    .style('font-size', '13px')
                    .text('✗ ' + clause);
            });

            resDiv.append('div')
                .style('margin', '10px 0 5px 0')
                .style('background', '#fff3cd')
                .style('border', '1px solid #ffc107')
                .style('padding', '8px')
                .style('border-radius', '3px')
                .html(`<strong>⚠️ Resolution creates ${data.expected} new clauses (${data.positiveClauses.length} × ${data.negativeClauses.length})</strong>`);
        }

        // Current clauses
        if (data.clauses && data.clauses.length > 0) {
            stepCard.append('p')
                .style('margin', '10px 0 5px 0')
                .style('font-size', '14px')
                .html(`<strong>Resulting clauses (${data.clauses.length}):</strong>`);

            const clauseList = stepCard.append('ul')
                .style('margin', '5px 0 0 15px')
                .style('padding', '0')
                .style('list-style', 'none')
                .style('max-height', '150px')
                .style('overflow-y', 'auto')
                .style('background', '#f8f9fa')
                .style('border-radius', '3px')
                .style('padding', '8px');

            data.clauses.forEach(clause => {
                clauseList.append('li')
                    .style('padding', '2px 0')
                    .style('font-family', 'monospace')
                    .style('font-size', '13px')
                    .style('color', '#495057')
                    .text(clause);
            });
        }

        // Auto-scroll to bottom
        this.detailsDiv.node().scrollTop = this.detailsDiv.node().scrollHeight;
    }

    getStepColor(type) {
        const colors = {
            'start': '#17a2b8',
            'elimination': '#ffc107',
            'after_resolution': '#28a745'
        };
        return colors[type] || '#6c757d';
    }

    render() {
        if (this.clauseCounts.length === 0) return;

        // Clear previous chart
        this.chartGroup.selectAll('.chart-element').remove();

        // Create scales
        const xScale = d3.scaleLinear()
            .domain([0, this.clauseCounts.length - 1])
            .range([0, this.chartWidth]);

        const yScale = d3.scaleLinear()
            .domain([0, Math.max(...this.clauseCounts) * 1.1])
            .range([this.chartHeight, 0]);

        // Create axes
        const xAxis = d3.axisBottom(xScale)
            .ticks(Math.min(this.clauseCounts.length, 10))
            .tickFormat((d, i) => this.variablesEliminated[d] || d);

        const yAxis = d3.axisLeft(yScale);

        // Draw axes
        this.chartGroup.append('g')
            .attr('class', 'chart-element axis')
            .attr('transform', `translate(0, ${this.chartHeight})`)
            .call(xAxis)
            .selectAll('text')
            .attr('transform', 'rotate(-45)')
            .style('text-anchor', 'end');

        this.chartGroup.append('g')
            .attr('class', 'chart-element axis')
            .call(yAxis);

        // Axis labels
        this.chartGroup.append('text')
            .attr('class', 'chart-element axis-label')
            .attr('x', this.chartWidth / 2)
            .attr('y', this.chartHeight + 50)
            .attr('text-anchor', 'middle')
            .text('Variables Eliminated');

        this.chartGroup.append('text')
            .attr('class', 'chart-element axis-label')
            .attr('transform', 'rotate(-90)')
            .attr('x', -this.chartHeight / 2)
            .attr('y', -45)
            .attr('text-anchor', 'middle')
            .text('Number of Clauses');

        // Create line generator
        const line = d3.line()
            .x((d, i) => xScale(i))
            .y(d => yScale(d))
            .curve(d3.curveMonotoneX);

        // Draw line
        this.chartGroup.append('path')
            .datum(this.clauseCounts)
            .attr('class', 'chart-element chart-line')
            .attr('d', line);

        // Draw points
        this.chartGroup.selectAll('.chart-dot')
            .data(this.clauseCounts)
            .join('circle')
            .attr('class', 'chart-element chart-dot')
            .attr('cx', (d, i) => xScale(i))
            .attr('cy', d => yScale(d))
            .attr('r', 5);

        // Add value labels
        this.chartGroup.selectAll('.value-label')
            .data(this.clauseCounts)
            .join('text')
            .attr('class', 'chart-element value-label')
            .attr('x', (d, i) => xScale(i))
            .attr('y', d => yScale(d) - 10)
            .attr('text-anchor', 'middle')
            .attr('font-size', '10px')
            .text(d => d);
    }
}
