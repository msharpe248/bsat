"""Solver wrappers that emit state updates for visualization."""

import sys
import os
import asyncio
import time
from typing import Dict, Any, Optional, Callable
from fastapi import WebSocket

# Add the src directory to the path so we can import bsat
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat import (
    CNFExpression,
    DPLLSolver,
    CDCLSolver,
    TwoSATSolver,
    DavisPutnamSolver,
    WalkSATSolver,
    HornSATSolver,
    solve_2sat,
    solve_horn_sat
)


class BaseSolverWrapper:
    """Base class for solver wrappers."""

    def __init__(self, cnf: CNFExpression, websocket: Optional[WebSocket] = None, speed_ms: int = 500):
        self.cnf = cnf
        self.websocket = websocket
        self.speed_ms = speed_ms
        self.step_count = 0
        self.state_history = []

    async def emit_state(self, action: str, data: Dict[str, Any]):
        """Emit a state update via WebSocket."""
        state = {
            "type": "state_update",
            "step": self.step_count,
            "action": action,
            "data": data,
            "timestamp": time.time()
        }

        self.state_history.append(state)
        self.step_count += 1

        if self.websocket:
            try:
                await self.websocket.send_json(state)
                await asyncio.sleep(self.speed_ms / 1000.0)  # Delay for visualization
            except Exception as e:
                print(f"Error sending state: {e}")

    async def emit_complete(self, result: Optional[Dict[str, bool]], stats: Dict[str, Any] = None):
        """Emit completion message."""
        complete_msg = {
            "type": "complete",
            "result": "SAT" if result else "UNSAT",
            "solution": result if result else {},
            "stats": stats if stats else {},
            "total_steps": self.step_count
        }

        if self.websocket:
            try:
                await self.websocket.send_json(complete_msg)
            except Exception as e:
                print(f"Error sending completion: {e}")


class DPLLWrapper(BaseSolverWrapper):
    """DPLL solver wrapper with visualization."""

    async def solve(self) -> Optional[Dict[str, bool]]:
        """Solve with step-by-step visualization."""
        await self.emit_state("start", {
            "variables": sorted(self.cnf.get_variables()),
            "clauses": [str(c) for c in self.cnf.clauses],
            "num_variables": len(self.cnf.get_variables()),
            "num_clauses": len(self.cnf.clauses)
        })

        # Create custom DPLL solver that we'll instrument
        solver = DPLLSolver(self.cnf)

        # Monkey-patch the solver to emit states
        original_dpll = solver._dpll
        tree_nodes = []

        async def instrumented_dpll(assignment, clauses, depth=0):
            # Emit current state
            await self.emit_state("decision_point", {
                "depth": depth,
                "assignment": assignment.copy(),
                "num_clauses": len(clauses),
                "num_unassigned": len([v for v in solver.variables if v not in assignment])
            })

            # Simplify clauses based on current assignment
            simplified_clauses = solver._simplify_clauses(clauses, assignment)

            # Check for empty clause (conflict)
            for clause in simplified_clauses:
                if len(clause.literals) == 0:
                    await self.emit_state("conflict", {
                        "depth": depth,
                        "assignment": assignment.copy(),
                        "message": "Empty clause found - backtracking"
                    })
                    return None

            # Check if all clauses satisfied
            if not simplified_clauses:
                await self.emit_state("solution_found", {
                    "depth": depth,
                    "assignment": assignment.copy()
                })
                return assignment

            # Unit propagation - pass both clauses and assignment
            unit_clause = solver._find_unit_clause(simplified_clauses, assignment)
            if unit_clause:
                unit_literal = unit_clause.literals[0]
                var = unit_literal.variable
                value = not unit_literal.negated

                await self.emit_state("unit_propagation", {
                    "variable": var,
                    "value": value,
                    "clause": str(unit_clause),
                    "depth": depth
                })

                assignment = assignment.copy()
                assignment[var] = value
                return await instrumented_dpll(assignment, clauses, depth)

            # Pure literal elimination - pass both clauses and assignment
            pure_literal = solver._find_pure_literal(simplified_clauses, assignment)
            if pure_literal:
                var = pure_literal.variable
                value = not pure_literal.negated

                await self.emit_state("pure_literal", {
                    "variable": var,
                    "value": value,
                    "depth": depth
                })

                assignment = assignment.copy()
                assignment[var] = value
                return await instrumented_dpll(assignment, clauses, depth)

            # Choose a variable to branch on
            unassigned = [v for v in solver.variables if v not in assignment]
            if not unassigned:
                # All variables assigned
                if solver.cnf.evaluate(assignment):
                    return assignment
                return None

            var = unassigned[0]

            # Try True
            await self.emit_state("branch", {
                "variable": var,
                "value": True,
                "depth": depth + 1,
                "branch": "left"
            })

            new_assignment = assignment.copy()
            new_assignment[var] = True
            result = await instrumented_dpll(new_assignment, simplified_clauses, depth + 1)

            if result is not None:
                return result

            # Backtrack and try False
            await self.emit_state("backtrack", {
                "variable": var,
                "depth": depth,
                "message": f"Trying {var}=False"
            })

            await self.emit_state("branch", {
                "variable": var,
                "value": False,
                "depth": depth + 1,
                "branch": "right"
            })

            new_assignment = assignment.copy()
            new_assignment[var] = False
            result = await instrumented_dpll(new_assignment, simplified_clauses, depth + 1)

            return result

        # Replace the method
        solver._dpll = instrumented_dpll

        # Run the solver
        result = await solver._dpll({}, list(self.cnf.clauses))

        stats = {
            "decisions": solver.num_decisions,
            "unit_propagations": solver.num_unit_propagations,
            "pure_literals": solver.num_pure_literals
        }

        await self.emit_complete(result, stats)
        return result


