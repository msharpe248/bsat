#!/usr/bin/env python3
"""Build pinned CakeML checker assembly, verifying upstream source hashes."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import urllib.request

REVISION='a36874a8b750b43fe4b385b8ddbf5b033e46a3fa'
HASHES={'basis_ffi.c':'8e30d84fdcb2177aa5571d7fa6661a2fae5ecfd56baa0ce49c65f9233a9f87cb',
        'cake_lpr.S':'2f3af32d55083839b3fa0e693afd817679c0b8944bef41def05a8b0ec72b7d4a',
        'cake_lpr_arm8.S':'95b64883edc0cb09feedbcb1ebec233e2490f5b458fdda9dc29c212ed916f00c',
        'README.md':'5bd6f5949181cb41065333491154bbac3550b6587170702e444101cee6cc825e'}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--directory',type=Path,required=True)
    p.add_argument('--source',type=Path,help='Use an existing checkout, with the same hash checks')
    p.add_argument('--cc',default='cc');args=p.parse_args();folder=args.directory.resolve();folder.mkdir(parents=True,exist_ok=True)
    machine=platform.machine().lower()
    if machine in ['arm64','aarch64']:assembly='cake_lpr_arm8.S'
    elif machine in ['x86_64','amd64']:assembly='cake_lpr.S'
    else:p.error(f'unsupported architecture {machine}')
    sources={}
    for name in ['basis_ffi.c',assembly,'README.md']:
        data=(args.source/name).read_bytes() if args.source else urllib.request.urlopen(
            f'https://raw.githubusercontent.com/tanyongkiam/cake_lpr/{REVISION}/{name}',timeout=60).read()
        digest=hashlib.sha256(data).hexdigest()
        if digest != HASHES[name]:
            raise ValueError(f'upstream hash mismatch: {name}')
        (folder/name).write_bytes(data);sources[name]=digest
    command=[args.cc,'-O2','-std=c99',str(folder/'basis_ffi.c'),str(folder/assembly),'-o',str(folder/'cake_lpr')]
    subprocess.run(command,check=True)
    record={'upstream_revision':REVISION,'sources':sources,'command':command,'platform':platform.platform(),
            'binary_sha256':hashlib.sha256((folder/'cake_lpr').read_bytes()).hexdigest(),
            'scope':'Build of upstream distributed verified checker assembly; not an independent replay of its HOL4/CakeML correctness proofs.'}
    (folder/'build.json').write_text(json.dumps(record,indent=2)+'\n')
    print(folder/'cake_lpr')


if __name__=='__main__':main()
