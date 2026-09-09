#!/usr/bin/env python3
"""Build an isolated pinned CaDiCaL with read-only public statistics access."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess

PIN='c60730422e758ef1cebe7aeddf2dda31c996bf04'
EXTENSION='''
// BSAT test-only extension, using the same Wrapper and public statistics API.
extern "C" int64_t bsat_reference_statistic (void *opaque, const char *name) {
  return ((CaDiCaL::Wrapper *) opaque)->solver->get_statistic_value (name);
}
'''

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();source=a.source.resolve();output=a.output.resolve()
    revision=subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
    assert revision==PIN,revision
    # Verify tracked input content too; a matching HEAD alone cannot identify a build.
    subprocess.run(['git','-C',str(source),'diff','--exit-code','HEAD','--','src','configure','makefile.in','scripts'],check=True)
    assert not output.exists(),'use a new output directory'
    shutil.copytree(source,output,ignore=shutil.ignore_patterns('.git','build'))
    target=output/'src/ccadical.cpp';target.write_text(target.read_text()+EXTENSION)
    subprocess.run(['./configure','-shared'],cwd=output,check=True)
    subprocess.run(['make','-j2'],cwd=output,check=True)
    (output/'diagnostic-extension.sha256').write_text(hashlib.sha256(EXTENSION.encode()).hexdigest()+'\n')

if __name__=='__main__':main()
