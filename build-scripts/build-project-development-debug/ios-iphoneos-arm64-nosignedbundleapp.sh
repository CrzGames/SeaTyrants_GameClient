#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build/ios/iphoneos"
CACHE_FILE="${BUILD_DIR}/CMakeCache.txt"
TARGET="${1:-rc2d-game-template}"

if [[ ! -f "${CACHE_FILE}" ]]; then
  echo "[ERROR] Build directory not generated: ${BUILD_DIR}"
  echo "[INFO ] Run first: ./build-scripts/generate-project/ios-iphoneos-arm64-nosignedbundleapp.sh"
  exit 1
fi

if [[ $# -eq 0 ]]; then
  echo -e "\e[32m[INFO ] No target specified, using default target: ${TARGET}\e[0m"
fi

echo -e "\e[32m[INFO ] Rebuilding Debug configuration (iOS iphoneos arm64, no signing)...\e[0m"
echo -e "\e[32m[INFO ] Building target ${TARGET} (Debug)...\e[0m"
cmake --build "${BUILD_DIR}" \
  --config Debug \
  --target "${TARGET}" \
  --parallel 8 \
  -- \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO

echo -e "\e[32m[OK   ] Debug build completed.\e[0m"
