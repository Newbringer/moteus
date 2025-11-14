#!/usr/bin/env bash
set -euo pipefail

# Flash compiled firmware images using OpenOCD + ST-Link.
# Requires the three .bin files in ./out created by docker/build_firmware.sh.
#
# Usage:
#   docker/flash_firmware.sh
# Env overrides:
#   OPENOCD_ADAPTER_SPEED   default: 100
#   OPENOCD_INTERFACE_CFG   default: interface/stlink.cfg
#   OPENOCD_TARGET_CFG      default: target/stm32g4x.cfg
#   OUT_DIR                 default: <repo>/out
#   OPENOCD_MASS_ERASE      default: 1 (set to 0 to skip mass erase)
#   PROGRAM_OPTION_BYTES    default: 1 (set to 0 to skip forcing boot from user flash)
#
# Example with custom speed:
#   OPENOCD_ADAPTER_SPEED=50 docker/flash_firmware.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

OUT_DIR="${OUT_DIR:-${REPO_ROOT}/out}"
BIN_ISR="${OUT_DIR}/moteus.08000000.bin"
BIN_BOOT="${OUT_DIR}/moteus.0800c000.bin"
BIN_APP="${OUT_DIR}/moteus.08010000.bin"

ADAPTER_SPEED="${OPENOCD_ADAPTER_SPEED:-100}"
INTERFACE_CFG="${OPENOCD_INTERFACE_CFG:-interface/stlink.cfg}"
TARGET_CFG="${OPENOCD_TARGET_CFG:-target/stm32g4x.cfg}"
MASS_ERASE="${OPENOCD_MASS_ERASE:-1}"
PROGRAM_OPTION_BYTES="${PROGRAM_OPTION_BYTES:-1}"

for f in "${BIN_ISR}" "${BIN_BOOT}" "${BIN_APP}"; do
  if [[ ! -f "${f}" ]]; then
    echo "Missing firmware image: ${f}"
    echo "Build first: bash ${REPO_ROOT}/docker/build_firmware.sh"
    exit 1
  fi
done

# Build the OpenOCD command sequence
FLASH_CMDS="reset_config none separate; "
FLASH_CMDS+="halt; "
if [[ "${MASS_ERASE}" == "1" ]]; then
  FLASH_CMDS+="stm32l4x mass_erase 0; "
fi
FLASH_CMDS+="program ${BIN_ISR} verify 0x08000000; "
FLASH_CMDS+="program ${BIN_BOOT} verify 0x0800C000; "
FLASH_CMDS+="program ${BIN_APP} verify 0x08010000; "
if [[ "${PROGRAM_OPTION_BYTES}" == "1" ]]; then
  # Clear nSWBOOT0 so BOOT0 pin is ignored; boot from main flash.
  FLASH_CMDS+="stm32l4x option_write 0 0x20 0x00000000 0x04000000; "
  FLASH_CMDS+="stm32l4x option_load 0; "
fi
FLASH_CMDS+="reset run; exit"

set -x
openocd -f "${INTERFACE_CFG}" -f "${TARGET_CFG}" \
  -c "adapter speed ${ADAPTER_SPEED}" \
  -c "init" \
  -c "${FLASH_CMDS}"
set +x

echo "Flashing complete."


