#!/usr/bin/env python3
# Query moteus over the UART fdcanusb ASCII bridge.
#
# Usage:
#   python3 tools/query_uart.py --port /dev/ttyUSB0 --baud 460800 --id 1
#   python3 tools/query_uart.py --port COM5 --baud 460800 --id 1
#
# Optional:
#   --repeat 0.1   # poll at 10Hz

import argparse
import asyncio
import os
import sys
from typing import Dict, Any


def _add_repo_pythonpath():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.normpath(os.path.join(script_dir, '..'))
    sys.path.insert(0, os.path.join(repo_root, 'lib', 'python'))


async def _query_once(ctrl) -> Dict[str, Any]:
    # Query and present reply as a plain dict (works across versions)
    import moteus
    res = await ctrl.query()
    # Try to use helper if available
    try:
        from moteus.moteus import namedtuple_to_dict  # type: ignore
        return namedtuple_to_dict(res.values)
    except Exception:
        # Fallback: best-effort conversion
        values = getattr(res, 'values', res)
        try:
            return dict(values)
        except Exception:
            return {"values": values}


async def main():
    _add_repo_pythonpath()
    import moteus
    from moteus.transport_factory import make_transport_args, get_singleton_transport

    parser = argparse.ArgumentParser(description='Query moteus over UART fdcanusb transport')
    parser.add_argument('--port', required=True, help='Serial device path (e.g. /dev/ttyUSB0 or COM5)')
    parser.add_argument('--baud', type=int, default=460800, help='Serial baudrate (default 460800)')
    parser.add_argument('--id', type=int, default=1, help='moteus target ID (default 1)')
    parser.add_argument('--repeat', type=float, default=0.0, help='Repeat period in seconds (0 for single-shot)')
    # Add transport args and force fdcanusb
    make_transport_args(parser)
    args = parser.parse_args()
    args.force_transport = 'fdcanusb'
    args.fdcanusb = [args.port]
    args.fdcanusb_baud = args.baud

    transport = get_singleton_transport(args)
    ctrl = moteus.Controller(id=args.id, transport=transport)

    try:
        if args.repeat and args.repeat > 0:
            while True:
                values = await _query_once(ctrl)
                print(values)
                await asyncio.sleep(args.repeat)
        else:
            values = await _query_once(ctrl)
            print(values)
    finally:
        # Clean shutdown
        for device in transport._devices:
            device.close()
        await asyncio.sleep(0.1)  # Allow close to complete


if __name__ == '__main__':
    asyncio.run(main())


