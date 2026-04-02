#!/bin/bash
set -euo pipefail

# ==================================================
# RC2D - Build Android (Unix: macOS/Linux)
# ==================================================
# Ce script :
#   1) Vérifie JAVA_HOME et ANDROID_HOME
#   2) Build APK (assembleDebug/assembleRelease)
#   3) Build AAB (bundleDebug/bundleRelease)
#   4) Exporte APK/AAB dans build/android/apk et build/android/aab
#   5) Récupère librc2d_static.a (AGP/CMake) et copie vers build/android/<ABI>/<Config>/
#
# Notes :
# - AGP utilise souvent "RelWithDebInfo" pour la config CMake du "Release"
# - Les .a sont dans : android-project/app/.cxx/<Config>/<hash>/<abi>/librc2d_static.a
# - ABIs ciblées : arm64-v8a + armeabi-v7a
# ==================================================

die() {
  echo "❌ $*" >&2
  exit 1
}

# --------------------------------------------------
# Vérification environnement
# --------------------------------------------------

# JAVA_HOME obligatoire (Gradle tourne sur Java)
if [ -z "${JAVA_HOME:-}" ]; then
  die "JAVA_HOME n'est pas défini. Exemple macOS : export JAVA_HOME=/Library/Java/JavaVirtualMachines/temurin-17.jdk/Contents/Home"
fi
if [ ! -d "${JAVA_HOME}" ]; then
  die "Le chemin JAVA_HOME n'existe pas : ${JAVA_HOME}"
fi

# ANDROID_HOME obligatoire (SDK requis par Gradle + adb)
if [ -z "${ANDROID_HOME:-}" ]; then
  die "ANDROID_HOME n'est pas défini. Exemple macOS : export ANDROID_HOME=\$HOME/Library/Android/sdk"
fi

ANDROID_HOME="${ANDROID_HOME%/}"

if [ ! -d "${ANDROID_HOME}" ]; then
  die "Le chemin Android SDK n'existe pas : ${ANDROID_HOME}"
fi

# --------------------------------------------------
# Build Android via Gradle + export des libs statiques CMake (.a)
# --------------------------------------------------

GRADLE="./gradlew"

# Sortie (alignée avec les autres scripts)
OUT_BASE="build/android"
OUT_APK_BASE="${OUT_BASE}/apk"
OUT_AAB_BASE="${OUT_BASE}/aab"

# --------------------------------------------------
# Aller dans le projet Android
# --------------------------------------------------
cd android-project

# Ensure wrapper exists
if [ ! -f "gradlew" ]; then
  die "gradlew introuvable dans android-project/"
fi
chmod +x ./gradlew

# --------------------------------------------------
# Clean Android project (Gradle)
# --------------------------------------------------
echo -e "\e[32m\n==================================================\nClean project (Gradle)...\n==================================================\e[0m"
$GRADLE clean

# --------------------------------------------------
# Build Gradle (APK)
# --------------------------------------------------
echo -e "\e[32m\n==================================================\nBuild APK Release (assembleRelease)...\n==================================================\e[0m"
$GRADLE assembleRelease

echo -e "\e[32m\n==================================================\nBuild APK Debug (assembleDebug)...\n==================================================\e[0m"
$GRADLE assembleDebug

# --------------------------------------------------
# Build Gradle (AAB)
# --------------------------------------------------
echo -e "\e[32m\n==================================================\nBuild AAB Release (bundleRelease)...\n==================================================\e[0m"
$GRADLE bundleRelease

echo -e "\e[32m\n==================================================\nBuild AAB Debug (bundleDebug)...\n==================================================\e[0m"
$GRADLE bundleDebug

# --------------------------------------------------
# Export APK / AAB vers build/android/apk et build/android/aab
# --------------------------------------------------
echo -e "\e[32m\n==================================================\nExport APK/AAB outputs...\n==================================================\e[0m"

mkdir -p "../${OUT_APK_BASE}/Debug" "../${OUT_APK_BASE}/Release"
mkdir -p "../${OUT_AAB_BASE}/Debug" "../${OUT_AAB_BASE}/Release"

APK_DEBUG="$(find "app/build/outputs" -type f -name "*debug*.apk" 2>/dev/null | head -n 1 || true)"
APK_RELEASE="$(find "app/build/outputs" -type f -name "*release*.apk" 2>/dev/null | head -n 1 || true)"

AAB_DEBUG="$(find "app/build/outputs" -type f -name "*debug*.aab" 2>/dev/null | head -n 1 || true)"
AAB_RELEASE="$(find "app/build/outputs" -type f -name "*release*.aab" 2>/dev/null | head -n 1 || true)"

if [ -n "$APK_DEBUG" ]; then
  cp -f "$APK_DEBUG" "../${OUT_APK_BASE}/Debug/rc2d-game-template-debug.apk"
  echo "APK Debug  -> ${OUT_APK_BASE}/Debug/rc2d-game-template-debug.apk"
else
  echo "⚠️  Aucun APK Debug trouvé dans app/build/outputs/"
fi

if [ -n "$APK_RELEASE" ]; then
  cp -f "$APK_RELEASE" "../${OUT_APK_BASE}/Release/rc2d-game-template-release.apk"
  echo "APK Release -> ${OUT_APK_BASE}/Release/rc2d-game-template-release.apk"
else
  echo "⚠️  Aucun APK Release trouvé dans app/build/outputs/"
fi

if [ -n "$AAB_DEBUG" ]; then
  cp -f "$AAB_DEBUG" "../${OUT_AAB_BASE}/Debug/rc2d-game-template-debug.aab"
  echo "AAB Debug  -> ${OUT_AAB_BASE}/Debug/rc2d-game-template-debug.aab"
else
  echo "⚠️  Aucun AAB Debug trouvé dans app/build/outputs/"
fi

if [ -n "$AAB_RELEASE" ]; then
  cp -f "$AAB_RELEASE" "../${OUT_AAB_BASE}/Release/rc2d-game-template-release.aab"
  echo "AAB Release -> ${OUT_AAB_BASE}/Release/rc2d-game-template-release.aab"
else
  echo "⚠️  Aucun AAB Release trouvé dans app/build/outputs/"
fi