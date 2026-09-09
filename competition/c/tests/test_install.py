#!/usr/bin/env python3
"""Install into a temporary staging root and exercise downstream C/C++ clients."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
def main():
    p=argparse.ArgumentParser();p.add_argument('--cc',default='cc');p.add_argument('--mode',default='release');a=p.parse_args()
    root=Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='bsat-install-') as tmp:
        stage=Path(tmp);prefix=stage/'opt/bsat'
        subprocess.run(['make','-C',str(root),'install',f'CC={a.cc}',f'MODE={a.mode}','PREFIX=/opt/bsat',f'DESTDIR={stage}'],check=True)
        assert (prefix/'share/doc/bsat/THIRD_PARTY_NOTICES').read_bytes()==(root/'THIRD_PARTY_NOTICES').read_bytes()
        env=dict(os.environ,PKG_CONFIG_PATH=str(prefix/'lib/pkgconfig'))
        pkg=['pkg-config',f'--define-variable=prefix={prefix}']
        version=subprocess.check_output([*pkg,'--modversion','bsat'],env=env,text=True).strip()
        assert version==(root/'VERSION').read_text().strip()
        flags=shlex.split(subprocess.check_output([*pkg,'--cflags','--libs','bsat'],env=env,text=True))
        for lang in ['c','cpp']:
            cc=a.cc if lang=='c' else ('clang++' if 'clang' in a.cc else 'g++' if 'gcc' in a.cc else 'c++')
            command=[cc,'-std=c11' if lang=='c' else '-std=c++11']
            if a.mode=='debug':command+=['-fsanitize=address,undefined']
            # The old C header takes precedence over the freshly installed header.
            if lang=='c':command+=['-I'+str(root/'tests/abi/v1')]
            binary=stage/f'consumer-{lang}'
            command += [str(root/f'tests/abi/consumer.{lang}'),*flags,'-Wl,-rpath,'+str(prefix/'lib'),'-o',str(binary)]
            subprocess.run(command,check=True);subprocess.run([str(binary)],check=True)
        assert (prefix/'include/bsat.h').is_file() and (prefix/'bin/bsat').is_file()
        print('PASS: staged installation, pkg-config, frozen ABI-v1 C client and installed-header C++ client')

if __name__=='__main__':main()
