// Main application logic for SAT Solver Visualizer

class SATVisualizerApp {
    constructor() {
        this.currentSessionId = null;
        this.websocket = null;
        this.stateHistory = [];
        this.currentStep = 0;
        this.isPlaying = false;
        this.currentVisualizer = null;
        this.stepMode = false;  // Track if we're in step-by-step mode
        this.paused = false;    // Track if we're paused (states buffering but not rendering)

        this.initializeElements();
        this.attachEventListeners();
        this.loadExamples();
        this.updateAlgorithmInfo();
    }

    initializeElements() {
        // Input elements
        this.formulaInput = document.getElementById('formulaInput');
        this.exampleSelect = document.getElementById('exampleSelect');
        this.algorithmSelect = document.getElementById('algorithmSelect');
        this.speedSlider = document.getElementById('speedSlider');
        this.speedValue = document.getElementById('speedValue');
        this.numClauses = document.getElementById('numClauses');
        this.numVariables = document.getElementById('numVariables');
        this.satTypeSelect = document.getElementById('satTypeSelect');

        // Button elements
        this.validateBtn = document.getElementById('validateBtn');
        this.solveBtn = document.getElementById('solveBtn');
        this.generateBtn = document.getElementById('generateBtn');
        this.pauseBtn = document.getElementById('pauseBtn');
        this.stepBtn = document.getElementById('stepBtn');
        this.resetBtn = document.getElementById('resetBtn');

        // Panel elements
        this.toggleConsoleBtn = document.getElementById('toggleConsoleBtn');
        this.toggleSolutionBtn = document.getElementById('toggleSolutionBtn');
        this.closeConsoleBtn = document.getElementById('closeConsoleBtn');
        this.closeSolutionBtn = document.getElementById('closeSolutionBtn');
        this.consolePanel = document.getElementById('consolePanel');
        this.solutionPanel = document.getElementById('solutionPanel');
        this.solutionContent = document.getElementById('solutionContent');

        // Display elements
        this.formulaError = document.getElementById('formulaError');
        this.formulaInfo = document.getElementById('formulaInfo');
        this.algorithmInfo = document.getElementById('algorithmInfo');
        this.visualization = document.getElementById('visualization');
        this.consoleOutput = document.getElementById('console');
        this.statistics = document.getElementById('statistics');
        this.progressFill = document.getElementById('progressFill');
        this.progressText = document.getElementById('progressText');
        this.controlsSection = document.getElementById('controlsSection');
        this.statsSection = document.getElementById('statsSection');
        this.vizTitle = document.getElementById('vizTitle');
    }

    attachEventListeners() {
        // Input change handlers
        this.exampleSelect.addEventListener('change', () => this.loadExample());
        this.formulaInput.addEventListener('input', () => this.clearError());
        this.algorithmSelect.addEventListener('change', () => this.updateAlgorithmInfo());
        this.speedSlider.addEventListener('input', () => {
            this.speedValue.textContent = `${this.speedSlider.value}ms`;
        });

        // Button click handlers
        this.validateBtn.addEventListener('click', () => this.validateFormula());
        this.solveBtn.addEventListener('click', () => this.startSolving());
        this.generateBtn.addEventListener('click', () => this.generateFormula());
        this.pauseBtn.addEventListener('click', () => this.pauseSolving());
        this.stepBtn.addEventListener('click', () => this.stepForward());
        this.resetBtn.addEventListener('click', () => this.reset());

        // Panel toggle handlers
        this.toggleConsoleBtn.addEventListener('click', () => this.toggleConsole());
        this.toggleSolutionBtn.addEventListener('click', () => this.toggleSolution());
        this.closeConsoleBtn.addEventListener('click', () => this.closeConsole());
        this.closeSolutionBtn.addEventListener('click', () => this.closeSolution());
    }

    toggleConsole() {
        this.consolePanel.classList.toggle('open');
        // Close solution if open
        if (this.consolePanel.classList.contains('open')) {
            this.solutionPanel.classList.remove('open');
        }
    }

    toggleSolution() {
        this.solutionPanel.classList.toggle('open');
        // Close console if open
        if (this.solutionPanel.classList.contains('open')) {
            this.consolePanel.classList.remove('open');
        }
    }

    closeConsole() {
        this.consolePanel.classList.remove('open');
    }

