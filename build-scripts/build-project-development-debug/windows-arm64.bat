@echo off
setlocal

set "BUILD_DIR=build\windows\arm64"
set "CACHE_FILE=%BUILD_DIR%\CMakeCache.txt"
set "TARGET=%~1"

if not exist "%CACHE_FILE%" (
  echo [ERROR] Build directory not generated: %BUILD_DIR%
  echo [INFO ] Run first: build-scripts\generate-project\windows-arm64.bat
  exit /b 1
)

if "%TARGET%"=="" (
  set "TARGET=rc2d-game-template"
  echo [INFO ] No target specified, using default target: %TARGET%
)

echo [INFO ] Rebuilding Debug configuration (Windows arm64)...
call :build_target "%TARGET%"
if errorlevel 1 exit /b 1

echo [OK   ] Debug build completed.
exit /b 0

:build_target
echo [INFO ] Building target %~1 (Debug)...
cmake --build "%BUILD_DIR%" --config Debug --target "%~1" --parallel 8
if errorlevel 1 (
  echo [ERROR] Debug build failed for target: %~1
  exit /b 1
)
exit /b 0
