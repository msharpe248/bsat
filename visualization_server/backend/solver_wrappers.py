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
from bsat.reductions import reduce_to_3sat


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

    async def emit_complete(self, result: Optional[Dict[str, bool]] = None, stats: Dict[str, Any] = None, result_type: str = None, solution_data: Any = None):
        """Emit completion message."""
        # Handle different result types
        if result_type:
            # Custom result type (e.g., "REDUCTION_COMPLETE")
            complete_msg = {
                "type": "complete",
                "result": result_type,
                "solution": solution_data,
                "stats": stats if stats else {},
                "total_steps": self.step_count
            }
        else:
            # Standard SAT/UNSAT result
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
            # Simplify clauses based on current assignment
            simplified_clauses = solver._simplify_clauses(clauses, assignment)

            # Check for empty clause (conflict)
            for clause in simplified_clauses:
                if len(clause.literals) == 0:
                    await self.emit_state("conflict", {
                        "depth": depth,
                        "assignment": assignment.copy(),
                        "message": "Empty clause found - backtracking",
                        "clauses": [str(c) for c in simplified_clauses]
                    })
                    return None

            # Check if all clauses satisfied
            if not simplified_clauses:
                await self.emit_state("solution_found", {
                    "depth": depth,
                    "assignment": assignment.copy(),
                    "clauses": []  # All clauses satisfied
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
                    "depth": depth,
                    "assignment": assignment.copy(),
                    "clauses": [str(c) for c in simplified_clauses]
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
                "branch": "left",
                "assignment": assignment.copy(),
                "clauses": [str(c) for c in simplified_clauses]
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
                "message": f"Trying {var}=False",
                "assignment": assignment.copy()
            })

            await self.emit_state("branch", {
                "variable": var,
                "value": False,
                "depth": depth + 1,
                "branch": "right",
                "assignment": assignment.copy(),
                "clauses": [str(c) for c in simplified_clauses]
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
            "assignment": result,
            "method": "Reverse topological order of SCCs"
        })

        await self.emit_complete(result, {"sccs": len(sccs)})
        return result


