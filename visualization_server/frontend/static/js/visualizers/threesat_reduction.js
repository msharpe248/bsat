// 3-SAT Reduction Visualizer using D3.js

class ThreeSATReductionVisualizer {
    constructor(container) {
        this.container = container;
        this.originalClauses = [];
        this.reducedClauses = [];
        this.currentClauseIndex = 0;
        this.auxVariables = new Set();

        this.initializeLayout();
    }

    initializeLayout() {
        // Clear container
        this.container.innerHTML = '';

        // Create main layout with two columns
        const layout = document.createElement('div');
        layout.className = 'reduction-layout';
        layout.innerHTML = `
            <div class="reduction-header">
                <h3>k-SAT to 3-SAT Reduction</h3>
                <div class="reduction-stats" id="reductionStats"></div>
            </div>
            <div class="reduction-columns">
                <div class="reduction-column">
                    <h4>Original Formula</h4>
                    <div class="clause-list" id="originalClauses"></div>
                </div>
                <div class="reduction-arrow">→</div>
                <div class="reduction-column">
                    <h4>Reduced 3-SAT Formula</h4>
                    <div class="clause-list" id="reducedClauses"></div>
                </div>
            </div>
            <div class="reduction-explanation" id="reductionExplanation"></div>
        `;

        this.container.appendChild(layout);

        this.statsDiv = document.getElementById('reductionStats');
        this.originalDiv = document.getElementById('originalClauses');
        this.reducedDiv = document.getElementById('reducedClauses');
        this.explanationDiv = document.getElementById('reductionExplanation');
    }

    update(state) {
        const action = state.action;
        const data = state.data;

        switch (action) {
            case 'start':
                this.handleStart(data);
                break;
            case 'keep_clause':
                this.handleKeepClause(data);
                break;
            case 'split_clause_start':
                this.handleSplitStart(data);
                break;
            case 'add_first_clause':
                this.handleAddClause(data, 'first');
                break;
            case 'add_middle_clause':
                this.handleAddClause(data, 'middle');
                break;
            case 'add_last_clause':
                this.handleAddClause(data, 'last');
                break;
            case 'split_clause_complete':
                this.handleSplitComplete(data);
                break;
            case 'reduction_complete':
                this.handleComplete(data);
                break;
        }
    }

    handleStart(data) {
        this.statsDiv.innerHTML = `
            <div class="stat">Original: <strong>${data.num_clauses}</strong> clauses, <strong>${data.num_variables}</strong> variables</div>
            <div class="stat">Max clause size: <strong>${data.max_clause_size}</strong></div>
        `;
    }

    handleKeepClause(data) {
        // Add to original list
        const originalClause = this.createClauseCard(data.original_clause, data.clause_index, 'kept');
        this.originalDiv.appendChild(originalClause);

        // Add to reduced list (same clause)
        const reducedClause = this.createClauseCard(data.original_clause, data.clause_index, 'kept');
        this.reducedDiv.appendChild(reducedClause);

        this.explanationDiv.innerHTML = `
            <div class="explanation-item">
                Clause ${data.clause_index + 1}: ${data.reason}
            </div>
        `;
    }

    handleSplitStart(data) {
        // Add original clause
        const originalClause = this.createClauseCard(data.original_clause, data.clause_index, 'splitting');
        this.originalDiv.appendChild(originalClause);

        // Track auxiliary variables
        data.aux_variables.forEach(aux => this.auxVariables.add(aux));

        this.explanationDiv.innerHTML = `
            <div class="explanation-item highlight">
                Splitting clause ${data.clause_index + 1}: ${data.original_clause}
                <br>Size: ${data.clause_size} literals → needs ${data.num_aux_needed} auxiliary variable(s)
                <br>Auxiliary variables: ${data.aux_variables.map(v => `<code class="aux-var">${v}</code>`).join(', ')}
            </div>
        `;
    }

    handleAddClause(data, position) {
        const reducedClause = this.createClauseCard(data.new_clause, null, 'new', position);
        this.reducedDiv.appendChild(reducedClause);

        // Update explanation
        const explanationItem = document.createElement('div');
        explanationItem.className = 'explanation-item';
        explanationItem.innerHTML = `
            <strong>${position === 'first' ? 'First' : position === 'last' ? 'Last' : 'Middle'} clause:</strong>
            ${data.new_clause}
            <br><small>${data.explanation}</small>
        `;
        this.explanationDiv.appendChild(explanationItem);
    }

    handleSplitComplete(data) {
        // Highlight the completed split
        this.explanationDiv.innerHTML = `
            <div class="explanation-item success">
                ✓ Clause ${data.clause_index + 1} split into ${data.num_new_clauses} 3-SAT clauses
            </div>
        `;
    }

    handleComplete(data) {
        this.statsDiv.innerHTML = `
            <div class="stat-grid">
                <div class="stat">
                    <div class="stat-label">Original</div>
                    <div class="stat-value">${data.original_clauses} clauses, ${data.original_variables} vars</div>
                </div>
                <div class="stat">
                    <div class="stat-label">Reduced</div>
                    <div class="stat-value">${data.reduced_clauses} clauses, ${data.total_variables} vars</div>
                </div>
                <div class="stat">
                    <div class="stat-label">Added</div>
                    <div class="stat-value">${data.auxiliary_variables} auxiliary variables</div>
                </div>
            </div>
        `;

        this.explanationDiv.innerHTML = `
            <div class="explanation-item success">
                <strong>✓ Reduction Complete!</strong>
                <br>The formula has been successfully converted to 3-SAT.
                <br>All clauses now have at most 3 literals.
            </div>
        `;
    }

    createClauseCard(clauseStr, index, status, position = null) {
        const card = document.createElement('div');
        card.className = `clause-card ${status}`;
        if (position) card.classList.add(position);

        // Parse clause to highlight literals
        const literals = this.parseLiterals(clauseStr);
        const literalsHTML = literals.map(lit => {
            const isAux = this.auxVariables.has(lit.replace(/^~/, ''));
            const litClass = isAux ? 'literal aux-literal' : 'literal';
            return `<span class="${litClass}">${lit}</span>`;
        }).join(' <span class="or-symbol">∨</span> ');

        card.innerHTML = `
            ${index !== null ? `<div class="clause-number">#${index + 1}</div>` : ''}
            <div class="clause-content">${literalsHTML}</div>
        `;

        return card;
    }

    parseLiterals(clauseStr) {
        // Remove outer parentheses and split by |
        const cleaned = clauseStr.replace(/^\(|\)$/g, '').trim();
        return cleaned.split('|').map(lit => lit.trim());
    }
}