class TwoSATWrapper(BaseSolverWrapper):
    """2SAT solver wrapper with implication graph visualization."""

    async def solve(self) -> Optional[Dict[str, bool]]:
        """Solve with SCC visualization."""
        await self.emit_state("start", {
            "variables": sorted(self.cnf.get_variables()),
            "clauses": [str(c) for c in self.cnf.clauses],
            "algorithm": "2SAT (SCC-based)"
        })

        # Build implication graph
        solver = TwoSATSolver(self.cnf)

        # Emit implication graph
        graph_data = {
            "nodes": list(solver.implication_graph.keys()),
            "edges": [
                {"from": src, "to": dst}
                for src, dests in solver.implication_graph.items()
                for dst in dests
            ]
        }

        await self.emit_state("implication_graph", {
            "graph": graph_data,
            "num_nodes": len(graph_data["nodes"]),
            "num_edges": len(graph_data["edges"])
        })

        # Find SCCs
        await self.emit_state("finding_sccs", {
            "message": "Running Tarjan's algorithm to find strongly connected components"
        })

        sccs = solver._find_sccs()

        await self.emit_state("sccs_found", {
            "sccs": [list(scc) for scc in sccs],
            "num_sccs": len(sccs)
        })

        # Check for conflicts
        var_to_scc = {}
        for i, scc in enumerate(sccs):
            for lit in scc:
                var, neg = solver._parse_literal_key(lit)
                if var not in var_to_scc:
                    var_to_scc[var] = {}
                var_to_scc[var][neg] = i

        # Find conflicts
        conflicts = []
        for var, scc_map in var_to_scc.items():
            if True in scc_map and False in scc_map:
                if scc_map[True] == scc_map[False]:
                    conflicts.append(var)

        if conflicts:
            await self.emit_state("conflict", {
                "conflicts": conflicts,
                "message": f"Variables {conflicts} have both polarities in same SCC - UNSAT"
            })
            await self.emit_complete(None, {"sccs": len(sccs)})
            return None

        await self.emit_state("no_conflicts", {
            "message": "No conflicts found - formula is SAT"
        })

        # Build solution
        result = solver.solve()

        await self.emit_state("solution_constructed", {
            "solution": result,
            "method": "Reverse topological order of SCCs"
        })

        await self.emit_complete(result, {"sccs": len(sccs)})
        return result


class DavisPutnamWrapper(BaseSolverWrapper):
    """Davis-Putnam solver wrapper with clause growth visualization."""

    async def solve(self) -> Optional[Dict[str, bool]]:
        """Solve with clause growth tracking."""
        await self.emit_state("start", {
            "variables": sorted(self.cnf.get_variables()),
            "initial_clauses": len(self.cnf.clauses),
            "algorithm": "Davis-Putnam (1960)"
        })

        from bsat.davis_putnam import DavisPutnamSolver

        solver = DavisPutnamSolver(self.cnf)
        working_clauses = list(self.cnf.clauses)

        clause_counts = [len(working_clauses)]
        variables_eliminated = []

        async def track_elimination(var):
            """Track variable elimination."""
            # Count clauses with variable
            pos_count = sum(1 for c in working_clauses
                          for lit in c.literals
                          if lit.variable == var and not lit.negated)
            neg_count = sum(1 for c in working_clauses
                          for lit in c.literals
                          if lit.variable == var and lit.negated)

            await self.emit_state("eliminate_variable", {
                "variable": var,
                "positive_clauses": pos_count,
                "negative_clauses": neg_count,
                "expected_new_clauses": pos_count * neg_count,
                "current_clause_count": len(working_clauses)
            })

        # Monkey-patch to track progress
        original_resolve = solver._resolve_variable

        def tracked_resolve(clauses, var):
            asyncio.create_task(track_elimination(var))
            result = original_resolve(clauses, var)
            variables_eliminated.append(var)
            clause_counts.append(len(result))
            return result

        solver._resolve_variable = tracked_resolve

        # Run solver
        result = solver.solve()

        # Emit clause growth chart
        await self.emit_state("clause_growth", {
            "variables_eliminated": variables_eliminated,
            "clause_counts": clause_counts,
            "max_clauses": max(clause_counts),
            "growth_factor": max(clause_counts) / clause_counts[0] if clause_counts[0] > 0 else 0
        })

        stats = solver.get_statistics()
        await self.emit_complete(result, {
            "initial_clauses": stats.initial_clauses,
            "max_clauses": stats.max_clauses,
            "resolutions": stats.resolutions_performed
        })

        return result


# Factory function to create appropriate wrapper
def create_solver_wrapper(
    algorithm: str,
    cnf: CNFExpression,
    websocket: Optional[WebSocket] = None,
    speed_ms: int = 500
) -> BaseSolverWrapper:
    """Create the appropriate solver wrapper for the algorithm."""
    if algorithm == "dpll":
        return DPLLWrapper(cnf, websocket, speed_ms)
    elif algorithm == "2sat":
        return TwoSATWrapper(cnf, websocket, speed_ms)
    elif algorithm == "davis_putnam":
        return DavisPutnamWrapper(cnf, websocket, speed_ms)
    else:
        # For other algorithms, use a simple wrapper
        return BaseSolverWrapper(cnf, websocket, speed_ms)