class DavisPutnamWrapper(BaseSolverWrapper):
    """Davis-Putnam solver wrapper with clause growth visualization."""

    async def solve(self) -> Optional[Dict[str, bool]]:
        """Solve with clause growth tracking."""
        from bsat.cnf import Clause, Literal

        await self.emit_state("start", {
            "variables": sorted(self.cnf.get_variables()),
            "initial_clauses": len(self.cnf.clauses),
            "clauses": [str(c) for c in self.cnf.clauses],
            "algorithm": "Davis-Putnam (1960)"
        })

        # We'll implement our own simplified Davis-Putnam that emits states
        working_clauses = [Clause(list(c.literals)) for c in self.cnf.clauses]
        assignment = {}

        clause_counts = [len(working_clauses)]
        variables_eliminated = []

        # Track each step
        while working_clauses:
            # Find unit clause
            unit_lit = None
            for clause in working_clauses:
                if len(clause.literals) == 1:
                    unit_lit = clause.literals[0]
                    break

            if unit_lit:
                # Apply unit propagation
                var = unit_lit.variable
                value = not unit_lit.negated
                assignment[var] = value
                working_clauses = self._apply_assignment(working_clauses, var, value)

                clause_counts.append(len(working_clauses))
                await self.emit_state("eliminate_variable", {
                    "variable": var,
                    "method": "unit_propagation",
                    "current_clause_count": len(working_clauses),
                    "clauses": [str(c) for c in working_clauses],
                    "message": f"Unit propagation: {var} = {value}"
                })

                if any(len(c.literals) == 0 for c in working_clauses):
                    await self.emit_complete(None, {"initial_clauses": clause_counts[0]})
                    return None
                continue

            # Find pure literal
            pure_lit = self._find_pure_literal(working_clauses)
            if pure_lit:
                var = pure_lit.variable
                value = not pure_lit.negated
                assignment[var] = value
                working_clauses = self._eliminate_satisfied(working_clauses, pure_lit)

                clause_counts.append(len(working_clauses))
                await self.emit_state("eliminate_variable", {
                    "variable": var,
                    "method": "pure_literal",
                    "current_clause_count": len(working_clauses),
                    "clauses": [str(c) for c in working_clauses],
                    "message": f"Pure literal: {var} = {value}"
                })
                continue

            # Choose variable for resolution
            var = self._choose_variable(working_clauses)
            if not var:
                break

            # Count clauses before resolution
            pos_clauses = [c for c in working_clauses
                          if any(lit.variable == var and not lit.negated for lit in c.literals)]
            neg_clauses = [c for c in working_clauses
                          if any(lit.variable == var and lit.negated for lit in c.literals)]

            await self.emit_state("eliminate_variable", {
                "variable": var,
                "method": "resolution",
                "positive_clauses": len(pos_clauses),
                "negative_clauses": len(neg_clauses),
                "expected_new_clauses": len(pos_clauses) * len(neg_clauses),
                "current_clause_count": len(working_clauses),
                "clauses_before": [str(c) for c in working_clauses],
                "pos_clause_list": [str(c) for c in pos_clauses],
                "neg_clause_list": [str(c) for c in neg_clauses],
                "message": f"Resolution on {var}: {len(pos_clauses)} × {len(neg_clauses)} = {len(pos_clauses) * len(neg_clauses)} new clauses"
            })

            # Perform resolution
            working_clauses = self._resolve_variable(working_clauses, var)
            variables_eliminated.append(var)
            clause_counts.append(len(working_clauses))

            # Emit after resolution
            await self.emit_state("after_resolution", {
                "variable": var,
                "clauses": [str(c) for c in working_clauses],
                "clause_count": len(working_clauses)
            })

            if any(len(c.literals) == 0 for c in working_clauses):
                await self.emit_complete(None, {
                    "initial_clauses": clause_counts[0],
                    "max_clauses": max(clause_counts)
                })
                return None

        # Complete assignment
        all_vars = sorted(self.cnf.get_variables())
        for var in all_vars:
            if var not in assignment:
                assignment[var] = False

        # Emit final clause growth
        await self.emit_state("clause_growth", {
            "variables_eliminated": variables_eliminated,
            "clause_counts": clause_counts,
            "max_clauses": max(clause_counts) if clause_counts else 0,
            "growth_factor": (max(clause_counts) / clause_counts[0]) if clause_counts and clause_counts[0] > 0 else 0
        })

        await self.emit_complete(assignment, {
            "initial_clauses": clause_counts[0] if clause_counts else 0,
            "max_clauses": max(clause_counts) if clause_counts else 0,
            "resolutions": len(variables_eliminated)
        })

        return assignment

    def _apply_assignment(self, clauses, var, value):
        """Apply assignment to clauses."""
        from bsat.cnf import Clause
        new_clauses = []
        for clause in clauses:
            satisfied = False
            new_lits = []
            for lit in clause.literals:
                if lit.variable == var:
                    lit_value = value if not lit.negated else not value
                    if lit_value:
                        satisfied = True
                        break
                else:
                    new_lits.append(lit)
            if not satisfied:
                new_clauses.append(Clause(new_lits))
        return new_clauses

    def _find_pure_literal(self, clauses):
        """Find pure literal."""
        from bsat.cnf import Literal
        pos_vars = set()
        neg_vars = set()
        for clause in clauses:
            for lit in clause.literals:
                if lit.negated:
                    neg_vars.add(lit.variable)
                else:
                    pos_vars.add(lit.variable)
        pure_pos = pos_vars - neg_vars
        pure_neg = neg_vars - pos_vars
        if pure_pos:
            return Literal(pure_pos.pop(), False)
        if pure_neg:
            return Literal(pure_neg.pop(), True)
        return None

    def _eliminate_satisfied(self, clauses, literal):
        """Remove clauses containing literal."""
        new_clauses = []
        for clause in clauses:
            if not any(lit.variable == literal.variable and lit.negated == literal.negated
                      for lit in clause.literals):
                new_clauses.append(clause)
        return new_clauses

    def _choose_variable(self, clauses):
        """Choose variable for elimination."""
        from collections import defaultdict
        counts = defaultdict(int)
        for clause in clauses:
            for lit in clause.literals:
                counts[lit.variable] += 1
        return min(counts.keys(), key=lambda v: counts[v]) if counts else None

    def _resolve_variable(self, clauses, var):
        """Resolve on variable."""
        from bsat.cnf import Clause
        pos_clauses = []
        neg_clauses = []
        other_clauses = []

        for clause in clauses:
            has_pos = any(lit.variable == var and not lit.negated for lit in clause.literals)
            has_neg = any(lit.variable == var and lit.negated for lit in clause.literals)
            if has_pos:
                pos_clauses.append(clause)
            elif has_neg:
                neg_clauses.append(clause)
            else:
                other_clauses.append(clause)

        resolved = []
        for pos_clause in pos_clauses:
            for neg_clause in neg_clauses:
                # Resolve pair
                new_lits = {}
                for lit in pos_clause.literals + neg_clause.literals:
                    if lit.variable != var:
                        key = (lit.variable, lit.negated)
                        new_lits[key] = lit

                new_clause = Clause(list(new_lits.values()))

                # Check for tautology
                vars_seen = {}
                is_taut = False
                for lit in new_clause.literals:
                    if lit.variable in vars_seen and vars_seen[lit.variable] != lit.negated:
                        is_taut = True
                        break
                    vars_seen[lit.variable] = lit.negated

                if not is_taut:
                    resolved.append(new_clause)

        return resolved + other_clauses


