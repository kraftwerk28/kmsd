#!/bin/bash
set -euo pipefail
sockfile=${1:-"${XDG_RUNTIME_DIR}/kmsd.sock"}
for f in {001..100}; do
	cat <(echo "testfiles/file${f}.txt") <(echo "Hello, ${f}th world!") | \
	socat - UNIX-CONNECT:${sockfile} &
done
