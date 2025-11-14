#!/usr/bin/env bash
set -euo pipefail

# Simple smoke test for the UART ASCII fdcanusb bridge.
# Sends a minimal command and expects an 'OK' response.
#
# Usage:
#   docker/test_uart.sh <serial-device> [baud]
#   UART_DEV=/dev/ttyUSB0 UART_BAUD=921600 docker/test_uart.sh
#
# Exit code 0 on success, non-zero on failure.

DEV="${1:-${UART_DEV:-}}"
BAUD="${2:-${UART_BAUD:-460800}}"
TIMEOUT_SEC="${TIMEOUT_SEC:-2}"

if [[ -z "${DEV}" ]]; then
  echo "Usage: docker/test_uart.sh <serial-device> [baud]" >&2
  exit 2
fi

python3 - "$DEV" "$BAUD" "$TIMEOUT_SEC" << 'PY'
import sys, time
try:
    import serial  # pyserial
except Exception as e:
    print("ERROR: pyserial not available (pip install pyserial)", file=sys.stderr)
    sys.exit(2)

dev = sys.argv[1]
baud = int(sys.argv[2])
timeout_s = float(sys.argv[3])

ser = serial.Serial(dev, baudrate=baud, timeout=timeout_s, write_timeout=1)
try:
    # Flush any stale input
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    # Minimal command that should elicit 'OK'
    ser.write(b"can send 0000 00\n")
    ser.flush()
    line = ser.readline().decode(errors="ignore").strip()
    if line != "OK":
        print(f"Unexpected response: '{line}'", file=sys.stderr)
        sys.exit(1)
    print("UART OK")
    sys.exit(0)
finally:
    ser.close()
PY


