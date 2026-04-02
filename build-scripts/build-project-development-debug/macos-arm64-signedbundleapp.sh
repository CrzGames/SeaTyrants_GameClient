#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build/macos/arm64"
CACHE_FILE="${BUILD_DIR}/CMakeCache.txt"
TARGET="${1:-rc2d-game-template}"

if [[ ! -f "${CACHE_FILE}" ]]; then
  echo "[ERROR] Build directory not generated: ${BUILD_DIR}"
  echo "[INFO ] Run first: ./build-scripts/generate-project/macos-arm64-signedbundleapp.sh"
  exit 1
fi

if [[ $# -eq 0 ]]; then
  echo -e "\e[32m[INFO ] No target specified, using default target: ${TARGET}\e[0m"
fi

echo -e "\e[32m[INFO ] Rebuilding Debug configuration (macOS arm64, signing enabled)...\e[0m"
echo -e "\e[32m[INFO ] Building target ${TARGET} (Debug)...\e[0m"
cmake --build "${BUILD_DIR}" \
  --config Debug \
  --target "${TARGET}" \
  --parallel 8

echo -e "\e[32m[OK   ] Debug build completed.\e[0m"