class HornSATWrapper(BaseSolverWrapper):
    """Horn-SAT solver wrapper with unit propagation visualization."""

    async def solve(self) -> Optional[Dict[str, bool]]:
        """Solve with step-by-step unit propagation visualization."""
        await self.emit_state("start", {
            "variables": sorted(self.cnf.get_variables()),
            "clauses": [str(c) for c in self.cnf.clauses],
            "algorithm": "Horn-SAT (Unit Propagation)"
        })

        # Initialize all variables to False
        variables = sorted(self.cnf.get_variables())
        assignment = {var: False for var in variables}

        await self.emit_state("initialize", {
            "assignment": assignment.copy(),
            "message": "All variables initialized to False (Horn-SAT monotonic property)"
        })

        # Keep propagating unit clauses until fixpoint
        iteration = 0
        changed = True
        propagations = []

        while changed:
            changed = False
            iteration += 1

            for clause_idx, clause in enumerate(self.cnf.clauses):
                # Skip if clause is already satisfied
                if clause.evaluate(assignment):
                    continue

                # Find literals that could still make this clause true
                candidate_literals = []

                for literal in clause.literals:
                    var_value = assignment[literal.variable]
                    lit_value = (not var_value) if literal.negated else var_value

                    if not lit_value:
                        # This literal is currently false
                        if not literal.negated and var_value == False:
                            candidate_literals.append(literal)
                    else:
                        # Literal is already true, clause is satisfied
                        candidate_literals = None
                        break

                # If no candidates and clause not satisfied, it's UNSAT
                if candidate_literals is not None and len(candidate_literals) == 0:
                    await self.emit_state("unsat", {
                        "clause": str(clause),
                        "clause_index": clause_idx,
                        "assignment": assignment.copy(),
                        "message": f"All literals in clause '{clause}' are false - UNSAT"
                    })
                    await self.emit_complete(None, {
                        "iterations": iteration,
                        "propagations": len(propagations)
                    })
                    return None

                # Unit clause - exactly one literal can make it true
                if candidate_literals is not None and len(candidate_literals) == 1:
                    literal = candidate_literals[0]
                    if not literal.negated:
                        old_value = assignment[literal.variable]
                        assignment[literal.variable] = True
                        propagations.append({
                            "variable": literal.variable,
                            "clause": str(clause),
                            "iteration": iteration
                        })

                        await self.emit_state("unit_propagation", {
                            "variable": literal.variable,
                            "value": True,
                            "clause": str(clause),
                            "clause_index": clause_idx,
                            "iteration": iteration,
                            "assignment": assignment.copy(),
                            "clauses": [str(c) for c in self.cnf.clauses],
                            "message": f"Clause '{clause}' forces {literal.variable} = True"
                        })

                        changed = True

        # Check if all clauses are satisfied
        if self.cnf.evaluate(assignment):
            await self.emit_state("sat", {
                "assignment": assignment.copy(),
                "iterations": iteration,
                "propagations": propagations,
                "message": "All clauses satisfied - formula is SAT"
            })
            await self.emit_complete(assignment, {
                "iterations": iteration,
                "propagations": len(propagations)
            })
            return assignment

        # If not satisfied, it's UNSAT
        unsatisfied_clauses = [str(c) for c in self.cnf.clauses if not c.evaluate(assignment)]
        await self.emit_state("unsat", {
            "assignment": assignment.copy(),
            "unsatisfied_clauses": unsatisfied_clauses,
            "message": f"No more unit propagations possible, but {len(unsatisfied_clauses)} clauses remain unsatisfied"
        })
        await self.emit_complete(None, {
            "iterations": iteration,
            "propagations": len(propagations)
        })
        return None


