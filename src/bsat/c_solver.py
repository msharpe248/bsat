"""Zero-dependency ctypes bindings to BSAT's native C11 CDCL engine (libbsat ABI v1).

Provides both a high-level one-shot helper (`solve_c`) matching the rest of the
`bsat` Python package (`CNFExpression` -> `Dict[str, bool] | None`) and an
incremental context-managed solver (`CSATSolver`) supporting assumptions,
unsatisfiable cores (`get_failed_assumptions`), query/service limits,
checkpoints, and certified query export (`BSAT_CERTIFICATES`).

Build the shared library once with:
    make -C competition/c shared
"""

from __future__ import annotations

import ctypes
import ctypes.util
import os
import platform
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple, Union

from .cnf import CNFExpression, Clause, Literal

BSAT_ABI_VERSION = 1
BSAT_REUSE_LEARNTS = 1
BSAT_CERTIFICATES = 2
BSAT_CERTIFIED_PROBING = 4

BSAT_UNKNOWN = 0
BSAT_SAT = 10
BSAT_UNSAT = 20

BSAT_SERVICE_NONE = 0
BSAT_SERVICE_WALL = 1
BSAT_SERVICE_CAPACITY = 2


class _BsatStatsV1(ctypes.Structure):
    _fields_ = [
        ("version", ctypes.c_uint32),
        ("result", ctypes.c_int32),
        ("conflicts", ctypes.c_uint64),
        ("decisions", ctypes.c_uint64),
        ("propagations", ctypes.c_uint64),
        ("reused_preparations", ctypes.c_uint64),
        ("owned_capacity_bytes", ctypes.c_uint64),
        ("cpu_seconds", ctypes.c_double),
    ]


@dataclass
class CSolverStats:
    """Snapshot of the latest query statistics from libbsat."""

    result: int
    conflicts: int
    decisions: int
    propagations: int
    reused_preparations: int
    owned_capacity_bytes: int
    cpu_seconds: float

    @property
    def status(self) -> str:
        if self.result == BSAT_SAT:
            return "SAT"
        if self.result == BSAT_UNSAT:
            return "UNSAT"
        return "UNKNOWN"


_CACHED_LIBS: Dict[str, ctypes.CDLL] = {}


def _candidate_library_paths(lib_path: Optional[Union[str, Path]] = None) -> List[Path]:
    candidates: List[Path] = []
    if lib_path is not None:
        candidates.append(Path(lib_path))
    env_path = os.environ.get("BSAT_LIB_PATH")
    if env_path:
        candidates.append(Path(env_path))

    repo_root = Path(__file__).resolve().parents[2]
    sys_name = platform.system()
    if sys_name == "Darwin":
        names = [
            repo_root / "competition" / "c" / "bin" / "libbsat_release.dylib",
            repo_root / "competition" / "c" / "build" / "package-release" / "libbsat.1.dylib",
            repo_root / "competition" / "c" / "bin" / "libbsat_debug.dylib",
        ]
    else:
        names = [
            repo_root / "competition" / "c" / "bin" / "libbsat_release.so",
            repo_root / "competition" / "c" / "build" / "package-release" / "libbsat.so.1",
            repo_root / "competition" / "c" / "bin" / "libbsat_debug.so",
        ]
    candidates.extend(names)
    return candidates


