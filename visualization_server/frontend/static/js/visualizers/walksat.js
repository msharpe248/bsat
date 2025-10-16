// WalkSAT Local Search Visualizer

class WalkSATVisualizer {
    constructor(container) {
        this.container = container;
        this.assignment = {};
        this.stepHistory = [];
        this.unsatisfiedHistory = [];
        this.currentTry = 0;

        this.initialize();
    }

    initialize() {
        this.container.innerHTML = '';

        // Create main layout
        const wrapper = document.createElement('div');
        wrapper.style.cssText = 'display: flex; flex-direction: column; gap: 20px; height: 100%; overflow: auto;';

        // Progress chart area
        this.chartDiv = document.createElement('div');
        this.chartDiv.className = 'walksat-chart';
        this.chartDiv.style.cssText = `
            padding: 15px;
            background: #f8f9fa;
            border-radius: 5px;
            border: 2px solid #dee2e6;
        `;

        const chartTitle = document.createElement('h3');
        chartTitle.textContent = 'Progress: Unsatisfied Clauses Over Time';
        chartTitle.style.cssText = 'margin: 0 0 10px 0; color: #343a40; font-size: 16px;';
        this.chartDiv.appendChild(chartTitle);

        this.chartCanvas = document.createElement('canvas');
        this.chartCanvas.width = 600;
        this.chartCanvas.height = 150;
        this.chartCanvas.style.cssText = 'width: 100%; height: 150px; background: white; border-radius: 3px;';
        this.chartDiv.appendChild(this.chartCanvas);

        // Current state
        this.stateDiv = document.createElement('div');
        this.stateDiv.className = 'walksat-state';
        this.stateDiv.style.cssText = `
            padding: 15px;
            background: #f8f9fa;
            border-radius: 5px;
            border: 2px solid #dee2e6;
        `;

        const stateTitle = document.createElement('h3');
        stateTitle.textContent = 'Current Assignment';
        stateTitle.style.cssText = 'margin: 0 0 10px 0; color: #343a40; font-size: 16px;';
        this.stateDiv.appendChild(stateTitle);

        this.stateContent = document.createElement('div');
        this.stateContent.style.cssText = `
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
            font-family: monospace;
            font-size: 13px;
        `;
        this.stateDiv.appendChild(this.stateContent);

        // Step history
        this.historyDiv = document.createElement('div');
        this.historyDiv.className = 'walksat-history';
        this.historyDiv.style.cssText = `
            padding: 15px;
            background: #f8f9fa;
            border-radius: 5px;
            border: 2px solid #dee2e6;
            flex: 1;
            overflow-y: auto;
        `;

        const historyTitle = document.createElement('h3');
        historyTitle.textContent = 'WalkSAT Steps';
        historyTitle.style.cssText = 'margin: 0 0 15px 0; color: #343a40; font-size: 16px;';
        this.historyDiv.appendChild(historyTitle);

        this.historyContent = document.createElement('div');
        this.historyContent.style.cssText = 'display: flex; flex-direction: column; gap: 10px;';
        this.historyDiv.appendChild(this.historyContent);

        wrapper.appendChild(this.chartDiv);
        wrapper.appendChild(this.stateDiv);
        wrapper.appendChild(this.historyDiv);
        this.container.appendChild(wrapper);
    }

    update(state) {
        const action = state.action;
        const data = state.data;

        switch (action) {
            case 'start':
                this.handleStart(data);
                break;
            case 'restart':
                this.handleRestart(data);
                break;
            case 'flip':
                this.handleFlip(data);
                break;
            case 'sat':
                this.handleSAT(data);
                break;
            case 'no_solution':
                this.handleNoSolution(data);
                break;
        }

        this.renderAssignment();
        this.renderChart();
    }

    handleStart(data) {
        this.addStep({
            type: 'start',
            message: 'Starting WalkSAT local search',
            details: `${data.clauses.length} clauses, noise=0.5 (50% random, 50% greedy)`,
            color: '#17a2b8'
        });
    }

    handleRestart(data) {
        this.currentTry = data.try_number;
        this.assignment = data.assignment || {};

        this.addStep({
            type: 'restart',
            message: `🔄 Restart #${data.try_number}`,
            details: data.message || 'New random assignment',
            color: '#9b59b6'
        });
    }

    handleFlip(data) {
        this.assignment = data.assignment || {};
        this.unsatisfiedHistory.push(data.num_unsatisfied);

        const flipIcon = data.flip_type === 'random' ? '🎲' : '🎯';
        const flipColor = data.flip_type === 'random' ? '#ffc107' : '#28a745';

        this.addStep({
            type: 'flip',
            message: `${flipIcon} Flip #${data.total_flips}: ${data.variable} ${data.old_value} → ${data.new_value}`,
            details: `${data.flip_type.toUpperCase()} flip (${data.num_unsatisfied} unsatisfied clauses)`,
            variable: data.variable,
            flipType: data.flip_type,
            breakCount: data.break_count,
            breakCounts: data.break_counts,
            unsatisfiedClause: data.unsatisfied_clause,
            color: flipColor
        });
    }

    handleSAT(data) {
        this.assignment = data.assignment || {};

        this.addStep({
            type: 'sat',
            message: `✓ SAT - Solution found!`,
            details: data.message || `Found after ${data.total_flips} flips`,
            color: '#28a745'
        });
    }

    handleNoSolution(data) {
        this.addStep({
            type: 'no_solution',
            message: '⚠ No solution found',
            details: data.message || 'WalkSAT is incomplete - may not find solution even if one exists',
            color: '#dc3545'
        });
    }

