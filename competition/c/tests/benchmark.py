#!/usr/bin/env python3
"""Reproducible serial benchmarks with independently verified answers.

Example:
  python3 tests/benchmark.py --checker /path/to/drat-trim \
    --solver 'bsat=bin/bsat --proof {proof} {input}' \
    --solver 'kissat=/path/to/kissat {input} {proof}' \
    --solver 'cadical=/path/to/cadical {input} {proof}' \
    --timeout 30 --repeats 3 --output results.json ../../dataset/medium_tests

Quote each solver template. Binaries and inputs are SHA-256 pinned in results.
Use a separate --split heldout run after tuning. Only checked SAT models and
verified UNSAT proofs count as solved. Timeouts, errors and unchecked answers
receive the PAR-2 penalty. Certificate checking is outside the solver timing.
Use --retain-unverified DIR to preserve unverified answer artifacts for retry.
"""
import argparse
import hashlib
import json
import os
import platform
import random
import resource
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from validate import checker_verified, model_valid, parse_cnf


def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1024*1024), b''):
            h.update(chunk)
    return h.hexdigest()


def retain_unverified(directory, config, command, temporary, result):
    """Keep exact evidence after timing, without upgrading an unchecked answer."""
    root = Path(directory).resolve()
    root.mkdir(parents=True, exist_ok=True)
    saved = Path(tempfile.mkdtemp(prefix='unverified-', dir=root))
    files = {}
    for name, source in [('input.cnf', Path(config['input'])),
                         *[(name, temporary/name) for name in
                           ('proof.drat', 'out', 'err', 'checker-out', 'checker-err')]]:
        if source.exists():
            target = saved/name
            shutil.copyfile(source, target)
            files[name] = dict(sha256=digest(target), bytes=target.stat().st_size)
    metadata = dict(schema=1, config=config, executed_command=command,
                    solver_sha256=digest(command[0]),
                    checker_sha256=digest(config['checker']) if config.get('checker') else None,
                    result=result, files=files)
    (saved/'run.json').write_text(json.dumps(metadata, indent=2)+'\n')
    return dict(directory=str(saved), metadata_sha256=digest(saved/'run.json'))


def worker(config):
    with tempfile.TemporaryDirectory(prefix='bsat-benchmark-') as tmp:
        tmp = Path(tmp)
        proof = tmp/'proof.drat'
        cmd = [s.replace('{input}', config['input']).replace('{proof}', str(proof)) for s in config['command']]
        if not any('{input}' in s for s in config['command']):
            cmd.append(config['input'])
        started = time.perf_counter()
        with (tmp/'out').open('w') as out, (tmp/'err').open('w') as err:
            proc = subprocess.Popen(cmd, stdout=out, stderr=err, start_new_session=True)
            try:
                code = proc.wait(timeout=config['timeout'])
            except subprocess.TimeoutExpired:
                import signal
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait()
                code = 0
        elapsed = time.perf_counter()-started
        usage = resource.getrusage(resource.RUSAGE_CHILDREN)
        peak = usage.ru_maxrss if sys.platform == 'darwin' else usage.ru_maxrss*1024
        output = (tmp/'out').read_text(errors='replace')
        status = 'SAT' if code == 10 else 'UNSAT' if code == 20 else 'UNKNOWN' if code == 0 else 'ERROR'
        verified = False
        diagnostic = ''
        if status == 'SAT':
            _, clauses = parse_cnf(Path(config['input']).read_text())
            verified = model_valid(clauses, output)
            if not verified:
                status = 'ERROR'
                diagnostic = 'invalid SAT model'
        elif status == 'UNSAT' and config['checker'] and proof.exists():
            try:
                checked = subprocess.run([config['checker'], config['input'], str(proof)], capture_output=True, text=True, timeout=config['check_timeout'])
                (tmp/'checker-out').write_text(checked.stdout)
                (tmp/'checker-err').write_text(checked.stderr)
                verified = checker_verified(checked)
                if not verified:
                    status = 'ERROR'
                    diagnostic = checked.stdout[-2000:] + checked.stderr[-2000:]
            except subprocess.TimeoutExpired as error:
                diagnostic = 'proof check timed out'
                for name, data in [('checker-out', error.stdout), ('checker-err', error.stderr)]:
                    (tmp/name).write_bytes(data.encode() if isinstance(data, str) else data or b'')
        stats = {}
        for line in output.splitlines():
            if line.startswith('c ') and ':' in line:
                name, value = line[2:].split(':', 1)
                stats[name.strip()] = value.strip()
        result = dict(status=status, verified=verified, seconds=elapsed,
                    proof_sha256=digest(proof) if proof.exists() else None,
                    cpu_seconds=usage.ru_utime+usage.ru_stime, peak_rss_bytes=peak,
                    par2=elapsed if verified else 2*config['timeout'], stats=stats,
                    diagnostic=diagnostic or (tmp/'err').read_text(errors='replace')[-2000:])
        if config.get('retain_unverified') and status != 'UNKNOWN' and not verified:
            result['artifacts'] = retain_unverified(config['retain_unverified'], config, cmd, tmp, result)
        return result


