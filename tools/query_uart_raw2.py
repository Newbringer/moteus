#!/usr/bin/env python3
"""
Raw UART test with non-blocking reads to capture all responses.
"""

import serial
import time
import sys

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else '/dev/ttyUSB0'
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 460800
    
    print(f"Opening {port} at {baud} baud")
    
    ser = serial.Serial(
        port=port,
        baudrate=baud,
        timeout=0,  # Non-blocking!
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        xonxoff=False,
        rtscts=False,
        dsrdtr=False
    )
    
    time.sleep(0.05)
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    
    # Send command
    cmd = "can send 8001 11001F01130D BF\n"
    print(f"Sending: {cmd.strip()}")
    ser.write(cmd.encode('ascii'))
    ser.flush()
    
    # Collect all responses for 200ms
    all_data = b''
    start = time.time()
    
    while time.time() - start < 0.2:
        chunk = ser.read(1024)
        if chunk:
            all_data += chunk
            print(f"[{(time.time()-start)*1000:.1f}ms] Got {len(chunk)} bytes: {chunk!r}")
        time.sleep(0.001)
    
    ser.close()
    
    # Decode and display
    print("\n=== Complete Response ===")
    if all_data:
        decoded = all_data.decode('ascii', errors='replace')
        print(decoded)
        
        lines = decoded.split('\n')
        has_ok = any('OK' in line for line in lines)
        has_rcv = any('rcv' in line for line in lines)
        
        if has_ok and has_rcv:
            print("\n✓ SUCCESS: Got both OK and rcv")
            return 0
        elif has_ok:
            print("\n⚠ PARTIAL: Got OK but no rcv")
            return 1
        else:
            print("\n✗ FAILED: Unexpected response")
            return 1
    else:
        print("✗ FAILED: No response")
        return 1

if __name__ == '__main__':
    sys.exit(main())

