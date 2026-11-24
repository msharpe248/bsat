#!/usr/bin/env python3
"""Turnkey production acceptance gate for BSAT.

Evaluates a candidate BSAT binary against a JSON SLO policy over a pinned
corpus or directory with:
  1. Candidate integrity checks (git revision, clean-tree gate, binary/checker
     and policy SHA-256 digests).
  2. POSIX OS containment per child process (RLIMIT_CPU, RLIMIT_FSIZE, and
     RLIMIT_DATA/RLIMIT_AS where supported), plus peak RSS via wait4().
  3. Independent SAT model verification against original DIMACS clauses and
     independent UNSAT DRAT verification via drat-trim.
  4. Explicit numerical SLO gates (max_bad, min_solve_rate, max_p95_sec,
     max_par2_sec, max_rss_mb, max_proof_mb, require_drat_trim).

Exits 0 on PASS, 1 on SLO/gate failure, 2 on usage/environment error.
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import lzma
import math
import os
import platform
import resource
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent.parent.parent
DEFAULT_CORPORA = REPO_ROOT / "dataset" / "corpora"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def git_info(repo: Path) -> dict[str, Any]:
    info: dict[str, Any] = {"commit": None, "dirty": None}
    try:
        rev = subprocess.run(
            ["git", "-C", str(repo), "rev-parse", "HEAD"],
            capture_output=True, text=True, timeout=5,
        )
        if rev.returncode == 0:
            info["commit"] = rev.stdout.strip()
        st = subprocess.run(
            ["git", "-C", str(repo), "status", "--porcelain"],
            capture_output=True, text=True, timeout=5,
        )
        if st.returncode == 0:
            info["dirty"] = bool(st.stdout.strip())
    except Exception:
        pass
    return info


def open_text_maybe_compressed(path: Path):
    s = str(path)
    if s.endswith(".gz"):
        return gzip.open(path, "rt", encoding="ascii", errors="ignore")
    if s.endswith(".xz"):
        return lzma.open(path, "rt", encoding="ascii", errors="ignore")
    return open(path, "rt", encoding="ascii", errors="ignore")


def materialize_cnf(src: Path, workdir: Path) -> tuple[Path, bool]:
    s = str(src)
    if s.endswith(".gz") or s.endswith(".xz"):
        out = workdir / "input.cnf"
        opener = gzip.open if s.endswith(".gz") else lzma.open
        with opener(src, "rb") as fin, open(out, "wb") as fout:
            shutil.copyfileobj(fin, fout)
        return out, True
    return src, False


def parse_model_from_stdout(stdout: str) -> tuple[str, dict[int, bool]]:
    status = "UNKNOWN"
    assignment: dict[int, bool] = {}
    for line in stdout.splitlines():
        if line.startswith("s "):
            tok = line[2:].strip()
            if "UNSATISFIABLE" in tok:
                status = "UNSAT"
            elif "SATISFIABLE" in tok:
                status = "SAT"
        elif line.startswith("v ") or line.startswith("V "):
            for tok in line[2:].split():
                try:
                    lit = int(tok)
                except ValueError:
                    continue
                if lit == 0:
                    continue
                assignment[abs(lit)] = lit > 0
    return status, assignment


def verify_sat_model(cnf_path: Path, assignment: dict[int, bool]) -> tuple[bool, str]:
    clause_lits: list[int] = []
    clause_idx = 0
    with open_text_maybe_compressed(cnf_path) as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("c") or line.startswith("p") or line.startswith("%"):
                if line.startswith("%"):
                    break
                continue
            for tok in line.split():
                try:
                    lit = int(tok)
                except ValueError:
                    continue
                if lit == 0:
                    clause_idx += 1
                    sat = False
                    for l in clause_lits:
                        val = assignment.get(abs(l))
                        if val is not None and ((l > 0 and val) or (l < 0 and not val)):
                            sat = True
                            break
                    if not sat:
                        return False, f"clause {clause_idx} falsified"
                    clause_lits.clear()
                else:
                    clause_lits.append(lit)
    return True, f"{clause_idx} clauses verified"


def make_rlimit_preexec(cpu_sec: int, fsize_bytes: int, mem_bytes: int):
    is_linux = sys.platform.startswith("linux")

    def _apply() -> None:
        if cpu_sec > 0:
            try:
                resource.setrlimit(resource.RLIMIT_CPU, (cpu_sec, cpu_sec))
            except (ValueError, OSError):
                pass
        if fsize_bytes > 0:
            try:
                resource.setrlimit(resource.RLIMIT_FSIZE, (fsize_bytes, fsize_bytes))
            except (ValueError, OSError):
                pass
        if hasattr(resource, "RLIMIT_CORE"):
            try:
                resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
            except (ValueError, OSError):
                pass
        if mem_bytes > 0:
            # RLIMIT_AS is reliable on Linux; on macOS, RLIMIT_DATA avoids dyld map failures.
            lim_const = resource.RLIMIT_AS if is_linux else getattr(resource, "RLIMIT_DATA", None)
            if lim_const is not None:
                try:
                    soft, hard = resource.getrlimit(lim_const)
                    cap = mem_bytes if hard == resource.RLIM_INFINITY else min(mem_bytes, hard)
                    resource.setrlimit(lim_const, (cap, cap))
                except (ValueError, OSError):
                    pass

    return _apply


def run_contained(
    argv: list[str],
    timeout_sec: float,
    cpu_limit_sec: int,
    fsize_bytes: int,
    mem_bytes: int,
) -> dict[str, Any]:
    """Execute a subprocess with POSIX setrlimit containment and measure peak RSS."""
    t0 = time.monotonic()
    proc = subprocess.Popen(
        argv,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        preexec_fn=make_rlimit_preexec(cpu_limit_sec, fsize_bytes, mem_bytes),
    )
    timed_out = False
    rss_bytes = 0
    stdout_b = b""
    stderr_b = b""

    def _read_pipes() -> tuple[bytes, bytes]:
        out = proc.stdout.read() if proc.stdout else b""
        err = proc.stderr.read() if proc.stderr else b""
        return out, err

    # Wait with os.wait4 so we capture exact per-child ru_maxrss!
    deadline = t0 + timeout_sec
    status_raw: int | None = None
    with ThreadPoolExecutor(max_workers=2) as io_pool:
        fut_out = io_pool.submit(proc.stdout.read) if proc.stdout else None
        fut_err = io_pool.submit(proc.stderr.read) if proc.stderr else None
        while True:
            pid, st, rusage = os.wait4(proc.pid, os.WNOHANG)
            if pid == proc.pid:
                status_raw = st
                # macOS reports ru_maxrss in bytes; Linux reports kilobytes.
                rss_bytes = int(rusage.ru_maxrss) * (1 if sys.platform == "darwin" else 1024)
                break
            if time.monotonic() >= deadline:
                timed_out = True
                try:
                    proc.kill()
                except OSError:
                    pass
                pid, st, rusage = os.wait4(proc.pid, 0)
                status_raw = st
                rss_bytes = int(rusage.ru_maxrss) * (1 if sys.platform == "darwin" else 1024)
                break
            time.sleep(0.005)
        stdout_b = fut_out.result() if fut_out else b""
        stderr_b = fut_err.result() if fut_err else b""

    elapsed = time.monotonic() - t0
    if status_raw is None:
        rc = -1
    elif os.WIFEXITED(status_raw):
        rc = os.WEXITSTATUS(status_raw)
    elif os.WIFSIGNALED(status_raw):
        rc = -os.WTERMSIG(status_raw)
    else:
        rc = -1
    # Mark Popen internal state as reaped so __del__ won't call waitpid again.
    proc.returncode = rc
    return {
        "returncode": rc,
        "elapsed_sec": elapsed,
        "timed_out": timed_out,
        "rss_mb": rss_bytes / (1024.0 * 1024.0),
        "stdout": stdout_b.decode("utf-8", errors="replace"),
        "stderr": stderr_b.decode("utf-8", errors="replace"),
    }


def checker_verified(rc: int, stdout: str, stderr: str) -> bool:
    text = stdout + "\n" + stderr
    if "s VERIFIED" in text:
        return rc == 0 or (rc == 1 and "c trivial UNSAT" in text)
    return False


def collect_instances(policy: dict[str, Any], corpus_override: str | None) -> list[dict[str, Any]]:
    source = corpus_override or policy.get("corpus") or policy.get("dir")
    if not source:
        raise ValueError("Policy must specify 'corpus' or 'dir' (or pass --corpus/--dir)")
    p = Path(source)
    if not p.is_absolute():
        cand = DEFAULT_CORPORA / source
        p = cand if cand.exists() else (REPO_ROOT / source)
    if not p.exists():
        raise FileNotFoundError(f"Corpus path not found: {p}")

    manifest = p / "manifest.json"
    instances: list[dict[str, Any]] = []
    if manifest.exists():
        data = json.loads(manifest.read_text())
        for item in data.get("instances", []):
            rel = item["path"]
            instances.append({
                "id": rel,
                "path": p / rel,
                "expected": item.get("expected", "UNKNOWN"),
                "family": item.get("family", Path(rel).parent.name),
            })
    else:
        for ext in ("*.cnf", "*.cnf.gz", "*.cnf.xz"):
            for f in sorted(p.rglob(ext)):
                instances.append({
                    "id": str(f.relative_to(p)),
                    "path": f,
                    "expected": "UNKNOWN",
                    "family": f.parent.name,
                })
    max_n = int(policy.get("max_instances", 0) or 0)
    if max_n > 0:
        instances = instances[:max_n]
    return instances


def evaluate_instance(
    inst: dict[str, Any],
    solver_bin: Path,
    extra_args: list[str],
    checker_bin: Path | None,
    timeout_sec: float,
    check_timeout_sec: float,
    use_proof: bool,
    binary_proof: bool,
    fsize_bytes: int,
    mem_bytes: int,
) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="bsat_gate_") as td:
        tdir = Path(td)
        cnf_path, _ = materialize_cnf(inst["path"], tdir)
        proof_path = tdir / "proof.drat"
        argv = [str(solver_bin), str(cnf_path), *extra_args]
        if use_proof:
            argv += ["--proof", str(proof_path)]
            if binary_proof:
                argv.append("--binary-proof")

        cpu_lim = max(2, int(math.ceil(timeout_sec)) + 2)
        run = run_contained(argv, timeout_sec, cpu_lim, fsize_bytes, mem_bytes)
        status, model = parse_model_from_stdout(run["stdout"])
        if run["timed_out"]:
            status = "UNKNOWN"

        bad_reason: str | None = None
        proof_mb = 0.0
        check_sec = 0.0
        proof_ok: bool | None = None

        if status == "SAT":
            ok, msg = verify_sat_model(cnf_path, model)
            if not ok:
                bad_reason = f"MODEL_FAIL: {msg}"
            elif inst["expected"] == "UNSAT":
                bad_reason = "WRONG_SAT (expected UNSAT)"
        elif status == "UNSAT":
            if inst["expected"] == "SAT":
                bad_reason = "WRONG_UNSAT (expected SAT)"
            elif use_proof:
                if not proof_path.exists():
                    bad_reason = "PROOF_MISSING"
                else:
                    proof_mb = proof_path.stat().st_size / (1024.0 * 1024.0)
                    if checker_bin is not None:
                        chk = run_contained(
                            [str(checker_bin), str(cnf_path), str(proof_path)],
                            check_timeout_sec,
                            max(2, int(math.ceil(check_timeout_sec)) + 2),
                            fsize_bytes,
                            mem_bytes,
                        )
                        check_sec = chk["elapsed_sec"]
                        proof_ok = (not chk["timed_out"]) and checker_verified(
                            chk["returncode"], chk["stdout"], chk["stderr"]
                        )
                        if not proof_ok:
                            bad_reason = f"PROOF_FAIL (rc={chk['returncode']}, timed_out={chk['timed_out']})"
        else:
            if not run["timed_out"] and run["returncode"] not in (0, 10, 20):
                bad_reason = f"CRASH_OR_ERROR (rc={run['returncode']})"

        return {
            "id": inst["id"],
            "family": inst["family"],
            "expected": inst["expected"],
            "status": status,
            "elapsed_sec": round(run["elapsed_sec"], 4),
            "rss_mb": round(run["rss_mb"], 2),
            "proof_mb": round(proof_mb, 4),
            "check_sec": round(check_sec, 4),
            "proof_verified": proof_ok,
            "bad_reason": bad_reason,
        }


def percentile(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    xs = sorted(values)
    idx = min(len(xs) - 1, max(0, int(math.ceil((p / 100.0) * len(xs))) - 1))
    return xs[idx]


def main() -> int:
    ap = argparse.ArgumentParser(description="Turnkey BSAT production acceptance gate")
    ap.add_argument("--policy", required=True, help="Path to JSON SLO policy file")
    ap.add_argument("--solver", default=str(REPO_ROOT / "competition/c/bin/bsat"), help="Candidate BSAT binary")
    ap.add_argument("--checker", default=None, help="Path to drat-trim binary (overrides policy)")
    ap.add_argument("--corpus", default=None, help="Override corpus name or directory")
    ap.add_argument("--workers", type=int, default=None, help="Parallel worker threads")
    ap.add_argument("--report", default=None, help="Write JSON acceptance report to this path")
    ap.add_argument("--allow-dirty", action="store_true", help="Override require_clean_git for local testing")
    args = ap.parse_args()

    policy_path = Path(args.policy).resolve()
    policy = json.loads(policy_path.read_text())
    solver_bin = Path(args.solver).resolve()
    if not solver_bin.exists():
        print(f"ERROR: solver binary not found: {solver_bin}", file=sys.stderr)
        return 2

    require_drat = bool(policy.get("require_drat_trim", False))
    checker_raw = args.checker or policy.get("checker") or shutil.which("drat-trim")
    checker_bin = Path(checker_raw).resolve() if checker_raw else None
    if require_drat and (checker_bin is None or not checker_bin.exists()):
        print("ERROR: policy requires drat-trim, but no checker binary was found (pass --checker)", file=sys.stderr)
        return 2

    instances = collect_instances(policy, args.corpus)
    if not instances:
        print("ERROR: zero instances found in target corpus", file=sys.stderr)
        return 2

    timeout_sec = float(policy.get("timeout_sec", 15.0))
    check_timeout_sec = float(policy.get("check_timeout_sec", 60.0))
    use_proof = bool(policy.get("proof", require_drat))
    binary_proof = bool(policy.get("binary_proof", True))
    extra_args = list(policy.get("solver_args", []))
    workers = int(args.workers or policy.get("workers", min(8, os.cpu_count() or 4)))
    fsize_mb = float(policy.get("rlimit_fsize_mb", 128))
    mem_mb = float(policy.get("rlimit_mem_mb", 2048))
    fsize_bytes = int(fsize_mb * 1024 * 1024) if fsize_mb > 0 else 0
    mem_bytes = int(mem_mb * 1024 * 1024) if mem_mb > 0 else 0

    ginfo = git_info(REPO_ROOT)
    t_start = time.monotonic()
    results: list[dict[str, Any]] = []
    with ThreadPoolExecutor(max_workers=workers) as ex:
        futs = [
            ex.submit(
                evaluate_instance,
                inst, solver_bin, extra_args, checker_bin,
                timeout_sec, check_timeout_sec, use_proof, binary_proof,
                fsize_bytes, mem_bytes,
            )
            for inst in instances
        ]
        for fut in as_completed(futs):
            results.append(fut.result())
    results.sort(key=lambda r: r["id"])
    wall_sec = time.monotonic() - t_start

    total = len(results)
    bad_rows = [r for r in results if r["bad_reason"] is not None]
    sat_cnt = sum(1 for r in results if r["status"] == "SAT" and r["bad_reason"] is None)
    unsat_cnt = sum(1 for r in results if r["status"] == "UNSAT" and r["bad_reason"] is None)
    solved = sat_cnt + unsat_cnt
    solve_rate = solved / total if total else 0.0
    solved_times = [r["elapsed_sec"] for r in results if r["status"] in ("SAT", "UNSAT") and r["bad_reason"] is None]
    p50 = percentile(solved_times, 50.0)
    p95 = percentile(solved_times, 95.0)
    max_t = max(solved_times) if solved_times else 0.0
    par2 = sum(
        r["elapsed_sec"] if (r["status"] in ("SAT", "UNSAT") and r["bad_reason"] is None) else 2.0 * timeout_sec
        for r in results
    ) / total
    max_rss_mb = max((r["rss_mb"] for r in results), default=0.0)
    max_proof_mb = max((r["proof_mb"] for r in results), default=0.0)
    verified_proofs = sum(1 for r in results if r["proof_verified"] is True)

    # Evaluate SLO gates
    slos = policy.get("slos", {})
    violations: list[str] = []
    if policy.get("require_clean_git", False) and not args.allow_dirty and ginfo.get("dirty"):
        violations.append("git_tree_dirty (require_clean_git=true)")
    max_bad = int(slos.get("max_bad", 0))
    if len(bad_rows) > max_bad:
        violations.append(f"bad={len(bad_rows)} > max_bad={max_bad}")
    min_solve_rate = float(slos.get("min_solve_rate", 1.0))
    if solve_rate < min_solve_rate:
        violations.append(f"solve_rate={solve_rate:.4f} < min_solve_rate={min_solve_rate:.4f}")
    if "max_p95_sec" in slos and p95 > float(slos["max_p95_sec"]):
        violations.append(f"p95_sec={p95:.4f} > max_p95_sec={float(slos['max_p95_sec']):.4f}")
    if "max_par2_sec" in slos and par2 > float(slos["max_par2_sec"]):
        violations.append(f"par2_sec={par2:.4f} > max_par2_sec={float(slos['max_par2_sec']):.4f}")
    if "max_rss_mb" in slos and max_rss_mb > float(slos["max_rss_mb"]):
        violations.append(f"max_rss_mb={max_rss_mb:.2f} > max_rss_mb={float(slos['max_rss_mb']):.2f}")
    if "max_proof_mb" in slos and max_proof_mb > float(slos["max_proof_mb"]):
        violations.append(f"max_proof_mb={max_proof_mb:.2f} > max_proof_mb={float(slos['max_proof_mb']):.2f}")
    if require_drat and verified_proofs < unsat_cnt:
        violations.append(f"verified_proofs={verified_proofs} < unsat_cnt={unsat_cnt}")

    passed = len(violations) == 0
    report = {
        "schema_version": 1,
        "verdict": "PASS" if passed else "FAIL",
        "policy_name": policy.get("name", policy_path.name),
        "violations": violations,
        "integrity": {
            "git_commit": ginfo["commit"],
            "git_dirty": ginfo["dirty"],
            "solver_path": str(solver_bin),
            "solver_sha256": sha256_file(solver_bin),
            "checker_path": str(checker_bin) if checker_bin else None,
            "checker_sha256": sha256_file(checker_bin) if checker_bin else None,
            "policy_sha256": sha256_file(policy_path),
            "platform": platform.platform(),
            "machine": platform.machine(),
        },
        "containment": {
            "timeout_sec": timeout_sec,
            "rlimit_fsize_mb": fsize_mb,
            "rlimit_mem_mb": mem_mb,
            "rlimit_mem_kind": "RLIMIT_AS" if sys.platform.startswith("linux") else "RLIMIT_DATA",
        },
        "metrics": {
            "instances": total,
            "solved": solved,
            "sat": sat_cnt,
            "unsat": unsat_cnt,
            "unknown": total - solved - len(bad_rows),
            "bad": len(bad_rows),
            "solve_rate": round(solve_rate, 6),
            "p50_sec": round(p50, 4),
            "p95_sec": round(p95, 4),
            "max_sec": round(max_t, 4),
            "par2_sec": round(par2, 4),
            "max_rss_mb": round(max_rss_mb, 2),
            "max_proof_mb": round(max_proof_mb, 4),
            "verified_proofs": verified_proofs,
            "wall_sec": round(wall_sec, 3),
        },
        "failures": bad_rows[:20],
        "results": results,
    }

    if args.report:
        rp = Path(args.report)
        rp.parent.mkdir(parents=True, exist_ok=True)
        rp.write_text(json.dumps(report, indent=2) + "\n")

    print("=" * 72)
    print(f"BSAT ACCEPTANCE GATE: {report['verdict']}  ({report['policy_name']})")
    print("=" * 72)
    print(f"  Candidate : {solver_bin.name}  sha256={report['integrity']['solver_sha256'][:16]}...")
    print(f"  Git rev   : {ginfo['commit'] or 'unknown'}  dirty={ginfo['dirty']}")
    print(f"  Solved    : {solved}/{total} ({solve_rate*100:.1f}%)  SAT={sat_cnt} UNSAT={unsat_cnt} BAD={len(bad_rows)}")
    print(f"  Latency   : p50={p50:.4f}s  p95={p95:.4f}s  max={max_t:.4f}s  PAR-2={par2:.4f}s  wall={wall_sec:.2f}s")
    print(f"  Resources : max_rss={max_rss_mb:.2f} MiB  max_proof={max_proof_mb:.2f} MiB  drat_verified={verified_proofs}/{unsat_cnt}")
    if violations:
        print("  VIOLATIONS:")
        for v in violations:
            print(f"    - {v}")
    print("=" * 72)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
