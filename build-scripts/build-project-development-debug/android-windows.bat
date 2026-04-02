@echo off
setlocal EnableDelayedExpansion

set "CXX_BASE=android-project\app\.cxx\Debug"
set "TARGET=%~1"
set "FOUND=0"

if not exist "%CXX_BASE%" (
  echo [ERROR] Android CMake debug directory not generated: %CXX_BASE%
  echo [INFO ] Run first: build-scripts\generate-project\android-windows.bat
  exit /b 1
)

if "%TARGET%"=="" (
  set "TARGET=rc2d-game-template"
  echo [INFO ] No target specified, using default target: %TARGET%
)

if /I "%TARGET%"=="rc2d-game-template" (
  echo [INFO ] Android uses target 'main' for the app. Mapping rc2d-game-template -^> main.
  set "TARGET=main"
)

echo [INFO ] Rebuilding Debug configuration (Android Windows host)...

for /R "%CXX_BASE%" %%F in (CMakeCache.txt) do (
  set "FOUND=1"
  set "CMAKE_DIR=%%~dpF"
  if "!CMAKE_DIR:~-1!"=="\" set "CMAKE_DIR=!CMAKE_DIR:~0,-1!"

  echo [INFO ] Using Android CMake dir: !CMAKE_DIR!
  call :build_target "!CMAKE_DIR!" "%TARGET%"
  if errorlevel 1 exit /b 1
)

if "%FOUND%"=="0" (
  echo [ERROR] No Android CMake build directories found under: %CXX_BASE%
  echo [INFO ] Run first: build-scripts\generate-project\android-windows.bat
  exit /b 1
)

echo [OK   ] Debug build completed.
exit /b 0

:build_target
echo [INFO ] Building target %~2 (Debug)...
cmake --build "%~1" --target "%~2" --parallel 8
if errorlevel 1 (
  echo [ERROR] Debug build failed for target: %~2
  exit /b 1
)
exit /b 0
