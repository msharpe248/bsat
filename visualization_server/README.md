# SAT Solver Visualization Server

An interactive web-based visualization server for Boolean Satisfiability (SAT) algorithms. Watch SAT solvers work in real-time with beautiful D3.js visualizations.

## Features

- **Real-time Visualization**: Watch algorithms solve step-by-step via WebSocket
- **7 SAT Algorithms**:
  - **DPLL** - Classic backtracking with decision tree visualization
  - **2SAT** - Polynomial-time SCC algorithm with implication graph
  - **Davis-Putnam** - Original 1960 resolution algorithm showing clause growth
  - **HornSAT** - Polynomial-time unit propagation for Horn formulas
  - **CDCL** - Modern conflict-driven clause learning with backjumping
  - **WalkSAT** - Randomized local search with flip visualization
  - **3SAT Reduction** - k-SAT to 3SAT polynomial reduction
- **Interactive Controls**: Solve, Step, Pause, Reset with real-time feedback
- **Rich Examples**: 20+ pre-loaded formulas showcasing different algorithms
- **Clean Separation**: Original BSAT code remains untouched

## Why This Visualizer?

Understanding SAT algorithms is crucial for computer science education and research. This visualizer makes abstract algorithms tangible:

- **Educational**: See exactly how each algorithm makes decisions, propagates constraints, and learns from conflicts
- **Comparative**: Run the same formula on different algorithms to understand their tradeoffs
- **Interactive**: Step through execution at your own pace, pause when interesting things happen
- **Beautiful**: D3.js visualizations that are both informative and aesthetically pleasing
- **Real-time**: Watch algorithms solve problems as they happen, not after-the-fact traces

Perfect for:
- 🎓 **Students** learning computational complexity and automated reasoning
- 👨‍🏫 **Educators** teaching SAT, NP-completeness, and algorithm design
- 🔬 **Researchers** prototyping new SAT techniques
- 💻 **Developers** understanding industrial SAT solver internals

## Installation

The visualization server is located in the `visualization_server/` directory and uses its own dependencies.

### 1. Install Dependencies

From the `bsat` root directory:

```bash
# Make sure you're in the bsat venv
source venv/bin/activate

# Install visualization server dependencies
pip install -r visualization_server/requirements.txt
```

Dependencies include:
- FastAPI (web framework)
- Uvicorn (ASGI server)
- WebSockets (real-time communication)
- Pydantic (data validation)

### 2. Run the Server

```bash
# From the bsat root directory
cd visualization_server

# Run with Python
python -m uvicorn backend.main:app --reload --port 8000

# Or run the backend directly
python backend/main.py
```

### 3. Open in Browser

Navigate to: **http://localhost:8000**

## Usage

### Quick Start

1. **Select an Example**: Choose from the dropdown menu (e.g., "Simple 2SAT")
2. **Choose Algorithm**: Select the appropriate algorithm for your formula
3. **Click "Solve"**: Watch the visualization in real-time!

### Manual Input

1. **Enter a CNF Formula** in the text area:
   ```
   (x | y) & (~x | z) & (y | ~z)
   ```

2. **Validate** (optional): Click "Validate" to check syntax

3. **Select Algorithm**:
   - `DPLL`: Classic backtracking for general SAT (complete, exponential worst-case)
   - `2SAT`: SCC-based solver for 2-CNF formulas (complete, polynomial O(n+m))
   - `Davis-Putnam`: Original resolution algorithm from 1960 (complete, shows clause explosion)
   - `HornSAT`: Unit propagation for Horn formulas (complete, polynomial O(n+m))
   - `CDCL`: Modern clause learning with VSIDS and backjumping (complete, state-of-the-art)
   - `WalkSAT`: Randomized local search (incomplete, fast for SAT instances)
   - `3SAT Reduction`: Polynomial reduction from k-SAT to 3-SAT

4. **Adjust Speed**: Use the slider to control visualization speed (100ms - 2s per step)

5. **Click "Solve"** or **"Step"**: Start solving automatically or step-by-step

### Playback Controls

- **Solve**: Run the algorithm automatically with delays between steps
- **Pause**: Pause auto-solving (states continue buffering in background)
- **Step**: Execute one step at a time (starts solving if not already started)
- **Reset**: Clear visualization and return to initial state

