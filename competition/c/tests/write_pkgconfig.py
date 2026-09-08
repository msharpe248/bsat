"""Render package metadata without interpreting prefix/version as shell code."""
import argparse
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--template',type=Path,required=True)
p.add_argument('--prefix',required=True);p.add_argument('--version',required=True)
p.add_argument('--output',type=Path,required=True);a=p.parse_args()
if any(c in a.prefix+a.version for c in '\n\r\x00'):p.error('metadata contains a line break or NUL')
a.output.write_text(a.template.read_text().replace('@PREFIX@',a.prefix).replace('@VERSION@',a.version))
