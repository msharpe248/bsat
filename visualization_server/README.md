# SAT Solver Visualization Server

An interactive web-based visualization server for Boolean Satisfiability (SAT) algorithms. Watch SAT solvers work in real-time with beautiful D3.js visualizations.

## Features

- **Real-time Visualization**: Watch algorithms solve step-by-step via WebSocket
- **Multiple Algorithms**:
  - DPLL (Decision tree visualization)
  - 2SAT (Implication graph with SCC visualization)
  - Davis-Putnam (Clause growth chart)
  - CDCL (Conflict-driven clause learning)
  - Horn-SAT
  - WalkSAT
- **Interactive Controls**: Play, pause, step forward/back through execution
- **Example Formulas**: Pre-loaded examples for each algorithm
- **Clean Separation**: Original BSAT code remains untouched

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
   - `DPLL`: For general SAT/3SAT problems
   - `2SAT`: For formulas with exactly 2 literals per clause
   - `Davis-Putnam`: Educational demo of the original 1960 algorithm
   - `CDCL`: Modern industrial-strength algorithm
   - `Horn-SAT`: For Horn formulas (≤1 positive literal per clause)
   - `WalkSAT`: Randomized local search

4. **Adjust Speed**: Use the slider to control visualization speed (100ms - 2s per step)

5. **Click "Solve"**: The solver runs and visualizes each step

### Playback Controls

Once solving starts:
- **⏮ Reset**: Go back to the beginning
- **◀ Step Back**: Go to previous step
- **▶/⏸ Play/Pause**: Auto-play through steps
- **▶▶ Step Forward**: Advance one step
- **⏭ Go to End**: Jump to final result

## Visualizations

### DPLL Search Tree
Shows the decision tree with:
- **Blue nodes**: Decision points
- **Green nodes**: Unit propagations
- **Red nodes**: Conflicts
- **Dashed lines**: Backtracking

### 2SAT Implication Graph
Shows:
- **Directed graph**: Literals and implications
- **Color-coded SCCs**: Each strongly connected component in different color
- **Red nodes**: Conflicts (variable and negation in same SCC)
- **Interactive**: Drag nodes to rearrange

### Davis-Putnam Clause Growth
Shows:
- **Line chart**: Clause count over time
- **Growth statistics**: Initial, max, and growth factor
- **Variable elimination**: X-axis shows which variables were eliminated

## Architecture

```
visualization_server/
├── backend/
│   ├── main.py              # FastAPI application
│   ├── solver_wrappers.py   # Instrumented solvers
│   ├── models.py            # Pydantic models
│   └── session_manager.py   # Session state management
├── frontend/
│   ├── index.html           # Main UI
│   └── static/
│       ├── css/
│       │   └── style.css    # Styling
│       └── js/
│           ├── app.js              # Main application
│           ├── visualizers/        # D3.js visualizers
│           │   ├── dpll.js
│           │   ├── twosat.js
│           │   └── davis_putnam.js
│           └── components/         # Reusable components
│               ├── formula_input.js
│               └── controls.js
└── requirements.txt         # Python dependencies
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

| Name | Formula | Algorithm | Description |
|------|---------|-----------|-------------|
| Simple 2SAT | `(x \| y) & (~x \| z) & (~y \| ~z)` | 2SAT | Basic satisfiable 2SAT |
| Simple 3SAT | `(a \| b \| c) & (~a \| b \| ~c) & (a \| ~b \| c)` | DPLL | Simple 3SAT formula |
| UNSAT Example | `(x \| y) & (~x \| y) & (x \| ~y) & (~x \| ~y)` | DPLL | Forces contradiction |
| Davis-Putnam Demo | `(a \| b) & (a \| c) & (~a \| d) & (~a \| e)` | Davis-Putnam | Shows clause growth |
| Implication Chain | `(x) & (~x \| y) & (~y \| z) & (~z \| w)` | DPLL | Unit propagation cascade |

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

- [ ] CDCL implication graph visualization
- [ ] WalkSAT flip animation
- [ ] Export visualization as PNG/SVG
- [ ] Shareable links (formula + algorithm encoded in URL)
- [ ] Comparison mode (side-by-side algorithms)
- [ ] Step-by-step explanations
- [ ] Jupyter notebook integration

## Credits

Built with:
- [FastAPI](https://fastapi.tiangolo.com/) - Modern Python web framework
- [D3.js](https://d3js.org/) - Data visualization library
- [BSAT](../README.md) - Boolean satisfiability package

## License

Same as BSAT package (MIT License)
