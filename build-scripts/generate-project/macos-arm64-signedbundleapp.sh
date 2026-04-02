#!/bin/bash

echo -e "\e[32m\nGenerating Xcode project for macOS Apple Silicon arm64...\e[0m"

cmake -S . -B build/macos/arm64 -G Xcode \
  -DCMAKE_SYSTEM_NAME=Darwin \
  -DGAME_BUILD_APPLE_CODE_SIGNING=ON

for build_type in Debug Release; do
  echo -e "\e[32m\nBuilding $build_type...\e[0m"
  cmake --build build/macos/arm64 --config $build_type
done

echo -e "\033[32m\nLib RC2D for macOS Apple Silicon (arm64) built in Debug and Release. Go to the build/macos/arm64 directory...\n\033[0m"
