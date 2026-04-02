@echo off
setlocal EnableDelayedExpansion

REM ==================================================
REM RC2D - Build Android (Windows)
REM ==================================================
REM Ce script :
REM   1) Détecte la racine du repo (2 niveaux au-dessus du script)
REM   2) Vérifie JAVA_HOME et ANDROID_HOME
REM   3) Build APK (assembleDebug/assembleRelease)
REM   4) Build AAB (bundleDebug/bundleRelease)
REM   5) Exporte APK/AAB dans build\android\apk et build\android\aab
REM   6) Récupère librc2d_static.a (AGP/CMake) et copie vers build\android\<ABI>\<Config>\
REM
REM Notes :
REM - AGP utilise souvent "RelWithDebInfo" pour la config CMake du "Release"
REM - Les .a sont dans : android-project\app\.cxx\<Config>\<hash>\<abi>\librc2d_static.a
REM - ABIs ciblées : arm64-v8a + armeabi-v7a
REM ==================================================

REM --------------------------------------------------
REM Détection racine du repo :
REM Le .bat est dans build-scripts\generate-project\
REM Donc racine = ..\.. (Crzgames_RC2D)
REM --------------------------------------------------
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

for %%I in ("%SCRIPT_DIR%\..\..") do set "ROOT=%%~fI"

REM --------------------------------------------------
REM Vérification environnement
REM --------------------------------------------------

REM JAVA_HOME obligatoire
if "%JAVA_HOME%"=="" (
  call :die "JAVA_HOME n'est pas defini. Exemple : setx JAVA_HOME ""C:\Program Files\Eclipse Adoptium\jdk-17.0.18.8-hotspot"""
)
if not exist "%JAVA_HOME%" (
  call :die "Le chemin JAVA_HOME n'existe pas : %JAVA_HOME%"
)

REM ANDROID_HOME obligatoire
if "%ANDROID_HOME%"=="" (
  call :die "ANDROID_HOME n'est pas defini. Exemple : setx ANDROID_HOME ""C:\Users\<your-user>\AppData\Local\Android\Sdk"""
)

REM Retirer trailing backslash si présent
set "ANDROID_HOME=%ANDROID_HOME%"
if "%ANDROID_HOME:~-1%"=="\" set "ANDROID_HOME=%ANDROID_HOME:~0,-1%"

if not exist "%ANDROID_HOME%" (
  call :die "Le chemin Android SDK n'existe pas : %ANDROID_HOME%"
)

REM --------------------------------------------------
REM Variables
REM --------------------------------------------------
set "ANDROID_PROJECT=%ROOT%\android-project"
set "GRADLE=gradlew.bat"

set "OUT_BASE=%ROOT%\build\android"
set "OUT_APK_BASE=%OUT_BASE%\apk"
set "OUT_AAB_BASE=%OUT_BASE%\aab"

REM --------------------------------------------------
REM Vérifier que android-project existe
REM --------------------------------------------------
if not exist "%ANDROID_PROJECT%" (
  call :die "android-project introuvable : %ANDROID_PROJECT%"
)

REM --------------------------------------------------
REM Aller dans le projet Android
REM --------------------------------------------------
pushd "%ANDROID_PROJECT%" || call :die "Impossible d'entrer dans %ANDROID_PROJECT%"

REM Ensure wrapper exists
if not exist "%GRADLE%" (
  popd
  call :die "gradlew.bat introuvable dans %ANDROID_PROJECT%"
)

REM --------------------------------------------------
REM Clean Gradle
REM --------------------------------------------------
echo.
echo ==================================================
echo Clean project (Gradle)...
echo ==================================================
call "%GRADLE%" clean
if errorlevel 1 (
  popd
  exit /b 1
)

REM --------------------------------------------------
REM Build Gradle (APK)
REM --------------------------------------------------
echo.
echo ==================================================
echo Build APK Release (assembleRelease)...
echo ==================================================
call "%GRADLE%" assembleRelease
if errorlevel 1 (
  popd
  exit /b 1
)

echo.
echo ==================================================
echo Build APK Debug (assembleDebug)...
echo ==================================================
call "%GRADLE%" assembleDebug
if errorlevel 1 (
  popd
  exit /b 1
)

REM --------------------------------------------------
REM Build Gradle (AAB)
REM --------------------------------------------------
echo.
echo ==================================================
echo Build AAB Release (bundleRelease)...
echo ==================================================
call "%GRADLE%" bundleRelease
if errorlevel 1 (
  popd
  exit /b 1
)

echo.
echo ==================================================
echo Build AAB Debug (bundleDebug)...
echo ==================================================
call "%GRADLE%" bundleDebug
if errorlevel 1 (
  popd
  exit /b 1
)

REM --------------------------------------------------
REM Export APK / AAB vers build\android\apk et build\android\aab
REM --------------------------------------------------
echo.
echo ==================================================
echo Export APK/AAB outputs...
echo ==================================================

mkdir "%OUT_APK_BASE%\Debug" 2>nul
mkdir "%OUT_APK_BASE%\Release" 2>nul
mkdir "%OUT_AAB_BASE%\Debug" 2>nul
mkdir "%OUT_AAB_BASE%\Release" 2>nul

REM ---- Trouver 1 APK Debug/Release (premier trouvé) ----
set "APK_DEBUG="
for /R "app\build\outputs" %%F in (*debug*.apk) do (
  if not defined APK_DEBUG set "APK_DEBUG=%%F"
)

set "APK_RELEASE="
for /R "app\build\outputs" %%F in (*release*.apk) do (
  if not defined APK_RELEASE set "APK_RELEASE=%%F"
)

if defined APK_DEBUG (
  copy /Y "!APK_DEBUG!" "%OUT_APK_BASE%\Debug\rc2d-game-template-debug.apk" >nul
  echo APK Debug  -> %OUT_APK_BASE%\Debug\rc2d-game-template-debug.apk
) else (
  echo ^(warn^) Aucun APK Debug trouvé dans app\build\outputs\
)

if defined APK_RELEASE (
  copy /Y "!APK_RELEASE!" "%OUT_APK_BASE%\Release\rc2d-game-template-release.apk" >nul
  echo APK Release -> %OUT_APK_BASE%\Release\rc2d-game-template-release.apk
) else (
  echo ^(warn^) Aucun APK Release trouvé dans app\build\outputs\
)

REM ---- Trouver 1 AAB Debug/Release (premier trouvé) ----
set "AAB_DEBUG="
for /R "app\build\outputs" %%F in (*debug*.aab) do (
  if not defined AAB_DEBUG set "AAB_DEBUG=%%F"
)

set "AAB_RELEASE="
for /R "app\build\outputs" %%F in (*release*.aab) do (
  if not defined AAB_RELEASE set "AAB_RELEASE=%%F"
)

if defined AAB_DEBUG (
  copy /Y "!AAB_DEBUG!" "%OUT_AAB_BASE%\Debug\rc2d-game-template-debug.aab" >nul
  echo AAB Debug  -> %OUT_AAB_BASE%\Debug\rc2d-game-template-debug.aab
) else (
  echo ^(warn^) Aucun AAB Debug trouvé dans app\build\outputs\
)

if defined AAB_RELEASE (
  copy /Y "!AAB_RELEASE!" "%OUT_AAB_BASE%\Release\rc2d-game-template-release.aab" >nul
  echo AAB Release -> %OUT_AAB_BASE%\Release\rc2d-game-template-release.aab
) else (
  echo ^(warn^) Aucun AAB Release trouvé dans app\build\outputs\
)