    renderAssignment() {
        this.stateContent.innerHTML = '';

        if (Object.keys(this.assignment).length === 0) {
            this.stateContent.textContent = 'No assignment yet';
            return;
        }

        const sortedVars = Object.keys(this.assignment).sort();

        sortedVars.forEach(variable => {
            const value = this.assignment[variable];
            const badge = document.createElement('div');
            badge.style.cssText = `
                padding: 6px 12px;
                border-radius: 4px;
                background: ${value ? '#d4edda' : '#f8d7da'};
                color: ${value ? '#155724' : '#721c24'};
                font-weight: bold;
                font-size: 13px;
            `;
            badge.textContent = `${variable} = ${value}`;
            this.stateContent.appendChild(badge);
        });
    }

    renderChart() {
        const canvas = this.chartCanvas;
        const ctx = canvas.getContext('2d');
        const width = canvas.width;
        const height = canvas.height;

        // Clear canvas
        ctx.clearRect(0, 0, width, height);

        if (this.unsatisfiedHistory.length === 0) {
            ctx.fillStyle = '#6c757d';
            ctx.font = '14px sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText('No data yet - waiting for flips...', width / 2, height / 2);
            return;
        }

        // Draw chart
        const padding = 40;
        const chartWidth = width - padding * 2;
        const chartHeight = height - padding * 2;

        const maxUnsatisfied = Math.max(...this.unsatisfiedHistory, 1);
        const numPoints = this.unsatisfiedHistory.length;

        // Draw axes
        ctx.strokeStyle = '#dee2e6';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(padding, padding);
        ctx.lineTo(padding, height - padding);
        ctx.lineTo(width - padding, height - padding);
        ctx.stroke();

        // Draw y-axis labels
        ctx.fillStyle = '#6c757d';
        ctx.font = '12px sans-serif';
        ctx.textAlign = 'right';
        ctx.fillText(maxUnsatisfied.toString(), padding - 5, padding + 5);
        ctx.fillText('0', padding - 5, height - padding + 5);
        ctx.textAlign = 'center';
        ctx.fillText('Unsatisfied', padding - 20, height / 2);

        // Draw x-axis label
        ctx.fillText('Flips', width / 2, height - 5);

        // Draw line
        if (numPoints > 1) {
            ctx.strokeStyle = '#0066cc';
            ctx.lineWidth = 2;
            ctx.beginPath();

            for (let i = 0; i < numPoints; i++) {
                const x = padding + (i / (numPoints - 1)) * chartWidth;
                const y = height - padding - (this.unsatisfiedHistory[i] / maxUnsatisfied) * chartHeight;

                if (i === 0) {
                    ctx.moveTo(x, y);
                } else {
                    ctx.lineTo(x, y);
                }
            }

            ctx.stroke();

            // Draw points
            ctx.fillStyle = '#0066cc';
            for (let i = 0; i < numPoints; i++) {
                const x = padding + (i / (numPoints - 1)) * chartWidth;
                const y = height - padding - (this.unsatisfiedHistory[i] / maxUnsatisfied) * chartHeight;

                ctx.beginPath();
                ctx.arc(x, y, 3, 0, 2 * Math.PI);
                ctx.fill();
            }
        }
    }

    addStep(data) {
        this.stepHistory.push(data);

        const stepCard = document.createElement('div');
        stepCard.style.cssText = `
            margin-bottom: 10px;
            padding: 12px;
            background: white;
            border-radius: 4px;
            border-left: 4px solid ${data.color};
            box-shadow: 0 1px 2px rgba(0,0,0,0.1);
        `;

        // Step header
        const header = document.createElement('div');
        header.style.cssText = `
            font-weight: bold;
            color: #495057;
            font-size: 14px;
            margin-bottom: 4px;
        `;
        header.innerHTML = `<span style="color: #6c757d; font-weight: normal;">#${this.stepHistory.length}:</span> ${data.message}`;
        stepCard.appendChild(header);

        // Step details
        if (data.details) {
            const details = document.createElement('div');
            details.style.cssText = `
                font-size: 13px;
                color: #6c757d;
                margin-top: 4px;
            `;
            details.textContent = data.details;
            stepCard.appendChild(details);
        }

        // Show flipped variable if present
        if (data.variable) {
            const varDiv = document.createElement('div');
            varDiv.style.cssText = `
                margin-top: 8px;
                padding: 6px 10px;
                background: #e7f3ff;
                border-radius: 3px;
                font-family: monospace;
                font-size: 13px;
                color: #0066cc;
                font-weight: bold;
            `;
            varDiv.textContent = `Flipped: ${data.variable}`;
            stepCard.appendChild(varDiv);
        }

        // Show unsatisfied clause
        if (data.unsatisfiedClause) {
            const clauseDiv = document.createElement('div');
            clauseDiv.style.cssText = `
                margin-top: 6px;
                padding: 6px 10px;
                background: #fff3cd;
                border-radius: 3px;
                font-family: monospace;
                font-size: 12px;
                color: #856404;
            `;
            clauseDiv.textContent = `Unsatisfied clause: ${data.unsatisfiedClause}`;
            stepCard.appendChild(clauseDiv);
        }

        // Show break counts if present
        if (data.breakCounts) {
            const breaksDiv = document.createElement('div');
            breaksDiv.style.cssText = `
                margin-top: 6px;
                padding: 6px 10px;
                background: #f8f9fa;
                border-radius: 3px;
                font-size: 12px;
                color: #495057;
            `;

            const breaksList = Object.entries(data.breakCounts)
                .map(([v, b]) => `${v}: ${b} breaks`)
                .join(', ');

            breaksDiv.innerHTML = `<strong>Break counts:</strong> ${breaksList}`;
            stepCard.appendChild(breaksDiv);
        }

        this.historyContent.appendChild(stepCard);

        // Auto-scroll to bottom
        this.historyDiv.scrollTop = this.historyDiv.scrollHeight;
    }
}
