#!/usr/bin/env python3
"""On-demand SAT benchmark corpus fetcher, normalizer, and hard synthetic generator.

Downloads and caches large public SAT corpora (SATLIB, SAT Competition 2023-2025
via Global Benchmark Database) or generates deterministic hard structured suites
under dataset/corpora/<name>/ (gitignored), with a manifest.json recording exact
file paths, problem families, SHA-256 hashes, and known SAT/UNSAT ground truth.

Examples:
  python3 competition/c/scripts/fetch_corpora.py list
  python3 competition/c/scripts/fetch_corpora.py fetch synthetic-hard
  python3 competition/c/scripts/fetch_corpora.py fetch satlib-core
  python3 competition/c/scripts/fetch_corpora.py fetch satcomp-2024 --limit 60 --max-mb 2
  python3 competition/c/scripts/fetch_corpora.py info satlib-core
"""
import argparse
import concurrent.futures
import gzip
import hashlib
import io
import json
import os
import random
import re
import shutil
import sys
import tarfile
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[2]
CORPORA_ROOT = REPO_ROOT / "dataset" / "corpora"

SATLIB_BASE = "https://www.cs.ubc.ca/~hoos/SATLIB/Benchmarks/SAT/"
GBD_BASE = "https://benchmark-database.de"

# Known SATLIB ground-truth labels for mixed DIMACS families where filenames
# do not directly encode satisfiability.
SATLIB_KNOWN_STATUS = {
    # BMC (Bounded Model Checking)
    "bmc-galileo-8": "SAT",
    "bmc-galileo-9": "SAT",
    "bmc-ibm-1": "SAT",
    "bmc-ibm-2": "SAT",
    "bmc-ibm-3": "SAT",
    "bmc-ibm-4": "SAT",
    "bmc-ibm-5": "SAT",
    "bmc-ibm-6": "SAT",
    "bmc-ibm-7": "SAT",
    "bmc-ibm-10": "SAT",
    "bmc-ibm-11": "SAT",
    "bmc-ibm-12": "SAT",
    "bmc-ibm-13": "SAT",
    # SSA (Circuit fault analysis)
    "ssa0432-003": "UNSAT",
    "ssa2670-130": "UNSAT",
    "ssa2670-141": "UNSAT",
    "ssa6288-047": "UNSAT",
    "ssa7552-038": "SAT",
    "ssa7552-158": "SAT",
    "ssa7552-159": "SAT",
    "ssa7552-160": "SAT",
    # QG (Quasigroup existence)
    "qg1-07": "SAT",
    "qg1-08": "SAT",
    "qg2-07": "SAT",
    "qg2-08": "SAT",
    "qg3-08": "SAT",
    "qg3-09": "UNSAT",
    "qg4-08": "UNSAT",
    "qg4-09": "SAT",
    "qg5-09": "UNSAT",
    "qg5-10": "UNSAT",
    "qg5-11": "SAT",
    "qg5-12": "UNSAT",
    "qg5-13": "UNSAT",
    "qg6-09": "SAT",
    "qg6-10": "UNSAT",
    "qg6-11": "UNSAT",
    "qg6-12": "UNSAT",
    "qg7-09": "SAT",
    "qg7-10": "UNSAT",
    "qg7-11": "UNSAT",
    "qg7-12": "UNSAT",
    "qg7-13": "SAT",
    # Dubois: dubois20..50 are UNSAT, while SATLIB's archived dubois100.cnf has
    # missing '0' terminators that merge pairs of clauses into tautologies (SAT).
    "dubois100": "SAT",
    # JNH (Known SAT/UNSAT from DIMACS benchmark documentation)
    "jnh1": "SAT", "jnh2": "UNSAT", "jnh3": "UNSAT", "jnh4": "UNSAT", "jnh5": "UNSAT",
    "jnh6": "UNSAT", "jnh7": "SAT", "jnh8": "UNSAT", "jnh9": "UNSAT", "jnh10": "UNSAT",
    "jnh11": "UNSAT", "jnh12": "SAT", "jnh13": "UNSAT", "jnh14": "UNSAT", "jnh15": "UNSAT",
    "jnh16": "UNSAT", "jnh17": "SAT", "jnh18": "UNSAT", "jnh19": "UNSAT", "jnh20": "UNSAT",
    "jnh201": "SAT", "jnh202": "UNSAT", "jnh203": "UNSAT", "jnh204": "SAT", "jnh205": "SAT",
    "jnh206": "UNSAT", "jnh207": "SAT", "jnh208": "UNSAT", "jnh209": "SAT", "jnh210": "SAT",
    "jnh211": "UNSAT", "jnh212": "SAT", "jnh213": "SAT", "jnh214": "UNSAT", "jnh215": "UNSAT",
    "jnh216": "UNSAT", "jnh217": "SAT", "jnh218": "SAT", "jnh219": "UNSAT", "jnh220": "SAT",
    "jnh301": "SAT", "jnh302": "UNSAT", "jnh303": "UNSAT", "jnh304": "UNSAT", "jnh305": "UNSAT",
    "jnh306": "UNSAT", "jnh307": "UNSAT", "jnh308": "UNSAT", "jnh309": "UNSAT", "jnh310": "UNSAT",
}

