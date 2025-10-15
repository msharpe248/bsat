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
            case 'clause_growth':
                this.handleGrowthComplete(data);
                break;
        }
    }

    handleStart(data) {
        this.clauseCounts = [data.initial_clauses];
        this.variablesEliminated = ['start'];
        this.render();
    }

    handleEliminate(data) {
        this.variablesEliminated.push(data.variable);
        this.clauseCounts.push(data.current_clause_count);

        // Update info text
        this.showEliminationInfo(data);
        this.render();
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