    closeSolution() {
        this.solutionPanel.classList.remove('open');
    }

    async loadExamples() {
        try {
            const response = await fetch('/api/examples');
            const examples = await response.json();

            examples.forEach(example => {
                const option = document.createElement('option');
                option.value = JSON.stringify(example);
                option.textContent = `${example.name} (${example.difficulty})`;
                this.exampleSelect.appendChild(option);
            });
        } catch (error) {
            this.logConsole('Error loading examples: ' + error.message, 'error');
        }
    }

    loadExample() {
        const selectedValue = this.exampleSelect.value;
        if (!selectedValue) return;

        try {
            const example = JSON.parse(selectedValue);
            this.formulaInput.value = example.formula;
            this.algorithmSelect.value = example.algorithm;
            this.updateAlgorithmInfo();
            this.logConsole(`Loaded example: ${example.name}`, 'info');
        } catch (error) {
            this.logConsole('Error loading example: ' + error.message, 'error');
        }
    }

    updateAlgorithmInfo() {
        const algorithm = this.algorithmSelect.value;
        const infoMap = {
            'dpll': 'Classic backtracking with unit propagation and pure literal elimination. O(2ⁿ) worst case.',
            '2sat': 'Polynomial-time algorithm using strongly connected components. O(n+m) time.',
            'davis_putnam': 'Original 1960 resolution-based algorithm. Shows exponential clause growth.',
            'cdcl': 'Modern conflict-driven clause learning. Used in industrial SAT solvers.',
            'hornsat': 'Polynomial-time solver for Horn formulas (≤1 positive literal per clause).',
            'walksat': 'Randomized local search. Incomplete but often fast on satisfiable instances.',
            '3sat_reduction': 'Reduces k-SAT formulas to 3-SAT by introducing auxiliary variables. Shows polynomial reduction.'
        };

        this.algorithmInfo.textContent = infoMap[algorithm] || '';
    }

    clearError() {
        this.formulaError.classList.remove('show');
        this.formulaInfo.classList.remove('show');
    }

    showError(message) {
        this.formulaError.textContent = message;
        this.formulaError.classList.add('show');
        this.formulaInfo.classList.remove('show');
    }

    showInfo(message) {
        this.formulaInfo.textContent = message;
        this.formulaInfo.classList.add('show');
        this.formulaError.classList.remove('show');
    }