def verified_manifest(path):
    """Validate all selected bytes before launching any timed solver process."""
    path = path.resolve()
    raw = path.read_bytes()
    manifest = json.loads(raw)
    entries = manifest.get('inputs') if isinstance(manifest, dict) else None
    if not isinstance(entries, dict) or not entries:
        raise ValueError('manifest must contain a nonempty inputs object')
    inputs = {}
    for name, metadata in entries.items():
        candidate = Path(name)
        if not candidate.is_absolute():
            candidate = path.parent/candidate
        candidate = candidate.resolve()
        expected = metadata.get('sha256') if isinstance(metadata, dict) else None
        if not isinstance(expected, str) or len(expected) != 64 or any(c not in '0123456789abcdef' for c in expected.lower()):
            raise ValueError(f'manifest input lacks a valid SHA256: {name}')
        if str(candidate) in inputs:
            raise ValueError(f'duplicate resolved manifest input: {name}')
        actual = digest(candidate)
        if actual != expected.lower():
            raise ValueError(f'manifest input hash mismatch: {name}')
        inputs[str(candidate)] = dict(sha256=actual, family=candidate.parent.name)
    return inputs, dict(path=str(path), sha256=hashlib.sha256(raw).hexdigest())


def main():
    if len(sys.argv) == 2 and sys.argv[1] == '--worker':
        print(json.dumps(worker(json.load(sys.stdin))))
        return
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--solver', action='append', required=True, metavar='NAME=COMMAND')
    p.add_argument('--checker', help='drat-trim executable')
    p.add_argument('--timeout', type=float, default=30)
    p.add_argument('--check-timeout', type=float, default=60)
    p.add_argument('--retain-unverified', type=Path, help='Keep input, proof, outputs and metadata for unchecked answers/errors')
    p.add_argument('--repeats', type=int, default=3)
    p.add_argument('--seed', type=int, default=1)
    p.add_argument('--split', choices=('development','heldout'), default='development')
    p.add_argument('--output', type=Path, default=Path('benchmark.json'))
    p.add_argument('--manifest', type=Path, help='Hash-verified input manifest, instead of positional inputs')
    p.add_argument('inputs', nargs='*', type=Path)
    args = p.parse_args()
    if args.timeout <= 0 or args.check_timeout <= 0 or args.repeats < 1:
        p.error('timeouts and repeats must be positive')
    checker = shutil.which(args.checker) if args.checker else None
    if args.checker and not checker:
        p.error('checker not found')
    solvers = {}
    for spec in args.solver:
        name, cmd = spec.split('=',1)
        command = shlex.split(cmd)
        binary = shutil.which(command[0])
        if not binary or name in solvers:
            p.error(f'missing executable or duplicate solver name: {name}')
        command[0] = str(Path(binary).resolve())
        solvers[name] = dict(command=command, sha256=digest(binary))
    if args.manifest and args.inputs:
        p.error('use either --manifest or positional inputs')
    manifest_info = None
    if args.manifest:
        try:
            inputs, manifest_info = verified_manifest(args.manifest)
        except (ValueError, OSError) as error:
            p.error(str(error))
        paths = sorted(Path(name) for name in inputs)
    else:
        paths = sorted({p.resolve() for entry in args.inputs for p in (entry.rglob('*.cnf') if entry.is_dir() else [entry])})
        if not paths:
            p.error('no CNF files found')
        inputs = {str(p):dict(sha256=digest(p), family=p.parent.name) for p in paths}
    try:
        revision = subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
        dirty = bool(subprocess.check_output(['git','status','--porcelain'],text=True).strip())
    except subprocess.CalledProcessError:
        revision, dirty = None, None
    report = dict(schema=1, platform=platform.platform(), machine=platform.machine(),
                  revision=revision, dirty=dirty, split=args.split, seed=args.seed,
                  timeout=args.timeout, repeats=args.repeats, solvers=solvers, inputs=inputs,
                  checker=dict(path=checker, sha256=digest(checker)) if checker else None, runs=[])
    if manifest_info:
        report['manifest'] = manifest_info
    jobs = [(name, str(path), repeat) for name in solvers for path in paths for repeat in range(args.repeats)]
    random.Random(args.seed).shuffle(jobs)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    for name, path, repeat in jobs:
        config = dict(input=path, command=solvers[name]['command'], timeout=args.timeout,
                      checker=checker, check_timeout=args.check_timeout,
                      retain_unverified=str(args.retain_unverified.resolve()) if args.retain_unverified else None)
        run = subprocess.run([sys.executable, str(Path(__file__).resolve()), '--worker'],
                             input=json.dumps(config), capture_output=True, text=True)
        if run.returncode:
            raise SystemExit(run.stderr)
        result = dict(solver=name, input=path, repeat=repeat, **json.loads(run.stdout))
        report['runs'].append(result)
        args.output.write_text(json.dumps(report,indent=2)+'\n')
        print(f"{name}: {Path(path).name}: {result['status']} verified={result['verified']} {result['seconds']:.3f}s",flush=True)
    report['summary'] = {}
    for name in solvers:
        rows = [r for r in report['runs'] if r['solver']==name]
        report['summary'][name] = dict(verified_runs=sum(r['verified'] for r in rows), runs=len(rows),
                                      mean_par2=sum(r['par2'] for r in rows)/len(rows),
                                      max_rss_bytes=max(r['peak_rss_bytes'] for r in rows),
                                      errors=sum(r['status']=='ERROR' for r in rows))
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report['summary'],indent=2))
    if any(r['status']=='ERROR' for r in report['runs']):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
