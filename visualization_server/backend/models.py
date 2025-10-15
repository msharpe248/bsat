"""Pydantic models for the visualization server."""

from pydantic import BaseModel
from typing import List, Dict, Any, Optional, Literal


class FormulaRequest(BaseModel):
    """Request to validate or parse a formula."""
    formula: str
    format: Literal["text", "dimacs", "json"] = "text"


class FormulaResponse(BaseModel):
    """Response with parsed formula information."""
    valid: bool
    error: Optional[str] = None
    variables: List[str] = []
    num_clauses: int = 0
    parsed_formula: Optional[str] = None


class SolveRequest(BaseModel):
    """Request to start a solving session."""
    formula: str
    algorithm: Literal["dpll", "cdcl", "2sat", "davis_putnam", "walksat", "hornsat"]
    format: Literal["text", "dimacs", "json"] = "text"
    speed: int = 500  # milliseconds per step


class SolveResponse(BaseModel):
    """Response with session ID."""
    session_id: str
    message: str


class StateUpdate(BaseModel):
    """State update sent via WebSocket."""
    type: Literal["state_update", "complete", "error"]
    step: int
    algorithm: str
    state: Dict[str, Any]


class ExampleFormula(BaseModel):
    """Example formula with metadata."""
    name: str
    formula: str
    description: str
    algorithm: str
    difficulty: Literal["easy", "medium", "hard"]


class GenerateFormulaRequest(BaseModel):
    """Request to generate a random CNF formula."""
    sat_type: Literal["2sat", "3sat", "4sat", "5sat", "hornsat", "xorsat"]
    num_clauses: int
    num_variables: int = 0  # 0 means auto-determine
    clause_size: int = 0  # For k-SAT, 0 means use sat_type default


class GenerateFormulaResponse(BaseModel):
    """Response with generated formula."""
    formula: str
    num_clauses: int
    num_variables: int
    sat_type: str