def _configure_lib(lib: ctypes.CDLL) -> ctypes.CDLL:
    lib.bsat_abi_version.argtypes = []
    lib.bsat_abi_version.restype = ctypes.c_uint32

    lib.bsat_create.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
    lib.bsat_create.restype = ctypes.c_void_p

    lib.bsat_destroy.argtypes = [ctypes.c_void_p]
    lib.bsat_destroy.restype = None

    lib.bsat_set_limits.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_uint32, ctypes.c_uint32]
    lib.bsat_set_limits.restype = ctypes.c_int

    lib.bsat_set_query_limits.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_uint32, ctypes.c_uint32]
    lib.bsat_set_query_limits.restype = ctypes.c_int

    lib.bsat_set_service_limits.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_uint64]
    lib.bsat_set_service_limits.restype = ctypes.c_int

    lib.bsat_service_limit_hit.argtypes = [ctypes.c_void_p]
    lib.bsat_service_limit_hit.restype = ctypes.c_int

    lib.bsat_get_stats.argtypes = [ctypes.c_void_p, ctypes.POINTER(_BsatStatsV1), ctypes.c_size_t]
    lib.bsat_get_stats.restype = ctypes.c_int

    lib.bsat_add_clause.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int), ctypes.c_size_t]
    lib.bsat_add_clause.restype = ctypes.c_int

    lib.bsat_solve.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int), ctypes.c_size_t]
    lib.bsat_solve.restype = ctypes.c_int

    lib.bsat_value.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.bsat_value.restype = ctypes.c_int

    lib.bsat_failed.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.bsat_failed.restype = ctypes.c_int

    lib.bsat_error.argtypes = [ctypes.c_void_p]
    lib.bsat_error.restype = ctypes.c_int

    lib.bsat_checkpoint.argtypes = [ctypes.c_void_p]
    lib.bsat_checkpoint.restype = ctypes.c_int

    lib.bsat_export_query.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
    lib.bsat_export_query.restype = ctypes.c_int

    if hasattr(lib, "bsat_freeze"):
        lib.bsat_freeze.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.bsat_freeze.restype = ctypes.c_int
        lib.bsat_melt.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.bsat_melt.restype = ctypes.c_int
        lib.bsat_is_frozen.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.bsat_is_frozen.restype = ctypes.c_int
        lib.bsat_set_threads.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        lib.bsat_set_threads.restype = ctypes.c_int
        lib.bsat_set_proof.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
        lib.bsat_set_proof.restype = ctypes.c_int
        lib.bsat_set_lrat.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
        lib.bsat_set_lrat.restype = ctypes.c_int
        lib.bsat_add_xor.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int), ctypes.c_size_t, ctypes.c_int]
        lib.bsat_add_xor.restype = ctypes.c_int
        lib.bsat_add_atmost.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int), ctypes.c_size_t, ctypes.c_uint32]
        lib.bsat_add_atmost.restype = ctypes.c_int
        lib.bsat_add_pb_le.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_int),
            ctypes.POINTER(ctypes.c_uint64),
            ctypes.c_size_t,
            ctypes.c_uint64,
        ]
        lib.bsat_add_pb_le.restype = ctypes.c_int
        lib.bsat_backbone.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_int),
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_size_t),
        ]
        lib.bsat_backbone.restype = ctypes.c_int
        lib.bsat_mus.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_uint32),
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_size_t),
        ]
        lib.bsat_mus.restype = ctypes.c_int

    abi = lib.bsat_abi_version()
    if abi != BSAT_ABI_VERSION:
        raise RuntimeError(f"Unsupported libbsat ABI version {abi} (expected {BSAT_ABI_VERSION})")
    return lib


def load_c_library(lib_path: Optional[Union[str, Path]] = None) -> ctypes.CDLL:
    """Locate and load libbsat, raising RuntimeError with build instructions if unavailable."""
    for p in _candidate_library_paths(lib_path):
        if p.is_file():
            key = str(p.resolve())
            if key not in _CACHED_LIBS:
                _CACHED_LIBS[key] = _configure_lib(ctypes.CDLL(key))
            return _CACHED_LIBS[key]

    found = ctypes.util.find_library("bsat")
    if found:
        if found not in _CACHED_LIBS:
            _CACHED_LIBS[found] = _configure_lib(ctypes.CDLL(found))
        return _CACHED_LIBS[found]

    raise RuntimeError(
        "Could not locate BSAT native shared library (libbsat). "
        "Build it with `make -C competition/c shared` or set BSAT_LIB_PATH."
    )


def is_c_solver_available(lib_path: Optional[Union[str, Path]] = None) -> bool:
    """Return True if the native C libbsat shared library can be loaded."""
    try:
        load_c_library(lib_path)
        return True
    except (RuntimeError, OSError):
        return False


