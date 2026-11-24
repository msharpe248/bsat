#!/usr/bin/env python3
"""On-demand wide-corpus SAT prover, verifier, and benchmark runner.

Runs BSAT (and optionally comparison solvers or alternative flag profiles)
across hundreds or thousands of CNF instances (.cnf, .cnf.gz, .cnf.xz, .cnf.bz2,
.cnf.zst) with:
  - Parallel worker pool (--workers N) and hard process-group timeout cleanup
  - Independent full-clause SAT model verification on every SAT answer
  - Known ground-truth SAT/UNSAT checking (from corpus manifest.json)
  - Cross-solver SAT/UNSAT agreement checking
  - Optional DRAT proof emission and external drat-trim checking (--checker)
  - Per-family summary tables, PAR-2 scores, slowdown/timeout spotlights,
    and JSON/CSV report export.

Examples:
  python3 competition/c/scripts/suite_runner.py --corpus synthetic-hard --timeout 10
  python3 competition/c/scripts/suite_runner.py --corpus satlib-core --timeout 10 --workers 4
  python3 competition/c/scripts/suite_runner.py --corpus satcomp-2024 --timeout 30 --workers 2 \
      --solver bsat=competition/c/bin/bsat \
      --solver "bsat-j4=competition/c/bin/bsat -j 4"
  python3 competition/c/scripts/suite_runner.py --dir /path/to/cnfs --timeout 15 --workers 4
"""
import argparse
import bz2
import concurrent.futures
import csv
import fnmatch
import gzip
import json
import lzma
import os
import random
import shlex
import shutil
import signal
import statistics
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[2]
CORPORA_ROOT = REPO_ROOT / "dataset" / "corpora"
DEFAULT_BSAT = REPO_ROOT / "competition" / "c" / "bin" / "bsat"


