"""Bounded subprocess groups for solver/checker tooling (POSIX)."""
import os
import signal
import subprocess


def run_capture(command, timeout):
    child=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                           text=True,start_new_session=True)
    try:
        out,err=child.communicate(timeout=timeout)
        return subprocess.CompletedProcess(command,child.returncode,out,err)
    except BaseException:
        # Give wrappers a chance to clean up their own checker groups first.
        try:os.killpg(child.pid,signal.SIGTERM)
        except ProcessLookupError:pass
        try:child.communicate(timeout=1)
        except subprocess.TimeoutExpired:
            try:os.killpg(child.pid,signal.SIGKILL)
            except ProcessLookupError:pass
            child.communicate()
        raise


def interrupt_on_term(signum,frame):
    raise SystemExit(128+signum)
