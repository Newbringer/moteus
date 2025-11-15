#!/usr/bin/env python3
import asyncio
import argparse
from moteus.transport_factory import get_singleton_transport

async def test():
    print("Creating transport...")
    parser = argparse.ArgumentParser()
    parser.add_argument('--fdcanusb', type=str, action='append')
    parser.add_argument('--fdcanusb-baud', type=int)
    parser.add_argument('--force-transport', type=str)
    parser.add_argument('--can-debug', type=str)
    parser.add_argument('--can-disable-brs', action='store_true')
    
    args = parser.parse_args(['--fdcanusb', '/dev/ttyUSB0', '--fdcanusb-baud', '460800'])
    
    t = get_singleton_transport(args)
    print(f"Transport created: {t}")
    print(f"Devices: {t._devices}")
    
    await asyncio.sleep(1)
    print("Done")

if __name__ == '__main__':
    asyncio.run(test())

