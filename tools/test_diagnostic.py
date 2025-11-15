#!/usr/bin/env python3
import asyncio
import sys
import os

# Add repo to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../lib/python'))

import moteus
from moteus.transport_factory import get_singleton_transport
import argparse

async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--fdcanusb', type=str, action='append')
    parser.add_argument('--fdcanusb-baud', type=int)
    parser.add_argument('--force-transport', type=str)
    parser.add_argument('--can-debug', type=str)
    parser.add_argument('--can-disable-brs', action='store_true')
    args = parser.parse_args(['--fdcanusb', '/dev/ttyUSB0', '--fdcanusb-baud', '460800'])
    
    transport = get_singleton_transport(args)
    c = moteus.Controller(id=1, transport=transport)
    s = moteus.Stream(c, verbose=True)
    
    print("Sending: tel stop")
    await s.write_message(b'tel stop')
    print("Flushing...")
    await s.flush_read(timeout=1.0)
    
    print("\nSending: tel list")
    try:
        result = await s.command(b'tel list', timeout=2.0)
        print(f"Result: {result}")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
    
    # Clean up
    for device in transport._devices:
        device.close()

if __name__ == '__main__':
    asyncio.run(main())

