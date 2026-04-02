#!/bin/bash
set -euo pipefail

# ==================================================
# 🔧 CONFIGURATION (À MODIFIER SI PROJET CHANGE)
# ==================================================
# Nom de la target CMake / exécutable
APP_NAME="rc2d-game-template"

# Bundle identifier iOS
BUNDLE_ID="com.crzgames.testexe"

# ==================================================
# 🎨 Colors
# ==================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

CONFIGURATION="Debug"

# ==================================================
# 📦 Args
# ==================================================
while [[ "$#" -gt 0 ]]; do
  case $1 in
    --config) CONFIGURATION="$2"; shift ;;
    *) echo -e "${RED}Erreur : Argument non reconnu : $1${NC}"; exit 1 ;;
  esac
  shift
done

# ==================================================
# 🏗 Build
# ==================================================
BUILD_DIR="./build/ios/iphoneos"

if [ ! -d "$BUILD_DIR" ]; then
  echo -e "${GREEN}Generating Xcode project for iOS (iphoneos)...${NC}"
  cmake -S . -B "$BUILD_DIR" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DGAME_BUILD_APPLE_CODE_SIGNING=ON
fi

echo -e "${GREEN}Cleaning and rebuilding the project...${NC}"
cmake --build "$BUILD_DIR" --target clean
cmake --build "$BUILD_DIR" --config "$CONFIGURATION"

APP_PATH="$(find "$BUILD_DIR/$CONFIGURATION" -name "$APP_NAME.app" -print -quit)"
if [ ! -d "$APP_PATH" ]; then
  echo -e "${RED}Erreur : Aucun fichier $APP_NAME.app trouvé.${NC}"
  exit 1
fi

# ==================================================
# 📱 Device detection (CoreDevice UUID)
# ==================================================
DEVICE_ID="$(
  xcrun devicectl list devices 2>/dev/null \
    | grep -i 'connected' \
    | grep -Eo '[0-9A-Fa-f-]{36}' \
    | head -n 1 || true
)"

if [ -z "$DEVICE_ID" ]; then
  echo -e "${RED}Aucun device iOS détecté.${NC}"
  echo -e "${RED}Tips: USB + déverrouille + Trust.${NC}"
  exit 1
fi

echo -e "${GREEN}Device: $DEVICE_ID${NC}"
echo -e "${GREEN}Bundle ID: $BUNDLE_ID${NC}"

# ==================================================
# 📲 Install
# ==================================================
echo -e "${GREEN}\nInstalling app...${NC}"
xcrun devicectl device install app --device "$DEVICE_ID" "$APP_PATH"

# ==================================================
# 🚀 Launch + Attach Console
# ==================================================
echo -e "${GREEN}\nLaunching app (attached console)...${NC}"

# Pattern pour filtrer les logs
LOG_PATTERN="$APP_NAME"

# Lancement de l'app avec attachement de la console, et filtrage des logs
xcrun devicectl device process launch \
  --device "$DEVICE_ID" \
  --console \
  "$BUNDLE_ID" 2>&1 \
  | grep -E --line-buffered "$LOG_PATTERN"