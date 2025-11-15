# UART Protocol Firmware - Build and Flash Guide

## Overview
This guide shows how to build and flash the moteus firmware with UART protocol support enabled.

## Prerequisites

Install required tools:
```bash
sudo apt install openocd binutils-arm-none-eabi gdb-multiarch
```

Connect an ST-Link programmer to:
- Your computer's USB port
- The moteus SWD debug port (6-pin JST ZH connector)

## Build Firmware

Navigate to the repository root and build:
```bash
cd /home/per/moteus
tools/bazel build --config=target //:target
```

This creates:
- `bazel-out/stm32g4-opt/bin/fw/moteus.elf` (main firmware with UART protocol enabled)
- `bazel-out/stm32g4-opt/bin/fw/can_bootloader.elf` (bootloader)

## Flash Firmware

### Option 1: Build and Flash in One Command
```bash
tools/bazel build --config=target //fw:flash
```

### Option 2: Flash Pre-built Firmware
```bash
./fw/flash.py
```

Or with explicit paths:
```bash
./fw/flash.py bazel-out/stm32g4-opt/bin/fw/moteus.elf bazel-out/stm32g4-opt/bin/fw/can_bootloader.elf
```

## Verify UART Communication

After flashing, test the UART protocol:

### Using moteus_tool
```bash
python3 -m moteus.moteus_tool --target 1 --fdcanusb /dev/ttyUSB0 --fdcanusb-baud 460800 --info
```

### Using tview GUI
```bash
python3 -m moteus_gui.tview --fdcanusb /dev/ttyUSB0 --fdcanusb-baud 460800 --target 1
```

## Hardware Setup

**UART Connection:**
- **TX (PC10)**: Connect to USB-UART adapter RX
- **RX (PC11)**: Connect to USB-UART adapter TX
- **GND**: Connect grounds together
- **Baud Rate**: 460800

**Important:** The firmware uses PC10/PC11 with no RS485 direction control pin (TTL mode).

## Troubleshooting

**No response from firmware:**
- Check UART TX/RX are not swapped
- Verify baud rate is set to 460800
- Ensure serial port has correct permissions (`/dev/ttyUSB0` typically requires `dialout` group)
- Check that the firmware was flashed successfully (LED should be breathing)

**Garbled output:**
- Baud rate mismatch - ensure `--fdcanusb-baud 460800` is specified
- Check if old firmware without UART protocol is still running

**Permission denied on /dev/ttyUSB0:**
```bash
sudo usermod -a -G dialout $USER
# Log out and back in for changes to take effect
```

## Macro Configuration

The firmware is built with `-DMOTEUS_ENABLE_UART_PROTOCOL` which:
- Enables the UART fdcanusb ASCII protocol server
- Disables debug UART output on PC10/PC11 to prevent pin contention
- Configures UART at 460800 baud (TTL mode, no RS485 direction pin)

## Additional Notes

- UART protocol is opt-in and does not affect CAN-FD functionality
- Both CAN-FD and UART can be used simultaneously (dual transport mode)
- The UART protocol implements the same fdcanusb ASCII protocol used by the fdcanusb USB adapter