# (family_name, relative_url, default_expected_status)
SATLIB_PACKS = {
    "satlib-core": [
        ("uf150-645", "RND3SAT/uf150-645.tar.gz", "SAT"),
        ("uuf150-645", "RND3SAT/uuf150-645.tar.gz", "UNSAT"),
        ("flat100-239", "GCP/flat100-239.tar.gz", "SAT"),
        ("ais", "AIS/ais.tar.gz", "SAT"),
        ("logistics", "PLANNING/Logistics/logistics.tar.gz", "SAT"),
        ("blocksworld", "PLANNING/BlocksWorld/blocksworld.tar.gz", "SAT"),
        ("pigeon-hole", "DIMACS/PHOLE/pigeon-hole.tar.gz", "UNSAT"),
        ("parity", "DIMACS/PARITY/parity.tar.gz", "SAT"),
        ("dubois", "DIMACS/DUBOIS/dubois.tar.gz", "UNSAT"),
        ("pret", "DIMACS/PRET/pret.tar.gz", "UNSAT"),
        ("aim", "DIMACS/AIM/aim.tar.gz", "BY_NAME"),
        ("hanoi", "DIMACS/HANOI/hanoi.tar.gz", "SAT"),
        ("bmc", "BMC/bmc.tar.gz", "BY_TABLE"),
        ("bf", "DIMACS/BF/bf.tar.gz", "UNSAT"),
        ("ssa", "DIMACS/SSA/ssa.tar.gz", "BY_TABLE"),
        ("qg", "QG/QG.tar.gz", "BY_TABLE"),
    ],
    "satlib-hard": [
        ("uf200-860", "RND3SAT/uf200-860.tar.gz", "SAT"),
        ("uuf200-860", "RND3SAT/uuf200-860.tar.gz", "UNSAT"),
        ("uf250-1065", "RND3SAT/uf250-1065.tar.gz", "SAT"),
        ("uuf250-1065", "RND3SAT/uuf250-1065.tar.gz", "UNSAT"),
        ("flat200-479", "GCP/flat200-479.tar.gz", "SAT"),
        ("sw100-8-lp0", "SW-GCP/sw100-8-lp0-c5.tar.gz", "SAT"),
        ("sw100-8-lp1", "SW-GCP/sw100-8-lp1-c5.tar.gz", "SAT"),
        ("sw100-8-lp2", "SW-GCP/sw100-8-lp2-c5.tar.gz", "SAT"),
        ("sw100-8-lp3", "SW-GCP/sw100-8-lp3-c5.tar.gz", "SAT"),
        ("bejing", "Bejing/Bejing.tar.gz", "UNKNOWN"),
        ("gcp-large", "DIMACS/GCP/gcp-large.tar.gz", "UNKNOWN"),
    ],
    "satlib-scale": [
        ("uf50-218", "RND3SAT/uf50-218.tar.gz", "SAT"),
        ("uuf50-218", "RND3SAT/uuf50-218.tar.gz", "UNSAT"),
        ("uf100-430", "RND3SAT/uf100-430.tar.gz", "SAT"),
        ("uuf100-430", "RND3SAT/uuf100-430.tar.gz", "UNSAT"),
        ("cbs-n100-b10", "CBS/CBS_k3_n100_m403_b10.tar.gz", "SAT"),
        ("bms-n100", "BMS/BMS_k3_n100_m429.tar.gz", "SAT"),
        ("rti-n100", "BMS/RTI_k3_n100_m429.tar.gz", "SAT"),
        ("jnh", "DIMACS/JNH/jnh.tar.gz", "BY_TABLE"),
        ("inductive-inference", "DIMACS/II/inductive-inference.tar.gz", "SAT"),
    ],
}

SATCOMP_TRACKS = {
    "satcomp-2023": "main_2023",
    "satcomp-2024": "main_2024",
    "satcomp-2025": "main_2025",
}


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def normalize_satlib_dimacs(raw_bytes: bytes) -> bytes:
    """Normalize legacy 1990s SATLIB DIMACS quirks without altering any clause.

    Strips SATLIB's trailing '%' EOF marker and fixes legacy off-by-one clause
    counts in headers so strict DIMACS parsers (BSAT, drat-trim) accept every file.
    """
    text = raw_bytes.decode("latin-1")
    comments = []
    clauses = []
    cur = []
    header_vars = 0
    max_var = 0
    saw_header = False

    for line in text.splitlines():
        s = line.strip()
        if not s:
            continue
        if s.startswith("%"):
            break
        if s.startswith("c"):
            if not saw_header and len(comments) < 20:
                comments.append(s)
            continue
        if s.startswith("p"):
            parts = s.split()
            if len(parts) >= 4 and parts[1] == "cnf":
                header_vars = int(parts[2])
                saw_header = True
            continue
        for tok in s.split():
            if tok == "%":
                cur = []
                break
            val = int(tok)
            if val == 0:
                clauses.append(cur)
                cur = []
            else:
                av = abs(val)
                if av > max_var:
                    max_var = av
                cur.append(val)
        else:
            continue
        break

    if cur:
        clauses.append(cur)

    nvars = max(header_vars, max_var)
    out = io.StringIO()
    for c in comments:
        out.write(c + "\n")
    out.write(f"p cnf {nvars} {len(clauses)}\n")
    for cl in clauses:
        if cl:
            out.write(" ".join(str(x) for x in cl) + " 0\n")
        else:
            out.write("0\n")
    return out.getvalue().encode("ascii")


