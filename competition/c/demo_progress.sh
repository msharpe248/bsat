#!/bin/bash

# Demo script showing how to monitor solver progress via SIGUSR1
#
# Usage:
#   ./demo_progress.sh <input.cnf>
#
# While the solver is running, send SIGUSR1 to get progress updates:
#   kill -USR1 <pid>
#
# Or in another terminal:
#   watch -n 1 "ps aux | grep bsat | grep -v grep | awk '{print \$2}' | xargs -I {} kill -USR1 {}"

if [ $# -eq 0 ]; then
    echo "Usage: $0 <input.cnf>"
    echo ""
    echo "Example:"
    echo "  $0 ../../dataset/sat_competition2025/subgraph-isomorphism/5c29ccca91235475f762f4b106f804f2.cnf"
    echo ""
    echo "To monitor progress while running, open another terminal and run:"
    echo "  kill -USR1 \$(pgrep bsat)"
    echo ""
    echo "Or for continuous monitoring (every 2 seconds):"
    echo "  watch -n 2 \"pgrep bsat | xargs -I {} kill -USR1 {} 2>/dev/null\""
    exit 1
fi

INPUT="$1"

if [ ! -f "$INPUT" ]; then
    echo "Error: File not found: $INPUT"
    exit 1
fi

echo "Starting solver on: $INPUT"
echo "Process ID: $$"
echo ""
echo "To get progress updates, run in another terminal:"
echo "  kill -USR1 $$"
echo ""
echo "========================================="
echo ""

./bin/bsat "$INPUT"