class CDCLWrapper(BaseSolverWrapper):
    """CDCL solver wrapper with conflict learning visualization."""

    async def solve(self) -> Optional[Dict[str, bool]]:
        """Solve with CDCL visualization."""
        from bsat.cdcl import CDCLSolver

        await self.emit_state("start", {
            "variables": sorted(self.cnf.get_variables()),
            "clauses": [str(c) for c in self.cnf.clauses],
            "num_original_clauses": len(self.cnf.clauses),
            "algorithm": "CDCL (Conflict-Driven Clause Learning)"
        })

        # Create CDCL solver
        solver = CDCLSolver(self.cnf)

        # Initial propagation
        conflict = solver._propagate()
        if conflict is not None:
            await self.emit_state("unsat", {
                "message": "Conflict at decision level 0 - UNSAT",
                "conflict_clause": str(conflict)
            })
            await self.emit_complete(None, solver.get_stats().__dict__)
            return None

        await self.emit_state("initial_propagation", {
            "assignment": dict(solver.assignment),
            "trail": [str(a) for a in solver.trail],
            "message": "Initial unit propagation complete"
        })

        # Main CDCL loop
        iteration = 0
        max_iterations = 100  # Limit for visualization

        while iteration < max_iterations:
            iteration += 1

            # Pick branching variable
            var = solver._pick_branching_variable()

            if var is None:
                # All variables assigned - SAT!
                await self.emit_state("sat", {
                    "assignment": dict(solver.assignment),
                    "message": "All variables assigned - SAT"
                })
                await self.emit_complete(dict(solver.assignment), solver.get_stats().__dict__)
                return dict(solver.assignment)

            # Make decision
            solver.decision_level += 1
            solver.stats.max_decision_level = max(solver.stats.max_decision_level, solver.decision_level)

            # Show VSIDS scores for context
            top_vars = sorted(solver.vsids_scores.items(), key=lambda x: x[1], reverse=True)[:5]

            await self.emit_state("decision", {
                "variable": var,
                "value": True,
                "decision_level": solver.decision_level,
                "vsids_score": solver.vsids_scores[var],
                "top_vsids": dict(top_vars),
                "assignment": dict(solver.assignment),
                "trail": [str(a) for a in solver.trail]
            })

            solver._assign(var, True)

            # Propagation loop
            while True:
                conflict = solver._propagate()

                if conflict is None:
                    # No conflict - show current state
                    await self.emit_state("propagation_complete", {
                        "decision_level": solver.decision_level,
                        "assignment": dict(solver.assignment),
                        "trail": [str(a) for a in solver.trail],
                        "num_assigned": len(solver.assignment),
                        "num_variables": len(solver.variables)
                    })
                    break

                # Conflict!
                solver.stats.conflicts += 1

                if solver.decision_level == 0:
                    await self.emit_state("unsat", {
                        "message": "Conflict at decision level 0 - UNSAT",
                        "conflict_clause": str(conflict)
                    })
                    await self.emit_complete(None, solver.get_stats().__dict__)
                    return None

                await self.emit_state("conflict", {
                    "conflict_clause": str(conflict),
                    "decision_level": solver.decision_level,
                    "assignment": dict(solver.assignment),
                    "trail": [str(a) for a in solver.trail],
                    "message": f"Conflict found: {conflict}"
                })

                # Analyze conflict and learn clause
                learned_clause, backtrack_level = solver._analyze_conflict(conflict)

                if backtrack_level < 0:
                    await self.emit_state("unsat", {
                        "message": "Conflict analysis shows UNSAT"
                    })
                    await self.emit_complete(None, solver.get_stats().__dict__)
                    return None

                await self.emit_state("learn_clause", {
                    "learned_clause": str(learned_clause),
                    "backtrack_level": backtrack_level,
                    "current_level": solver.decision_level,
                    "num_learned_total": solver.stats.learned_clauses + 1,
                    "message": f"Learned: {learned_clause}, backtrack to level {backtrack_level}"
                })

                # Add learned clause
                solver._add_learned_clause(learned_clause)

                # Backtrack
                old_level = solver.decision_level
                solver._unassign_to_level(backtrack_level)
                solver.decision_level = backtrack_level
                solver.stats.backjumps += 1

                await self.emit_state("backjump", {
                    "from_level": old_level,
                    "to_level": backtrack_level,
                    "assignment": dict(solver.assignment),
                    "trail": [str(a) for a in solver.trail],
                    "message": f"Backjumped from level {old_level} to {backtrack_level}"
                })

                # Decay VSIDS scores
                solver._decay_vsids_scores()

                # Check for restart
                if solver._should_restart():
                    solver._restart()
                    await self.emit_state("restart", {
                        "restart_count": solver.stats.restarts,
                        "total_conflicts": solver.stats.conflicts,
                        "learned_clauses": solver.stats.learned_clauses,
                        "message": f"Restart #{solver.stats.restarts} (keeping {solver.stats.learned_clauses} learned clauses)"
                    })

                    conflict = solver._propagate()
                    if conflict is not None:
                        await self.emit_state("unsat", {
                            "message": "Conflict after restart - UNSAT"
                        })
                        await self.emit_complete(None, solver.get_stats().__dict__)
                        return None
                    break

        # Hit iteration limit
        await self.emit_state("iteration_limit", {
            "message": f"Reached visualization limit of {max_iterations} iterations",
            "stats": solver.get_stats().__dict__
        })

        # Continue solving without visualization
        result = solver.solve(max_conflicts=1000000)
        await self.emit_complete(result, solver.get_stats().__dict__)
        return result


