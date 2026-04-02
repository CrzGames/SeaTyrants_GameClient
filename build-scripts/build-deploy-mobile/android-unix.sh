#!/bin/bash
set -euo pipefail

# ==================================================
# 🔧 CONFIG (À CHANGER SEULEMENT SI PROJET CHANGE)
# ==================================================
APP_COMPONENT="com.crzgames.testexe/.MyGame" # Package / Activity à lancer (à adapter à ton app)

# --------------------------------------------------
# Couleurs ANSI
# --------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# --------------------------------------------------
# Helpers
# --------------------------------------------------
die()  { echo -e "${RED}Erreur : $*${NC}" 1>&2; exit 1; }
info() { echo -e "${GREEN}$*${NC}"; }
warn() { echo -e "${YELLOW}Attention : $*${NC}"; }

# --------------------------------------------------
# Vérification environnement
# --------------------------------------------------

# JAVA_HOME obligatoire (Gradle tourne sur Java)
if [ -z "${JAVA_HOME:-}" ]; then
  die "JAVA_HOME n'est pas défini. Exemple macOS : export JAVA_HOME=/Library/Java/JavaVirtualMachines/temurin-17.jdk"
  die "Assure-toi d'avoir Java JDK 17 installé (Temurin) et que JAVA_HOME pointe vers le dossier racine du JDK."
fi

# ANDROID_HOME obligatoire (SDK requis par Gradle + adb)
if [ -z "${ANDROID_HOME:-}" ]; then
  die "ANDROID_HOME n'est pas défini. Exemple macOS : export ANDROID_HOME=/Users/<your-user>/Library/Android/sdk"
  die "Assure-toi d'avoir le SDK Android installé (via Android Studio) et que ANDROID_HOME pointe vers le dossier racine du SDK."
fi

ANDROID_HOME="${ANDROID_HOME%/}"

if [ ! -d "${ANDROID_HOME}" ]; then
  die "Le chemin Android SDK n'existe pas : ${ANDROID_HOME}"
fi

# adb obligatoire
if ! command -v adb >/dev/null 2>&1; then
  die "adb introuvable. Ajoute '${ANDROID_HOME}/platform-tools' au PATH ou installe platform-tools."
fi

# --------------------------------------------------
# Vérifier qu'un appareil est connecté et autorisé
# --------------------------------------------------
ADB_LIST="$(adb devices | sed -n '2,$p' | awk 'NF{print $1" "$2}')"

if echo "$ADB_LIST" | grep -q "unauthorized"; then
  die "Un appareil est détecté mais non autorisé (unauthorized). Accepte la popup USB debugging sur l'appareil puis réessaie."
fi

if echo "$ADB_LIST" | grep -q "offline"; then
  die "Un appareil est détecté mais offline. Débranche/rebranche l'USB (ou redémarre adb: adb kill-server && adb start-server)."
fi

if ! echo "$ADB_LIST" | grep -q " device$"; then
  die "Aucun appareil Android prêt ('device') n'est connecté. Lance un émulateur ou branche un téléphone (USB debugging)."
fi

info "Appareil détecté :"
echo "$ADB_LIST" | sed 's/^/  - /'

# --------------------------------------------------
# Variables projet
# --------------------------------------------------

# Dossier du projet Android
ANDROID_PROJECT_DIR="android-project"

# Gradle wrapper (dans android-project/)
GRADLE="./gradlew"

# Tags logcat (tu peux les changer)
LOG_TAGS=( "SDL:V" "SDL/APP:V" )

# --------------------------------------------------
# Aller dans le projet Android
# --------------------------------------------------
if [ ! -d "${ANDROID_PROJECT_DIR}" ]; then
  die "Dossier '${ANDROID_PROJECT_DIR}' introuvable."
fi

cd "${ANDROID_PROJECT_DIR}"

# Vérifier gradlew
if [ ! -f "gradlew" ]; then
  die "gradlew introuvable dans ${ANDROID_PROJECT_DIR}/"
fi
chmod +x ./gradlew

# --------------------------------------------------
# Clean + installDebug (build + install sur device)
# --------------------------------------------------
info "\nClean project (Gradle)..."
$GRADLE clean

info "\nInstall APK Debug sur l'appareil..."
$GRADLE installDebug

# --------------------------------------------------
# Nettoyer logcat
# --------------------------------------------------
info "\nNettoyage des logs logcat..."
adb logcat -c

# --------------------------------------------------
# Lancer l'application
# --------------------------------------------------
info "\nLancement de l'application : ${APP_COMPONENT}"
adb shell am start -n "${APP_COMPONENT}"

# --------------------------------------------------
# Afficher logcat filtré
# --------------------------------------------------
info "\nAffichage logcat (filtré) : ${LOG_TAGS[*]}"
adb logcat -s "${LOG_TAGS[@]}"