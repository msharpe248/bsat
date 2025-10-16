// CDCL (Conflict-Driven Clause Learning) Visualizer

class CDCLVisualizer {
    constructor(container) {
        this.container = container;
        this.trail = [];
        this.learnedClauses = [];
        this.assignment = {};
        this.stepHistory = [];
        this.currentDecisionLevel = 0;

        this.initialize();
    }

    initialize() {
        this.container.innerHTML = '';

        // Create main layout
        const wrapper = document.createElement('div');
        wrapper.style.cssText = 'display: flex; flex-direction: column; gap: 20px; height: 100%; overflow: auto;';

        // Assignment trail
        this.trailDiv = document.createElement('div');
        this.trailDiv.className = 'cdcl-trail';
        this.trailDiv.style.cssText = `
            padding: 15px;
            background: #f8f9fa;
            border-radius: 5px;
            border: 2px solid #dee2e6;
        `;

        const trailTitle = document.createElement('h3');
        trailTitle.textContent = 'Assignment Trail (by Decision Level)';
        trailTitle.style.cssText = 'margin: 0 0 10px 0; color: #343a40; font-size: 16px;';
        this.trailDiv.appendChild(trailTitle);

        this.trailContent = document.createElement('div');
        this.trailContent.style.cssText = `
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
            font-family: monospace;
            font-size: 13px;
            max-height: 200px;
            overflow-y: auto;
        `;
        this.trailDiv.appendChild(this.trailContent);

        // Learned clauses
        this.learnedDiv = document.createElement('div');
        this.learnedDiv.className = 'cdcl-learned';
        this.learnedDiv.style.cssText = `
            padding: 15px;
            background: #f8f9fa;
            border-radius: 5px;
            border: 2px solid #dee2e6;
        `;

        const learnedTitle = document.createElement('h3');
        learnedTitle.textContent = 'Learned Clauses';
        learnedTitle.style.cssText = 'margin: 0 0 10px 0; color: #343a40; font-size: 16px;';
        this.learnedDiv.appendChild(learnedTitle);

        this.learnedContent = document.createElement('div');
        this.learnedContent.style.cssText = `
            display: flex;
            flex-direction: column;
            gap: 6px;
            max-height: 150px;
            overflow-y: auto;
        `;
        this.learnedDiv.appendChild(this.learnedContent);

        // Step history
        this.historyDiv = document.createElement('div');
        this.historyDiv.className = 'cdcl-history';
        this.historyDiv.style.cssText = `
            padding: 15px;
            background: #f8f9fa;
            border-radius: 5px;
            border: 2px solid #dee2e6;
            flex: 1;
            overflow-y: auto;
        `;

        const historyTitle = document.createElement('h3');
        historyTitle.textContent = 'CDCL Execution Steps';
        historyTitle.style.cssText = 'margin: 0 0 15px 0; color: #343a40; font-size: 16px;';
        this.historyDiv.appendChild(historyTitle);

        this.historyContent = document.createElement('div');
        this.historyContent.style.cssText = 'display: flex; flex-direction: column; gap: 10px;';
        this.historyDiv.appendChild(this.historyContent);

        wrapper.appendChild(this.trailDiv);
        wrapper.appendChild(this.learnedDiv);
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
            case 'initial_propagation':
                this.handleInitialPropagation(data);
                break;
            case 'decision':
                this.handleDecision(data);
                break;
            case 'propagation_complete':
                this.handlePropagationComplete(data);
                break;
            case 'conflict':
                this.handleConflict(data);
                break;
            case 'learn_clause':
                this.handleLearnClause(data);
                break;
            case 'backjump':
                this.handleBackjump(data);
                break;
            case 'restart':
                this.handleRestart(data);
                break;
            case 'sat':
                this.handleSAT(data);
                break;
            case 'unsat':
                this.handleUNSAT(data);
                break;
            case 'iteration_limit':
                this.handleIterationLimit(data);
                break;
        }

