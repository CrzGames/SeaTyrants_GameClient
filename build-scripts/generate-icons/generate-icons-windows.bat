@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM Vérifier si Cargo est installé
REM ============================================================
where cargo >nul 2>nul
if errorlevel 1 (
    echo Cargo n'est pas installe. Veuillez l'installer pour continuer.
    exit /b 1
)

REM ============================================================
REM Créer le dossier icons\outputs s'il n'existe pas
REM ============================================================
if not exist "icons\outputs" (
    mkdir "icons\outputs"
)

REM ============================================================
REM Générer les icônes via Tauri
REM ============================================================
cargo tauri icon --output icons\outputs icons\app-icon-default.png
if errorlevel 1 (
    echo Erreur lors de la generation des icones.
    exit /b 1
)

REM ============================================================
REM Windows
REM ============================================================
if exist "icons\outputs\icon.ico" (
    copy /Y "icons\outputs\icon.ico" "icons\windows\app-icon.ico" >nul
) else (
    echo icon.ico non trouve.
)

REM ============================================================
REM macOS / iOS
REM ============================================================
if exist "icons\outputs\icon.icns" (
    copy /Y "icons\outputs\icon.icns" "icons\macos\app-icon.icns" >nul
) else (
    echo icon.icns non trouve.
)

REM ============================================================
REM Linux
REM ============================================================
if exist "icons\outputs\32x32.png" (
    copy /Y "icons\outputs\32x32.png" "icons\linux\app-icon-32x32.png" >nul
) else (
    echo 32x32.png non trouve.
)

if exist "icons\outputs\128x128.png" (
    copy /Y "icons\outputs\128x128.png" "icons\linux\app-icon-128x128.png" >nul
) else (
    echo 128x128.png non trouve.
)

if exist "icons\outputs\128x128@2x.png" (
    copy /Y "icons\outputs\128x128@2x.png" "icons\linux\app-icon-256x256.png" >nul
) else (
    echo 128x128@2x.png non trouve.
)

if exist "icons\outputs\icon.png" (
    copy /Y "icons\outputs\icon.png" "icons\linux\app-icon.png" >nul
) else (
    echo icon.png non trouve.
)

REM ============================================================
REM Android (copie recursive des mipmap-*)
REM ============================================================
if exist "icons\outputs\android" (
    xcopy "icons\outputs\android\*" "android-project\app\src\main\res\" /E /I /Y >nul
) else (
    echo Icones Android non trouvees.
)

echo.
echo ===============================================
echo  Icons Windows, macOS, iOS et Android generes !
echo ===============================================
echo.

endlocal