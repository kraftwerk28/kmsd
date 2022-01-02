#!/bin/bash
set -euo pipefail
sockfile=$1
for f in {0001..1000}; do
	cat <(echo "testfiles/file${f}.txt") <(echo "Hello, ${f}th world!") | \
	socat - UNIX-CONNECT:${sockfile} &
done