class WalkSATWrapper(BaseSolverWrapper):
    """WalkSAT randomized local search wrapper with visualization."""

    async def solve(self) -> Optional[Dict[str, bool]]:
        """Solve with WalkSAT visualization."""
        import random

        await self.emit_state("start", {
            "variables": sorted(self.cnf.get_variables()),
            "clauses": [str(c) for c in self.cnf.clauses],
            "algorithm": "WalkSAT (Randomized Local Search)"
        })

        variables = sorted(self.cnf.get_variables())
        if not variables:
            await self.emit_complete({}, {"total_flips": 0, "total_tries": 1})
            return {}

        noise = 0.5
        max_flips_per_try = 100
        max_tries = 5

        # For visualization
        total_flips = 0

        # Try multiple random restarts
        for try_num in range(max_tries):
            # Random initial assignment
            assignment = {var: random.choice([True, False]) for var in variables}

            await self.emit_state("restart", {
                "try_number": try_num + 1,
                "assignment": assignment.copy(),
                "message": f"Random restart #{try_num + 1} - new random assignment"
            })

            # Local search
            for flip_num in range(max_flips_per_try):
                total_flips += 1

                # Find unsatisfied clauses
                unsatisfied = [c for c in self.cnf.clauses if not c.evaluate(assignment)]

                # Check if solved
                if not unsatisfied:
                    await self.emit_state("sat", {
                        "assignment": assignment.copy(),
                        "total_flips": total_flips,
                        "try_number": try_num + 1,
                        "message": f"Solution found after {total_flips} flips!"
                    })
                    await self.emit_complete(assignment, {
                        "total_flips": total_flips,
                        "total_tries": try_num + 1,
                        "flips_this_try": flip_num
                    })
                    return assignment

                # Pick random unsatisfied clause
                clause = random.choice(unsatisfied)

                # Compute break counts for all variables in clause
                clause_vars = list(set(lit.variable for lit in clause.literals))
                break_counts = {}

                for var in clause_vars:
                    # Count breaks
                    temp_assignment = assignment.copy()
                    temp_assignment[var] = not temp_assignment[var]
                    breaks = sum(1 for c in self.cnf.clauses
                                if c.evaluate(assignment) and not c.evaluate(temp_assignment))
                    break_counts[var] = breaks

                # Decide whether to make random or greedy move
                is_random_flip = random.random() < noise

                if is_random_flip:
                    # Random walk
                    var = random.choice(clause_vars)
                    flip_type = "random"
                else:
                    # Greedy: minimize breaks
                    var = min(clause_vars, key=lambda v: break_counts[v])
                    flip_type = "greedy"

                # Flip the variable
                old_value = assignment[var]
                assignment[var] = not assignment[var]

                await self.emit_state("flip", {
                    "variable": var,
                    "old_value": old_value,
                    "new_value": assignment[var],
                    "flip_type": flip_type,
                    "flip_number": flip_num + 1,
                    "total_flips": total_flips,
                    "unsatisfied_clause": str(clause),
                    "break_count": break_counts[var],
                    "break_counts": break_counts,
                    "num_unsatisfied": len(unsatisfied),
                    "assignment": assignment.copy(),
                    "clauses": [str(c) for c in self.cnf.clauses],
                    "unsatisfied_clauses": [str(c) for c in unsatisfied]
                })

                # Don't visualize every single flip for large formulas
                if flip_num % 5 == 0 or len(unsatisfied) < 3:
                    # Emit progress update
                    pass  # The flip state above already shows progress

        # No solution found
        await self.emit_state("no_solution", {
            "total_flips": total_flips,
            "total_tries": max_tries,
            "message": f"No solution found after {max_tries} tries and {total_flips} flips (incomplete algorithm)"
        })

        await self.emit_complete(None, {
            "total_flips": total_flips,
            "total_tries": max_tries
        })
        return None


