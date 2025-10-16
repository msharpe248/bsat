// Horn-SAT Unit Propagation Visualizer

class HornSATVisualizer {
    constructor(container) {
        this.container = container;
        this.assignment = {};
        this.clauses = [];
        this.stepHistory = [];

        this.initialize();
    }

    initialize() {
        this.container.innerHTML = '';

        // Create main layout
        const wrapper = document.createElement('div');
        wrapper.style.cssText = 'display: flex; flex-direction: column; gap: 20px; height: 100%; overflow: auto;';

        // Assignment display
        this.assignmentDiv = document.createElement('div');
        this.assignmentDiv.className = 'hornsat-assignment';
        this.assignmentDiv.style.cssText = `
            padding: 15px;
            background: #f8f9fa;
            border-radius: 5px;
            border: 2px solid #dee2e6;
        `;

        const assignmentTitle = document.createElement('h3');
        assignmentTitle.textContent = 'Variable Assignment';
        assignmentTitle.style.cssText = 'margin: 0 0 10px 0; color: #343a40; font-size: 16px;';
        this.assignmentDiv.appendChild(assignmentTitle);

        this.assignmentContent = document.createElement('div');
        this.assignmentContent.style.cssText = `
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
            font-family: monospace;
            font-size: 14px;
        `;
        this.assignmentDiv.appendChild(this.assignmentContent);

        // Clauses display
        this.clausesDiv = document.createElement('div');
        this.clausesDiv.className = 'hornsat-clauses';
        this.clausesDiv.style.cssText = `
            padding: 15px;
            background: #f8f9fa;
            border-radius: 5px;
            border: 2px solid #dee2e6;
        `;

        const clausesTitle = document.createElement('h3');
        clausesTitle.textContent = 'Clauses';
        clausesTitle.style.cssText = 'margin: 0 0 10px 0; color: #343a40; font-size: 16px;';
        this.clausesDiv.appendChild(clausesTitle);

        this.clausesContent = document.createElement('div');
        this.clausesContent.style.cssText = `
            display: flex;
            flex-direction: column;
            gap: 6px;
            max-height: 300px;
            overflow-y: auto;
        `;
        this.clausesDiv.appendChild(this.clausesContent);

        // Step history
        this.historyDiv = document.createElement('div');
        this.historyDiv.className = 'hornsat-history';
        this.historyDiv.style.cssText = `
            padding: 15px;
            background: #f8f9fa;
            border-radius: 5px;
            border: 2px solid #dee2e6;
            flex: 1;
            overflow-y: auto;
        `;

        const historyTitle = document.createElement('h3');
        historyTitle.textContent = 'Unit Propagation Steps';
        historyTitle.style.cssText = 'margin: 0 0 15px 0; color: #343a40; font-size: 16px;';
        this.historyDiv.appendChild(historyTitle);

        this.historyContent = document.createElement('div');
        this.historyContent.style.cssText = 'display: flex; flex-direction: column; gap: 10px;';
        this.historyDiv.appendChild(this.historyContent);

        wrapper.appendChild(this.assignmentDiv);
        wrapper.appendChild(this.clausesDiv);
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
            case 'initialize':
                this.handleInitialize(data);
                break;
            case 'unit_propagation':
                this.handleUnitPropagation(data);
                break;
            case 'sat':
                this.handleSAT(data);
                break;
            case 'unsat':
                this.handleUNSAT(data);
                break;
        }