**Tip**: Use Step mode to carefully examine each decision, propagation, or conflict!

## Visualizations

### DPLL - Search Tree
Interactive decision tree showing:
- **Decision nodes** (blue): Variable assignments chosen by heuristic
- **Unit propagation nodes** (green): Forced assignments from unit clauses
- **Conflict nodes** (red): Dead ends requiring backtracking
- **Backtrack edges** (dashed with scissors ✂): Failed paths
- **Step history**: Detailed log with clause highlighting and current assignments

### 2SAT - Implication Graph
Graph-based visualization with:
- **Directed graph**: Nodes are literals, edges are implications
- **Color-coded SCCs**: Each strongly connected component in unique color
- **Conflict detection** (red): When x and ¬x in same SCC
- **Interactive layout**: Drag nodes, force-directed positioning
- **Step-by-step SCC construction**: Shows Tarjan's algorithm in action

### Davis-Putnam - Resolution Visualization
Resolution-based algorithm showing:
- **Clause growth chart**: Live graph of clause count over time
- **Variable elimination**: Shows resolution on each variable
- **Clause explosion**: Visualizes exponential blowup (e.g., 3×3=9 clauses)
- **Before/after states**: Displays clauses before and after resolution
- **Statistics**: Initial, max, and growth factor metrics

### HornSAT - Unit Propagation
Polynomial-time solver visualization:
- **Assignment trail**: Variables colored by truth value (green=True, gray=False)
- **Monotonic assignment**: All variables start False, only increase to True
- **Unit propagation steps**: Shows which clause forces which assignment
- **Clause display**: Live-updating with highlighted satisfied literals
- **Linear time**: Demonstrates O(n+m) efficiency

### CDCL - Conflict Learning
Modern industrial-strength algorithm:
- **Assignment trail by decision level**: Variables grouped by decision level (L0, L1, L2...)
- **VSIDS heuristic**: Shows variable scores guiding decision making
- **Learned clauses panel**: Recent learned clauses with backtrack levels
- **Conflict analysis**: Shows 1UIP clause learning in action
- **Backjumping**: Non-chronological backtracking (e.g., L5→L2)
- **Restarts**: Luby sequence restarts while keeping learned clauses

### WalkSAT - Local Search
Randomized incomplete algorithm:
- **Progress chart**: Real-time graph of unsatisfied clauses over time
- **Current assignment**: Live variable assignments with color coding
- **Flip visualization**: Shows random (🎲) vs greedy (🎯) flips
- **Break counts**: Displays how many clauses would break for each variable
- **Convergence tracking**: Watch formula converge toward solution
- **Incomplete nature**: May not find solution even if one exists

### 3SAT Reduction - Polynomial Transform
Visualization of Cook-Levin transformation:
- **Original clauses**: Shows input k-SAT formula
- **Auxiliary variables**: Introduces _aux variables for large clauses
- **Splitting process**: Animates transformation of k-clauses into 3-clauses
- **Chain construction**: Shows (l₁ ∨ l₂ ∨ x₁), (¬x₁ ∨ l₃ ∨ x₂), ..., (¬xₙ ∨ lₖ₋₁ ∨ lₖ)
- **Reduction statistics**: Original vs reduced clause/variable counts

## Architecture

```
visualization_server/
├── backend/
│   ├── main.py              # FastAPI application & API endpoints
│   ├── solver_wrappers.py   # Instrumented solver wrappers (DPLL, 2SAT, etc.)
│   ├── models.py            # Pydantic request/response models
│   └── session_manager.py   # WebSocket session state management
├── frontend/
│   ├── index.html           # Main UI with controls
│   └── static/
│       ├── css/
│       │   └── style.css    # Styling and themes
│       └── js/
│           ├── app.js                    # Main application logic
│           ├── visualizers/              # Algorithm-specific visualizers
│           │   ├── dpll.js              # DPLL search tree (D3.js)
│           │   ├── twosat.js            # 2SAT implication graph (D3.js)
│           │   ├── davis_putnam.js      # Davis-Putnam clause growth
│           │   ├── hornsat.js           # HornSAT unit propagation
│           │   ├── cdcl.js              # CDCL conflict learning
│           │   ├── walksat.js           # WalkSAT local search
│           │   └── threesat_reduction.js # 3SAT reduction visualization
│           └── components/              # Reusable UI components
│               ├── formula_input.js     # Formula editor
│               └── controls.js          # Playback controls
└── requirements.txt         # Python dependencies (FastAPI, uvicorn, etc.)
```