class ThreeSATReductionWrapper(BaseSolverWrapper):
    """Wrapper for k-SAT to 3-SAT reduction visualization."""

    async def solve(self) -> str:
        """Perform and visualize the 3-SAT reduction."""
        await self.emit_state("start", {
            "original_formula": str(self.cnf),
            "num_clauses": len(self.cnf.clauses),
            "num_variables": len(self.cnf.get_variables()),
            "max_clause_size": max((len(c.literals) for c in self.cnf.clauses), default=0)
        })

        # Track auxiliary variable counter
        aux_counter = 0
        reduced_clauses = []
        total_aux_vars = 0

        for clause_idx, clause in enumerate(self.cnf.clauses):
            clause_size = len(clause.literals)

            if clause_size <= 3:
                # Small clause - keep as-is
                await self.emit_state("keep_clause", {
                    "clause_index": clause_idx,
                    "original_clause": str(clause),
                    "clause_size": clause_size,
                    "reason": "Already 3-SAT (≤3 literals)"
                })
                reduced_clauses.append(clause)
            else:
                # Large clause - needs splitting
                literals = clause.literals
                num_aux = clause_size - 3
                aux_vars = [f"_aux{aux_counter + i}" for i in range(num_aux)]
                aux_counter += num_aux
                total_aux_vars += num_aux

                await self.emit_state("split_clause_start", {
                    "clause_index": clause_idx,
                    "original_clause": str(clause),
                    "clause_size": clause_size,
                    "num_aux_needed": num_aux,
                    "aux_variables": aux_vars
                })

                # Build the chain of 3-SAT clauses
                new_clauses = []

                # First clause: (l₁ ∨ l₂ ∨ x₁)
                first_clause = f"({literals[0]} | {literals[1]} | {aux_vars[0]})"
                new_clauses.append(first_clause)
                reduced_clauses.append(first_clause)

                await self.emit_state("add_first_clause", {
                    "clause_index": clause_idx,
                    "new_clause": first_clause,
                    "literals": [str(literals[0]), str(literals[1])],
                    "aux_var": aux_vars[0],
                    "explanation": f"First 2 literals + auxiliary variable {aux_vars[0]}"
                })

                # Middle clauses: (¬xᵢ ∨ lᵢ₊₂ ∨ xᵢ₊₁)
                for i in range(num_aux - 1):
                    middle_clause = f"(~{aux_vars[i]} | {literals[i + 2]} | {aux_vars[i + 1]})"
                    new_clauses.append(middle_clause)
                    reduced_clauses.append(middle_clause)

                    await self.emit_state("add_middle_clause", {
                        "clause_index": clause_idx,
                        "new_clause": middle_clause,
                        "prev_aux": aux_vars[i],
                        "literal": str(literals[i + 2]),
                        "next_aux": aux_vars[i + 1],
                        "chain_position": i + 1,
                        "explanation": f"Chain: ~{aux_vars[i]} (prev) + literal {i+3} + {aux_vars[i+1]} (next)"
                    })

                # Last clause: (¬xₖ₋₃ ∨ lₖ₋₁ ∨ lₖ)
                last_clause = f"(~{aux_vars[-1]} | {literals[-2]} | {literals[-1]})"
                new_clauses.append(last_clause)
                reduced_clauses.append(last_clause)

                await self.emit_state("add_last_clause", {
                    "clause_index": clause_idx,
                    "new_clause": last_clause,
                    "prev_aux": aux_vars[-1],
                    "literals": [str(literals[-2]), str(literals[-1])],
                    "explanation": f"Final: ~{aux_vars[-1]} + last 2 literals"
                })

                await self.emit_state("split_clause_complete", {
                    "clause_index": clause_idx,
                    "original_clause": str(clause),
                    "new_clauses": new_clauses,
                    "num_new_clauses": len(new_clauses)
                })

        # Complete reduction
        reduced_formula_str = " & ".join(str(c) for c in reduced_clauses)

        await self.emit_state("reduction_complete", {
            "reduced_formula": reduced_formula_str,
            "original_clauses": len(self.cnf.clauses),
            "reduced_clauses": len(reduced_clauses),
            "original_variables": len(self.cnf.get_variables()),
            "auxiliary_variables": total_aux_vars,
            "total_variables": len(self.cnf.get_variables()) + total_aux_vars
        })

        # Emit final result with reduced formula as solution
        await self.emit_complete(
            result=None,
            stats={
                "original_clauses": len(self.cnf.clauses),
                "reduced_clauses": len(reduced_clauses),
                "auxiliary_variables_added": total_aux_vars,
                "reduction_ratio": f"{len(reduced_clauses) / len(self.cnf.clauses):.2f}" if self.cnf.clauses else "0"
            },
            result_type="REDUCTION_COMPLETE",
            solution_data={"reduced_formula": reduced_formula_str}
        )

        return "3SAT"


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
    elif algorithm == "hornsat":
        return HornSATWrapper(cnf, websocket, speed_ms)
    elif algorithm == "cdcl":
        return CDCLWrapper(cnf, websocket, speed_ms)
    elif algorithm == "walksat":
        return WalkSATWrapper(cnf, websocket, speed_ms)
    elif algorithm == "3sat_reduction":
        return ThreeSATReductionWrapper(cnf, websocket, speed_ms)
    else:
        # For other algorithms, use a simple wrapper
        return BaseSolverWrapper(cnf, websocket, speed_ms)
