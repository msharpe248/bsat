#!/usr/bin/env bash
# Run the authoritative Make target from any working directory.
set -euo pipefail
cd "$(dirname "$0")/.."
exec make test MODE="${MODE:-release}"