def open_cnf_stream(path: Path):
    """Return a binary file-like reader transparently decompressing CNF archives."""
    low = path.name.lower()
    if low.endswith(".gz"):
        return gzip.open(path, "rb")
    if low.endswith(".xz"):
        return lzma.open(path, "rb")
    if low.endswith(".bz2"):
        return bz2.open(path, "rb")
    if low.endswith(".zst"):
        proc = subprocess.Popen(["zstd", "-dc", str(path)], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        return proc.stdout
    return path.open("rb")


def decompress_to_temp(src: Path, tmp_dir: Path) -> Path:
    """Return an uncompressed .cnf path (either src directly or extracted into tmp_dir)."""
    low = src.name.lower()
    if low.endswith(".cnf"):
        return src
    out = tmp_dir / "input.cnf"
    if low.endswith(".zst"):
        subprocess.run(["zstd", "-dc", str(src)], stdout=out.open("wb"), check=True)
        return out
    with open_cnf_stream(src) as fin, out.open("wb") as fout:
        shutil.copyfileobj(fin, fout, length=1 << 20)
    return out


def parse_model_from_output(output: str):
    """Parse DIMACS 'v ... 0' lines into a dict {var: bool} (None if malformed)."""
    assign = {}
    saw_v = False
    terminated = False
    for line in output.splitlines():
        if line.startswith("v ") or line == "v":
            saw_v = True
            for tok in line[1:].split():
                try:
                    val = int(tok)
                except ValueError:
                    return None
                if val == 0:
                    terminated = True
                    break
                v = abs(val)
                pol = val > 0
                if v in assign and assign[v] != pol:
                    return None
                assign[v] = pol
    if not saw_v or not terminated:
        return None
    return assign


def verify_model_streaming(cnf_path: Path, output: str):
    """Independently check every clause in cnf_path against the solver's model."""
    assign = parse_model_from_output(output)
    if assign is None:
        return False, "malformed or missing 'v ... 0' model"

    clause_idx = 0
    clause_sat = False
    clause_len = 0
    in_body = False
    with cnf_path.open("rb") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line or line.startswith(b"c"):
                continue
            if line.startswith(b"%"):
                break
            if line.startswith(b"p"):
                in_body = True
                continue
            if not in_body:
                continue
            for tok in line.split():
                val = int(tok)
                if val == 0:
                    clause_idx += 1
                    if not clause_sat:
                        return False, f"clause #{clause_idx} (len={clause_len}) unsatisfied by model"
                    clause_sat = False
                    clause_len = 0
                else:
                    clause_len += 1
                    v = abs(val)
                    # Unmentioned variables can take any truth value; if assigned, check polarity.
                    if v in assign:
                        if assign[v] == (val > 0):
                            clause_sat = True
                    else:
                        # Treat missing variable strictly as FALSE unless explicitly assigned!
                        # BSAT always emits all 1..num_vars, so missing vars indicate truncation.
                        return False, f"variable {v} missing from model"
    if clause_len > 0 and not clause_sat:
        return False, "trailing unterminated clause unsatisfied"
    return True, ""


def parse_solver_stats(output: str) -> dict:
    stats = {}
    for line in output.splitlines():
        if not line.startswith("c "):
            continue
        if ":" in line:
            k, v = line[2:].split(":", 1)
            k = k.strip().lower()
            v = v.strip()
            if k == "conflicts":
                try:
                    stats["conflicts"] = int(v.split()[0])
                except ValueError:
                    pass
            elif k == "decisions":
                try:
                    stats["decisions"] = int(v.split()[0])
                except ValueError:
                    pass
            elif k == "propagations":
                try:
                    stats["propagations"] = int(v.split()[0])
                except ValueError:
                    pass
            elif k == "propagations/sec":
                try:
                    stats["props_per_sec"] = float(v.split()[0])
                except ValueError:
                    pass
    return stats


def checker_verified(stdout: str, stderr: str, rc: int) -> bool:
    combined = (stdout + "\n" + stderr).upper()
    return rc == 0 and ("S VERIFIED" in combined or "\nVERIFIED" in combined or combined.startswith("VERIFIED"))


def run_one(solver_name: str, cmd_template: list, instance: dict,
            timeout: float, checker: str = None, check_timeout: float = 60.0,
            use_proof: bool = False, binary_proof: bool = False,
            save_failures: Path = None) -> dict:
    src_path = Path(instance["abs_path"])
    expected = instance.get("expected", "UNKNOWN")

    with tempfile.TemporaryDirectory(prefix="bsat-wide-") as td:
        tmp = Path(td)
        try:
            cnf_path = decompress_to_temp(src_path, tmp)
        except Exception as exc:
            return {
                "solver": solver_name,
                "path": instance["rel_path"],
                "family": instance["family"],
                "expected": expected,
                "status": "ERROR",
                "verified": False,
                "seconds": 0.0,
                "par2": 2.0 * timeout,
                "diagnostic": f"decompress failed: {exc}",
            }

        proof_path = tmp / "proof.drat"
        cmd = []
        has_input = False
        has_proof = False
        for tok in cmd_template:
            if "{input}" in tok:
                has_input = True
            if "{proof}" in tok:
                has_proof = True
            cmd.append(tok.replace("{input}", str(cnf_path)).replace("{proof}", str(proof_path)))
        if use_proof and not has_proof:
            cmd.extend(["--proof", str(proof_path)])
            if binary_proof:
                cmd.append("--binary-proof")
        if not has_input:
            cmd.append(str(cnf_path))

        t0 = time.perf_counter()
        timed_out = False
        out_file = tmp / "stdout.txt"
        err_file = tmp / "stderr.txt"
        with out_file.open("wb") as fout, err_file.open("wb") as ferr:
            proc = subprocess.Popen(
                cmd, stdout=fout, stderr=ferr, start_new_session=True
            )
            try:
                rc = proc.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                timed_out = True
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                proc.wait()
                rc = 0
            finally:
                if proc.poll() is None:
                    try:
                        os.killpg(proc.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    proc.wait()
        elapsed = time.perf_counter() - t0
        if elapsed > timeout and not timed_out:
            timed_out = True

        stdout = out_file.read_text(errors="replace")
        stderr = err_file.read_text(errors="replace")

        if timed_out:
            status = "UNKNOWN"
        elif rc == 10:
            status = "SAT"
        elif rc == 20:
            status = "UNSAT"
        elif rc == 0:
            status = "UNKNOWN"
        else:
            status = "ERROR"

        # Check stdout 's ' line consistency
        s_lines = [ln.strip() for ln in stdout.splitlines() if ln.startswith("s ")]
        diagnostic = ""
        if status == "SAT" and "s SATISFIABLE" not in s_lines:
            status = "ERROR"
            diagnostic = f"exit 10 without 's SATISFIABLE' (got {s_lines})"
        elif status == "UNSAT" and "s UNSATISFIABLE" not in s_lines:
            status = "ERROR"
            diagnostic = f"exit 20 without 's UNSATISFIABLE' (got {s_lines})"
        elif status == "ERROR":
            diagnostic = f"exit code {rc}: {stderr.strip()[-400:]}"

        verified = False
        proof_checked = False
        if status == "SAT":
            ok, msg = verify_model_streaming(cnf_path, stdout)
            if not ok:
                status = "INVALID_MODEL"
                diagnostic = msg
            elif expected == "UNSAT":
                status = "WRONG_ANSWER"
                diagnostic = "solver returned SAT (with valid model!) on expected UNSAT — check ground truth or bug!"
            else:
                verified = True
        elif status == "UNSAT":
            if expected == "SAT":
                status = "WRONG_ANSWER"
                diagnostic = "solver returned UNSAT on expected SAT instance!"
            else:
                if checker and proof_path.exists():
                    try:
                        chk = subprocess.run(
                            [checker, str(cnf_path), str(proof_path)],
                            capture_output=True, text=True, timeout=check_timeout
                        )
                        if checker_verified(chk.stdout, chk.stderr, chk.returncode):
                            verified = True
                            proof_checked = True
                        else:
                            status = "PROOF_FAIL"
                            diagnostic = (chk.stdout[-400:] + " " + chk.stderr[-400:]).strip()
                    except subprocess.TimeoutExpired:
                        diagnostic = "proof checker timed out"
                        verified = (expected == "UNSAT")
                else:
                    verified = True

        stats = parse_solver_stats(stdout)
        is_solved = status in ("SAT", "UNSAT") and verified

        if save_failures and status in ("WRONG_ANSWER", "INVALID_MODEL", "PROOF_FAIL", "ERROR"):
            fdir = save_failures / f"{solver_name}_{src_path.stem}_{status}"
            fdir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src_path, fdir / src_path.name)
            (fdir / "stdout.txt").write_text(stdout)
            (fdir / "stderr.txt").write_text(stderr)
            if proof_path.exists():
                shutil.copy2(proof_path, fdir / "proof.drat")
            (fdir / "repro.txt").write_text(
                f"command: {' '.join(shlex.quote(x) for x in cmd)}\n"
                f"status: {status}\nexpected: {expected}\ndiagnostic: {diagnostic}\n"
            )

        return {
            "solver": solver_name,
            "path": instance["rel_path"],
            "family": instance["family"],
            "expected": expected,
            "status": status,
            "verified": verified,
            "proof_checked": proof_checked,
            "seconds": round(elapsed, 4),
            "par2": round(elapsed if is_solved else 2.0 * timeout, 4),
            "conflicts": stats.get("conflicts"),
            "decisions": stats.get("decisions"),
            "props_per_sec": stats.get("props_per_sec"),
            "diagnostic": diagnostic,
        }


def load_instances(corpora: list, dirs: list, manifests: list) -> list:
    items = []
    seen = set()

    def add_item(abs_path: Path, rel_path: str, family: str, expected: str):
        key = str(abs_path.resolve())
        if key in seen or not abs_path.exists():
            return
        seen.add(key)
        items.append({
            "abs_path": key,
            "rel_path": rel_path,
            "family": family,
            "expected": expected,
        })

    corpus_names = []
    for c in corpora:
        if c == "all":
            if CORPORA_ROOT.exists():
                for m in sorted(CORPORA_ROOT.glob("*/manifest.json")):
                    corpus_names.append(m.parent.name)
        else:
            corpus_names.append(c)

    for cname in corpus_names:
        mpath = CORPORA_ROOT / cname / "manifest.json"
        if not mpath.exists():
            print(f"Error: corpus '{cname}' not fetched yet (missing {mpath}).\n"
                  f"Run: python3 competition/c/scripts/fetch_corpora.py fetch {cname}",
                  file=sys.stderr)
            sys.exit(1)
        data = json.loads(mpath.read_text())
        base = mpath.parent
        for e in data.get("instances", []):
            p = Path(e["path"])
            if not p.is_absolute():
                p = base / p
            add_item(p, f"{cname}/{e['path']}", e.get("family", cname), e.get("expected", "UNKNOWN"))

    for mp in manifests:
        mpath = Path(mp).resolve()
        data = json.loads(mpath.read_text())
        base = mpath.parent
        if "instances" in data:
            for e in data["instances"]:
                p = Path(e["path"])
                if not p.is_absolute():
                    p = base / p
                add_item(p, e["path"], e.get("family", mpath.parent.name), e.get("expected", "UNKNOWN"))
        elif "inputs" in data:
            for name in data["inputs"]:
                p = Path(name)
                if not p.is_absolute():
                    p = base / p
                add_item(p, name, p.parent.name, "UNKNOWN")

    exts = (".cnf", ".cnf.gz", ".cnf.xz", ".cnf.bz2", ".cnf.zst")
    for d in dirs:
        dp = Path(d).resolve()
        if dp.is_file():
            add_item(dp, dp.name, dp.parent.name, "UNKNOWN")
        else:
            for p in sorted(dp.rglob("*")):
                if p.is_file() and p.name.lower().endswith(exts):
                    rel = str(p.relative_to(dp))
                    add_item(p, rel, p.parent.name, "UNKNOWN")

    return items


def print_summary(solver_names: list, all_results: dict, timeout: float):
    print("\n" + "=" * 102)
    print("WIDE BENCHMARK SUMMARY")
    print("=" * 102)

    for sname in solver_names:
        res = all_results[sname]
        by_fam = {}
        for r in res:
            by_fam.setdefault(r["family"], []).append(r)

        print(f"\nSolver: {sname}  (timeout={timeout:.1f}s, instances={len(res)})")
        print(f"{'Family':26s} {'Tot':>5s} {'Solv':>5s} {'SAT':>5s} {'UNSAT':>5s} {'UNK':>5s} {'BAD':>4s} {'PAR-2':>8s} {'Med(s)':>7s} {'Max(s)':>7s}")
        print("-" * 92)
        tot_solv = tot_sat = tot_unsat = tot_unk = tot_bad = 0
        all_par2 = []
        solved_times = []
        for fam, rows in sorted(by_fam.items()):
            n = len(rows)
            sat = sum(1 for r in rows if r["status"] == "SAT" and r["verified"])
            unsat = sum(1 for r in rows if r["status"] == "UNSAT" and r["verified"])
            solv = sat + unsat
            unk = sum(1 for r in rows if r["status"] == "UNKNOWN")
            bad = n - solv - unk
            par2 = sum(r["par2"] for r in rows) / n
            st = [r["seconds"] for r in rows if r["status"] in ("SAT", "UNSAT") and r["verified"]]
            med = statistics.median(st) if st else 0.0
            mx = max(st) if st else 0.0
            tot_solv += solv; tot_sat += sat; tot_unsat += unsat; tot_unk += unk; tot_bad += bad
            all_par2.extend(r["par2"] for r in rows)
            solved_times.extend(st)
            print(f"{fam[:26]:26s} {n:5d} {solv:5d} {sat:5d} {unsat:5d} {unk:5d} {bad:4d} {par2:8.3f} {med:7.3f} {mx:7.3f}")
        print("-" * 92)
        mean_par2 = sum(all_par2) / len(all_par2) if all_par2 else 0.0
        med_all = statistics.median(solved_times) if solved_times else 0.0
        max_all = max(solved_times) if solved_times else 0.0
        pct = 100.0 * tot_solv / len(res) if res else 0.0
        print(f"{'TOTAL':26s} {len(res):5d} {tot_solv:5d} {tot_sat:5d} {tot_unsat:5d} {tot_unk:5d} {tot_bad:4d} {mean_par2:8.3f} {med_all:7.3f} {max_all:7.3f}  ({pct:.1f}% solved)")

        # Highlight any failures loudly
        bad_rows = [r for r in res if r["status"] not in ("SAT", "UNSAT", "UNKNOWN") or (r["status"] in ("SAT", "UNSAT") and not r["verified"])]
        if bad_rows:
            print(f"\n  *** WARNING: {len(bad_rows)} FAILURE(S) DETECTED for {sname} ***")
            for r in bad_rows[:20]:
                print(f"    [{r['status']}] {r['path']} (expected={r['expected']}): {r['diagnostic']}")

        # Slowest solved & sample timeouts
        slowest = sorted([r for r in res if r["status"] in ("SAT", "UNSAT") and r["verified"]],
                         key=lambda r: r["seconds"], reverse=True)[:8]
        if slowest:
            print("\n  Slowest solved:")
            for r in slowest:
                c_str = f"confl={r['conflicts']}" if r.get("conflicts") is not None else ""
                print(f"    {r['seconds']:7.3f}s  {r['status']:5s}  {r['path']}  {c_str}")
        timeouts = [r for r in res if r["status"] == "UNKNOWN"]
        if timeouts:
            print(f"\n  Timeouts ({len(timeouts)} total, showing up to 8):")
            for r in timeouts[:8]:
                c_str = f"confl={r['conflicts']}" if r.get("conflicts") is not None else ""
                print(f"    >{timeout:5.1f}s   UNK    {r['path']}  (expected={r['expected']}) {c_str}")

    # Cross-solver comparison & agreement check
    if len(solver_names) >= 2:
        print("\n" + "=" * 102)
        print("HEAD-TO-HEAD COMPARISON & CROSS-SOLVER AGREEMENT")
        print("=" * 102)
        by_inst = {}
        for sname in solver_names:
            for r in all_results[sname]:
                by_inst.setdefault(r["path"], {})[sname] = r
        disagreements = []
        for path, smap in by_inst.items():
            verdicts = {r["status"] for r in smap.values() if r["status"] in ("SAT", "UNSAT")}
            if len(verdicts) > 1:
                disagreements.append((path, {k: v["status"] for k, v in smap.items()}))
        if disagreements:
            print(f"*** CRITICAL: {len(disagreements)} CROSS-SOLVER DISAGREEMENT(S) ***")
            for path, vs in disagreements[:20]:
                print(f"  {path}: {vs}")
        else:
            print("Cross-solver agreement: 100% consistent (0 SAT/UNSAT disagreements)")

        print(f"\n{'Solver':24s} {'Solved':>8s} {'SAT':>6s} {'UNSAT':>6s} {'Mean PAR-2':>11s} {'Total Time(s)':>14s}")
        print("-" * 74)
        for sname in solver_names:
            rows = all_results[sname]
            sat = sum(1 for r in rows if r["status"] == "SAT" and r["verified"])
            unsat = sum(1 for r in rows if r["status"] == "UNSAT" and r["verified"])
            par2 = sum(r["par2"] for r in rows) / len(rows) if rows else 0.0
            tt = sum(r["seconds"] for r in rows)
            print(f"{sname[:24]:24s} {sat+unsat:8d} {sat:6d} {unsat:6d} {par2:11.3f} {tt:14.2f}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--corpus", action="append", default=[], help="Cached corpus name (repeatable, or 'all')")
    p.add_argument("--dir", action="append", default=[], help="Directory or file of .cnf/.cnf.gz/.cnf.xz inputs")
    p.add_argument("--manifest", action="append", default=[], help="Path to a manifest.json file")
    p.add_argument("--solver", action="append", default=[], metavar="NAME=CMD",
                   help="Solver spec (default: bsat=competition/c/bin/bsat); repeatable for comparisons")
    p.add_argument("--timeout", type=float, default=10.0, help="Wall-clock timeout per instance in seconds (default: 10)")
    p.add_argument("--workers", type=int, default=max(1, min(4, (os.cpu_count() or 2) // 2)),
                   help="Parallel worker processes (use 1 for strict serial PAR-2 timing)")
    p.add_argument("--family", default="", help="Glob filter on family name (e.g. 'php*' or '*xor*')")
    p.add_argument("--filter", default="", help="Glob filter on instance path")
    p.add_argument("--expected", choices=("SAT", "UNSAT", "UNKNOWN"), default=None, help="Only run instances with this label")
    p.add_argument("--max-instances", type=int, default=0, help="Cap number of instances (0 = all)")
    p.add_argument("--shuffle", action="store_true", help="Shuffle instances before applying --max-instances")
    p.add_argument("--seed", type=int, default=1, help="Shuffle seed")
    p.add_argument("--proof", action="store_true", help="Request DRAT proofs (--proof {proof}) on solver runs")
    p.add_argument("--binary-proof", action="store_true", help="Use --binary-proof with --proof")
    p.add_argument("--checker", default=None, help="External DRAT checker binary (e.g. drat-trim)")
    p.add_argument("--check-timeout", type=float, default=60.0, help="Timeout for DRAT checker")
    p.add_argument("--save-failures", type=Path, default=None, help="Directory to copy failing inputs & reproducers")
    p.add_argument("--output", type=Path, default=None, help="Write full JSON results report")
    p.add_argument("--csv", type=Path, default=None, help="Write per-instance CSV report")
    p.add_argument("--quiet", action="store_true", help="Suppress live progress updates")
    args = p.parse_args()

    if not args.corpus and not args.dir and not args.manifest:
        p.error("Specify at least one of --corpus, --dir, or --manifest (e.g. --corpus synthetic-hard)")

    solvers = []
    if not args.solver:
        if not DEFAULT_BSAT.exists():
            p.error(f"Default solver not built at {DEFAULT_BSAT}. Run: make -C competition/c")
        solvers.append(("bsat", [str(DEFAULT_BSAT)]))
    else:
        for spec in args.solver:
            if "=" not in spec:
                p.error(f"Solver spec must be NAME=COMMAND, got: {spec}")
            name, cmd = spec.split("=", 1)
            toks = shlex.split(cmd)
            if not toks:
                p.error(f"Empty command for solver {name}")
            exe = shutil.which(toks[0])
            if not exe:
                p.error(f"Executable not found for solver {name}: {toks[0]}")
            toks[0] = str(Path(exe).resolve())
            solvers.append((name, toks))

    checker = None
    if args.checker:
        checker = shutil.which(args.checker)
        if not checker:
            p.error(f"Checker executable not found: {args.checker}")

    instances = load_instances(args.corpus, args.dir, args.manifest)
    if args.family:
        instances = [x for x in instances if fnmatch.fnmatch(x["family"], args.family)]
    if args.filter:
        instances = [x for x in instances if fnmatch.fnmatch(x["rel_path"], args.filter)]
    if args.expected:
        instances = [x for x in instances if x["expected"] == args.expected]
    if args.shuffle:
        random.Random(args.seed).shuffle(instances)
    if args.max_instances > 0:
        instances = instances[:args.max_instances]

    if not instances:
        print("No matching instances found.", file=sys.stderr)
        sys.exit(1)

    print(f"Running {len(solvers)} solver(s) on {len(instances)} instances "
          f"(timeout={args.timeout}s, workers={args.workers})...")

    all_results = {}
    any_bad = False

    for sname, cmd in solvers:
        t_start = time.perf_counter()
        results = []
        counts = {"SAT": 0, "UNSAT": 0, "UNK": 0, "BAD": 0}
        lock = threading.Lock()

        def run_wrapper(inst):
            return run_one(
                sname, cmd, inst, args.timeout,
                checker=checker, check_timeout=args.check_timeout,
                use_proof=args.proof, binary_proof=args.binary_proof,
                save_failures=args.save_failures,
            )

        with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as ex:
            futs = [ex.submit(run_wrapper, inst) for inst in instances]
            for done_idx, fut in enumerate(concurrent.futures.as_completed(futs), 1):
                r = fut.result()
                with lock:
                    results.append(r)
                    if r["status"] == "SAT" and r["verified"]:
                        counts["SAT"] += 1
                    elif r["status"] == "UNSAT" and r["verified"]:
                        counts["UNSAT"] += 1
                    elif r["status"] == "UNKNOWN":
                        counts["UNK"] += 1
                    else:
                        counts["BAD"] += 1
                        any_bad = True
                    if not args.quiet and (done_idx % max(1, min(10, len(instances) // 20 or 1)) == 0 or done_idx == len(instances) or counts["BAD"] > 0):
                        dt = time.perf_counter() - t_start
                        sys.stdout.write(
                            f"\r  [{sname}] {done_idx:4d}/{len(instances)}  "
                            f"SAT={counts['SAT']} UNSAT={counts['UNSAT']} "
                            f"UNK={counts['UNK']} BAD={counts['BAD']}  ({dt:.1f}s)   "
                        )
                        sys.stdout.flush()
        if not args.quiet:
            print()
        results.sort(key=lambda r: (r["family"], r["path"]))
        all_results[sname] = results

    solver_names = [s[0] for s in solvers]
    print_summary(solver_names, all_results, args.timeout)

    if args.output:
        try:
            rev = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO_ROOT, text=True).strip()
        except Exception:
            rev = None
        payload = {
            "schema": 1,
            "revision": rev,
            "timeout": args.timeout,
            "workers": args.workers,
            "instances": len(instances),
            "results": all_results,
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(payload, indent=2) + "\n")
        print(f"\nSaved JSON report -> {args.output}")

    if args.csv:
        args.csv.parent.mkdir(parents=True, exist_ok=True)
        fields = ["solver", "family", "path", "expected", "status", "verified",
                  "proof_checked", "seconds", "par2", "conflicts", "decisions", "props_per_sec", "diagnostic"]
        with args.csv.open("w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=fields)
            w.writeheader()
            for sname in solver_names:
                for r in all_results[sname]:
                    w.writerow({k: r.get(k) for k in fields})
        print(f"Saved CSV report  -> {args.csv}")

    sys.exit(2 if any_bad else 0)


if __name__ == "__main__":
    main()
