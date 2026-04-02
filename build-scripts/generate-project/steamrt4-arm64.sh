#!/bin/bash

echo -e "\e[32m\nGenerating Unix Makefiles project for STEAMRT4 arm64...\e[0m"

for build_type in Debug Release; do
  echo -e "\e[32m\nBuilding $build_type...\e[0m"
  cmake -S . -B build/steamrt4/arm64/$build_type \
    -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=$build_type \
    -DRC2D_ARCH=arm64 \
    -DRC2D_PLATFORM=STEAMRT4
  cmake --build build/steamrt4/arm64/$build_type
done

# Final message
echo -e "\033[32m \n Generate lib RC2D for SteamRT4 arm64 in Release and Debug generated successfully, go to the build/steamrt4/arm64 directory... \n\033[0m"