## How It Works

### Backend

1. **Solver Wrappers**: Wrap existing BSAT solvers without modifying them
2. **State Capture**: Intercept key methods to capture execution state
3. **WebSocket Streaming**: Send state updates in real-time to frontend
4. **Session Management**: Track multiple concurrent solving sessions

### Frontend

1. **User Input**: Formula editor with validation
2. **WebSocket Client**: Connects to backend solver
3. **State Reception**: Receives state updates as solver runs
4. **D3.js Rendering**: Visualizes algorithm execution
5. **Playback Control**: Step through execution history

## API Endpoints

### REST API

- `POST /api/formula/validate` - Validate CNF formula
- `POST /api/solve` - Create new solving session
- `GET /api/examples` - Get example formulas

### WebSocket

- `WS /ws/solve/{session_id}` - Real-time solver updates

## Example Formulas

The visualizer includes 20+ pre-loaded examples organized by algorithm:

### DPLL Examples
- **Simple 3SAT**: Basic satisfiable formula
- **3SAT with Unit Propagation**: Shows forced assignments
- **3SAT with Backtracking**: Requires exploring multiple paths
- **3SAT UNSAT**: All assignments blocked
- **Implication Chain**: Cascading unit propagations

### 2SAT Examples
- **Simple 2SAT**: Basic implication graph
- **2-Coloring Triangle**: UNSAT graph coloring problem

### Davis-Putnam Examples
- **Davis-Putnam Demo**: Moderate clause growth
- **Exponential Growth**: 3×3=9 clause explosion

### HornSAT Examples
- **Horn-SAT Simple**: Implication chain a→b→c→d
- **Horn-SAT Unit Propagation**: Multiple forced assignments
- **Horn-SAT UNSAT**: Contradictory constraints

### CDCL Examples
- **CDCL Simple**: Basic clause learning
- **CDCL Conflict Learning**: Multiple conflicts and learned clauses
- **CDCL UNSAT**: Requires learning to prove unsatisfiability

### WalkSAT Examples
- **WalkSAT Simple**: Basic local search
- **WalkSAT Random Walk**: Shows random vs greedy flips
- **WalkSAT Local Minimum**: Demonstrates noise parameter helping escape

All examples can be loaded from the dropdown menu and modified in the formula editor.

## Development

### Adding a New Visualizer

1. Create visualizer class in `frontend/static/js/visualizers/`
2. Implement `update(state)` method
3. Add to `createVisualizer()` in `app.js`
4. Create corresponding solver wrapper in `backend/solver_wrappers.py`

### Extending the Backend

1. Add new solver wrapper in `solver_wrappers.py`
2. Emit state updates with `await self.emit_state(action, data)`
3. Register in `create_solver_wrapper()` factory function

## Troubleshooting

**Server won't start:**
- Ensure dependencies are installed: `pip install -r requirements.txt`
- Check port 8000 is not in use
- Activate the venv: `source venv/bin/activate`

**Visualizations not showing:**
- Check browser console for JavaScript errors
- Ensure D3.js CDN is accessible
- Try refreshing the page

**WebSocket connection fails:**
- Check server logs for errors
- Verify formula is valid
- Try a different browser

## Performance

- Server handles multiple concurrent sessions
- Each session has independent WebSocket connection
- State history stored in memory (cleared after 1 hour)
- Recommended: < 100 steps per visualization for smooth performance

## Future Enhancements

- [ ] Export visualization as PNG/SVG
- [ ] Shareable links (formula + algorithm encoded in URL)
- [ ] Comparison mode (run multiple algorithms side-by-side)
- [ ] Interactive explanations (hover tooltips explaining each step)
- [ ] Jupyter notebook integration
- [ ] Formula generator with difficulty levels
- [ ] Benchmark suite with performance comparison
- [ ] DIMACS CNF format import/export

## Credits

Built with:
- [FastAPI](https://fastapi.tiangolo.com/) - Modern Python web framework
- [D3.js](https://d3js.org/) - Data visualization library
- [BSAT](../README.md) - Boolean satisfiability package

## License

Same as BSAT package (MIT License)