def infer_satlib_expected(stem: str, rule: str) -> str:
    low = stem.lower()
    if low in SATLIB_KNOWN_STATUS:
        return SATLIB_KNOWN_STATUS[low]
    if rule in ("SAT", "UNSAT", "UNKNOWN"):
        return rule
    if rule == "BY_NAME":
        if "-yes" in low or low.endswith("yes"):
            return "SAT"
        if "-no" in low or low.endswith("no"):
            return "UNSAT"
        return "UNKNOWN"
    if rule == "BY_TABLE":
        if low in SATLIB_KNOWN_STATUS:
            return SATLIB_KNOWN_STATUS[low]
        # Strip trailing extensions if double-dotted
        base = low.split(".")[0]
        return SATLIB_KNOWN_STATUS.get(base, "UNKNOWN")
    return "UNKNOWN"


def http_get(url: str, timeout: float = 30.0) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": "BSAT-Corpus-Fetcher/1.0"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read()


def http_head(url: str, timeout: float = 12.0):
    req = urllib.request.Request(url, method="HEAD", headers={"User-Agent": "BSAT-Corpus-Fetcher/1.0"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        cl = resp.headers.get("Content-Length")
        cd = resp.headers.get("Content-Disposition", "")
        size = int(cl) if cl and cl.isdigit() else None
        fname = None
        m = re.search(r'filename="?([^";\s]+)"?', cd)
        if m:
            fname = m.group(1)
        return size, fname


def fetch_satlib_pack(pack_name: str, force: bool = False) -> Path:
    dest = CORPORA_ROOT / pack_name
    if force and dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True, exist_ok=True)
    entries = []

    specs = SATLIB_PACKS[pack_name]
    print(f"Fetching SATLIB pack '{pack_name}' ({len(specs)} archives) -> {dest}")
    for family, rel_url, rule in specs:
        url = SATLIB_BASE + rel_url
        fam_dir = dest / family
        fam_dir.mkdir(parents=True, exist_ok=True)
        existing = sorted(fam_dir.glob("*.cnf"))
        if existing and not force:
            print(f"  [cached] {family}: {len(existing)} instances")
            for p in existing:
                entries.append({
                    "path": str(p.relative_to(dest)),
                    "family": family,
                    "expected": infer_satlib_expected(p.stem, rule),
                    "source": url,
                })
            continue

        print(f"  [download] {family} <- {url}", flush=True)
        try:
            data = http_get(url, timeout=60.0)
        except Exception as exc:
            print(f"    WARNING: failed to download {url}: {exc}", file=sys.stderr)
            continue

        count = 0
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tf:
            for member in tf.getmembers():
                if not member.isfile():
                    continue
                name = Path(member.name).name
                if not name.lower().endswith(".cnf"):
                    continue
                fobj = tf.extractfile(member)
                if fobj is None:
                    continue
                raw = fobj.read()
                try:
                    norm = normalize_satlib_dimacs(raw)
                except Exception as exc:
                    print(f"    WARNING: skipping unparseable {name}: {exc}", file=sys.stderr)
                    continue
                out_path = fam_dir / name
                out_path.write_bytes(norm)
                entries.append({
                    "path": str(out_path.relative_to(dest)),
                    "family": family,
                    "expected": infer_satlib_expected(out_path.stem, rule),
                    "source": url,
                })
                count += 1
        print(f"    extracted & normalized {count} instances")

    manifest_path = dest / "manifest.json"
    manifest = {
        "schema": 1,
        "corpus": pack_name,
        "description": f"SATLIB pack {pack_name}",
        "count": len(entries),
        "instances": sorted(entries, key=lambda e: (e["family"], e["path"])),
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Wrote {manifest_path} ({len(entries)} instances)")
    return dest


def fetch_satcomp_pack(pack_name: str, limit: int = 60, max_mb: float = 2.0,
                       custom_query: str = "", seed: int = 2026,
                       workers: int = 6, force: bool = False) -> Path:
    dest = CORPORA_ROOT / pack_name
    if force and dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True, exist_ok=True)
    max_bytes = int(max_mb * 1024 * 1024)

    track = SATCOMP_TRACKS.get(pack_name)
    queries = []
    if custom_query:
        queries.append((custom_query, "UNKNOWN", limit))
    elif track:
        half_sat = limit // 2
        half_unsat = limit - half_sat
        queries.append((f"track={track} and result=sat", "SAT", half_sat))
        queries.append((f"track={track} and result=unsat", "UNSAT", half_unsat))
    else:
        raise ValueError(f"Unknown SAT Competition pack: {pack_name}")

    print(f"Querying Global Benchmark Database for '{pack_name}' (limit={limit}, max_mb={max_mb})...")
    rng = random.Random(seed)
    selected = []  # (url, expected, size, filename)

    for q, expected, quota in queries:
        if quota <= 0:
            continue
        q_url = f"{GBD_BASE}/getinstances?query={urllib.parse.quote(q)}&context=cnf"
        text = http_get(q_url, timeout=30.0).decode("utf-8", errors="replace")
        urls = [line.strip().replace("http://", "https://", 1)
                for line in text.splitlines() if line.strip().startswith("http")]
        rng.shuffle(urls)
        print(f"  Query '{q}': {len(urls)} candidate URIs, probing sizes for up to {quota} <= {max_mb} MB...")

        picked = 0
        # Probe in parallel batches using HEAD so huge multi-GB files are never downloaded!
        batch_size = max(16, workers * 4)
        idx = 0
        while picked < quota and idx < len(urls):
            batch = urls[idx:idx + batch_size]
            idx += batch_size
            with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as ex:
                futs = {ex.submit(http_head, u): u for u in batch}
                for fut in concurrent.futures.as_completed(futs):
                    u = futs[fut]
                    try:
                        size, fname = fut.result()
                    except Exception:
                        continue
                    if size is not None and size <= max_bytes and picked < quota:
                        if not fname:
                            h = u.rstrip("/").split("/")[-1]
                            fname = f"{h}.cnf.xz"
                        selected.append((u, expected, size, fname))
                        picked += 1
        print(f"    selected {picked} instances ({expected})")

    entries = []

    def download_one(item):
        u, expected, size, fname = item
        fam = f"{pack_name}-{expected.lower()}"
        out_dir = dest / fam
        out_dir.mkdir(parents=True, exist_ok=True)
        out_path = out_dir / fname
        if not out_path.exists() or force:
            data = http_get(u, timeout=120.0)
            out_path.write_bytes(data)
        return {
            "path": str(out_path.relative_to(dest)),
            "family": fam,
            "expected": expected,
            "source": u,
            "compressed_bytes": out_path.stat().st_size,
        }

    print(f"  Downloading {len(selected)} instances with {workers} workers...", flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as ex:
        for i, res in enumerate(ex.map(download_one, selected), 1):
            entries.append(res)
            if i % 10 == 0 or i == len(selected):
                print(f"    [{i}/{len(selected)}] downloaded", flush=True)

    manifest_path = dest / "manifest.json"
    manifest = {
        "schema": 1,
        "corpus": pack_name,
        "description": f"SAT Competition GBD subset {pack_name} (max_mb={max_mb})",
        "count": len(entries),
        "instances": sorted(entries, key=lambda e: (e["family"], e["path"])),
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Wrote {manifest_path} ({len(entries)} instances)")
    return dest


# =====================================================================
# Offline Hard Structured Synthetic Suite Generator (Known Ground Truth)
# =====================================================================

def _write_cnf(path: Path, nvars: int, clauses: list, comment: str = ""):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as f:
        if comment:
            f.write(f"c {comment}\n")
        f.write(f"p cnf {nvars} {len(clauses)}\n")
        for cl in clauses:
            f.write(" ".join(str(x) for x in cl) + " 0\n")


def _add_xor_clauses(clauses: list, lits: list, rhs: int):
    """Encode XOR(lits) = rhs into 2^(k-1) clauses (split with Tseitin if k > 4)."""
    k = len(lits)
    for mask in range(1 << k):
        pop = bin(mask).count("1")
        if (pop & 1) != rhs:
            cl = [(-lits[i] if (mask >> i) & 1 else lits[i]) for i in range(k)]
            clauses.append(cl)


def _gen_php(pigeons: int, holes: int):
    nvars = pigeons * holes
    clauses = []
    for p in range(pigeons):
        clauses.append([p * holes + h + 1 for h in range(holes)])
    for h in range(holes):
        for p1 in range(pigeons):
            for p2 in range(p1 + 1, pigeons):
                clauses.append([-(p1 * holes + h + 1), -(p2 * holes + h + 1)])
    return nvars, clauses


def _gen_tseitin_xor(n_verts: int, degree: int, odd_charge: bool, rng: random.Random):
    """Tseitin formula on random d-regular-like graph with Tseitin chain splitting."""
    edges = []
    adj = [[] for _ in range(n_verts)]
    # Start with a Hamiltonian cycle so graph is connected, then add random chords.
    for u in range(n_verts):
        v = (u + 1) % n_verts
        eid = len(edges) + 1
        edges.append((u, v))
        adj[u].append(eid)
        adj[v].append(eid)
    attempts = 0
    while attempts < n_verts * degree * 4:
        attempts += 1
        u = rng.randrange(n_verts)
        v = rng.randrange(n_verts)
        if u == v or len(adj[u]) >= degree or len(adj[v]) >= degree:
            continue
        eid = len(edges) + 1
        edges.append((u, v))
        adj[u].append(eid)
        adj[v].append(eid)

    next_var = len(edges)
    clauses = []
    charges = [rng.randint(0, 1) for _ in range(n_verts)]
    parity = sum(charges) & 1
    if odd_charge and parity == 0:
        charges[0] ^= 1
    elif not odd_charge and parity == 1:
        charges[0] ^= 1

    for u in range(n_verts):
        vs = list(adj[u])
        rhs = charges[u]
        while len(vs) > 4:
            next_var += 1
            t = next_var
            _add_xor_clauses(clauses, [vs[0], vs[1], t], 0)
            vs = [t] + vs[2:]
        _add_xor_clauses(clauses, vs, rhs)
    return next_var, clauses


def _gen_mixed_xor_3sat(nvars: int, n_xors: int, n_3sat: int, sat: bool, rng: random.Random):
    clauses = []
    sol = [0] + [rng.randint(0, 1) for _ in range(nvars)]
    for _ in range(n_xors):
        vs = rng.sample(range(1, nvars + 1), 4)
        rhs = 0
        for v in vs:
            rhs ^= sol[v]
        if not sat and _ == 0:
            rhs ^= 1
        _add_xor_clauses(clauses, vs, rhs)
    for _ in range(n_3sat):
        vs = rng.sample(range(1, nvars + 1), 3)
        signs = [rng.choice((-1, 1)) for _ in range(3)]
        cl = [s * v for s, v in zip(signs, vs)]
        if sat:
            if not any((sol[v] == 1) == (s > 0) for s, v in zip(signs, vs)):
                cl[0] = -cl[0]
        clauses.append(cl)
    if not sat:
        # Guarantee UNSAT by adding a contradictory 4-XOR over the first 4 vars!
        vs = [1, 2, 3, 4]
        _add_xor_clauses(clauses, vs, 0)
        _add_xor_clauses(clauses, vs, 1)
    return nvars, clauses


def _gen_circuit_miter(n_inputs: int, n_gates: int, equivalent: bool, rng: random.Random):
    """Build two N-ary AND/XOR circuits; miter asserts out1 != out2."""
    clauses = []
    next_var = n_inputs

    def build_cone(specs, perturb=False):
        nonlocal next_var
        wire_map = list(range(1, n_inputs + 1))
        for idx, (gtype, ins, pols) in enumerate(specs):
            next_var += 1
            out = next_var
            in_lits = [wire_map[i] * p for i, p in zip(ins, pols)]
            if perturb and idx == len(specs) - 1:
                # Flip one input polarity on the final gate so cones differ!
                in_lits[0] = -in_lits[0]
            if gtype == "AND":
                # out <-> AND(in_lits)
                clauses.append([out] + [-l for l in in_lits])
                for l in in_lits:
                    clauses.append([-out, l])
            else:
                # out <-> XOR(a, b)
                a, b = in_lits[0], in_lits[1]
                clauses.append([-out, a, b])
                clauses.append([-out, -a, -b])
                clauses.append([out, -a, b])
                clauses.append([out, a, -b])
            wire_map.append(out)
        return wire_map[-1]

    specs = []
    for g in range(n_gates):
        pool_sz = n_inputs + g
        if rng.random() < 0.7:
            k = rng.randint(2, min(5, pool_sz))
            ins = rng.sample(range(pool_sz), k)
            pols = [rng.choice((-1, 1)) for _ in range(k)]
            specs.append(("AND", ins, pols))
        else:
            ins = rng.sample(range(pool_sz), 2)
            pols = [rng.choice((-1, 1)) for _ in range(2)]
            specs.append(("XOR", ins, pols))

    out1 = build_cone(specs, perturb=False)
    out2 = build_cone(specs, perturb=False)
    if not equivalent:
        # XOR a primary input into out2 so cones provably differ whenever x_1 = 1!
        next_var += 1
        out2_diff = next_var
        a, b = out2, 1
        clauses.append([-out2_diff, a, b])
        clauses.append([-out2_diff, -a, -b])
        clauses.append([out2_diff, -a, b])
        clauses.append([out2_diff, a, -b])
        out2 = out2_diff
    # Assert out1 != out2
    clauses.append([out1, out2])
    clauses.append([-out1, -out2])
    return next_var, clauses


def _gen_graph_coloring(n_nodes: int, colors: int, edge_prob: float, sat: bool, rng: random.Random):
    """k-coloring: SAT uses planted k-partition; UNSAT embeds a (k+1)-clique."""
    def vid(u, c):
        return u * colors + c + 1

    clauses = []
    for u in range(n_nodes):
        clauses.append([vid(u, c) for c in range(colors)])
        for c1 in range(colors):
            for c2 in range(c1 + 1, colors):
                clauses.append([-vid(u, c1), -vid(u, c2)])

    planted = [rng.randrange(colors) for _ in range(n_nodes)]
    edges = set()
    for u in range(n_nodes):
        for v in range(u + 1, n_nodes):
            if sat and planted[u] == planted[v]:
                continue
            if rng.random() < edge_prob:
                edges.add((u, v))
    if not sat:
        clique = rng.sample(range(n_nodes), colors + 1)
        for i in range(len(clique)):
            for j in range(i + 1, len(clique)):
                u, v = min(clique[i], clique[j]), max(clique[i], clique[j])
                edges.add((u, v))
    for u, v in sorted(edges):
        for c in range(colors):
            clauses.append([-vid(u, c), -vid(v, c)])
    return n_nodes * colors, clauses


def _gen_latin_square(n: int, clues: int, sat: bool, rng: random.Random):
    """N x N Latin Square completion (exact SAT or contradictory clue UNSAT)."""
    def vid(r, c, d):
        return (r * n + c) * n + d + 1

    clauses = []
    # Cell ALO + AMO
    for r in range(n):
        for c in range(n):
            clauses.append([vid(r, c, d) for d in range(n)])
            for d1 in range(n):
                for d2 in range(d1 + 1, n):
                    clauses.append([-vid(r, c, d1), -vid(r, c, d2)])
    # Row & column uniqueness
    for r in range(n):
        for d in range(n):
            for c1 in range(n):
                for c2 in range(c1 + 1, n):
                    clauses.append([-vid(r, c1, d), -vid(r, c2, d)])
    for c in range(n):
        for d in range(n):
            for r1 in range(n):
                for r2 in range(r1 + 1, n):
                    clauses.append([-vid(r1, c, d), -vid(r2, c, d)])

    # Generate a random valid Latin square via row/col/symbol permutations of cyclic square
    rp = list(range(n)); rng.shuffle(rp)
    cp = list(range(n)); rng.shuffle(cp)
    dp = list(range(n)); rng.shuffle(dp)
    grid = [[dp[(rp[r] + cp[c]) % n] for c in range(n)] for r in range(n)]

    cells = [(r, c) for r in range(n) for c in range(n)]
    rng.shuffle(cells)
    for r, c in cells[:clues]:
        clauses.append([vid(r, c, grid[r][c])])
    if not sat:
        # Force all values in row 0 to miss symbol 0 while requiring 0 somewhere in row 0
        # Simpler & subtle: fill row 0 cols 0..n-2 with symbols 0..n-2, AND col n-1 rows 1..n-1 with symbol n-1?
        # Even cleaner: force cell (0, 0) = grid[0][0] AND forbid grid[0][0] in column 0 except (0, 0) -> wait:
        # Simplest guaranteed UNSAT clue: place SAME digit in two cells of row 0 -> trivial.
        # Non-trivial UNSAT: place all n-1 OTHER digits in col 0 (rows 1..n-1) AND forbid the remaining digit in (0, 0)!
        clauses = clauses[:-clues]
        target_d = grid[0][0]
        # Force row 0 col c (c >= 1) to NOT have target_d, and col 0 row r (r >= 1) to NOT have target_d, AND (0,0) != target_d
        # Plus row 0 ALO for target_d!
        clauses.append([vid(0, c, target_d) for c in range(n)])
        for c in range(n):
            clauses.append([-vid(0, c, target_d)])
        # Wait: make it non-trivial by placing digits 1..n-1 in row 0 cols 1..n-1? No, better:
        # Build a clean Hall-violating Latin square: fix (n-1) cells in row 0 to digits 0..n-2,
        # AND fix cell (1, n-1) to digit n-1 -> forces (0, n-1) = n-1 AND (1, n-1) = n-1!
        # Let's make it multi-step: forbid digit d in row 0 for all d except {d1, d2} on 3 cells!
        clauses = []
        for r in range(n):
            for c in range(n):
                clauses.append([vid(r, c, d) for d in range(n)])
                for d1 in range(n):
                    for d2 in range(d1 + 1, n):
                        clauses.append([-vid(r, c, d1), -vid(r, c, d2)])
        for r in range(n):
            for d in range(n):
                clauses.append([vid(r, c, d) for c in range(n)])
                for c1 in range(n):
                    for c2 in range(c1 + 1, n):
                        clauses.append([-vid(r, c1, d), -vid(r, c2, d)])
        for c in range(n):
            for d in range(n):
                clauses.append([vid(r, c, d) for r in range(n)])
                for r1 in range(n):
                    for r2 in range(r1 + 1, n):
                        clauses.append([-vid(r1, c, d), -vid(r2, c, d)])
        # Hall obstruction: 3 cells in row 0 restricted to only 2 symbols (nontrivial multi-unit)!
        for c in range(3):
            for d in range(2, n):
                clauses.append([-vid(0, c, d)])
    return n * n * n, clauses


def _gen_rand3sat_planted(nvars: int, ratio: float, rng: random.Random):
    m = int(nvars * ratio)
    sol = [0] + [rng.randint(0, 1) for _ in range(nvars)]
    clauses = []
    for _ in range(m):
        vs = rng.sample(range(1, nvars + 1), 3)
        signs = [rng.choice((-1, 1)) for _ in range(3)]
        if not any((sol[v] == 1) == (s > 0) for s, v in zip(signs, vs)):
            flip = rng.randrange(3)
            signs[flip] = -signs[flip]
        clauses.append([s * v for s, v in zip(signs, vs)])
    return nvars, clauses


def generate_synthetic_hard(force: bool = False) -> Path:
    dest = CORPORA_ROOT / "synthetic-hard"
    if force and dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True, exist_ok=True)
    rng = random.Random(20261124)
    entries = []

    def emit(family: str, name: str, expected: str, nvars: int, clauses: list, desc: str):
        p = dest / family / f"{name}.cnf"
        _write_cnf(p, nvars, clauses, desc)
        entries.append({
            "path": str(p.relative_to(dest)),
            "family": family,
            "expected": expected,
            "source": "synthetic-hard",
            "vars": nvars,
            "clauses": len(clauses),
        })

    # 1. Pigeonhole Principle (SAT & UNSAT up to PHP(30, 29))
    for h in (8, 10, 12, 14, 16, 18, 20, 25, 30):
        nv, cls = _gen_php(h + 1, h)
        emit("php-unsat", f"php_{h+1}_{h}", "UNSAT", nv, cls, f"PHP({h+1},{h}) UNSAT")
    for h in (8, 10, 12, 14, 16, 18, 20):
        nv, cls = _gen_php(h, h)
        emit("php-sat", f"php_{h}_{h}", "SAT", nv, cls, f"PHP({h},{h}) SAT")

    # 2. Tseitin XOR on random regular graphs (SAT & UNSAT, 20..180 vertices)
    for idx, n_v in enumerate((20, 30, 40, 60, 80, 100, 120, 150, 180)):
        for odd, fam, exp in ((True, "tseitin-xor-unsat", "UNSAT"), (False, "tseitin-xor-sat", "SAT")):
            nv, cls = _gen_tseitin_xor(n_v, 4, odd, rng)
            emit(fam, f"tseitin_v{n_v}_{idx}_{exp.lower()}", exp, nv, cls, f"Tseitin XOR V={n_v} {exp}")

    # 3. Mixed underdetermined XOR + 3-SAT (tests live dynamic Bird/Han-Jiang Gauss!)
    for idx, nv in enumerate((60, 80, 100, 120, 140, 160)):
        for sat, fam, exp in ((True, "mixed-xor-sat", "SAT"), (False, "mixed-xor-unsat", "UNSAT")):
            v, cls = _gen_mixed_xor_3sat(nv, int(nv * 0.85), int(nv * 2.0), sat, rng)
            emit(fam, f"mixed_xor_n{nv}_{idx}_{exp.lower()}", exp, v, cls, f"Mixed XOR+3SAT N={nv} {exp}")

    # 4. Circuit Miter Equivalence Checking (N-ary AND + XOR DAGs, SAT & UNSAT)
    for idx, (nin, ng) in enumerate(((12, 40), (16, 60), (20, 90), (24, 120), (30, 160), (40, 220))):
        nv, cls = _gen_circuit_miter(nin, ng, True, rng)
        emit("circuit-miter-unsat", f"miter_eq_in{nin}_g{ng}_{idx}", "UNSAT", nv, cls, f"Identical circuit miter {nin}/{ng}")
        nv, cls = _gen_circuit_miter(nin, ng, False, rng)
        emit("circuit-miter-sat", f"miter_neq_in{nin}_g{ng}_{idx}", "SAT", nv, cls, f"Perturbed circuit miter {nin}/{ng}")

    # 5. Graph k-Coloring (SAT & clique-UNSAT)
    for idx, (n, k, ep) in enumerate(((40, 4, 0.25), (60, 4, 0.20), (80, 5, 0.18), (100, 5, 0.16), (120, 6, 0.15))):
        for rep in range(2):
            nv, cls = _gen_graph_coloring(n, k, ep, True, rng)
            emit("graph-coloring-sat", f"gcp_n{n}_k{k}_{idx}_{rep}", "SAT", nv, cls, f"Planted {k}-coloring N={n}")
            nv, cls = _gen_graph_coloring(n, k, ep, False, rng)
            emit("graph-coloring-unsat", f"gcp_clique_n{n}_k{k}_{idx}_{rep}", "UNSAT", nv, cls, f"Clique-UNSAT {k}-coloring N={n}")

    # 6. Latin Square Completion (SAT & Hall-UNSAT)
    for n in (6, 7, 8, 9, 10):
        for rep in range(2):
            nv, cls = _gen_latin_square(n, clues=n + rep * 2, sat=True, rng=rng)
            emit("latin-square-sat", f"latin_n{n}_{rep}", "SAT", nv, cls, f"Latin square N={n} SAT")
            nv, cls = _gen_latin_square(n, clues=0, sat=False, rng=rng)
            emit("latin-square-unsat", f"latin_hall_n{n}_{rep}", "UNSAT", nv, cls, f"Latin square Hall N={n} UNSAT")

    # 7. Phase-Transition Planted Random 3-SAT (tests ProbSAT + CDCL)
    for nv in (150, 200, 250, 300, 350):
        for rep in range(4):
            v, cls = _gen_rand3sat_planted(nv, 4.26, rng)
            emit("rand3sat-planted", f"r3sat_n{nv}_{rep}", "SAT", v, cls, f"Planted 3-SAT N={nv} ratio=4.26")

    manifest_path = dest / "manifest.json"
    manifest = {
        "schema": 1,
        "corpus": "synthetic-hard",
        "description": "Deterministic offline hard structured SAT/UNSAT suite (100% known ground truth)",
        "count": len(entries),
        "instances": sorted(entries, key=lambda e: (e["family"], e["path"])),
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Generated synthetic-hard corpus -> {dest} ({len(entries)} instances)")
    return dest


def index_repo_all() -> Path:
    dest = CORPORA_ROOT / "repo-all"
    dest.mkdir(parents=True, exist_ok=True)
    entries = []
    roots = [
        ("fixtures", REPO_ROOT / "competition" / "c" / "tests" / "fixtures"),
        ("medium_tests", REPO_ROOT / "dataset" / "medium_tests"),
        ("simple_tests", REPO_ROOT / "dataset" / "simple_tests"),
    ]
    for group, root in roots:
        if not root.exists():
            continue
        for p in sorted(root.rglob("*")):
            if not p.is_file():
                continue
            low = p.name.lower()
            if not (low.endswith(".cnf") or low.endswith(".cnf.gz") or low.endswith(".cnf.xz")):
                continue
            fam = f"{group}/{p.parent.name}"
            exp = "UNKNOWN"
            if "sat_known" in low or low.startswith("sat_v"):
                exp = "SAT"
            entries.append({
                "path": str(p.resolve()),
                "family": fam,
                "expected": exp,
                "source": "repo",
            })
    manifest_path = dest / "manifest.json"
    manifest = {
        "schema": 1,
        "corpus": "repo-all",
        "description": "All checked-in repository CNF fixtures and test suites",
        "count": len(entries),
        "instances": entries,
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Indexed repo-all corpus -> {manifest_path} ({len(entries)} instances)")
    return dest


def cmd_list():
    print("Available packs to fetch/generate:")
    packs = [
        ("synthetic-hard", "Offline", "~120", "PHP(8..30), Tseitin XOR, mixed XOR+3SAT, circuit miters, k-coloring, Latin squares, 3-SAT (100% ground truth)"),
        ("repo-all", "Offline", "~90", "All checked-in repo fixtures (competition/c/tests/fixtures + dataset/{simple,medium}_tests)"),
        ("satlib-core", "SATLIB (~2 MB)", "~490", "uf/uuf150, flat100, ais, logistics, blocksworld, php, parity, dubois, pret, aim, hanoi, bmc, bf, ssa, qg"),
        ("satlib-hard", "SATLIB (~5 MB)", "~920", "uf/uuf200, uf/uuf250, flat200, sw100-8-lp0..3, Bejing, gcp-large"),
        ("satlib-scale", "SATLIB (~15 MB)", "~5,100", "uf/uuf50 (2k), uf/uuf100 (2k), CBS (1k), BMS, RTI, jnh, inductive-inference"),
        ("satcomp-2023", "GBD (on-demand)", "default 60", "SAT Competition 2023 Main Track (balanced SAT/UNSAT with ground truth, size-filtered via --max-mb)"),
        ("satcomp-2024", "GBD (on-demand)", "default 60", "SAT Competition 2024 Main Track (balanced SAT/UNSAT with ground truth, size-filtered via --max-mb)"),
        ("satcomp-2025", "GBD (on-demand)", "default 60", "SAT Competition 2025 Main Track (balanced SAT/UNSAT with ground truth, size-filtered via --max-mb)"),
    ]
    print(f"{'Pack':16s} {'Source':16s} {'Count':11s} Description")
    print("-" * 96)
    for name, src, cnt, desc in packs:
        print(f"{name:16s} {src:16s} {cnt:11s} {desc}")

    print("\nLocally cached corpora in dataset/corpora/:")
    if not CORPORA_ROOT.exists():
        print("  (none yet — run: fetch_corpora.py fetch synthetic-hard satlib-core)")
        return
    found = False
    for m in sorted(CORPORA_ROOT.glob("*/manifest.json")):
        found = True
        data = json.loads(m.read_text())
        insts = data.get("instances", [])
        n_sat = sum(1 for x in insts if x.get("expected") == "SAT")
        n_unsat = sum(1 for x in insts if x.get("expected") == "UNSAT")
        n_unk = len(insts) - n_sat - n_unsat
        print(f"  {data['corpus']:18s} total={len(insts):5d}  SAT={n_sat:4d}  UNSAT={n_unsat:4d}  UNLABELED={n_unk:4d}")
    if not found:
        print("  (none yet)")


def cmd_info(corpus: str):
    m = CORPORA_ROOT / corpus / "manifest.json"
    if not m.exists():
        print(f"Error: corpus '{corpus}' not found at {m}", file=sys.stderr)
        sys.exit(1)
    data = json.loads(m.read_text())
    insts = data.get("instances", [])
    by_fam = {}
    for x in insts:
        f = x["family"]
        by_fam.setdefault(f, {"SAT": 0, "UNSAT": 0, "UNKNOWN": 0})
        by_fam[f][x.get("expected", "UNKNOWN")] += 1
    print(f"Corpus: {data['corpus']} ({len(insts)} instances) — {data.get('description', '')}")
    print(f"{'Family':28s} {'Total':>6s} {'SAT':>6s} {'UNSAT':>6s} {'Unlab':>6s}")
    print("-" * 58)
    for f, c in sorted(by_fam.items()):
        tot = c["SAT"] + c["UNSAT"] + c["UNKNOWN"]
        print(f"{f:28s} {tot:6d} {c['SAT']:6d} {c['UNSAT']:6d} {c['UNKNOWN']:6d}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("list", help="List available packs and locally cached corpora")

    pf = sub.add_parser("fetch", help="Fetch or generate one or more benchmark packs")
    pf.add_argument("packs", nargs="+", help="Pack names (e.g. synthetic-hard satlib-core satcomp-2024)")
    pf.add_argument("--limit", type=int, default=60, help="Max instances for satcomp-* packs (default: 60)")
    pf.add_argument("--max-mb", type=float, default=2.0, help="Max compressed MB per satcomp-* file (default: 2.0)")
    pf.add_argument("--query", default="", help="Custom GBD query for satcomp-* (e.g. 'family=cryptography')")
    pf.add_argument("--seed", type=int, default=2026, help="Random seed for sampling satcomp-* instances")
    pf.add_argument("--workers", type=int, default=6, help="Parallel download threads for GBD")
    pf.add_argument("--force", action="store_true", help="Re-download / regenerate even if cached")

    pi = sub.add_parser("info", help="Show family breakdown of a cached corpus")
    pi.add_argument("corpus", help="Cached corpus name")

    args = p.parse_args()
    if args.cmd == "list":
        cmd_list()
    elif args.cmd == "info":
        cmd_info(args.corpus)
    elif args.cmd == "fetch":
        for pack in args.packs:
            if pack == "synthetic-hard":
                generate_synthetic_hard(force=args.force)
            elif pack == "repo-all":
                index_repo_all()
            elif pack in SATLIB_PACKS:
                fetch_satlib_pack(pack, force=args.force)
            elif pack in SATCOMP_TRACKS or pack.startswith("satcomp-"):
                fetch_satcomp_pack(pack, limit=args.limit, max_mb=args.max_mb,
                                   custom_query=args.query, seed=args.seed,
                                   workers=args.workers, force=args.force)
            else:
                print(f"Error: unknown pack '{pack}'. Run 'list' to see available packs.", file=sys.stderr)
                sys.exit(1)


if __name__ == "__main__":
    main()
