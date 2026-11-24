#!/usr/bin/env python3
"""End-to-end verification of the Gap 3 deployment starter kit:
  1. contained_worker (durable fsync'd WAL + torn-tail recovery + POSIX setrlimit
     + certified query export checked against drat-trim and DIMACS CNF).
  2. acceptance_gate.py (candidate integrity hashes, POSIX setrlimit containment,
     peak RSS/proof reporting, PASS on satisfied SLOs, FAIL on violated SLOs).
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
C_DIR = TESTS_DIR.parent
SCRIPTS_DIR = C_DIR / "scripts"


def parse_model(path: Path) -> dict[int, bool]:
    assignment: dict[int, bool] = {}
    for line in path.read_text().splitlines():
        if line.startswith("v "):
            for tok in line[2:].split():
                lit = int(tok)
                if lit != 0:
                    assignment[abs(lit)] = lit > 0
    return assignment


def verify_cnf(cnf_path: Path, assignment: dict[int, bool]) -> None:
    clause: list[int] = []
    for raw in cnf_path.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("c") or line.startswith("p"):
            continue
        for tok in line.split():
            lit = int(tok)
            if lit == 0:
                if not any(assignment.get(abs(l)) == (l > 0) for l in clause):
                    raise AssertionError(f"Clause falsified in {cnf_path}: {clause}")
                clause.clear()
            else:
                clause.append(lit)


def test_contained_worker(worker_bin: Path, checker: Path | None) -> None:
    with tempfile.TemporaryDirectory(prefix="bsat_wal_test_") as td:
        tdir = Path(td)
        res = subprocess.run([str(worker_bin), str(tdir)], capture_output=True, text=True, timeout=30)
        if res.returncode != 0:
            raise AssertionError(f"contained_worker failed rc={res.returncode}: {res.stderr}")
        summary = json.loads(res.stdout.strip())
        assert summary["complete"] is True
        assert summary["wal_replayed_clauses"] == 64
        assert summary["torn_tail_ignored"] == 1
        assert summary["rlimit_enforced"] is True
        assert summary["killed_workers"] == 1
        assert summary["queries"] == 2

        cnf0 = tdir / "query-0.cnf"
        drat0 = tdir / "query-0.drat"
        cnf1 = tdir / "query-1.cnf"
        model1 = tdir / "query-1.model"
        assert cnf0.exists() and drat0.exists() and cnf1.exists() and model1.exists()

        assignment = parse_model(model1)
        assert len(assignment) == 7 and all(assignment[v] is True for v in range(1, 8))
        verify_cnf(cnf1, assignment)

        if checker is not None and checker.exists():
            chk = subprocess.run([str(checker), str(cnf0), str(drat0)], capture_output=True, text=True, timeout=30)
            out = chk.stdout + "\n" + chk.stderr
            if "s VERIFIED" not in out:
                raise AssertionError(f"drat-trim failed on contained_worker query-0: {out}")


def test_acceptance_gate(solver_bin: Path, checker: Path | None) -> None:
    gate_script = SCRIPTS_DIR / "acceptance_gate.py"
    with tempfile.TemporaryDirectory(prefix="bsat_gate_test_") as td:
        tdir = Path(td)
        corpus = tdir / "mini_corpus"
        corpus.mkdir()
        # 1 SAT + 1 UNSAT mini corpus
        (corpus / "sat.cnf").write_text("p cnf 3 3\n1 2 0\n-1 3 0\n2 3 0\n")
        (corpus / "unsat.cnf").write_text(
            "p cnf 2 4\n1 2 0\n-1 2 0\n1 -2 0\n-1 -2 0\n"
        )
        manifest = {
            "instances": [
                {"path": "sat.cnf", "expected": "SAT", "family": "mini"},
                {"path": "unsat.cnf", "expected": "UNSAT", "family": "mini"},
            ]
        }
        (corpus / "manifest.json").write_text(json.dumps(manifest))

        use_checker = checker is not None and checker.exists()
        pass_policy = {
            "name": "mini-pass-policy",
            "dir": str(corpus),
            "timeout_sec": 5.0,
            "workers": 2,
            "proof": True,
            "binary_proof": True,
            "require_drat_trim": use_checker,
            "require_clean_git": False,
            "rlimit_fsize_mb": 32,
            "rlimit_mem_mb": 1024,
            "slos": {
                "max_bad": 0,
                "min_solve_rate": 1.0,
                "max_p95_sec": 5.0,
                "max_par2_sec": 5.0,
                "max_rss_mb": 512.0,
                "max_proof_mb": 16.0,
            },
        }
        pass_pol_path = tdir / "pass.json"
        pass_rep_path = tdir / "pass_report.json"
        pass_pol_path.write_text(json.dumps(pass_policy))

        cmd = [
            sys.executable, str(gate_script),
            "--policy", str(pass_pol_path),
            "--solver", str(solver_bin),
            "--report", str(pass_rep_path),
        ]
        if use_checker:
            cmd += ["--checker", str(checker)]
        r_pass = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        assert r_pass.returncode == 0, f"Expected PASS (0), got {r_pass.returncode}:\n{r_pass.stdout}\n{r_pass.stderr}"
        rep = json.loads(pass_rep_path.read_text())
        assert rep["verdict"] == "PASS"
        assert rep["metrics"]["solved"] == 2
        assert rep["metrics"]["bad"] == 0
        assert rep["metrics"]["max_rss_mb"] > 0.0
        assert len(rep["integrity"]["solver_sha256"]) == 64
        if use_checker:
            assert rep["metrics"]["verified_proofs"] == 1

        # Deliberately impossible latency SLO -> must exit 1 and name max_p95_sec!
        fail_policy = json.loads(json.dumps(pass_policy))
        fail_policy["name"] = "mini-fail-slo"
        fail_policy["slos"]["max_p95_sec"] = 0.000001
        fail_pol_path = tdir / "fail.json"
        fail_rep_path = tdir / "fail_report.json"
        fail_pol_path.write_text(json.dumps(fail_policy))
        cmd_fail = [
            sys.executable, str(gate_script),
            "--policy", str(fail_pol_path),
            "--solver", str(solver_bin),
            "--report", str(fail_rep_path),
        ]
        if use_checker:
            cmd_fail += ["--checker", str(checker)]
        r_fail = subprocess.run(cmd_fail, capture_output=True, text=True, timeout=30)
        assert r_fail.returncode == 1, f"Expected FAIL (1), got {r_fail.returncode}"
        rep_fail = json.loads(fail_rep_path.read_text())
        assert rep_fail["verdict"] == "FAIL"
        assert any("max_p95_sec" in v for v in rep_fail["violations"])


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--worker", default=str(C_DIR / "bin/contained_worker_release"))
    ap.add_argument("--solver", default=str(C_DIR / "bin/bsat"))
    ap.add_argument("--checker", default=None)
    args = ap.parse_args()

    checker_raw = args.checker or shutil.which("drat-trim")
    checker = Path(checker_raw) if checker_raw else None
    test_contained_worker(Path(args.worker), checker)
    test_acceptance_gate(Path(args.solver), checker)
    print("PASS: contained_worker WAL+setrlimit+drat-trim and acceptance_gate PASS/FAIL SLOs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
