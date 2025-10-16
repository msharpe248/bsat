"""FastAPI main application for SAT solver visualization."""

import sys
import os
from pathlib import Path
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.staticfiles import StaticFiles
from fastapi.responses import HTMLResponse, FileResponse
from fastapi.middleware.cors import CORSMiddleware

# Add the src directory to the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat import CNFExpression

from .models import (
    FormulaRequest,
    FormulaResponse,
    SolveRequest,
    SolveResponse,
    ExampleFormula,
    GenerateFormulaRequest,
    GenerateFormulaResponse
)
from .session_manager import session_manager
from .solver_wrappers import create_solver_wrapper


app = FastAPI(title="SAT Solver Visualization Server")

# Enable CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Mount static files
frontend_dir = Path(__file__).parent.parent / "frontend"
app.mount("/static", StaticFiles(directory=str(frontend_dir / "static")), name="static")


@app.get("/")
async def root():
    """Serve the main HTML page."""
    html_file = frontend_dir / "index.html"
    if html_file.exists():
        return FileResponse(html_file)
    return {"message": "SAT Solver Visualization Server"}


@app.post("/api/formula/validate", response_model=FormulaResponse)
async def validate_formula(request: FormulaRequest):
    """Validate and parse a CNF formula."""
    try:
        if request.format == "text":
            cnf = CNFExpression.parse(request.formula)
        else:
            return FormulaResponse(
                valid=False,
                error=f"Format '{request.format}' not yet supported"
            )

        return FormulaResponse(
            valid=True,
            variables=sorted(cnf.get_variables()),
            num_clauses=len(cnf.clauses),
            parsed_formula=str(cnf)
        )
    except Exception as e:
        return FormulaResponse(
            valid=False,
            error=str(e)
        )


@app.post("/api/formula/generate", response_model=GenerateFormulaResponse)
async def generate_formula(request: GenerateFormulaRequest):
    """Generate a random CNF formula."""
    import random

    sat_type = request.sat_type
    num_clauses = request.num_clauses

    # Determine clause size based on SAT type
    if request.clause_size > 0:
        clause_size = request.clause_size
    else:
        clause_size_map = {
            "2sat": 2,
            "3sat": 3,
            "4sat": 4,
            "5sat": 5,
            "hornsat": 3,  # Horn clauses can vary, default to 3
            "xorsat": 3    # XOR clauses default to 3 literals
        }
        clause_size = clause_size_map.get(sat_type, 3)

    # Auto-determine number of variables if not specified
    if request.num_variables == 0:
        # Use reasonable ratios for satisfiable formulas
        ratio_map = {
            "2sat": 2,      # ratio ~1
            "3sat": 3,      # ratio ~2
            "4sat": 3,      # ratio ~1.5
            "5sat": 3,      # ratio ~1.5
            "hornsat": 3,   # Horn-SAT is polynomial, can handle higher ratios
            "xorsat": 2     # XOR-SAT is polynomial
        }
        divisor = ratio_map.get(sat_type, 3)
        num_variables = max(3, num_clauses // divisor)
    else:
        num_variables = request.num_variables

    # Generate variable names
    if num_variables <= 26:
        # Use single letters: a, b, c, ...
        variables = [chr(ord('a') + i) for i in range(num_variables)]
    else:
        # Use x1, x2, x3, ...
        variables = [f'x{i+1}' for i in range(num_variables)]

    # Generate clauses based on type
    clauses = []

    if sat_type == "hornsat":
        # Horn clauses: at most one positive literal per clause
        for _ in range(num_clauses):
            # Randomly select variables for this clause
            num_lits = random.randint(1, min(clause_size, num_variables))
            clause_vars = random.sample(variables, num_lits)

            # Decide if this clause has a positive literal (head)
            has_positive = random.random() < 0.6  # 60% chance of having head

            literals = []
            if has_positive and len(clause_vars) > 0:
                # Make one literal positive (the head), rest negative (body)
                positive_idx = random.randint(0, len(clause_vars) - 1)
                for i, var in enumerate(clause_vars):
                    if i == positive_idx:
                        literals.append(var)  # Positive
                    else:
                        literals.append(f"~{var}")  # Negative
            else:
                # All negative (constraint clause)
                literals = [f"~{var}" for var in clause_vars]

            clause_str = "(" + " | ".join(literals) + ")" if len(literals) > 1 else literals[0]
            clauses.append(clause_str)

    elif sat_type == "xorsat":
        # XOR clauses: encode as CNF
        # For XOR of k variables, we need specific CNF encoding
        # Simpler approach: generate clauses where even/odd parity must hold
        for _ in range(num_clauses):
            num_lits = random.randint(2, min(clause_size, num_variables))
            clause_vars = random.sample(variables, num_lits)

            # For XOR-SAT, we'll use a simplified encoding
            # Just generate normal clauses but mark them for XOR solver
            # The actual XOR constraint: x1 ⊕ x2 ⊕ ... ⊕ xn = 1
            # Can be encoded as: (x1 | x2) & (~x1 | ~x2) for 2 vars

            # For simplicity in visualization, generate regular CNF
            # that the XOR solver can work with
            literals = []
            for var in clause_vars:
                if random.random() < 0.5:
                    literals.append(f"~{var}")
                else:
                    literals.append(var)

            clause_str = "(" + " | ".join(literals) + ")"
            clauses.append(clause_str)

    else:
        # Regular k-SAT (2-SAT, 3-SAT, 4-SAT, 5-SAT, etc.)
        for _ in range(num_clauses):
            # Randomly select variables for this clause (without replacement)
            num_lits = min(clause_size, num_variables)
            clause_vars = random.sample(variables, num_lits)

            # Randomly negate each literal
            literals = []
            for var in clause_vars:
                if random.random() < 0.5:
                    literals.append(f"~{var}")
                else:
                    literals.append(var)

            # Create clause string
            clause_str = "(" + " | ".join(literals) + ")"
            clauses.append(clause_str)

    # Join clauses with &
    formula = " & ".join(clauses)

    return GenerateFormulaResponse(
        formula=formula,
        num_clauses=len(clauses),
        num_variables=num_variables,
        sat_type=sat_type
    )


@app.post("/api/solve", response_model=SolveResponse)
async def create_solve_session(request: SolveRequest):
    """Create a new solving session."""
    try:
        # Validate formula
        if request.format == "text":
            cnf = CNFExpression.parse(request.formula)
        else:
            raise HTTPException(status_code=400, detail=f"Format '{request.format}' not supported")

        # Create session
        session_id = session_manager.create_session(
            formula=request.formula,
            algorithm=request.algorithm,
            speed=request.speed
        )

        return SolveResponse(
            session_id=session_id,
            message=f"Session created for {request.algorithm} solver"
        )
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))


