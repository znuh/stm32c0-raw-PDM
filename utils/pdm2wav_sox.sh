#!/usr/bin/env bash
if [[ -z "$2" ]]; then
	echo "usage: $0 <input.pdm> <output.wav>"
else
	make && ./unpack_bits < $1 | sox -r 4800000 -t raw -e unsigned-integer -b 8 -c 1 - -b 16 $2 rate -h 160k
fi
