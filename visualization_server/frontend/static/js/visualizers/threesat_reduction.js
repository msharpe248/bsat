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
            <div class="reduction-columns" id="reductionColumns" style="position: relative;">
                <svg id="connectionLines" style="position: absolute; top: 0; left: 0; width: 100%; height: 100%; pointer-events: none; z-index: 1;">
                </svg>
                <div class="reduction-column" style="position: relative; z-index: 2;">
                    <h4>Original Formula</h4>
                    <div class="clause-list" id="originalClauses"></div>
                </div>
                <div class="reduction-arrow" style="position: relative; z-index: 2;">→</div>
                <div class="reduction-column" style="position: relative; z-index: 2;">
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
        this.columnsDiv = document.getElementById('reductionColumns');
        this.svg = document.getElementById('connectionLines');

        // Track clause mappings for drawing lines
        this.clauseMappings = [];
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

        // Add to reduced list (same clause) - mark with data attribute
        const reducedClause = this.createClauseCard(data.original_clause, data.clause_index, 'kept');
        reducedClause.setAttribute('data-original-index', data.clause_index);
        reducedClause.setAttribute('data-is-first-reduced', 'true');
        this.reducedDiv.appendChild(reducedClause);

        // Track mapping for connector line
        const mapping = {
            original: originalClause,
            reduced: [reducedClause],
            clauseIndex: data.clause_index
        };
        this.clauseMappings.push(mapping);

        // Draw connector line for this specific mapping after a short delay to ensure layout
        setTimeout(() => this.drawConnectorLine(mapping), 100);

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

        // Start tracking this split - will accumulate reduced clauses and need for spacing
        this.currentSplitMapping = {
            original: originalClause,
            reduced: [],
            clauseIndex: data.clause_index,
            numNewClauses: 0  // Will count as we add them
        };

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

        // Mark the first reduced clause for this split
        if (this.currentSplitMapping) {
            reducedClause.setAttribute('data-original-index', this.currentSplitMapping.clauseIndex);
            if (this.currentSplitMapping.numNewClauses === 0) {
                reducedClause.setAttribute('data-is-first-reduced', 'true');
            }
            this.currentSplitMapping.reduced.push(reducedClause);
            this.currentSplitMapping.numNewClauses++;
        }

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
        // Save the completed split mapping
        if (this.currentSplitMapping) {
            const mapping = this.currentSplitMapping;
            const numSpacers = mapping.numNewClauses - 1; // Add spacers for extra clauses

            this.clauseMappings.push(mapping);

            // Add spacer divs in the original column to align with the multiple reduced clauses
            for (let i = 0; i < numSpacers; i++) {
                const spacer = document.createElement('div');
                spacer.className = 'clause-spacer';
                this.originalDiv.appendChild(spacer);
            }

            this.currentSplitMapping = null;

            // Draw connector line for this specific mapping after a short delay to ensure layout
            setTimeout(() => this.drawConnectorLine(mapping), 100);
        }

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

    drawConnectorLine(mapping) {
        // Get the bounding rect of the columns container
        const containerRect = this.columnsDiv.getBoundingClientRect();

        const originalRect = mapping.original.getBoundingClientRect();

        // Find the FIRST reduced clause for this mapping using data attribute
        // This ensures we connect to the correct clause even if layout has shifted
        const firstReducedClause = this.reducedDiv.querySelector(
            `[data-original-index="${mapping.clauseIndex}"][data-is-first-reduced="true"]`
        );

        if (!firstReducedClause) {
            console.error('Could not find first reduced clause for', mapping.clauseIndex);
            return;
        }

        // Calculate the group bounding box for all reduced clauses
        if (mapping.reduced.length > 0) {
            const firstReducedRect = firstReducedClause.getBoundingClientRect();
            const lastReducedRect = mapping.reduced[mapping.reduced.length - 1].getBoundingClientRect();

            // Start point: right edge, middle of original clause
            const x1 = originalRect.right - containerRect.left;
            const y1 = originalRect.top + originalRect.height / 2 - containerRect.top;

            // End point: left edge, middle of the reduced clause group
            const x2 = firstReducedRect.left - containerRect.left;
            const y2Top = firstReducedRect.top - containerRect.top;
            const y2Bottom = lastReducedRect.bottom - containerRect.top;
            const y2 = (y2Top + y2Bottom) / 2;

            // Create SVG path with curved line
            const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');

            // Control points for bezier curve
            const midX = (x1 + x2) / 2;
            const pathData = `M ${x1} ${y1} C ${midX} ${y1}, ${midX} ${y2}, ${x2} ${y2}`;

            path.setAttribute('d', pathData);
            path.setAttribute('stroke', '#10b981'); // green color
            path.setAttribute('stroke-width', '2');
            path.setAttribute('fill', 'none');
            path.setAttribute('opacity', '0.6');

            this.svg.appendChild(path);

            // Add a small circle at the end
            const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
            circle.setAttribute('cx', x2);
            circle.setAttribute('cy', y2);
            circle.setAttribute('r', '4');
            circle.setAttribute('fill', '#10b981');
            circle.setAttribute('opacity', '0.8');

            this.svg.appendChild(circle);

            // Draw a bracket on the right side if there are multiple reduced clauses
            if (mapping.reduced.length > 1) {
                const bracketPath = document.createElementNS('http://www.w3.org/2000/svg', 'path');
                const bracketX = x2 - 10;
                const bracketData = `M ${bracketX} ${y2Top} L ${x2} ${y2Top} L ${x2} ${y2Bottom} L ${bracketX} ${y2Bottom}`;

                bracketPath.setAttribute('d', bracketData);
                bracketPath.setAttribute('stroke', '#10b981');
                bracketPath.setAttribute('stroke-width', '1.5');
                bracketPath.setAttribute('fill', 'none');
                bracketPath.setAttribute('opacity', '0.4');

                this.svg.appendChild(bracketPath);
            }
        }
    }
}
