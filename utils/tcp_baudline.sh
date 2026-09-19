#!/usr/bin/env bash
netcat -q0 localhost 2323 | ./unpack_bits | \
sox -t raw -e unsigned-integer -b 8 -r 4800000 -c 1 - \
    -t raw -e signed-integer -b 16 -c 1 - \
    rate -v 160k highpass 20 | \
baudline -stdin -samplerate 160000 -channels 1 -format le16 -record
