#!/bin/bash
set -e

echo -e "\e[32m\nGenerating Xcode project for iOS (iphoneos, arm64)...\e[0m"

cmake -S . -B build/ios/iphoneos \
  -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS

for cfg in Debug Release; do
  echo -e "\e[32m\nBuilding $cfg (iphoneos/arm64)...\e[0m"
  cmake --build build/ios/iphoneos --config "$cfg" -- \
    CODE_SIGNING_ALLOWED=NO \
    CODE_SIGNING_REQUIRED=NO
done

echo -e "\033[32m \n Generate lib RC2D for iOS (iphoneos/arm64) to Release/Debug generated successfully, go to the build/ios/iphoneos directory... \n\033[0m"