class CSATSolver:
    """Incremental native C11 SAT solver backed by libbsat (ABI v1).

    Supports both `CNFExpression`/`Clause`/`Literal` objects (with string variable
    names) and raw DIMACS integer clauses (`[1, -2, 3]`). Use as a context manager
    or call `close()` when finished.
    """

    def __init__(
        self,
        cnf: Optional[CNFExpression] = None,
        *,
        reuse_learnts: bool = True,
        certificates: bool = False,
        certified_probing: bool = False,
        cpu_limit: float = 0.0,
        wall_limit: float = 0.0,
        owned_bytes_limit: int = 0,
        max_conflicts: int = 0,
        max_decisions: int = 0,
        lib_path: Optional[Union[str, Path]] = None,
    ):
        self._lib = load_c_library(lib_path)
        flags = 0
        if reuse_learnts:
            flags |= BSAT_REUSE_LEARNTS
        if certificates:
            flags |= BSAT_CERTIFICATES
        if certified_probing:
            flags |= BSAT_CERTIFIED_PROBING
        self._handle = self._lib.bsat_create(BSAT_ABI_VERSION, flags)
        if not self._handle:
            raise RuntimeError(f"bsat_create failed (flags={flags})")

        self._var_to_id: Dict[str, int] = {}
        self._id_to_var: Dict[int, str] = {}
        self._next_id: int = 1
        self._has_clauses: bool = False
        self.last_status: int = BSAT_UNKNOWN

        if cpu_limit > 0.0 or max_conflicts > 0 or max_decisions > 0:
            if not self._lib.bsat_set_limits(self._handle, float(cpu_limit), int(max_conflicts), int(max_decisions)):
                self.close()
                raise RuntimeError("bsat_set_limits failed")
        if wall_limit > 0.0 or owned_bytes_limit > 0:
            if not self._lib.bsat_set_service_limits(self._handle, float(wall_limit), int(owned_bytes_limit)):
                self.close()
                raise RuntimeError("bsat_set_service_limits failed")

        if cnf is not None:
            self.add_cnf(cnf)

    def __enter__(self) -> "CSATSolver":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def __del__(self) -> None:
        self.close()

    def close(self) -> None:
        handle = getattr(self, "_handle", None)
        if handle:
            self._lib.bsat_destroy(handle)
            self._handle = None

    def _require_open(self) -> None:
        if not self._handle:
            raise RuntimeError("CSATSolver handle is closed")

    def _intern_var(self, name: str) -> int:
        vid = self._var_to_id.get(name)
        if vid is None:
            vid = self._next_id
            self._next_id += 1
            self._var_to_id[name] = vid
            self._id_to_var[vid] = name
        return vid

    def _encode_lit(self, lit: Union[Literal, int], *, allow_new: bool) -> int:
        if isinstance(lit, int):
            if lit == 0:
                raise ValueError("DIMACS literal cannot be 0")
            vid = abs(lit)
            if vid >= self._next_id:
                if not allow_new:
                    raise ValueError(f"Unknown assumption variable id {vid}")
                self._next_id = vid + 1
            name = str(vid)
            if vid not in self._id_to_var:
                self._id_to_var[vid] = name
                self._var_to_id.setdefault(name, vid)
            return lit
        if isinstance(lit, Literal):
            if allow_new:
                vid = self._intern_var(lit.variable)
            else:
                vid = self._var_to_id.get(lit.variable)
                if vid is None:
                    raise ValueError(f"Assumption references unknown variable {lit.variable!r}")
            return -vid if lit.negated else vid
        raise TypeError(f"Expected Literal or int, got {type(lit).__name__}")

    def add_clause(self, clause: Union[Clause, Iterable[Union[Literal, int]]]) -> None:
        """Add a clause (BSAT Clause or iterable of Literal/int) to the solver."""
        self._require_open()
        lits_iter = clause.literals if isinstance(clause, Clause) else clause
        encoded = [self._encode_lit(l, allow_new=True) for l in lits_iter]
        self._has_clauses = True
        if encoded:
            arr = (ctypes.c_int * len(encoded))(*encoded)
            ok = self._lib.bsat_add_clause(self._handle, arr, len(encoded))
        else:
            ok = self._lib.bsat_add_clause(self._handle, None, 0)
        if not ok or self._lib.bsat_error(self._handle):
            raise RuntimeError("bsat_add_clause failed")

    def add_cnf(self, cnf: CNFExpression) -> None:
        """Add all clauses from a CNFExpression."""
        for clause in cnf.clauses:
            self.add_clause(clause)

    def set_query_limits(
        self,
        *,
        cpu_limit: float = 0.0,
        max_conflicts: int = 0,
        max_decisions: int = 0,
    ) -> None:
        """Update per-query resource limits (0 means unlimited)."""
        self._require_open()
        if not self._lib.bsat_set_query_limits(
            self._handle, float(cpu_limit), int(max_conflicts), int(max_decisions)
        ):
            raise RuntimeError("bsat_set_query_limits failed")

    def solve_status(
        self,
        assumptions: Optional[Sequence[Union[Literal, int]]] = None,
        *,
        cpu_limit: Optional[float] = None,
        max_conflicts: Optional[int] = None,
        max_decisions: Optional[int] = None,
    ) -> int:
        """Solve under optional assumptions and return BSAT_SAT, BSAT_UNSAT, or BSAT_UNKNOWN."""
        self._require_open()
        if cpu_limit is not None or max_conflicts is not None or max_decisions is not None:
            self.set_query_limits(
                cpu_limit=0.0 if cpu_limit is None else cpu_limit,
                max_conflicts=0 if max_conflicts is None else max_conflicts,
                max_decisions=0 if max_decisions is None else max_decisions,
            )

        if assumptions:
            encoded = [self._encode_lit(a, allow_new=False) for a in assumptions]
            arr = (ctypes.c_int * len(encoded))(*encoded)
            rc = int(self._lib.bsat_solve(self._handle, arr, len(encoded)))
        else:
            rc = int(self._lib.bsat_solve(self._handle, None, 0))

        self.last_status = rc
        if self._lib.bsat_error(self._handle):
            raise RuntimeError("libbsat entered error state during solve")
        return rc

    def get_model(self) -> Optional[Dict[str, bool]]:
        """Return the latest satisfying assignment `{var_name: bool}`, or None if not SAT."""
        self._require_open()
        if self.last_status != BSAT_SAT:
            return None
        model: Dict[str, bool] = {}
        for vid, name in self._id_to_var.items():
            val = int(self._lib.bsat_value(self._handle, vid))
            model[name] = (val == vid)
        return model

    def solve(
        self,
        assumptions: Optional[Sequence[Union[Literal, int]]] = None,
        *,
        cpu_limit: Optional[float] = None,
        max_conflicts: Optional[int] = None,
        max_decisions: Optional[int] = None,
    ) -> Optional[Dict[str, bool]]:
        """Solve and return `{var: bool}` if SAT, else None (check `last_status` for UNSAT vs UNKNOWN)."""
        rc = self.solve_status(
            assumptions,
            cpu_limit=cpu_limit,
            max_conflicts=max_conflicts,
            max_decisions=max_decisions,
        )
        if rc == BSAT_SAT:
            return self.get_model()
        return None

    def get_failed_assumptions(
        self, assumptions: Sequence[Union[Literal, int]]
    ) -> List[Union[Literal, int]]:
        """Return the subset of `assumptions` belonging to the latest UNSAT core."""
        self._require_open()
        if self.last_status != BSAT_UNSAT:
            return []
        core: List[Union[Literal, int]] = []
        for a in assumptions:
            lit = self._encode_lit(a, allow_new=False)
            if self._lib.bsat_failed(self._handle, lit):
                core.append(a)
        return core

    def get_stats(self) -> CSolverStats:
        """Return statistics from the most recent query."""
        self._require_open()
        st = _BsatStatsV1()
        if not self._lib.bsat_get_stats(self._handle, ctypes.byref(st), ctypes.sizeof(st)):
            raise RuntimeError("bsat_get_stats failed")
        return CSolverStats(
            result=int(st.result),
            conflicts=int(st.conflicts),
            decisions=int(st.decisions),
            propagations=int(st.propagations),
            reused_preparations=int(st.reused_preparations),
            owned_capacity_bytes=int(st.owned_capacity_bytes),
            cpu_seconds=float(st.cpu_seconds),
        )

    def checkpoint(self) -> bool:
        """Rebuild permanent input and discard learned clauses/journal."""
        self._require_open()
        self.last_status = BSAT_UNKNOWN
        return bool(self._lib.bsat_checkpoint(self._handle))

    def export_query(self, cnf_path: Union[str, Path], proof_path: Union[str, Path]) -> bool:
        """Export the latest conclusive certified query's CNF and binary DRAT proof."""
        self._require_open()
        return bool(
            self._lib.bsat_export_query(
                self._handle,
                os.fsencode(str(cnf_path)),
                os.fsencode(str(proof_path)),
            )
        )

    def freeze(self, var: Union[str, Literal, int]) -> None:
        """Protect a variable from destructive simplification (BVE, SCC, BCE, sym)."""
        self._require_open()
        lit = self._intern_var(var) if isinstance(var, str) else self._encode_lit(var, allow_new=True)
        if not self._lib.bsat_freeze(self._handle, lit):
            raise RuntimeError("bsat_freeze failed")

    def melt(self, var: Union[str, Literal, int]) -> None:
        """Remove simplification protection from a variable."""
        self._require_open()
        lit = self._var_to_id.get(var, 0) if isinstance(var, str) else self._encode_lit(var, allow_new=False)
        if not lit or not self._lib.bsat_melt(self._handle, lit):
            raise RuntimeError("bsat_melt failed")

    def is_frozen(self, var: Union[str, Literal, int]) -> bool:
        """Return True if a variable is currently frozen."""
        self._require_open()
        lit = self._var_to_id.get(var, 0) if isinstance(var, str) else self._encode_lit(var, allow_new=False)
        return bool(lit and self._lib.bsat_is_frozen(self._handle, lit))

    def set_threads(self, threads: int) -> None:
        """Configure parallel portfolio thread count (1..64) for one-shot solving."""
        self._require_open()
        if not self._lib.bsat_set_threads(self._handle, int(threads)):
            raise RuntimeError(f"bsat_set_threads({threads}) failed")

    def set_proof(self, path: Union[str, Path], *, binary: bool = False) -> None:
        """Enable DRAT proof logging to `path` (before first input/solve)."""
        self._require_open()
        if not self._lib.bsat_set_proof(self._handle, os.fsencode(str(path)), int(bool(binary))):
            raise RuntimeError(f"bsat_set_proof({path}) failed")

    def set_lrat(self, path: Union[str, Path], *, binary: bool = False) -> None:
        """Enable native LRAT proof logging to `path` (before first input/solve)."""
        self._require_open()
        if not self._lib.bsat_set_lrat(self._handle, os.fsencode(str(path)), int(bool(binary))):
            raise RuntimeError(f"bsat_set_lrat({path}) failed")

    def add_xor(self, lits: Iterable[Union[Literal, int]], rhs: bool = True) -> None:
        """Add an exact XOR constraint (l1 XOR l2 XOR ... == rhs)."""
        self._require_open()
        encoded = [self._encode_lit(l, allow_new=True) for l in lits]
        arr = (ctypes.c_int * len(encoded))(*encoded) if encoded else None
        if not self._lib.bsat_add_xor(self._handle, arr, len(encoded), int(bool(rhs))):
            raise RuntimeError("bsat_add_xor failed")

    def add_atmost(self, lits: Iterable[Union[Literal, int]], k: int) -> None:
        """Add an unweighted cardinality constraint sum(lits) <= k."""
        self._require_open()
        encoded = [self._encode_lit(l, allow_new=True) for l in lits]
        arr = (ctypes.c_int * len(encoded))(*encoded) if encoded else None
        if not self._lib.bsat_add_atmost(self._handle, arr, len(encoded), int(k)):
            raise RuntimeError("bsat_add_atmost failed")

    def add_pb_le(
        self, lits: Sequence[Union[Literal, int]], weights: Sequence[int], rhs: int
    ) -> None:
        """Add a weighted Pseudo-Boolean constraint sum(weights[i] * lits[i]) <= rhs."""
        self._require_open()
        if len(lits) != len(weights):
            raise ValueError("lits and weights must have the same length")
        encoded = [self._encode_lit(l, allow_new=True) for l in lits]
        larr = (ctypes.c_int * len(encoded))(*encoded) if encoded else None
        warr = (ctypes.c_uint64 * len(weights))(*[int(w) for w in weights]) if weights else None
        if not self._lib.bsat_add_pb_le(self._handle, larr, warr, len(encoded), int(rhs)):
            raise RuntimeError("bsat_add_pb_le failed")

    def backbone(self) -> Optional[Dict[str, bool]]:
        """Compute the exact global backbone `{var_name: forced_value}`, or None if UNSAT."""
        self._require_open()
        cap = max(1, self._next_id)
        buf = (ctypes.c_int * cap)()
        cnt = ctypes.c_size_t(0)
        rc = int(self._lib.bsat_backbone(self._handle, buf, cap, ctypes.byref(cnt)))
        if rc == BSAT_UNSAT:
            return None
        if rc != BSAT_SAT:
            raise RuntimeError(f"bsat_backbone returned status {rc}")
        n = int(cnt.value)
        if n > cap:
            buf = (ctypes.c_int * n)()
            rc = int(self._lib.bsat_backbone(self._handle, buf, n, ctypes.byref(cnt)))
            if rc != BSAT_SAT:
                raise RuntimeError(f"bsat_backbone returned status {rc}")
        result: Dict[str, bool] = {}
        for i in range(n):
            lit = int(buf[i])
            vid = abs(lit)
            if vid in self._id_to_var:
                result[self._id_to_var[vid]] = (lit > 0)
        return result

    def mus(self) -> Optional[List[int]]:
        """Extract a Minimal Unsatisfiable Subset of 0-based clause indices, or None if SAT."""
        self._require_open()
        cap = 64
        buf = (ctypes.c_uint32 * cap)()
        cnt = ctypes.c_size_t(0)
        rc = int(self._lib.bsat_mus(self._handle, buf, cap, ctypes.byref(cnt)))
        if rc == BSAT_SAT:
            return None
        if rc != BSAT_UNSAT:
            raise RuntimeError(f"bsat_mus returned status {rc}")
        n = int(cnt.value)
        if n > cap:
            buf = (ctypes.c_uint32 * n)()
            rc = int(self._lib.bsat_mus(self._handle, buf, n, ctypes.byref(cnt)))
            if rc != BSAT_UNSAT:
                raise RuntimeError(f"bsat_mus returned status {rc}")
        return [int(buf[i]) for i in range(n)]


