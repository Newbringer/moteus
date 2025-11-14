#!/usr/bin/env bash
set -euo pipefail

# Build the Docker image and run the firmware build inside the container,
# mounting the current workspace and Bazel cache from the host.
#
# Usage:
#   bash docker/build_firmware.sh [additional bazel args...]
# Examples:
#   bash docker/build_firmware.sh
#   MOTEUS_ENABLE_UART_PROTOCOL=1 bash docker/build_firmware.sh
#   MOTEUS_UART_ECHO=1 bash docker/build_firmware.sh
#   MOTEUS_UART_TOGGLE_TEST=1 bash docker/build_firmware.sh
#   bash docker/build_firmware.sh --define SOME_FLAG=value

IMAGE_NAME="${IMAGE_NAME:-moteus-bionic}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Where to place artifacts on the host
OUT_DIR_HOST="${REPO_ROOT}/out"
mkdir -p "${OUT_DIR_HOST}"

# Build the image (cached if unchanged)
echo "Building Docker image: ${IMAGE_NAME}"
docker build -t "${IMAGE_NAME}" -f "${REPO_ROOT}/docker/Dockerfile" "${REPO_ROOT}"

# Default to UART protocol enabled unless overridden by environment
UART_PROTO="${MOTEUS_ENABLE_UART_PROTOCOL:-1}"
if [[ "${UART_PROTO}" == "1" ]]; then
  UART_COPT="--copt=-DMOTEUS_ENABLE_UART_PROTOCOL"
else
  UART_COPT=""
fi

HOST_UID="$(id -u)"
HOST_GID="$(id -g)"

echo "Running container build..."
# Forward any extra args via env to avoid injecting empty quoted tokens.
EXTRA_BAZEL_ARGS="${*:-}"
docker run --rm -it \
  -e HOST_UID="${HOST_UID}" \
  -e HOST_GID="${HOST_GID}" \
  -e EXTRA_BAZEL_ARGS="${EXTRA_BAZEL_ARGS}" \
  -v "${REPO_ROOT}:/workspace" \
  -v "${HOME}/.cache/bazel:/root/.cache/bazel" \
  "${IMAGE_NAME}" \
  bash -lc "\
    ./tools/bazel build --config=target //fw:bin ${UART_COPT} \${EXTRA_BAZEL_ARGS} && \
    mkdir -p /workspace/out && \
    cp -f bazel-bin/fw/*.bin bazel-bin/fw/moteus.elf /workspace/out/ && \
    if [ -n \"\${HOST_UID:-}\" ] && [ -n \"\${HOST_GID:-}\" ]; then chown -R \"\${HOST_UID}:\${HOST_GID}\" /workspace/out; fi && \
    echo 'Artifacts copied to /workspace/out' \
  "

echo "Done. Artifacts are in: ${OUT_DIR_HOST}"