        this.renderAssignment();
        this.renderClauses();
    }

    handleStart(data) {
        this.clauses = data.clauses || [];

        this.addStep({
            type: 'start',
            message: 'Starting Horn-SAT solver',
            details: `${data.clauses.length} Horn clauses (at most 1 positive literal per clause)`,
            color: '#17a2b8'
        });
    }

    handleInitialize(data) {
        this.assignment = data.assignment || {};

        this.addStep({
            type: 'initialize',
            message: 'Initialize all variables to False',
            details: data.message || 'Horn-SAT uses monotonic assignment: variables only change from False → True',
            color: '#6c757d'
        });
    }

    handleUnitPropagation(data) {
        this.assignment = data.assignment || {};
        const variable = data.variable;
        const clause = data.clause;

        this.addStep({
            type: 'unit_propagation',
            message: `Unit propagation: ${variable} = True`,
            details: data.message || `Forced by clause: ${clause}`,
            variable: variable,
            clause: clause,
            iteration: data.iteration,
            color: '#0066cc'
        });
    }

    handleSAT(data) {
        this.assignment = data.assignment || {};

        this.addStep({
            type: 'sat',
            message: '✓ SAT - Formula is satisfiable',
            details: data.message || `Solution found in ${data.iterations} iterations with ${data.propagations.length} unit propagations`,
            color: '#28a745'
        });
    }

    handleUNSAT(data) {
        this.assignment = data.assignment || {};

        const details = data.unsatisfied_clauses
            ? `Unsatisfied clauses: ${data.unsatisfied_clauses.join(', ')}`
            : data.message || 'No satisfying assignment exists';

        this.addStep({
            type: 'unsat',
            message: '✗ UNSAT - Formula is unsatisfiable',
            details: details,
            color: '#dc3545'
        });
    }

    renderAssignment() {
        this.assignmentContent.innerHTML = '';

        const sortedVars = Object.keys(this.assignment).sort();

        sortedVars.forEach(variable => {
            const value = this.assignment[variable];
            const badge = document.createElement('div');
            badge.style.cssText = `
                padding: 6px 12px;
                border-radius: 4px;
                background: ${value ? '#28a745' : '#6c757d'};
                color: white;
                font-weight: bold;
                font-size: 13px;
            `;
            badge.textContent = `${variable} = ${value}`;
            this.assignmentContent.appendChild(badge);
        });
    }

    renderClauses() {
        this.clausesContent.innerHTML = '';

        this.clauses.forEach((clause, idx) => {
            const clauseDiv = document.createElement('div');
            clauseDiv.style.cssText = `
                padding: 8px;
                background: white;
                border-radius: 3px;
                font-family: monospace;
                font-size: 13px;
                border-left: 3px solid #dee2e6;
            `;

            // Highlight clause based on assignment
            const highlighted = this.highlightClause(clause, this.assignment);
            clauseDiv.innerHTML = highlighted;

            this.clausesContent.appendChild(clauseDiv);
        });
    }

    highlightClause(clauseStr, assignment) {
        // Parse and highlight variables in a clause
        let result = clauseStr;

        // Find all variables (including negated ones)
        const variablePattern = /(~?)(\w+)/g;

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

            let color = '#495057'; // Default gray
            let bgColor = 'transparent';
            let fontWeight = 'normal';

            if (assignment.hasOwnProperty(varName)) {
                const value = assignment[varName];
                const literalValue = isNegated ? !value : value;

                if (literalValue) {
                    // This literal is true - green with background
                    color = '#28a745';
                    bgColor = '#d4edda';
                    fontWeight = 'bold';
                } else {
                    // This literal is false - red
                    color = '#dc3545';
                    fontWeight = 'normal';
                }
            }

            const highlighted = `<span style="color: ${color}; background: ${bgColor}; padding: 2px 4px; border-radius: 2px; font-weight: ${fontWeight};">${m.full}</span>`;

            result = result.substring(0, m.index) + highlighted + result.substring(m.index + m.full.length);
        });

        return result;
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

        // Highlight variable if present
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
            varDiv.textContent = `${data.variable} = True`;
            stepCard.appendChild(varDiv);
        }

        // Show forcing clause if present
        if (data.clause) {
            const clauseDiv = document.createElement('div');
            clauseDiv.style.cssText = `
                margin-top: 6px;
                padding: 6px 10px;
                background: #f8f9fa;
                border-radius: 3px;
                font-family: monospace;
                font-size: 12px;
                color: #495057;
            `;
            clauseDiv.textContent = `Forcing clause: ${data.clause}`;
            stepCard.appendChild(clauseDiv);
        }

        this.historyContent.appendChild(stepCard);

        // Auto-scroll to bottom
        this.historyDiv.scrollTop = this.historyDiv.scrollHeight;
    }
}
