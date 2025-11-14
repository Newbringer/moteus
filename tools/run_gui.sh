#!/usr/bin/env bash
set -euo pipefail

# Simple launcher for the moteus Python GUI over a serial port.
# Usage:
#   tools/run_gui.sh <serial-device>
#   MOTEUS_GUI_PORT=COM5 tools/run_gui.sh
#   MOTEUS_GUI_PORT=/dev/ttyUSB0 tools/run_gui.sh

PORT="${1:-${MOTEUS_GUI_PORT:-}}"

if [[ -z "${PORT}" ]]; then
  echo "Usage: tools/run_gui.sh <serial-device>"
  echo "       or set MOTEUS_GUI_PORT"
  exit 1
fi

# Use in-repo GUI and library (no external pip install required)
# utils/tview.py adjusts sys.path to include:
#   - utils/gui (moteus_gui package)
#   - lib/python (moteus package)
exec python3 utils/tview.py --force-transport fdcanusb --fdcanusb "${PORT}" --fdcanusb-baud "${MOTEUS_GUI_BAUD:-460800}"


