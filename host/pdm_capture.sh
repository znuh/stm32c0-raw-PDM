#!/usr/bin/env bash

# This script was vibe-coded with Gemini Flash 3.7 Extended

set -euo pipefail

# Configuration
CAPTURE_TOOL="./pdm_capture"
TCP_PORT=2323
TCP_HOST="127.0.0.1"

GNURADIO_VIEWER="./tcp_live_view.py"
BAUDLINE_VIEWER="./tcp_baudline.sh"

LIVE_VIEWER=""

# Default flags
OUTPUT_FILE=""
ENABLE_TCP=false

usage() {
    echo "Usage: $0 [-f filename] [-t] [-h]"
    echo "  -f FILENAME  Save output to specified file"
    echo "  -b           Enable TCP server and start live baudline viewer"
    echo "  -g           Enable TCP server and start live GNUradio viewer"
    echo "  -h           Show this help message"
    exit 1
}

# Parse command line arguments
while getopts "f:bgh" opt; do
    case "${opt}" in
        f)
            OUTPUT_FILE="${OPTARG}"
            ;;
        b)
            ENABLE_TCP=true
            LIVE_VIEWER="${BAUDLINE_VIEWER}"
            ;;
        g)
            ENABLE_TCP=true
            LIVE_VIEWER="${GNURADIO_VIEWER}"
            ;;
        h|*)
            usage
            ;;
    esac
done

# 1. File existence check
if [[ -n "$OUTPUT_FILE" ]]; then
    if [[ -e "$OUTPUT_FILE" ]]; then
        echo "Error: Output file '$OUTPUT_FILE' already exists." >&2
        exit 1
    fi
fi

# 2. Function to launch viewer once port 2323 opens
wait_and_launch_viewer() {
    # Poll port every 100ms for up to 5 seconds
    for _ in {1..50}; do
        if (echo > "/dev/tcp/$TCP_HOST/$TCP_PORT") 2>/dev/null; then
            echo "TCP server listening on $TCP_HOST:$TCP_PORT. Starting $LIVE_VIEWER..." >&2
            sleep 0.5
            "$LIVE_VIEWER" &
            return 0
        fi
        sleep 0.1
    done
    echo "Error: Timed out waiting for TCP server on $TCP_HOST:$TCP_PORT." >&2
}

if [[ ! -e "$CAPTURE_TOOL" ]]; then
	make
	if [[ ! -e "$CAPTURE_TOOL" ]]; then
		echo "Compiling pdm_capture failed!"
		exit 1
	fi
fi

# Start port monitor in background if TCP mode is enabled
if [[ "$ENABLE_TCP" == true ]]; then
    wait_and_launch_viewer &
fi

# 3. Construct pipeline based on active options
if [[ -n "$OUTPUT_FILE" ]] && [[ "$ENABLE_TCP" == true ]]; then
    $CAPTURE_TOOL | tee "$OUTPUT_FILE" | socat-mux.sh TCP-LISTEN:$TCP_PORT,reuseaddr,bind=$TCP_HOST STDIN

elif [[ -n "$OUTPUT_FILE" ]]; then
    $CAPTURE_TOOL > "$OUTPUT_FILE"

elif [[ "$ENABLE_TCP" == true ]]; then
    $CAPTURE_TOOL | socat-mux.sh TCP-LISTEN:$TCP_PORT,reuseaddr,bind=$TCP_HOST STDIN

else
    $CAPTURE_TOOL
fi
