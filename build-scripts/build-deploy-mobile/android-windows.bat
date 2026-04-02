@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ==================================================
REM 🔧 CONFIG (À CHANGER SEULEMENT SI PROJET CHANGE)
REM ==================================================
set "APP_COMPONENT=com.crzgames.testexe/.MyGame"

REM --------------------------------------------------
REM Couleurs (ANSI) : fonctionne sur Windows 10/11 si VT est activé.
REM Si ça n'affiche pas de couleurs, ça restera lisible quand même.
REM --------------------------------------------------
for /f %%A in ('echo prompt $E ^| cmd') do set "ESC=%%A"
set "RED=%ESC%[31m"
set "GREEN=%ESC%[32m"
set "YELLOW=%ESC%[33m"
set "NC=%ESC%[0m"

REM --------------------------------------------------
REM Helpers
REM --------------------------------------------------
set "ANDROID_PROJECT_DIR=android-project"
set "GRADLE=gradlew.bat"
set "LOG_TAGS=SDL:V SDL/APP:V"

call :info "Verification environnement..."

REM --------------------------------------------------
REM Vérification environnement
REM --------------------------------------------------

REM JAVA_HOME obligatoire
if "%JAVA_HOME%"=="" (
  call :die "JAVA_HOME n'est pas defini. Exemple : setx JAVA_HOME ""C:\Program Files\Eclipse Adoptium\jdk-17.0.18.8-hotspot"""
  call :die "Assure-toi d'avoir Java JDK 17 installé (Temurin) et que JAVA_HOME pointe vers le dossier racine du JDK."
)

REM ANDROID_HOME obligatoire
if "%ANDROID_HOME%"=="" (
  call :die "ANDROID_HOME n'est pas defini. Exemple : setx ANDROID_HOME ""C:\Users\<your-user>\AppData\Local\Android\Sdk"""
  call :die "Assure-toi d'avoir le SDK Android installé (via Android Studio) et que ANDROID_HOME pointe vers le dossier racine du SDK."
)

REM Retirer trailing backslash si présent
set "ANDROID_HOME=%ANDROID_HOME%"
if "%ANDROID_HOME:~-1%"=="\" set "ANDROID_HOME=%ANDROID_HOME:~0,-1%"

if not exist "%ANDROID_HOME%" (
  call :die "Le chemin Android SDK n'existe pas : %ANDROID_HOME%"
)

REM Vérifier adb
where adb >nul 2>&1
if errorlevel 1 (
  call :die "adb introuvable. Ajoute '%ANDROID_HOME%\platform-tools' au PATH."
)

REM --------------------------------------------------
REM Vérifier qu'un appareil est connecté et autorisé
REM --------------------------------------------------

REM unauthorized ?
adb devices | findstr /R /C:" unauthorized$" >nul 2>&1
if not errorlevel 1 (
  call :die "Un appareil est detecte mais non autorise (unauthorized). Accepte la popup USB debugging sur l'appareil puis reessaie."
)

REM offline ?
adb devices | findstr /R /C:" offline$" >nul 2>&1
if not errorlevel 1 (
  call :die "Un appareil est detecte mais offline. Debranche/rebranche l'USB (ou redemarre adb: adb kill-server && adb start-server)."
)

REM au moins un device prêt ?
adb devices | findstr /R /C:" device$" >nul 2>&1
if errorlevel 1 (
  call :die "Branche un telephone via USB ou accepte la popup 'Autoriser le debogage USB' sur l'appareil."
)

call :info "Appareil detecte :"
REM Afficher la liste sans l'entête
for /f "skip=1 tokens=1,2" %%A in ('adb devices') do (
  if not "%%A"=="" (
    echo   - %%A %%B
  )
)

REM --------------------------------------------------
REM Aller dans le projet Android
REM --------------------------------------------------
if not exist "%ANDROID_PROJECT_DIR%" (
  call :die "Dossier '%ANDROID_PROJECT_DIR%' introuvable."
)

pushd "%ANDROID_PROJECT_DIR%"

REM Vérifier gradlew
if not exist "%GRADLE%" (
  popd
  call :die "gradlew introuvable dans %ANDROID_PROJECT_DIR%\"
)

REM --------------------------------------------------
REM Clean + installDebug (build + install sur device)
REM --------------------------------------------------
call :info ""
call :info "Clean project (Gradle)..."
call "%GRADLE%" clean
if errorlevel 1 (
  popd
  call :die "Echec de 'gradlew clean'."
)

call :info ""
call :info "Install APK Debug sur l'appareil..."
call "%GRADLE%" installDebug
if errorlevel 1 (
  popd
  call :die "Echec de 'gradlew installDebug'."
)

popd

REM --------------------------------------------------
REM Nettoyer logcat
REM --------------------------------------------------
call :info ""
call :info "Nettoyage des logs logcat..."
adb logcat -c
if errorlevel 1 call :warn "Impossible de nettoyer logcat (adb logcat -c)."

REM --------------------------------------------------
REM Lancer l'application
REM --------------------------------------------------
call :info ""
call :info "Lancement de l'application : %APP_COMPONENT%"
adb shell am start -n "%APP_COMPONENT%"
if errorlevel 1 call :warn "Echec du lancement via am start (verifie APP_COMPONENT)."

REM --------------------------------------------------
REM Afficher logcat filtré
REM --------------------------------------------------
call :info ""
call :info "Affichage logcat (filtre) : %LOG_TAGS%"
adb logcat -s %LOG_TAGS%

REM --------------------------------------------------
REM Fin
REM --------------------------------------------------
exit /b 0

:die
echo %RED%Erreur : %~1%NC%
exit /b 1

:info
echo %GREEN%%~1%NC%
exit /b 0

:warn
echo %YELLOW%Attention : %~1%NC%
exit /b 0