        this.renderTrail();
        this.renderLearnedClauses();
    }

    handleStart(data) {
        this.addStep({
            type: 'start',
            message: 'Starting CDCL solver',
            details: `${data.num_original_clauses} clauses, VSIDS heuristic + clause learning`,
            color: '#17a2b8'
        });
    }

    handleInitialPropagation(data) {
        this.trail = data.trail || [];
        this.assignment = data.assignment || {};

        this.addStep({
            type: 'initial_propagation',
            message: 'Initial unit propagation',
            details: data.message || `Propagated ${Object.keys(this.assignment).length} variables`,
            color: '#6c757d'
        });
    }

    handleDecision(data) {
        this.currentDecisionLevel = data.decision_level;
        this.trail = data.trail || [];
        this.assignment = data.assignment || {};

        const topVsids = Object.entries(data.top_vsids || {})
            .map(([v, s]) => `${v}:${s.toFixed(2)}`)
            .join(', ');

        this.addStep({
            type: 'decision',
            message: `Decision @ level ${data.decision_level}: ${data.variable} = ${data.value}`,
            details: `VSIDS score: ${data.vsids_score.toFixed(3)}. Top vars: ${topVsids}`,
            variable: data.variable,
            color: '#0066cc'
        });
    }

    handlePropagationComplete(data) {
        this.trail = data.trail || [];
        this.assignment = data.assignment || {};

        this.addStep({
            type: 'propagation',
            message: `Unit propagation complete`,
            details: `${data.num_assigned}/${data.num_variables} variables assigned`,
            color: '#28a745'
        });
    }

    handleConflict(data) {
        this.trail = data.trail || [];
        this.assignment = data.assignment || {};

        this.addStep({
            type: 'conflict',
            message: `✗ Conflict @ level ${data.decision_level}`,
            details: data.message || `Conflicting clause: ${data.conflict_clause}`,
            clause: data.conflict_clause,
            color: '#dc3545'
        });
    }

    handleLearnClause(data) {
        this.learnedClauses.push({
            clause: data.learned_clause,
            backtrack_level: data.backtrack_level
        });

        this.addStep({
            type: 'learn',
            message: `📚 Learned clause #${data.num_learned_total}`,
            details: data.message || `Clause: ${data.learned_clause}`,
            clause: data.learned_clause,
            backtrack_info: `Level ${data.current_level} → ${data.backtrack_level}`,
            color: '#ffc107'
        });
    }

    handleBackjump(data) {
        this.trail = data.trail || [];
        this.assignment = data.assignment || {};
        this.currentDecisionLevel = data.to_level;

        this.addStep({
            type: 'backjump',
            message: `⬅ Backjump: level ${data.from_level} → ${data.to_level}`,
            details: data.message || 'Non-chronological backtracking',
            color: '#ff6b6b'
        });
    }

    handleRestart(data) {
        this.trail = [];
        this.assignment = {};
        this.currentDecisionLevel = 0;

        this.addStep({
            type: 'restart',
            message: `🔄 Restart #${data.restart_count}`,
            details: data.message || `${data.total_conflicts} conflicts, ${data.learned_clauses} learned clauses kept`,
            color: '#9b59b6'
        });
    }

    handleSAT(data) {
        this.assignment = data.assignment || {};

        this.addStep({
            type: 'sat',
            message: '✓ SAT - Formula is satisfiable',
            details: data.message || `Solution found with ${Object.keys(this.assignment).length} variables`,
            color: '#28a745'
        });
    }

    handleUNSAT(data) {
        this.addStep({
            type: 'unsat',
            message: '✗ UNSAT - Formula is unsatisfiable',
            details: data.message || 'Conflict analysis proved unsatisfiability',
            color: '#dc3545'
        });
    }

    handleIterationLimit(data) {
        this.addStep({
            type: 'limit',
            message: 'Visualization limit reached',
            details: data.message || 'Continuing to solve in background...',
            color: '#6c757d'
        });
    }

    renderTrail() {
        this.trailContent.innerHTML = '';

        if (this.trail.length === 0) {
            this.trailContent.textContent = 'No assignments yet';
            return;
        }

        // Group by decision level
        const byLevel = {};
        this.trail.forEach(assignment => {
            // Parse format: "var=value@level"
            const match = assignment.match(/(\w+)=(\w+)@(\d+)/);
            if (match) {
                const [_, variable, value, level] = match;
                if (!byLevel[level]) byLevel[level] = [];
                byLevel[level].push({ variable, value: value === 'True' });
            }
        });

        // Render each level
        Object.entries(byLevel).sort((a, b) => parseInt(a[0]) - parseInt(b[0])).forEach(([level, assignments]) => {
            const levelDiv = document.createElement('div');
            levelDiv.style.cssText = `
                display: flex;
                align-items: center;
                gap: 8px;
                padding: 6px;
                background: ${level === '0' ? '#e9ecef' : '#fff'};
                border-radius: 4px;
                width: 100%;
            `;

            const levelLabel = document.createElement('span');
            levelLabel.style.cssText = 'font-weight: bold; color: #6c757d; min-width: 60px;';
            levelLabel.textContent = `L${level}:`;
            levelDiv.appendChild(levelLabel);

            const assignmentsDiv = document.createElement('div');
            assignmentsDiv.style.cssText = 'display: flex; gap: 6px; flex-wrap: wrap;';

            assignments.forEach(({ variable, value }) => {
                const badge = document.createElement('span');
                badge.style.cssText = `
                    padding: 3px 8px;
                    border-radius: 3px;
                    background: ${value ? '#d4edda' : '#f8d7da'};
                    color: ${value ? '#155724' : '#721c24'};
                    font-size: 12px;
                    font-weight: bold;
                `;
                badge.textContent = `${variable}=${value}`;
                assignmentsDiv.appendChild(badge);
            });

            levelDiv.appendChild(assignmentsDiv);
            this.trailContent.appendChild(levelDiv);
        });
    }

    renderLearnedClauses() {
        this.learnedContent.innerHTML = '';

        if (this.learnedClauses.length === 0) {
            this.learnedContent.textContent = 'No learned clauses yet';
            return;
        }

        // Show most recent learned clauses (limit to last 10)
        const recent = this.learnedClauses.slice(-10);

        recent.forEach((item, idx) => {
            const clauseDiv = document.createElement('div');
            clauseDiv.style.cssText = `
                padding: 6px 10px;
                background: white;
                border-radius: 3px;
                font-family: monospace;
                font-size: 12px;
                border-left: 3px solid #ffc107;
            `;

            const header = document.createElement('div');
            header.style.cssText = 'color: #6c757d; font-size: 11px;';
            header.textContent = `#${this.learnedClauses.length - recent.length + idx + 1} (backtrack to L${item.backtrack_level})`;
            clauseDiv.appendChild(header);

            const clause = document.createElement('div');
            clause.style.cssText = 'margin-top: 2px; color: #495057;';
            clause.textContent = item.clause;
            clauseDiv.appendChild(clause);

            this.learnedContent.appendChild(clauseDiv);
        });

        if (this.learnedClauses.length > 10) {
            const more = document.createElement('div');
            more.style.cssText = 'text-align: center; color: #6c757d; font-size: 12px; font-style: italic;';
            more.textContent = `... and ${this.learnedClauses.length - 10} more`;
            this.learnedContent.appendChild(more);
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
            varDiv.textContent = `Variable: ${data.variable}`;
            stepCard.appendChild(varDiv);
        }

        // Show clause if present
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
            clauseDiv.textContent = data.clause;
            stepCard.appendChild(clauseDiv);
        }

        // Show backtrack info if present
        if (data.backtrack_info) {
            const btDiv = document.createElement('div');
            btDiv.style.cssText = `
                margin-top: 6px;
                padding: 4px 8px;
                background: #fff3cd;
                border-radius: 3px;
                font-size: 12px;
                color: #856404;
            `;
            btDiv.textContent = data.backtrack_info;
            stepCard.appendChild(btDiv);
        }

        this.historyContent.appendChild(stepCard);

        // Auto-scroll to bottom
        this.historyDiv.scrollTop = this.historyDiv.scrollHeight;
    }
}
