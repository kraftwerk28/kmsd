#!/bin/bash
rm testfiles/*.txt
set -euo pipefail
bin="a.out"
make
export SOCKPATH="kmsd.sock"
./${bin} &
server_pid=$!
sleep 1
ls -l "$SOCKPATH"
for f in {01..10}; do
	cat <(echo "testfiles/file${f}.txt") <(echo "Hello, ${f}th world!") | \
	socat - "UNIX-CONNECT:${SOCKPATH}" &
done
sleep 1
kill -TERM $server_pid