@app.websocket("/ws/solve/{session_id}")
async def solve_websocket(websocket: WebSocket, session_id: str):
    """WebSocket endpoint for real-time solver visualization."""
    await websocket.accept()

    try:
        # Get session
        session = session_manager.get_session(session_id)
        if not session:
            await websocket.send_json({
                "type": "error",
                "message": "Session not found or expired"
            })
            await websocket.close()
            return

        # Parse formula
        cnf = CNFExpression.parse(session["formula"])

        # Create solver wrapper
        wrapper = create_solver_wrapper(
            algorithm=session["algorithm"],
            cnf=cnf,
            websocket=websocket,
            speed_ms=session["speed"]
        )

        # Solve with visualization
        result = await wrapper.solve()

        # Update session
        session_manager.update_session(
            session_id,
            result=result,
            completed=True
        )

    except WebSocketDisconnect:
        print(f"Client disconnected from session {session_id}")
        session_manager.close_session(session_id)
    except Exception as e:
        print(f"Error in WebSocket: {e}")
        await websocket.send_json({
            "type": "error",
            "message": str(e)
        })
    finally:
        try:
            await websocket.close()
        except:
            pass


@app.get("/api/examples", response_model=list[ExampleFormula])
async def get_examples():
    """Get example formulas."""
    return [
        ExampleFormula(
            name="Simple 2SAT",
            formula="(x | y) & (~x | z) & (~y | ~z)",
            description="A simple satisfiable 2SAT formula",
            algorithm="2sat",
            difficulty="easy"
        ),
        ExampleFormula(
            name="Simple 3SAT",
            formula="(a | b | c) & (~a | b | ~c) & (a | ~b | c)",
            description="A simple 3SAT formula with 3 variables and 3 clauses",
            algorithm="dpll",
            difficulty="easy"
        ),
        ExampleFormula(
            name="3SAT with Unit Propagation",
            formula="(a | b | c) & (a) & (~a | d | e) & (~d | ~e | f)",
            description="Demonstrates unit propagation: 'a' is forced true, which forces (d|e), etc.",
            algorithm="dpll",
            difficulty="easy"
        ),
        ExampleFormula(
            name="3SAT Medium",
            formula="(a | b | c) & (~a | b | d) & (~b | c | e) & (~c | d | ~e) & (a | ~d | e) & (~a | c | ~d)",
            description="Medium complexity 3SAT with 5 variables and 6 clauses",
            algorithm="dpll",
            difficulty="medium"
        ),
        ExampleFormula(
            name="3SAT with Backtracking",
            formula="(a | b | c) & (~a | b | c) & (a | ~b | c) & (~a | ~b | c) & (a | b | ~c) & (~a | b | ~c) & (a | ~b | ~c)",
            description="Requires backtracking - 7 clauses force specific assignments",
            algorithm="dpll",
            difficulty="medium"
        ),
        ExampleFormula(
            name="3SAT Hard",
            formula="(a | b | c) & (a | b | ~c) & (a | ~b | c) & (~a | b | c) & (~a | ~b | d) & (~a | ~c | d) & (~b | ~c | d) & (a | e | f) & (b | e | ~f) & (c | ~e | f) & (d | ~e | ~f)",
            description="Complex 3SAT with 6 variables and 11 clauses - extensive search required",
            algorithm="dpll",
            difficulty="hard"
        ),
        ExampleFormula(
            name="3SAT UNSAT",
            formula="(a | b | c) & (~a | b | c) & (a | ~b | c) & (~a | ~b | c) & (a | b | ~c) & (~a | b | ~c) & (a | ~b | ~c) & (~a | ~b | ~c)",
            description="Unsatisfiable 3SAT - all 8 possible assignments to 3 variables are blocked",
            algorithm="dpll",
            difficulty="medium"
        ),
        ExampleFormula(
            name="UNSAT Example",
            formula="(x | y) & (~x | y) & (x | ~y) & (~x | ~y)",
            description="An unsatisfiable formula forcing both y and ~y",
            algorithm="dpll",
            difficulty="easy"
        ),
        ExampleFormula(
            name="Davis-Putnam Demo",
            formula="(a | b) & (a | c) & (~a | d) & (~a | e)",
            description="Shows clause growth in Davis-Putnam algorithm",
            algorithm="davis_putnam",
            difficulty="medium"
        ),
        ExampleFormula(
            name="Davis-Putnam Exponential Growth",
            formula="(a | b) & (a | c) & (a | d) & (~a | e) & (~a | f) & (~a | g) & (b | e | h) & (c | f | i) & (d | g | j)",
            description="Dramatic clause explosion: 3 positive × 3 negative = 9 new clauses when eliminating 'a'",
            algorithm="davis_putnam",
            difficulty="hard"
        ),
        ExampleFormula(
            name="Implication Chain",
            formula="(x) & (~x | y) & (~y | z) & (~z | w)",
            description="Shows unit propagation cascading through implications",
            algorithm="dpll",
            difficulty="easy"
        ),
        ExampleFormula(
            name="2-Coloring Triangle",
            formula="(v1 | v2) & (~v1 | ~v2) & (v2 | v3) & (~v2 | ~v3) & (v1 | v3) & (~v1 | ~v3)",
            description="Trying to 2-color a triangle graph (UNSAT)",
            algorithm="2sat",
            difficulty="medium"
        ),
        ExampleFormula(
            name="Horn-SAT Simple",
            formula="(a) & (~a | b) & (~b | c) & (~c | d)",
            description="Simple Horn-SAT with chain of implications: a → b → c → d",
            algorithm="hornsat",
            difficulty="easy"
        ),
        ExampleFormula(
            name="Horn-SAT Unit Propagation",
            formula="(~a | ~b | c) & (~c | d) & (~d | ~e | f) & (a) & (b) & (e)",
            description="Horn-SAT demonstrating multiple unit propagations from initial assignments",
            algorithm="hornsat",
            difficulty="easy"
        ),
        ExampleFormula(
            name="Horn-SAT UNSAT",
            formula="(a) & (~a | b) & (~b | c) & (~c) & (~a | ~b | ~c)",
            description="Unsatisfiable Horn formula - contradictory constraints",
            algorithm="hornsat",
            difficulty="medium"
        ),
        ExampleFormula(
            name="CDCL Simple",
            formula="(a | b | c) & (~a | b | ~c) & (a | ~b | c) & (~a | ~b | ~c)",
            description="Simple 3SAT showing CDCL clause learning and backjumping",
            algorithm="cdcl",
            difficulty="easy"
        ),
        ExampleFormula(
            name="CDCL Conflict Learning",
            formula="(a | b | c) & (~a | d) & (~b | d) & (~c | d) & (~d | e) & (~d | f) & (~e | ~f)",
            description="Demonstrates conflict-driven clause learning with multiple conflicts",
            algorithm="cdcl",
            difficulty="medium"
        ),
        ExampleFormula(
            name="CDCL UNSAT",
            formula="(a | b) & (~a | b) & (a | ~b) & (~a | ~b) & (b | c) & (~b | c) & (b | ~c) & (~b | ~c)",
            description="Unsatisfiable formula requiring clause learning to prove UNSAT",
            algorithm="cdcl",
            difficulty="medium"
        ),
    ]


@app.on_event("startup")
async def startup_event():
    """Startup event handler."""
    print("SAT Solver Visualization Server starting...")
    print("Open your browser to http://localhost:8000")


@app.on_event("shutdown")
async def shutdown_event():
    """Shutdown event handler."""
    print("Server shutting down...")


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