def solve_c(
    cnf: CNFExpression,
    *,
    assumptions: Optional[Sequence[Union[Literal, int]]] = None,
    cpu_limit: float = 0.0,
    wall_limit: float = 0.0,
    max_conflicts: int = 0,
    max_decisions: int = 0,
    lib_path: Optional[Union[str, Path]] = None,
) -> Optional[Dict[str, bool]]:
    """Solve a CNFExpression using BSAT's native C11 CDCL engine (libbsat)."""
    if not cnf.clauses:
        return {}
    with CSATSolver(
        cnf,
        reuse_learnts=False,
        cpu_limit=cpu_limit,
        wall_limit=wall_limit,
        max_conflicts=max_conflicts,
        max_decisions=max_decisions,
        lib_path=lib_path,
    ) as solver:
        return solver.solve(assumptions)


def get_c_solver_stats(
    cnf: CNFExpression,
    *,
    assumptions: Optional[Sequence[Union[Literal, int]]] = None,
    cpu_limit: float = 0.0,
    wall_limit: float = 0.0,
    max_conflicts: int = 0,
    max_decisions: int = 0,
    lib_path: Optional[Union[str, Path]] = None,
) -> Tuple[Optional[Dict[str, bool]], CSolverStats]:
    """Solve a CNFExpression with libbsat and return `(solution_or_none, CSolverStats)`."""
    with CSATSolver(
        cnf,
        reuse_learnts=False,
        cpu_limit=cpu_limit,
        wall_limit=wall_limit,
        max_conflicts=max_conflicts,
        max_decisions=max_decisions,
        lib_path=lib_path,
    ) as solver:
        sol = solver.solve(assumptions)
        return sol, solver.get_stats()
