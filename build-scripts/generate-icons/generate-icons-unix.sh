#!/bin/bash

# Vérifier si Cargo est installé
if ! command -v cargo &> /dev/null; then
    echo "Cargo n'est pas installé. Veuillez l'installer pour continuer."
    exit 1
fi

# Fonction pour créer un dossier s'il n'existe pas
create_directory_if_not_exists() {
    if [ ! -d "$1" ]; then
        mkdir -p "$1"
    fi
}

# Créer le dossier 'outputs' dans 'icons' s'il n'existe pas
create_directory_if_not_exists "icons/outputs"

# Exécuter la commande Tauri pour générer les icônes (1024x1024) à partir de l'image source
cargo tauri icon --output icons/outputs icons/app-icon-default.png

# Renommer et copier les fichiers icônes dans leurs dossiers respectifs
# Windows
cp icons/outputs/icon.ico icons/windows/app-icon.ico 2>/dev/null || echo "icon.ico non trouvé."
# macOS / iOS
cp icons/outputs/icon.icns icons/macos/app-icon.icns 2>/dev/null || echo "icon.icns non trouvé."
# Linux
cp icons/outputs/32x32.png icons/linux/app-icon-32x32.png 2>/dev/null || echo "32x32.png non trouvé."
cp icons/outputs/128x128.png icons/linux/app-icon-128x128.png 2>/dev/null || echo "128x128.png non trouvé."
cp icons/outputs/128x128@2x.png icons/linux/app-icon-256x256.png 2>/dev/null || echo "128x128@2x.png non trouvé."
cp icons/outputs/icon.png icons/linux/app-icon.png 2>/dev/null || echo "icon.png non trouvé."
# Android
cp -r icons/outputs/android/* android-project/app/src/main/res/ 2>/dev/null || echo "Icônes Android non trouvées."

echo -e "\e[32m Icons Windows, macOS, iOS et Android generer avec succès !\e[0m"