    async generateFormula() {
        this.clearError();

        // Get selected SAT type from dropdown
        const satType = this.satTypeSelect.value;
        const numClauses = parseInt(this.numClauses.value);
        const numVariables = parseInt(this.numVariables.value) || 0;

        if (isNaN(numClauses) || numClauses < 1) {
            this.showError('Please enter a valid number of clauses (≥ 1)');
            return;
        }

        // Disable button during generation
        this.generateBtn.disabled = true;
        this.generateBtn.textContent = 'Generating...';

        try {
            const response = await fetch('/api/formula/generate', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    sat_type: satType,
                    num_clauses: numClauses,
                    num_variables: numVariables
                })
            });

            if (!response.ok) {
                throw new Error('Failed to generate formula');
            }

            const result = await response.json();

            // Set the generated formula
            this.formulaInput.value = result.formula;

            // Keep the current algorithm selection - don't override it
            // This allows users to test different algorithms on the same formula type

            // Show success message
            this.showInfo(
                `✓ Generated ${result.sat_type.toUpperCase()} with ${result.num_clauses} clauses, ${result.num_variables} variables`
            );
            this.logConsole(
                `Generated random ${result.sat_type.toUpperCase()} formula: ${result.num_clauses} clauses, ${result.num_variables} variables`,
                'success'
            );
        } catch (error) {
            this.showError('Error generating formula: ' + error.message);
            this.logConsole('Generation error: ' + error.message, 'error');
        } finally {
            this.generateBtn.disabled = false;
            this.generateBtn.textContent = '🎲 Generate';
        }
    }

    async validateFormula() {
        this.clearError();
        const formula = this.formulaInput.value.trim();

        if (!formula) {
            this.showError('Please enter a formula');
            return;
        }

        try {
            const response = await fetch('/api/formula/validate', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ formula, format: 'text' })
            });

            const result = await response.json();

            if (result.valid) {
                this.showInfo(
                    `✓ Valid! ${result.num_clauses} clauses, ${result.variables.length} variables: ${result.variables.join(', ')}`
                );
                this.logConsole('Formula validated successfully', 'success');
            } else {
                this.showError(`✗ Invalid: ${result.error}`);
                this.logConsole('Formula validation failed: ' + result.error, 'error');
            }
        } catch (error) {
            this.showError('Error validating formula: ' + error.message);
            this.logConsole('Validation error: ' + error.message, 'error');
        }
    }

    async startSolving() {
        this.clearError();
        const formula = this.formulaInput.value.trim();
        const algorithm = this.algorithmSelect.value;
        const speed = parseInt(this.speedSlider.value);

        if (!formula) {
            this.showError('Please enter a formula');
            return;
        }

        // If we're paused with an active session, just unpause
        if (this.paused && this.websocket && this.websocket.readyState === WebSocket.OPEN) {
            this.paused = false;
            this.stepMode = false;
            this.logConsole('Resuming...', 'info');
            this.solveBtn.style.display = 'none';
            this.pauseBtn.style.display = 'inline-block';
            return;
        }

        // Disable solve button
        this.solveBtn.disabled = true;
        this.solveBtn.textContent = 'Solving...';

        try {
            // Create session
            const response = await fetch('/api/solve', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ formula, algorithm, format: 'text', speed })
            });

            if (!response.ok) {
                throw new Error('Failed to create session');
            }

            const result = await response.json();
            this.currentSessionId = result.session_id;

            this.logConsole(`Starting ${algorithm.toUpperCase()} solver...`, 'info');

            // Clear previous visualization
            this.clearVisualization();
            this.stateHistory = [];
            this.currentStep = 0;

            // Show controls and buttons
            this.controlsSection.style.display = 'block';
            this.statsSection.style.display = 'block';
            this.pauseBtn.style.display = 'inline-block';
            this.resetBtn.style.display = 'inline-block';
            this.solveBtn.style.display = 'none';  // Hide solve button while solving

            // Connect WebSocket
            this.connectWebSocket();

        } catch (error) {
            this.showError('Error starting solver: ' + error.message);
            this.logConsole('Solver error: ' + error.message, 'error');
            this.solveBtn.disabled = false;
            this.solveBtn.textContent = 'Solve';
        }
    }

    connectWebSocket() {
        const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${wsProtocol}//${window.location.host}/ws/solve/${this.currentSessionId}`;

        this.websocket = new WebSocket(wsUrl);

        this.websocket.onopen = () => {
            this.logConsole('Connected to solver', 'success');
        };

        this.websocket.onmessage = (event) => {
            const message = JSON.parse(event.data);
            this.handleWebSocketMessage(message);
        };

        this.websocket.onerror = (error) => {
            this.logConsole('WebSocket error', 'error');
            console.error('WebSocket error:', error);
        };

        this.websocket.onclose = () => {
            this.logConsole('Connection closed', 'info');
            this.solveBtn.disabled = false;
            this.solveBtn.textContent = 'Solve';
            this.solveBtn.style.display = 'inline-block';
            this.pauseBtn.style.display = 'none';
        };
    }

    handleWebSocketMessage(message) {
        if (message.type === 'state_update') {
            this.stateHistory.push(message);

            // If in step mode or paused, don't auto-render
            if (this.stepMode || this.paused) {
                this.logConsole(`Step ${this.stateHistory.length} buffered (click Step to view)`, 'info');
                return;  // Don't render yet
            }

            // Auto-render if not paused
            this.currentStep = this.stateHistory.length;
            this.renderState(message);
            this.updateProgress();
        } else if (message.type === 'complete') {
            this.handleCompletion(message);
        } else if (message.type === 'error') {
            this.logConsole('Solver error: ' + message.message, 'error');
            this.showError(message.message);
        }
    }

    renderState(state) {
        const action = state.action;
        const data = state.data;

        // Log to console
        this.logConsole(`Step ${state.step}: ${action}`, 'info');

        // Create or update visualizer
        if (!this.currentVisualizer) {
            const algorithm = this.algorithmSelect.value;
            this.currentVisualizer = this.createVisualizer(algorithm);
        }

        // Update visualizer
        if (this.currentVisualizer && this.currentVisualizer.update) {
            this.currentVisualizer.update(state);
        }
    }

    createVisualizer(algorithm) {
        // Clear existing visualization
        this.visualization.innerHTML = '';

        if (algorithm === 'dpll' && typeof DPLLVisualizer !== 'undefined') {
            return new DPLLVisualizer(this.visualization);
        } else if (algorithm === '2sat' && typeof TwoSATVisualizer !== 'undefined') {
            return new TwoSATVisualizer(this.visualization);
        } else if (algorithm === 'davis_putnam' && typeof DavisPutnamVisualizer !== 'undefined') {
            return new DavisPutnamVisualizer(this.visualization);
        } else if (algorithm === '3sat_reduction' && typeof ThreeSATReductionVisualizer !== 'undefined') {
            return new ThreeSATReductionVisualizer(this.visualization);
        } else {
            // Default simple visualizer
            return {
                update: (state) => {
                    const div = document.createElement('div');
                    div.className = 'state-display';
                    div.textContent = JSON.stringify(state.data, null, 2);
                    this.visualization.appendChild(div);
                }
            };
        }
    }

    handleCompletion(message) {
        // Handle different result types
        if (message.result === 'REDUCTION_COMPLETE') {
            this.logConsole('Reduction completed successfully', 'success');
            this.vizTitle.textContent = 'Visualization - Reduction Complete';
            // Show the reduced formula in solution panel
            if (message.solution && message.solution.reduced_formula) {
                this.displaySolution(message.result, message.solution);
                this.toggleSolutionBtn.style.display = 'inline-block';
            }
        } else {
            // Standard SAT/UNSAT result
            this.logConsole(
                `Solver completed: ${message.result}`,
                message.result === 'SAT' ? 'success' : 'warning'
            );

            if (message.result === 'SAT' && message.solution) {
                this.logConsole('Solution: ' + JSON.stringify(message.solution), 'success');
                this.displaySolution(message.result, message.solution);
            } else {
                this.displaySolution(message.result, null);
            }

            this.vizTitle.textContent = `Visualization - ${message.result}`;
            // Show solution button for SAT/UNSAT results
            this.toggleSolutionBtn.style.display = 'inline-block';
        }

        // Display statistics
        if (message.stats) {
            this.displayStatistics(message.stats);
        }
    }

    displaySolution(result, solution) {
        this.solutionContent.innerHTML = '';

        if (result === 'REDUCTION_COMPLETE' && solution && solution.reduced_formula) {
            const headerDiv = document.createElement('div');
            headerDiv.className = 'solution-item';
            headerDiv.innerHTML = '<strong>Reduced 3-SAT Formula</strong><br>Copy the formula below:';
            this.solutionContent.appendChild(headerDiv);

            // Display reduced formula in a copyable text area
            const formulaContainer = document.createElement('div');
            formulaContainer.className = 'solution-item';
            formulaContainer.style.position = 'relative';

            const textarea = document.createElement('textarea');
            textarea.className = 'formula-output';
            textarea.value = solution.reduced_formula;
            textarea.readOnly = true;
            textarea.rows = 6;
            textarea.style.width = '100%';
            textarea.style.fontFamily = 'monospace';
            textarea.style.padding = '0.5rem';
            textarea.style.border = '1px solid var(--border-color)';
            textarea.style.borderRadius = '4px';
            textarea.style.background = 'var(--bg-color)';
            textarea.style.color = 'var(--text-color)';
            textarea.style.resize = 'vertical';

            const copyBtn = document.createElement('button');
            copyBtn.className = 'btn btn-sm';
            copyBtn.textContent = '📋 Copy';
            copyBtn.style.marginTop = '0.5rem';
            copyBtn.onclick = () => {
                textarea.select();
                document.execCommand('copy');
                copyBtn.textContent = '✓ Copied!';
                setTimeout(() => { copyBtn.textContent = '📋 Copy'; }, 2000);
            };

            formulaContainer.appendChild(textarea);
            formulaContainer.appendChild(copyBtn);
            this.solutionContent.appendChild(formulaContainer);
            return;
        }

        if (result === 'UNSAT') {
            const unsatDiv = document.createElement('div');
            unsatDiv.className = 'solution-item unsat';
            unsatDiv.innerHTML = '<strong>UNSATISFIABLE</strong><br>No solution exists for this formula.';
            this.solutionContent.appendChild(unsatDiv);
            return;
        }

        if (result === 'SAT' && solution) {
            const headerDiv = document.createElement('div');
            headerDiv.className = 'solution-item';
            headerDiv.innerHTML = '<strong>SATISFIABLE</strong><br>Variable assignments:';
            this.solutionContent.appendChild(headerDiv);

            // Display each variable assignment
            const sortedVars = Object.keys(solution).sort();
            sortedVars.forEach(variable => {
                const value = solution[variable];
                const varDiv = document.createElement('div');
                varDiv.className = 'solution-item';
                varDiv.innerHTML = `<code>${variable} = ${value}</code>`;
                this.solutionContent.appendChild(varDiv);
            });
        }
    }

    displayStatistics(stats) {
        this.statistics.innerHTML = '';

        for (const [key, value] of Object.entries(stats)) {
            const statItem = document.createElement('div');
            statItem.className = 'stat-item';

            const label = document.createElement('div');
            label.className = 'stat-label';
            label.textContent = key.replace(/_/g, ' ').toUpperCase();

            const statValue = document.createElement('div');
            statValue.className = 'stat-value';
            statValue.textContent = value;

            statItem.appendChild(label);
            statItem.appendChild(statValue);
            this.statistics.appendChild(statItem);
        }
    }

    updateProgress() {
        const total = this.stateHistory.length;
        const current = this.currentStep;
        const percent = total > 0 ? (current / total) * 100 : 0;

        this.progressFill.style.width = `${percent}%`;
        this.progressText.textContent = `Step ${current} / ${total}`;
    }

    clearVisualization() {
        this.visualization.innerHTML = '';
        this.currentVisualizer = null;
        this.vizTitle.textContent = 'Visualization';
        this.toggleSolutionBtn.style.display = 'none';
        this.closeSolution();
    }

    logConsole(message, type = 'info') {
        const line = document.createElement('div');
        line.className = `console-line ${type}`;

        const timestamp = document.createElement('span');
        timestamp.className = 'console-timestamp';
        timestamp.textContent = new Date().toLocaleTimeString();

        const text = document.createElement('span');
        text.textContent = message;

        line.appendChild(timestamp);
        line.appendChild(text);

        this.consoleOutput.appendChild(line);
        this.consoleOutput.scrollTop = this.consoleOutput.scrollHeight;
    }

    // Control methods
    pauseSolving() {
        // Set paused flag - websocket stays open but states are buffered
        this.paused = true;
        this.stepMode = false;  // No longer in initial step mode
        this.logConsole('Paused - use Step to continue reviewing', 'info');
        this.pauseBtn.style.display = 'none';
        this.solveBtn.style.display = 'inline-block';
        this.solveBtn.disabled = false;
    }

    async stepForward() {
        // If no session exists, start one in step mode
        if (this.stateHistory.length === 0 && !this.websocket) {
            this.stepMode = true;  // Set before starting
            this.paused = true;    // Start paused
            await this.startSolving();
            // States will buffer, we'll show the first one below
            return;
        }

        // If we have buffered states ahead of current position, show the next one
        if (this.currentStep < this.stateHistory.length) {
            this.currentStep++;
            this.updateProgress();
            this.renderState(this.stateHistory[this.currentStep - 1]);
            this.logConsole(`Showing step ${this.currentStep}`, 'info');
        } else {
            // We're caught up - wait for next state if websocket is still open
            if (this.websocket && this.websocket.readyState === WebSocket.OPEN) {
                this.logConsole('Waiting for next step...', 'info');
                // The next state will be buffered and we can step to it
            } else {
                this.logConsole('No more steps available', 'info');
            }
        }
    }

    reset() {
        // Close websocket if still open
        if (this.websocket) {
            this.websocket.close();
            this.websocket = null;  // Clear the reference
        }

        this.currentStep = 0;
        this.stepMode = false;
        this.paused = false;
        this.currentSessionId = null;
        this.updateProgress();
        this.clearVisualization();
        this.resetBtn.style.display = 'none';
        this.pauseBtn.style.display = 'none';
        this.solveBtn.style.display = 'inline-block';
        this.solveBtn.disabled = false;
        this.solveBtn.textContent = 'Solve';
        this.stateHistory = [];
        this.controlsSection.style.display = 'none';
        this.statsSection.style.display = 'none';
    }
}

// Initialize app when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.app = new SATVisualizerApp();
});
