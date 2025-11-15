#!/usr/bin/env python3
"""
Minimal raw UART query script - no moteus library dependencies.
Sends fdcanusb ASCII commands directly and prints responses.
"""

import serial
import time
import sys

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else '/dev/ttyUSB0'
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 460800
    device_id = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    
    print(f"Opening {port} at {baud} baud, talking to ID {device_id}")
    
    # Open serial port with timeout
    ser = serial.Serial(
        port=port,
        baudrate=baud,
        timeout=0.5,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        xonxoff=False,
        rtscts=False,
        dsrdtr=False
    )
    
    time.sleep(0.1)  # Let port stabilize
    
    # Flush any existing data
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    
    # Build the exact command the user specified
    # Format: "can send <id> <data> [flags]\n"
    cmd_id = f"{device_id | 0x8000:04X}"  # 8001 for device ID 1
    payload = "11001F01130D"
    flags = "BF"
    
    cmd = f"can send {cmd_id} {payload} {flags}\n"
    
    print(f"Sending: {cmd.strip()}")
    ser.write(cmd.encode('ascii'))
    ser.flush()
    
    # Read responses
    responses = []
    start_time = time.time()
    
    while time.time() - start_time < 1.0:
        line = ser.readline()
        if line:
            decoded = line.decode('ascii', errors='replace').strip()
            print(f"Received: {decoded}")
            responses.append(decoded)
            
            # If we got both OK and rcv, we're done
            if any('rcv' in r for r in responses) and any('OK' in r for r in responses):
                break
        else:
            # Small delay before retry
            time.sleep(0.01)
    
    ser.close()
    
    if not responses:
        print("ERROR: No response received!")
        return 1
    
    # Check if we got expected responses
    has_ok = any('OK' in r for r in responses)
    has_rcv = any('rcv' in r for r in responses)
    
    if has_ok and has_rcv:
        print("\nSUCCESS: Got both OK and rcv response")
        return 0
    elif has_ok:
        print("\nPARTIAL: Got OK but no rcv response")
        return 1
    else:
        print("\nFAILURE: Unexpected response pattern")
        return 1

if __name__ == '__main__':
    sys.exit(main())

