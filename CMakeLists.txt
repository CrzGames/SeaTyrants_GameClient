# CMake 3.28.0 est la version minimale requise pour ce projet
cmake_minimum_required(VERSION 3.28.0)

# ============================================================
# Project Configuration
# ============================================================
# Nom du projet : rc2d-game-template
#
# Langages :
#   - C   : cœur du moteur RC2D (C99)
#   - CXX : requis uniquement pour :
#       • Steamworks (SDK en C++) si RC2D_STEAMWORKS_SDK_ENABLED=ON
#         → compile src/RC2D/RC2D_steamworks.cpp
#       • Les exemples du projet RC2D (sources en C++)
#       • Le Game Template (sources en C++)
#
# Même si RC2D est majoritairement en C, le support C++ est
# nécessaire pour certaines intégrations, pour les exemples et pour le Game Template
# qui est en C++.
# ============================================================
project(rc2d-game-template
  LANGUAGES C CXX
)

# ============================================================
# Build Options RC2D (Feature toggles)
# ============================================================
# Toutes les options RC2D_* qui permettent d'activer/désactiver
# des modules, features ou intégrations SDK.
# ============================================================

# Toujours désactivé dans un game template
set(RC2D_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RC2D_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
# Le reste : défauts, mais la CI peut override avec -D
set(RC2D_EOS_SDK_ENABLED OFF CACHE BOOL "")
set(RC2D_STEAMWORKS_SDK_ENABLED OFF CACHE BOOL "")
set(RC2D_GPU_SHADER_HOT_RELOAD_ENABLED ON CACHE BOOL "") # CONSEILLE: Pour la production il est recommandé de le désactiver.
set(RC2D_MEMORY_DEBUG_ENABLED ON CACHE BOOL "")          # CONSEILLE: Pour la production il est recommandé de le désactiver.
set(RC2D_DATA_MODULE_ENABLED ON CACHE BOOL "")
set(RC2D_NET_MODULE_ENABLED ON CACHE BOOL "")
set(RC2D_VIDEO_MODULE_ENABLED ON CACHE BOOL "")
set(RC2D_ONNX_MODULE_ENABLED ON CACHE BOOL "")
set(RC2D_ASSERT_LEVEL 3 CACHE STRING "")
set(RC2D_BUILDING_SHARED OFF CACHE BOOL "")

# ============================================================
# Build Options Game Template
# ============================================================
option(GAME_BUILD_APPLE_CODE_SIGNING "Activer ou désactiver le code signing pour les builds Apple (iOS/macOS)." OFF)

# ============================================================
# Application Metadata
#
# Définit les métadonnées de l'application (version, nom de l'entreprise, description, droits d'auteur, etc.)
# en utilisant des variables d'environnement si elles sont définies, ou des valeurs par défaut sinon.
#
# C'est utiliser pour générer dynamiquement le fichier de ressources Windows (.rc) ou les Info.plist pour Apple, 
# afin d'inclure ces métadonnées dans l'exécutable final.
# ============================================================

if(DEFINED ENV{APP_NAME})
  set(APP_NAME "$ENV{APP_NAME}" CACHE STRING "Nom de l'application")
else()
  set(APP_NAME "rc2d-game-template" CACHE STRING "Nom de l'application")
endif()

# Version "numérique" strict (SemVer) -> pour FILEVERSION/PRODUCTVERSION (Windows) et CFBundleShortVersionString (iOS/macOS).
if(DEFINED ENV{APP_VERSION_NUMERIC})
  set(APP_VERSION_NUMERIC "$ENV{APP_VERSION_NUMERIC}" CACHE STRING "Version numerique (major.minor.patch)")
else()
  set(APP_VERSION_NUMERIC "1.0.0" CACHE STRING "Version numerique (major.minor.patch)")
endif()

# Version "string" libre -> visible dans les propriétés Windows, Android, Linux(AppImage).
if(DEFINED ENV{APP_VERSION_STR})
  set(APP_VERSION_STR "$ENV{APP_VERSION_STR}" CACHE STRING "Version string (peut contenir branch/sha)")
else()
  set(APP_VERSION_STR "1.0.0-staging-commitshashort" CACHE STRING "Version string (peut contenir branch/sha)")
endif()

# Découper la version en parties
string(REPLACE "." ";" VERSION_LIST ${APP_VERSION_NUMERIC})
list(GET VERSION_LIST 0 APP_VERSION_MAJOR)
list(GET VERSION_LIST 1 APP_VERSION_MINOR)
list(GET VERSION_LIST 2 APP_VERSION_PATCH)

if(DEFINED ENV{APP_COMPANY_NAME})
  set(APP_COMPANY_NAME "$ENV{APP_COMPANY_NAME}" CACHE STRING "Nom de l'entreprise")
else()
  set(APP_COMPANY_NAME "CrzGames" CACHE STRING "Nom de l'entreprise")
endif()

if(DEFINED ENV{APP_GAME_DESCRIPTION})
  set(APP_GAME_DESCRIPTION "$ENV{APP_GAME_DESCRIPTION}" CACHE STRING "Game description")
else()
  set(APP_GAME_DESCRIPTION "MyGame - MMORPG" CACHE STRING "Game description")
endif()

if(DEFINED ENV{APP_LEGAL_COPYRIGHT})
  set(APP_LEGAL_COPYRIGHT "$ENV{APP_LEGAL_COPYRIGHT}" CACHE STRING "Droits d'auteur")
else()
  set(APP_LEGAL_COPYRIGHT "Copyright 2026 CrzGames" CACHE STRING "Droits d'auteur")
endif()

if(DEFINED ENV{APP_IOSMACOS_BUILD_VERSION})
  set(APP_IOSMACOS_BUILD_VERSION "$ENV{APP_IOSMACOS_BUILD_VERSION}" CACHE STRING "Build version")
else()
  set(APP_IOSMACOS_BUILD_VERSION "1" CACHE STRING "Build version")
endif()

if(DEFINED ENV{APP_IOSMACOS_IDENTIFIER})
  set(APP_IOSMACOS_IDENTIFIER "$ENV{APP_IOSMACOS_IDENTIFIER}" CACHE STRING "Identifiant de l'application")
else()
  set(APP_IOSMACOS_IDENTIFIER "com.crzgames.testexe" CACHE STRING "Identifiant de l'application")
endif()

if(DEFINED ENV{APP_IOSMACOS_DEVELOPMENT_TEAM_ID})
  set(APP_IOSMACOS_DEVELOPMENT_TEAM_ID "$ENV{APP_IOSMACOS_DEVELOPMENT_TEAM_ID}" CACHE STRING "Apple Development Team ID")
else()
  set(APP_IOSMACOS_DEVELOPMENT_TEAM_ID "U3D28WJ8DV" CACHE STRING "Apple Development Team ID")
endif()

if(DEFINED ENV{APP_IOS_PROVISIONING_PROFILE_NAME})
  set(APP_IOS_PROVISIONING_PROFILE_NAME "$ENV{APP_IOS_PROVISIONING_PROFILE_NAME}" CACHE STRING "Nom du profil de provisioning Apple")
else()
  set(APP_IOS_PROVISIONING_PROFILE_NAME "TestExe" CACHE STRING "Nom du profil de provisioning Apple")
endif()

if(DEFINED ENV{APP_IOS_PROVISIONING_PROFILE_UUID})
  set(APP_IOS_PROVISIONING_PROFILE_UUID "$ENV{APP_IOS_PROVISIONING_PROFILE_UUID}" CACHE STRING "UUID du profil de provisioning Apple")
else()
  set(APP_IOS_PROVISIONING_PROFILE_UUID "d5115451-c438-4aa6-9c68-dee8a87a36e8" CACHE STRING "UUID du profil de provisioning Apple")
endif()

if(DEFINED ENV{APP_IOS_CODE_SIGN_IDENTITY})
  set(APP_IOS_CODE_SIGN_IDENTITY "$ENV{APP_IOS_CODE_SIGN_IDENTITY}" CACHE STRING "Identité de code signing Apple iOS")
else()
  set(APP_IOS_CODE_SIGN_IDENTITY "Apple Development: Corentin Recanzone (2X96T6AM4K)" CACHE STRING "Identité de code signing Apple iOS")
endif()

if(DEFINED ENV{APP_MACOS_CODE_SIGN_IDENTITY})
  set(APP_MACOS_CODE_SIGN_IDENTITY "$ENV{APP_MACOS_CODE_SIGN_IDENTITY}" CACHE STRING "Identité de code signing Apple macOS")
else()
  set(APP_MACOS_CODE_SIGN_IDENTITY "Developer ID Application: Corentin Recanzone (U3D28WJ8DV)" CACHE STRING "Identité de code signing Apple macOS")
endif()

# ============================================================
# Build Environment (dev / staging / production)
# ============================================================
set(GAME_ENV "dev" CACHE STRING "Build environment: dev, staging or production")
set_property(CACHE GAME_ENV PROPERTY STRINGS "dev" "staging" "production")
if(NOT GAME_ENV STREQUAL "dev"
   AND NOT GAME_ENV STREQUAL "staging"
   AND NOT GAME_ENV STREQUAL "production")
  message(FATAL_ERROR "GAME_ENV invalide: '${GAME_ENV}' (attendu: dev, staging ou production)")
endif()

# ============================================================
# version.txt (APP_VERSION_STR) — generated file
# ============================================================

# Emplacement du fichier généré (gère multi-config VS/Xcode)
if(CMAKE_GENERATOR MATCHES "Visual Studio|Xcode")
  set(GENERATED_VERSION_TXT "${CMAKE_BINARY_DIR}/$<CONFIG>/version.txt")
else()
  set(GENERATED_VERSION_TXT "${CMAKE_BINARY_DIR}/version.txt")
endif()
# Génère le fichier au moment du "generate" CMake (pas au runtime)
file(GENERATE
  OUTPUT "${GENERATED_VERSION_TXT}"
  CONTENT "${APP_VERSION_STR}\n"
)

# ============================================================
# Dépendances vendored — add_subdirectory
# ============================================================
# Ajouter les dépendances vendored pour RC2D, avec add_subdirectory pour que CMake 
# puisse les compiler et les linker correctement.
# ============================================================
add_subdirectory(dependencies/Crzgames_RC2D EXCLUDE_FROM_ALL)

# ============================================================
# Paths
# ============================================================
# DEPENDENCIES_PATH : racine de ton dossier dependencies/
# CRZGAMES_LIBRARIES_PATH: dossier où on ranges les libs précompilées (ffmpeg, openssl, rcenet, onnxruntime, sdl3shadercross, etc.) pour chaque plateforme.
# ============================================================
set(DEPENDENCIES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
set(CRZGAMES_LIBRARIES_PATH "${DEPENDENCIES_PATH}/Crzgames_RC2D/dependencies/Crzgames_Libraries")

# ============================================================
# Fichiers source de l’exécutable
#
# Récupère automatiquement tous les fichiers sources .cpp de manière récursive 
# dans "src/" pour les compiler dans la target de l’exécutable.
# ============================================================
file(GLOB_RECURSE SOURCES 
  "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

# ============================================================
# Fichiers d’en-tête (headers) de l’exécutable
#
# Récupère automatiquement tous les fichiers d’en-tête .h de manière récursive 
# dans "include/" pour les inclure dans la target de l’exécutable.
# ============================================================
file(GLOB_RECURSE HEADERS 
  "${PROJECT_SOURCE_DIR}/include/*.h"
)

# ============================================================
# Création de la target exécutable
#
# Pour une application Android, il faut impérativement que la target soit une bibliothèque partagée (SHARED) 
# et que son nom soit "main" pour que le système Android puisse la reconnaître comme point d'entrée de l'application.
# 
# Pour les autres plateformes, on peut utiliser le nom du projet (rc2d-game-template) comme nom de target pour l’exécutable.
#
# ============================================================
if(ANDROID)
  set(APP_TARGET main)
  add_library(${APP_TARGET} SHARED 
    ${SOURCES} 
    ${HEADERS}
  )
else()
  set(APP_TARGET ${PROJECT_NAME})
  add_executable(${APP_TARGET} 
    ${SOURCES} 
    ${HEADERS}
  )
endif()

# ============================================================
# Compile-time environment defines
# ============================================================

# Environement de build (dev/staging/production) : ajoute des macros préprocesseur pour différencier les builds
# dans le code source, par exemple pour activer des logs de debug en dev mais pas en production.
if(GAME_ENV STREQUAL "dev")
  target_compile_definitions(${APP_TARGET} PRIVATE
    GAME_ENV_DEV=1
    GAME_ENV_STAGING=0
    GAME_ENV_PRODUCTION=0
  )
elseif(GAME_ENV STREQUAL "staging")
  target_compile_definitions(${APP_TARGET} PRIVATE
    GAME_ENV_DEV=0
    GAME_ENV_STAGING=1
    GAME_ENV_PRODUCTION=0
  )
else() # production
  target_compile_definitions(${APP_TARGET} PRIVATE
    GAME_ENV_DEV=0
    GAME_ENV_STAGING=0
    GAME_ENV_PRODUCTION=1
  )
endif()

# Ajouter au target compile definitions la version de l'application (APP_VERSION_STR) pour 
# qu'elle soit accessible dans le code source via une macro préprocesseur, via : APP_VERSION.
target_compile_definitions(${APP_TARGET} PRIVATE
  APP_VERSION="${APP_VERSION_STR}"
)

# ============================================================
# Output Directories
# ============================================================
# Gestion du dossier de sortie selon le générateur CMake :
# - Visual Studio / Xcode : multi-config (Debug/Release/...)
# - Ninja / Makefiles     : single-config
# ============================================================
if(CMAKE_GENERATOR MATCHES "Visual Studio|Xcode")
  # Multi-config generators
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
else()
  # Single-config generators (Makefiles, Ninja, etc.)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

# ============================================================
# Include directories pour l’exécutable
# 
# Ajoute les dossiers d'inclusion nécessaires pour compiler le projet, notamment :
# - Le dossier include du projet lui-même (pour les headers locaux)
# ============================================================
target_include_directories(${APP_TARGET} PRIVATE
  "${PROJECT_SOURCE_DIR}/include"
)

# ============================================================
# Linker l’exécutable avec les bibliothèques nécessaires
#
# Lie la target de l’exécutable à la bibliothèque RC2D pour pouvoir 
# utiliser le moteur de jeu dans le projet.
# ============================================================
target_link_libraries(${APP_TARGET} PRIVATE
  rc2d
)

# ============================================================
# rc2d_configure_ffmpeg
#
# Objectif :
# - Copier les libs dynamique/partagée (DLL/.so/.dylib) quand nécessaire
#
# Notes :
# - FFmpeg est optionnel via RC2D_VIDEO_MODULE_ENABLED
# ============================================================
function(rc2d_configure_ffmpeg target_name)
  if (RC2D_VIDEO_MODULE_ENABLED)
    if (WIN32) # Windows
      # Copier aussi les DLLs dans le répertoire de l'exécutable
      file(GLOB_RECURSE FFMPEG_DLLS
        "${CRZGAMES_LIBRARIES_PATH}/windows/bin/ffmpeg/${RC2D_ARCH}/*.dll"
      )
      foreach(FILE IN LISTS FFMPEG_DLLS)
        get_filename_component(FILE_NAME "${FILE}" NAME)
        add_custom_command(TARGET ${target_name} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  "${FILE}"
                  "$<TARGET_FILE_DIR:${target_name}>/${FILE_NAME}"
        )
      endforeach()
    endif()
  endif()
endfunction()

# ============================================================
# rc2d_configure_onnxruntime
#
# Configure ONNX Runtime (inférence IA) pour une target donnée.
#
# Objectif :
# - Copier les libs dynamique/partagée (DLL/.so/.dylib) quand nécessaire
#
# Notes :s
# - ONNX Runtime est optionnel via RC2D_ONNX_MODULE_ENABLED
# ============================================================
function(rc2d_configure_onnxruntime target_name)
  if(RC2D_ONNX_MODULE_ENABLED)
    if(WIN32) # Windows
      # Copier aussi les DLLs dans le répertoire de l'exécutable
      file(GLOB_RECURSE ONNX_DLLS
        "${CRZGAMES_LIBRARIES_PATH}/windows/bin/onnxruntime/${RC2D_ARCH}/*.dll"
      )
      foreach(FILE IN LISTS ONNX_DLLS)
        get_filename_component(FILE_NAME "${FILE}" NAME)
        add_custom_command(TARGET ${target_name} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  "${FILE}"
                  "$<TARGET_FILE_DIR:${target_name}>/${FILE_NAME}"
        )
      endforeach()
    elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin") # macOS
      # Destination = Frameworks du bundle
      set(ONNX_DEST_DIR "$<TARGET_BUNDLE_DIR:${target_name}>/Contents/Frameworks")

      # Crée le dossier Frameworks
      add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ONNX_DEST_DIR}"
        COMMENT "Create Contents/Frameworks for ONNX Runtime"
      )

      # Prend les dylibs (versionnées ou non)
      file(GLOB ONNX_DYLIBS
        "${CRZGAMES_LIBRARIES_PATH}/macos/lib/onnxruntime/*.dylib"
        "${CRZGAMES_LIBRARIES_PATH}/macos/lib/onnxruntime/*.dylib.*"
      )

      foreach(FILE IN LISTS ONNX_DYLIBS)
        get_filename_component(FILE_NAME "${FILE}" NAME)
        add_custom_command(TARGET ${target_name} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  "${FILE}"
                  "${ONNX_DEST_DIR}/${FILE_NAME}"
          COMMENT "Copy ONNX Runtime: ${FILE_NAME}"
        )
      endforeach()
    elseif (RC2D_PLATFORM STREQUAL "STEAMRT4") # Steam Deck / Steam Linux
      # Copier aussi les libraries dynamique dans le répertoire de l'exécutable
      file(GLOB_RECURSE ONNX_SO
        "${CRZGAMES_LIBRARIES_PATH}/steamrt4/lib/onnxruntime/${RC2D_ARCH}/*.so"
        "${CRZGAMES_LIBRARIES_PATH}/steamrt4/lib/onnxruntime/${RC2D_ARCH}/*.so.*"
      )
      add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND cp -a ${ONNX_SO} "$<TARGET_FILE_DIR:${target_name}>/"
        COMMENT "Copie des bibliothèques ONNX Runtime avec conservation des liens symboliques"
        VERBATIM
      )
    elseif (UNIX AND NOT APPLE AND NOT ANDROID AND NOT (RC2D_PLATFORM STREQUAL "STEAMRT4")) # Linux, Raspberry PI 5 etc.
        # Copier aussi les libraries dynamique dans le répertoire de l'exécutable
        file(GLOB_RECURSE ONNX_SO
          "${CRZGAMES_LIBRARIES_PATH}/linux/lib/onnxruntime/${RC2D_ARCH}/*.so"
          "${CRZGAMES_LIBRARIES_PATH}/linux/lib/onnxruntime/${RC2D_ARCH}/*.so.*"
        )
        add_custom_command(TARGET ${target_name} POST_BUILD
          COMMAND cp -a ${ONNX_SO} "$<TARGET_FILE_DIR:${target_name}>/"
          COMMENT "Copie des bibliothèques ONNX Runtime avec conservation des liens symboliques"
          VERBATIM
        )
    endif()
  endif()
endfunction()

# ============================================================
# rc2d_configure_shadercross
#
# Configure SDL3_shadercross pour la compilation hot reload des shaders, si
# RC2D_GPU_SHADER_HOT_RELOAD_ENABLED est activé.
#
# ============================================================
function(rc2d_configure_shadercross target_name)
  if(RC2D_GPU_SHADER_HOT_RELOAD_ENABLED)
    if (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin") # macOS
      # Destination = Frameworks du bundle
      set(DEST_DIR "$<TARGET_BUNDLE_DIR:${target_name}>/Contents/Frameworks")

      # Crée le dossier Frameworks
      add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEST_DIR}"
        COMMENT "Create Contents/Frameworks for shadercross"
      )

      # Prend les dylibs (versionnées ou non), mais exclut les dylibs SDL3 eux-mêmes (qui sont déjà gérés séparément) pour éviter les doublons et les conflits de version.
      file(GLOB SHADERCROSS_DYLIBS
        "${CRZGAMES_LIBRARIES_PATH}/macos/lib/SDL3_shadercross/*.dylib"
        "${CRZGAMES_LIBRARIES_PATH}/macos/lib/SDL3_shadercross/*.dylib.*"
      )
      list(FILTER SHADERCROSS_DYLIBS EXCLUDE REGEX ".*/libSDL3(\\..*)?\\.dylib(\\..*)?$")

      # Copier les dylibs de shadercross dans le bundle, en conservant les liens symboliques et en évitant de copier les dylibs SDL3 qui sont déjà gérés séparément.
      foreach(FILE IN LISTS SHADERCROSS_DYLIBS)
        get_filename_component(FILE_NAME "${FILE}" NAME)
        add_custom_command(TARGET ${target_name} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  "${FILE}"
                  "${DEST_DIR}/${FILE_NAME}"
          COMMENT "Copy shadercross dylib: ${FILE_NAME}"
        )
      endforeach()
    elseif(WIN32) # Windows
      # Copier aussi les DLLs dans le répertoire de l'exécutable
      file(GLOB_RECURSE SHADERCROSS_DLLS
        "${CRZGAMES_LIBRARIES_PATH}/windows/bin/SDL3_shadercross/${RC2D_ARCH}/*.dll"
      )
      list(FILTER SHADERCROSS_DLLS EXCLUDE REGEX ".*/SDL3(\\..*)?\\.dll$") # Exclut les DLLs SDL3 eux-mêmes pour éviter les doublons et les conflits de version, car elles sont déjà gérées séparément
      foreach(FILE IN LISTS SHADERCROSS_DLLS)
        get_filename_component(FILE_NAME "${FILE}" NAME)
        add_custom_command(TARGET ${target_name} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  "${FILE}"
                  "$<TARGET_FILE_DIR:${target_name}>/${FILE_NAME}"
          COMMENT "Copy shadercross DLL: ${FILE_NAME}"
        )
      endforeach()
    elseif(UNIX AND NOT APPLE AND NOT ANDROID AND NOT (RC2D_PLATFORM STREQUAL "STEAMRT4")) # Linux, Raspberry PI 5 etc. 
      file(GLOB_RECURSE SHADERCROSS_SO
        "${CRZGAMES_LIBRARIES_PATH}/linux/lib/SDL3_shadercross/${RC2D_ARCH}/*.so"
        "${CRZGAMES_LIBRARIES_PATH}/linux/lib/SDL3_shadercross/${RC2D_ARCH}/*.so.*"
      )
      list(FILTER SHADERCROSS_SO EXCLUDE REGEX ".*/libSDL3(\\..*)?\\.so(\\..*)?$") # Exclut les .so SDL3 eux-mêmes pour éviter les doublons et les conflits de version, car elles sont déjà gérés séparément

      # Copier aussi les .so dans le répertoire de l'exécutable
      add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND cp -a ${SHADERCROSS_SO} "$<TARGET_FILE_DIR:${target_name}>/"
        COMMENT "Copie des bibliothèques SDL3_shadercross avec conservation des liens symboliques"
        VERBATIM
      )
    endif()
  endif()
endfunction()

# ============================================================
# rc2d_configure_shader_pipeline
#
# Objectifs :
# 1) Stager SDL3_shadercross dans shaders/tools AVANT la compilation shaders
# 2) Compiler les shaders AVANT le build/link de l'app
#
# Résultat :
# stage_shadercross_tools -> compile_shaders -> ${APP_TARGET} -> POST_BUILD copies (assets/shaders/etc)
# ============================================================
function(rc2d_configure_shader_pipeline target_name)
  set(SHADER_TOOLS_DIR "${PROJECT_SOURCE_DIR}/shaders/tools")
  set(SHADERS_SCRIPTS_DIR "${PROJECT_SOURCE_DIR}/shaders/scripts")
  # Créer les dossiers shaders/compiled et shaders/reflection dans le source tree s'ils n'existent pas déjà, pour éviter les erreurs de CMake quand on les ajoute comme ressources du bundle.
  file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/shaders/compiled")
  file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/shaders/reflection")

  # ------------------------------------------------------------
  # 1) Déterminer les dossiers source de shadercross selon la plateforme
  # ------------------------------------------------------------
  set(SHADERCROSS_BIN_DIR "")
  set(SHADERCROSS_LIB_DIR "")

  if(APPLE)
    set(SHADERCROSS_BIN_DIR "${CRZGAMES_LIBRARIES_PATH}/macos/bin/SDL3_shadercross")
    set(SHADERCROSS_LIB_DIR "${CRZGAMES_LIBRARIES_PATH}/macos/lib/SDL3_shadercross")
  elseif(WIN32)
    set(SHADERCROSS_BIN_DIR "${CRZGAMES_LIBRARIES_PATH}/windows/bin/SDL3_shadercross/${RC2D_ARCH}")
  elseif(RC2D_PLATFORM STREQUAL "STEAMRT4")
    set(SHADERCROSS_BIN_DIR "${CRZGAMES_LIBRARIES_PATH}/steamrt4/bin/SDL3_shadercross/${RC2D_ARCH}")
    set(SHADERCROSS_LIB_DIR "${CRZGAMES_LIBRARIES_PATH}/steamrt4/lib/SDL3_shadercross/${RC2D_ARCH}")
  elseif(UNIX AND NOT APPLE AND NOT ANDROID)
    set(SHADERCROSS_BIN_DIR "${CRZGAMES_LIBRARIES_PATH}/linux/bin/SDL3_shadercross/${RC2D_ARCH}")
    set(SHADERCROSS_LIB_DIR "${CRZGAMES_LIBRARIES_PATH}/linux/lib/SDL3_shadercross/${RC2D_ARCH}")
  elseif(ANDROID)
    # Android build: on compile les shaders avec des outils HOST.
    set(_host_proc "${CMAKE_HOST_SYSTEM_PROCESSOR}")
    string(TOLOWER "${_host_proc}" _host_proc_l)

    set(HOST_ARCH "x64")
    if(_host_proc_l MATCHES "arm64|aarch64")
      set(HOST_ARCH "arm64")
    elseif(_host_proc_l MATCHES "x86_64|amd64")
      set(HOST_ARCH "x64")
    endif()

    if(CMAKE_HOST_WIN32)
      set(SHADERCROSS_BIN_DIR "${CRZGAMES_LIBRARIES_PATH}/windows/bin/SDL3_shadercross/${HOST_ARCH}")
    elseif(CMAKE_HOST_APPLE)
      set(SHADERCROSS_BIN_DIR "${CRZGAMES_LIBRARIES_PATH}/macos/bin/SDL3_shadercross")
      set(SHADERCROSS_LIB_DIR "${CRZGAMES_LIBRARIES_PATH}/macos/lib/SDL3_shadercross")
    else()
      set(SHADERCROSS_BIN_DIR "${CRZGAMES_LIBRARIES_PATH}/linux/bin/SDL3_shadercross/${HOST_ARCH}")
      set(SHADERCROSS_LIB_DIR "${CRZGAMES_LIBRARIES_PATH}/linux/lib/SDL3_shadercross/${HOST_ARCH}")
    endif()
  endif()

  # ------------------------------------------------------------
  # 2) Lister les fichiers à copier
  # ------------------------------------------------------------
  set(_stage_files "")

  if(CMAKE_HOST_WIN32) # Windows, Android-host Windows
    file(GLOB _bins
      "${SHADERCROSS_BIN_DIR}/*.exe"
      "${SHADERCROSS_BIN_DIR}/*.dll"
    )
    list(APPEND _stage_files ${_bins})
  elseif(CMAKE_HOST_APPLE) # macOS/iOS, Android-host macOS
    file(GLOB _bins "${SHADERCROSS_BIN_DIR}/*")
    file(GLOB _dylibs "${SHADERCROSS_LIB_DIR}/*.dylib")
    list(APPEND _stage_files ${_bins} ${_dylibs})
  elseif(CMAKE_HOST_UNIX) # Linux, SteamRT4, Android-host Linux
    file(GLOB _bins "${SHADERCROSS_BIN_DIR}/*")
    file(GLOB _sos
      "${SHADERCROSS_LIB_DIR}/*.so"
      "${SHADERCROSS_LIB_DIR}/*.so.*"
    )
    list(APPEND _stage_files ${_bins} ${_sos})
  endif()

  if(_stage_files STREQUAL "")
    message(FATAL_ERROR
      "stage_shadercross_tools: aucun fichier shadercross trouvé.\n"
      "BIN='${SHADERCROSS_BIN_DIR}'\n"
      "LIB='${SHADERCROSS_LIB_DIR}'\n"
      "Vérifie CRZGAMES_LIBRARIES_PATH et RC2D_ARCH/RC2D_PLATFORM."
    )
  endif()

  # ------------------------------------------------------------
  # 3) Target portable : stage_shadercross_tools (stamp file)
  # ------------------------------------------------------------
  set(_stage_stamp "${CMAKE_CURRENT_BINARY_DIR}/stage_shadercross_tools.stamp")
  if(CMAKE_HOST_WIN32 OR CMAKE_HOST_APPLE) # Windows, macOS/iOS, Android-host Windows/macOS
    add_custom_command(
      OUTPUT "${_stage_stamp}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_TOOLS_DIR}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_stage_files} "${SHADER_TOOLS_DIR}/"
      COMMAND ${CMAKE_COMMAND} -E touch "${_stage_stamp}"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      COMMENT "Stage SDL3_shadercross into shaders/tools (before shader compilation)"
      VERBATIM
    )
  else() # Linux, SteamRT4, Android-host Linux
    # -> copier fichier par fichier sans passer par sh (évite le problème des ';' des listes CMake)
    set(_copy_script "${CMAKE_CURRENT_BINARY_DIR}/stage_shadercross_copy.cmake")
    file(WRITE "${_copy_script}" "file(MAKE_DIRECTORY \"${SHADER_TOOLS_DIR}\")\n")
    foreach(f IN LISTS _stage_files)
      file(APPEND "${_copy_script}"
        "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy_if_different \"${f}\" \"${SHADER_TOOLS_DIR}/\")\n")
    endforeach()

    add_custom_command(
      OUTPUT "${_stage_stamp}"
      COMMAND ${CMAKE_COMMAND} -P "${_copy_script}"
      COMMAND ${CMAKE_COMMAND} -E touch "${_stage_stamp}"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      COMMENT "Stage SDL3_shadercross into shaders/tools (before shader compilation)"
      VERBATIM
    )
  endif()

  # Ajustements spécifiques (après copie)
  if(CMAKE_HOST_APPLE) # macOS/iOS, Android-host macOS
    add_custom_command(
      OUTPUT "${_stage_stamp}.applefix"
      DEPENDS "${_stage_stamp}"
      COMMAND /bin/chmod +x "${SHADER_TOOLS_DIR}/shadercross"
      COMMAND /bin/sh -c "install_name_tool -add_rpath @loader_path \"${SHADER_TOOLS_DIR}/shadercross\" || true"
      COMMAND ${CMAKE_COMMAND} -E touch "${_stage_stamp}.applefix"
      VERBATIM
    )
    add_custom_target(stage_shadercross_tools DEPENDS "${_stage_stamp}.applefix")
  elseif(CMAKE_HOST_UNIX) # Linux, SteamRT4, Android-host Linux
    add_custom_command(
      OUTPUT "${_stage_stamp}.unixfix"
      DEPENDS "${_stage_stamp}"
      COMMAND chmod +x "${SHADER_TOOLS_DIR}/shadercross" || true
      COMMAND patchelf --set-rpath '$ORIGIN' "${SHADER_TOOLS_DIR}/shadercross" || true
      COMMAND ${CMAKE_COMMAND} -E touch "${_stage_stamp}.unixfix"
      VERBATIM
    )
    add_custom_target(stage_shadercross_tools DEPENDS "${_stage_stamp}.unixfix")
  else()
    add_custom_target(stage_shadercross_tools DEPENDS "${_stage_stamp}")
  endif()

  # ------------------------------------------------------------
  # 4) Target compile_shaders (dépend du staging)
  # ------------------------------------------------------------
  if(CMAKE_HOST_WIN32) # Windows, Android-host Windows
    set(SHADERS_COMPILE_SCRIPT "${SHADERS_SCRIPTS_DIR}/compile-shaders-windows.bat")
    add_custom_target(compile_shaders
      COMMAND "${SHADERS_COMPILE_SCRIPT}"
      WORKING_DIRECTORY "${SHADERS_SCRIPTS_DIR}"
      COMMENT "Compile shaders (Windows)"
      VERBATIM
    )
  elseif(CMAKE_HOST_APPLE) # macOS/iOS, Android-host macOS
    set(SHADERS_COMPILE_SCRIPT "${SHADERS_SCRIPTS_DIR}/compile-shaders-unix.sh")
    add_custom_target(compile_shaders
      COMMAND /bin/chmod +x "${SHADERS_COMPILE_SCRIPT}"
      COMMAND "${SHADERS_COMPILE_SCRIPT}"
      WORKING_DIRECTORY "${SHADERS_SCRIPTS_DIR}"
      COMMENT "Compile shaders (Unix)"
      VERBATIM
    )
  elseif(CMAKE_HOST_UNIX) # Linux, SteamRT4, Android-host Linux
    set(SHADERS_COMPILE_SCRIPT "${SHADERS_SCRIPTS_DIR}/compile-shaders-unix.sh")
    add_custom_target(compile_shaders
      COMMAND /bin/chmod +x "${SHADERS_COMPILE_SCRIPT}"
      COMMAND /bin/sh -c "export LD_LIBRARY_PATH='${SHADER_TOOLS_DIR}':\$\$LD_LIBRARY_PATH; '${SHADERS_COMPILE_SCRIPT}'"
      WORKING_DIRECTORY "${SHADERS_SCRIPTS_DIR}"
      COMMENT "Compile shaders (Unix)"
      VERBATIM
    )
  endif()

  # Ordre garanti
  add_dependencies(compile_shaders stage_shadercross_tools)
  add_dependencies(${target_name} compile_shaders)
endfunction()

# ============================================================
# Deploy version.txt per-platform
# ============================================================
# 1) Windows / Linux / SteamRT4 : à côté de l'exécutable
if(NOT ANDROID AND NOT APPLE)
  add_custom_command(TARGET ${APP_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GENERATED_VERSION_TXT}"
            "$<TARGET_FILE_DIR:${APP_TARGET}>/version.txt"
    COMMENT "Copy version.txt next to executable"
    VERBATIM
  )
endif()
# 2) macOS : dans .app/Contents/Resources
if(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  add_custom_command(TARGET ${APP_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GENERATED_VERSION_TXT}"
            "$<TARGET_BUNDLE_DIR:${APP_TARGET}>/Contents/Resources/version.txt"
    COMMENT "Embed version.txt into macOS bundle Resources"
    VERBATIM
  )
endif()
# 3) iOS : dans la racine du bundle .app
if(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
  add_custom_command(TARGET ${APP_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GENERATED_VERSION_TXT}"
            "$<TARGET_BUNDLE_DIR:${APP_TARGET}>/version.txt"
    COMMENT "Embed version.txt into iOS app bundle"
    VERBATIM
  )
endif()

# Appeler les fonctions de configuration pour FFmpeg, ONNX Runtime et SDL3_shadercross 
# pour copier les lib dynamiques nécessaires et configurer les outils de shadercross pour la compilation hors ligne des shaders.
rc2d_configure_ffmpeg(${APP_TARGET})
rc2d_configure_onnxruntime(${APP_TARGET})
rc2d_configure_shadercross(${APP_TARGET})

# Configurer la pipeline de compilation des shaders pour s'assurer que shadercross est stage avant la compilation des shaders, et que les shaders sont compilés avant le build de l'application.
rc2d_configure_shader_pipeline(${APP_TARGET})

# ============================================================
# Configurations spécifiques à chaque plateforme pour l’exécutable
# ============================================================
if (WIN32)
  # Définir l'executable comme une application Windows (pour éviter l'affichage de la console quand on ouvre l'executable) en mode Release
  set_target_properties(${APP_TARGET} PROPERTIES 
    WIN32_EXECUTABLE $<CONFIG:Release>
  )

  # Pour MSVC spécifiquement :
  if(MSVC)
    # ------------------------------------------------------------
    # Définition du chemin du fichier .rc généré
    # ------------------------------------------------------------
    # CMAKE_CURRENT_BINARY_DIR :
    #   → Répertoire de build courant (dossier généré par CMake).
    #   → On évite volontairement d’écrire dans le source tree pour :
    #       - ne pas polluer le repo Git
    #       - éviter les conflits
    #       - séparer clairement sources et artefacts générés
    #
    # resource.rc sera généré à partir de resource.rc.in
    #
    set(GENERATED_RC "${CMAKE_CURRENT_BINARY_DIR}/resource.rc")

    # ------------------------------------------------------------
    # Génération du fichier resource.rc à partir du template .rc.in
    # ------------------------------------------------------------
    # configure_file :
    #   → Remplace les variables CMake (@VAR@) dans le fichier .in
    #   → Génère un fichier concret utilisé par le compilateur
    #
    # @ONLY :
    #   → Remplace uniquement les variables au format @VAR@
    #   → Évite toute substitution accidentelle du style ${VAR}
    #
    # Exemple :
    #   resource.rc.in contient :
    #     VALUE "FileVersion", "@APP_VERSION@"
    #
    #   CMake remplace @APP_VERSION@ par la valeur définie dans le CMakeLists
    #
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/platforms/windows/resource.rc.in"
      "${GENERATED_RC}"
      @ONLY
    )

    # ------------------------------------------------------------
    # Indique explicitement à CMake que ce fichier est un fichier
    # de ressources Windows (.rc)
    # ------------------------------------------------------------
    # LANGUAGE RC :
    #   → Force CMake à utiliser le compilateur de ressources Windows (rc.exe)
    #   → Important surtout si le fichier est généré
    #
    # Cela permet :
    #   - d’intégrer les métadonnées (version, société, copyright)
    #   - d’intégrer l’icône de l’exécutable
    #   - d’inclure les informations visibles dans les propriétés Windows
    #
    set_source_files_properties("${GENERATED_RC}" PROPERTIES LANGUAGE RC)

    # Et enfin, on ajoute ce fichier de ressources à la target de l’exemple pour qu’il soit compilé et lié à l’exécutable final.
    target_sources(${APP_TARGET} PRIVATE "${GENERATED_RC}")

    # Définir le projet de démarrage dans Visual Studio pour que l’exécutable de l’exemple soit celui qui se lance par défaut quand on appuie sur "Play" (F5)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${APP_TARGET})

    # Définir le répertoire de travail pour le débogueur dans Visual Studio
    # Cela permet de lancer l'exécutable avec le bon répertoire de travail (working directory)
    set_target_properties(${APP_TARGET} PROPERTIES
      VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${APP_TARGET}>"
    )
  endif()
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  # Ajouter le fichier d'icône de l'application dans les ressources de la target de l’exemple
  target_sources(${APP_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/icons/macos/app-icon.icns"
  )
  set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/icons/macos/app-icon.icns" PROPERTIES 
    MACOSX_PACKAGE_LOCATION "Resources"
  )

  # Ajouter les assets génériques (images, sons, etc.) dans les ressources du bundle pour qu'ils soient accessibles à l'exécution
  target_sources(${APP_TARGET} PRIVATE
    "${PROJECT_SOURCE_DIR}/assets"
  )
  set_source_files_properties("${PROJECT_SOURCE_DIR}/assets" PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
  )

  # Ajouter le dossier shaders/compiled + shaders/reflection dans les ressources du bundle pour qu'ils soient accessibles à l'exécution
  # Contents/Resources/shaders/{compiled,reflection}  
  target_sources(${APP_TARGET} PRIVATE
    "${PROJECT_SOURCE_DIR}/shaders/compiled"
    "${PROJECT_SOURCE_DIR}/shaders/reflection"
  )
  set_source_files_properties(
    "${PROJECT_SOURCE_DIR}/shaders/compiled"
    "${PROJECT_SOURCE_DIR}/shaders/reflection"
    PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources/shaders"
  )

  # Configurez le bundle (.app)
  set_target_properties(${APP_TARGET} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME ${APP_TARGET} # Nom de l'application (à modifier pour chaque application)
    MACOSX_BUNDLE_GUI_IDENTIFIER ${APP_IOSMACOS_IDENTIFIER} # L'identifiant de votre application (via identifiers sur le compte apple developer), à modifier pour chaque application
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${APP_VERSION_NUMERIC} # Version utilisateur lisible (à modifier pour chaque application)
    MACOSX_BUNDLE_BUNDLE_VERSION ${APP_IOSMACOS_BUILD_VERSION} # Version de build unique (à modifier pour chaque application)
    MACOSX_BUNDLE_COPYRIGHT ${APP_LEGAL_COPYRIGHT} # Droits d'auteur (à modifier pour chaque application)
    MACOSX_BUNDLE_ICON_FILE "app-icon.icns"
    MACOSX_BUNDLE_INFO_STRING ${APP_GAME_DESCRIPTION} # Description de l'application (à modifier pour chaque application)
  )

  # --- Code signing (optionnel) ---
  if(GAME_BUILD_APPLE_CODE_SIGNING)
    # SIGNATURE ACTIVÉE
    set_target_properties(${APP_TARGET} PROPERTIES
      XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ${APP_IOSMACOS_DEVELOPMENT_TEAM_ID} # Remplacez par votre Team ID Apple
      XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ${APP_MACOS_CODE_SIGN_IDENTITY} # Remplacez par votre identité de signature
      XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual" # Style de signature (Manual ou Automatic)
      XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--options=runtime --strict --deep --force --timestamp --verbose" # Options supplémentaires pour la signature
    )
  endif()

  set_target_properties(${APP_TARGET} PROPERTIES
    # ---------------------------------------------------------------------------
    # macOS — RPATH pour les dépendances embarquées dans le bundle (.app)
    # Pour que les .dylib embarquées soient trouvées à l’exécution, il faut configurer le RPATH
    # pour indiquer à dyld où chercher ces bibliothèques dynamiques.
    # ---------------------------------------------------------------------------
    #
    # Contexte :
    # - Sur macOS, l’exécutable d’un bundle est ici :
    #     MyApp.app/Contents/MacOS/MyApp
    # - Les bibliothèques dynamiques que l’on embarque (SDL3, SDL3_image, SDL3_ttf,
    #   SDL3_mixer, onnxruntime, SDL3_shadercross, etc.) doivent être copiées dans :
    #     MyApp.app/Contents/Frameworks/
    #
    # Objectif :
    # - Faire en sorte que dyld (le loader macOS) trouve automatiquement ces .dylib
    #   à l’exécution, sans dépendre d’un chemin absolu de la machine de build,
    #   ni de libs installées sur le système.
    #
    # ---------------------------------------------------------------------------

    # INSTALL_RPATH :
    # - Ajoute un chemin de recherche "runtime" dans l’exécutable.
    # - "@executable_path" = dossier contenant l’exécutable du bundle :
    #     .../MyApp.app/Contents/MacOS
    # - Donc "@executable_path/../Frameworks" pointe vers :
    #     .../MyApp.app/Contents/Frameworks
    # - C’est la convention standard pour résoudre les .dylib embarquées.
    INSTALL_RPATH "@executable_path/../Frameworks"

    # BUILD_WITH_INSTALL_RPATH :
    # - Applique ce RPATH dès la phase de build, pas uniquement après un "cmake --install".
    # - Utile car on exécute généralement l’app directement depuis le dossier de build
    #   (et pas via une étape d’installation).
    BUILD_WITH_INSTALL_RPATH TRUE

    # MACOSX_RPATH :
    # - Active le mécanisme RPATH sur macOS (utilisation de LC_RPATH / @rpath).
    # - Requis/attendu pour que "@rpath" et les chemins ci-dessus soient pris en compte.
    MACOSX_RPATH TRUE
  )

  # Ajoute une étape de post-build pour modifier le fichier Info.plist (macOS)
  set(MACOS_INFO_PLIST "$<TARGET_BUNDLE_DIR:${APP_TARGET}>/Contents/Info.plist")
  add_custom_command(TARGET ${APP_TARGET} POST_BUILD
    # ------------------------------------------------------------
    # Indirect input (souris / trackpad / clavier / manettes)
    # ------------------------------------------------------------
    COMMAND /usr/libexec/PlistBuddy -c "Add :UIApplicationSupportsIndirectInputEvents bool true" "${MACOS_INFO_PLIST}"

    # ------------------------------------------------------------
    # Bluetooth (CoreBluetooth)
    # ------------------------------------------------------------
    COMMAND /usr/libexec/PlistBuddy -c "Add :NSBluetoothAlwaysUsageDescription string 'MyApp souhaite rester connecté aux contrôleurs de jeu Bluetooth à proximité, même lorsque vous n\\x27utilisez pas l\\x27application.'" "${MACOS_INFO_PLIST}"

    # ------------------------------------------------------------
    # High DPI
    # ------------------------------------------------------------
    COMMAND /usr/libexec/PlistBuddy -c "Add :NSHighResolutionCapable bool true" "${MACOS_INFO_PLIST}"

    # ------------------------------------------------------------
    # Non-exempt encryption
    # ------------------------------------------------------------
    COMMAND /usr/libexec/PlistBuddy -c "Add :ITSAppUsesNonExemptEncryption bool false" "${MACOS_INFO_PLIST}"

    # ------------------------------------------------------------
    # Caméra (SDL_Camera)
    # ------------------------------------------------------------
    COMMAND /usr/libexec/PlistBuddy -c "Add :NSCameraUsageDescription string 'Cette application utilise la caméra pour des fonctionnalités en jeu.'" "${MACOS_INFO_PLIST}"

    COMMENT "Adding keys to Info.plist (macOS)"
  )

  # Créer le dossier Contents/Frameworks dans le bundle .app pour y copier les .dylib embarquées (SDL3, SDL3_image, SDL3_ttf, SDL3_mixer, onnxruntime, SDL3_shadercross, etc.)
  set(FRAMEWORKS_DIR "$<TARGET_BUNDLE_DIR:${APP_TARGET}>/Contents/Frameworks")
  add_custom_command(TARGET ${APP_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${FRAMEWORKS_DIR}"
    COMMENT "Create Contents/Frameworks in app bundle"
  )

  # Copier les .dylib générées dans le dossier Contents/Frameworks du bundle .app
  set(DYLIB_SOURCE_DIR "${CMAKE_BINARY_DIR}/$<CONFIG>")
  add_custom_command(TARGET ${APP_TARGET} POST_BUILD
    COMMAND /bin/sh -c "cp -a \"${DYLIB_SOURCE_DIR}\"/*.dylib \"${FRAMEWORKS_DIR}\"/ 2>/dev/null || true"
    COMMENT "Copy dylibs into Contents/Frameworks (cp -a keeps symlinks)"
    VERBATIM
  )
elseif(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
  # Ressource : Storyboard de lancement et Asset Catalog pour les icônes (obligatoire sur iOS)
  set(IOS_ASSETCATALOG "${CMAKE_CURRENT_SOURCE_DIR}/icons/ios/Assets.xcassets")
  set(IOS_LAUNCH_STORYBOARD "${CMAKE_CURRENT_SOURCE_DIR}/platforms/ios/LaunchScreen.storyboard")

  # Ajout à la target des ressources spécifiques à iOS (Asset Catalog et Storyboard de lancement)
  target_sources(${APP_TARGET} PRIVATE
    "${IOS_ASSETCATALOG}"
    "${IOS_LAUNCH_STORYBOARD}"
  )

  # Configurez les propriétés des fichiers de ressources pour qu'ils soient correctement traités par Xcode
  set_source_files_properties("${IOS_ASSETCATALOG}" PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
    XCODE_EXPLICIT_FILE_TYPE "folder.assetcatalog"
  )
  set_source_files_properties("${IOS_LAUNCH_STORYBOARD}" PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
    XCODE_EXPLICIT_FILE_TYPE "file.storyboard"
  )

  # Ajouter les assets génériques (images, sons, etc.) dans les ressources du bundle pour qu'ils soient accessibles à l'exécution
  target_sources(${APP_TARGET} PRIVATE
    "${PROJECT_SOURCE_DIR}/assets"
  )
  set_source_files_properties("${PROJECT_SOURCE_DIR}/assets" PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
  )

  # Ajouter le dossier shaders/compiled + shaders/reflection dans les ressources du bundle pour qu'ils soient accessibles à l'exécution
  # Contents/Resources/shaders/{compiled,reflection}
  target_sources(${APP_TARGET} PRIVATE
    "${PROJECT_SOURCE_DIR}/shaders/compiled"
    "${PROJECT_SOURCE_DIR}/shaders/reflection"
  )
  set_source_files_properties(
    "${PROJECT_SOURCE_DIR}/shaders/compiled"
    "${PROJECT_SOURCE_DIR}/shaders/reflection"
    PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources/shaders"
  )

  # Configurez le bundle (.app)
  set_target_properties(${APP_TARGET} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME ${APP_TARGET} # Nom de l'application (à modifier pour chaque application)
    MACOSX_BUNDLE_GUI_IDENTIFIER ${APP_IOSMACOS_IDENTIFIER} # L'identifiant de votre application (via identifiers sur le compte apple developer) à modifier pour chaque application
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${APP_VERSION_NUMERIC} # Version utilisateur lisible (à modifier pour chaque application)
    MACOSX_BUNDLE_BUNDLE_VERSION ${APP_IOSMACOS_BUILD_VERSION} # Version de build unique (à modifier pour chaque application)
    MACOSX_BUNDLE_COPYRIGHT ${APP_LEGAL_COPYRIGHT} # Droits d'auteur
    MACOSX_BUNDLE_INFO_STRING ${APP_GAME_DESCRIPTION} # Description de l'application (à modifier pour chaque application)
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon" # Nom de l'icône de l'application
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2" # 1 pour iPhone, 2 pour iPad
  )

  # --- Code signing (optionnel) ---
  if(GAME_BUILD_APPLE_CODE_SIGNING)
    # SIGNATURE ACTIVÉE
    set_target_properties(${APP_TARGET} PROPERTIES
      XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ${APP_IOSMACOS_DEVELOPMENT_TEAM_ID} # Remplacez par votre Team ID Apple
      XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ${APP_IOS_CODE_SIGN_IDENTITY} # Remplacez par votre identité de signature
      XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual" # Style de signature (Manual ou Automatic)
      XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--deep --strict --force --timestamp --verbose" # Options supplémentaires pour la signature
      XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER ${APP_IOS_PROVISIONING_PROFILE_NAME} # Remplacez par le nom de votre profil de provisionnement
      XCODE_ATTRIBUTE_PROVISIONING_PROFILE ${APP_IOS_PROVISIONING_PROFILE_UUID} # Remplacez par l'UUID de votre profil de provisionnement
    )
  endif()

  # Ajoute une étape de post-build pour ajouter des clés au fichier Info.plist (iOS)
  set(IOS_INFO_PLIST "$<TARGET_BUNDLE_DIR:${APP_TARGET}>/Info.plist")
  add_custom_command(TARGET ${APP_TARGET} POST_BUILD
    # ------------------------------------------------------------
    # Support souris / trackpad / clavier / manettes
    # ------------------------------------------------------------
    COMMAND /usr/libexec/PlistBuddy -c "Add :UIApplicationSupportsIndirectInputEvents bool true" "${IOS_INFO_PLIST}"
    COMMENT "Enable indirect input devices support"

    # ------------------------------------------------------------
    # Bluetooth (HIDAPI / CoreBluetooth)
    # ------------------------------------------------------------
    COMMAND /usr/libexec/PlistBuddy -c "Add :NSBluetoothAlwaysUsageDescription string 'MyApp souhaite rester connecté aux contrôleurs de jeu Bluetooth à proximité.'" "${IOS_INFO_PLIST}"
    COMMENT "Adding Bluetooth usage description"

    # ------------------------------------------------------------
    # High DPI
    # ------------------------------------------------------------
    COMMAND /usr/libexec/PlistBuddy -c "Add :NSHighResolutionCapable bool true" "${IOS_INFO_PLIST}"
    COMMENT "Enable High DPI"

    # ------------------------------------------------------------
    # Orientations supportées
    # ------------------------------------------------------------
    COMMAND /usr/libexec/PlistBuddy -c "Add :UISupportedInterfaceOrientations array" "${IOS_INFO_PLIST}"
    COMMAND /usr/libexec/PlistBuddy -c "Add :UISupportedInterfaceOrientations:0 string UIInterfaceOrientationPortrait" "${IOS_INFO_PLIST}"
    COMMAND /usr/libexec/PlistBuddy -c "Add :UISupportedInterfaceOrientations:1 string UIInterfaceOrientationPortraitUpsideDown" "${IOS_INFO_PLIST}"
    COMMAND /usr/libexec/PlistBuddy -c "Add :UISupportedInterfaceOrientations:2 string UIInterfaceOrientationLandscapeLeft" "${IOS_INFO_PLIST}"
    COMMAND /usr/libexec/PlistBuddy -c "Add :UISupportedInterfaceOrientations:3 string UIInterfaceOrientationLandscapeRight" "${IOS_INFO_PLIST}"
    COMMENT "Adding supported interface orientations"

    # ------------------------------------------------------------
    # Non-exempt encryption
    # ------------------------------------------------------------
    COMMAND /usr/libexec/PlistBuddy -c "Add :ITSAppUsesNonExemptEncryption bool false" "${IOS_INFO_PLIST}"
    COMMENT "Adding ITSAppUsesNonExemptEncryption key"

    # ------------------------------------------------------------
    # LaunchScreen
    # ------------------------------------------------------------
    COMMAND /usr/libexec/PlistBuddy -c "Add :UILaunchStoryboardName string LaunchScreen" "${IOS_INFO_PLIST}"
    COMMENT "Adding launch storyboard name"

    # ------------------------------------------------------------
    # Caméra (SDL_Camera)
    # ------------------------------------------------------------
    COMMAND /usr/libexec/PlistBuddy -c "Add :NSCameraUsageDescription string 'Cette application utilise la caméra pour des fonctionnalités en jeu.'" "${IOS_INFO_PLIST}"
    COMMENT "Adding camera usage description"

    # ------------------------------------------------------------
    # 120Hz ProMotion
    # ------------------------------------------------------------
    COMMAND /usr/libexec/PlistBuddy -c "Add :CADisableMinimumFrameDurationOnPhone bool true" "${IOS_INFO_PLIST}"
    COMMENT "Enable 120Hz ProMotion support"
  )
elseif(UNIX AND NOT APPLE AND NOT ANDROID AND NOT (RC2D_PLATFORM STREQUAL "STEAMRT4")) # Linux "classique" (Ubuntu, Debian, Fedora, Arch, etc.) et Raspberry PI 5
  # Pour l'executable "classique" sur Linux (on ne parle pas de l'AppImage ici, mais rc2d-game-template)
  set_target_properties(${APP_TARGET} PROPERTIES
    BUILD_RPATH "\$ORIGIN;\$ORIGIN/usr/lib"
    INSTALL_RPATH "\$ORIGIN;\$ORIGIN/usr/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
  )

  # ------------------------------------------------------------
  # Arch AppImage depuis RC2D_ARCH
  # ------------------------------------------------------------
  if(RC2D_ARCH STREQUAL "x64")
    set(APPIMAGE_ARCH "x86_64")
  elseif(RC2D_ARCH STREQUAL "arm64")
    set(APPIMAGE_ARCH "aarch64")
  else()
    message(FATAL_ERROR "RC2D_ARCH invalide: '${RC2D_ARCH}' (attendu: x64 ou arm64)")
  endif()

  # ------------------------------------------------------------
  # APPDIR : isolé par arch + build_type (évite collisions)
  # On le met dans le build tree, pas dans le source tree.
  # Exemple :
  #   build/linux/x64/Debug/AppDir
  #   build/linux/x64/Release/AppDir
  #   build/linux/arm64/Debug/AppDir
  #   build/linux/arm64/Release/AppDir
  # ------------------------------------------------------------
  set(APPDIR "${CMAKE_BINARY_DIR}/AppDir")
  set(APPIMAGE_OUTPUT "${CMAKE_BINARY_DIR}/${APP_TARGET}.AppImage")

  install(CODE
    "include(\"${CMAKE_CURRENT_SOURCE_DIR}/platforms/linux/appimage.cmake\")
    make_appimage(
      EXE \"\$<TARGET_FILE:${APP_TARGET}>\"
      NAME \"${APP_TARGET}\"
      ICON \"${PROJECT_SOURCE_DIR}/icons/linux/app-icon.png\"
      DIR_ICON \"${PROJECT_SOURCE_DIR}/icons/linux/app-icon.png\"
      OUTPUT_NAME \"${APPIMAGE_OUTPUT}\"
      APP_VERSION \"${APP_VERSION_STR}\"
      APP_ARCH \"${APPIMAGE_ARCH}\"
      APPDIR \"${APPDIR}\"

      # 👇 Copie des assets, shaders dans AppDir/ (à côté de l’exe)
      ASSETS
        \"${CMAKE_BINARY_DIR}/assets\"
        \"${CMAKE_BINARY_DIR}/shaders\"
        \"${CMAKE_BINARY_DIR}/version.txt\"
    )"
    COMPONENT Runtime
  )
elseif(RC2D_PLATFORM STREQUAL "STEAMRT4") # Steam Deck / Steam Linux
  # ============================================================
  # SteamRT4 — Configure RPATH to $ORIGIN
  # ============================================================
  # Explication technique :
  # - "$ORIGIN" est une variable spéciale du loader ELF.
  #   Elle représente le dossier contenant l'exécutable.
  #
  #   Exemple de structure :
  #     ./rc2d_example
  #     ./libonnxruntime.so
  #     ./libavcodec.so
  #
  #   Avec RPATH = $ORIGIN, le loader cherchera :
  #     ./libonnxruntime.so
  #
  set_target_properties(${APP_TARGET} PROPERTIES
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
    BUILD_WITH_INSTALL_RPATH TRUE
  )
endif()

# ============================================================
# Copie des ressources (dossiers : shaders, assets) dans le dossier de sortie de l'exécutable pour : Windows, Linux, SteamRT4
# Infos :
# Android copie automatiquement les ressources dans le dossier assets/ de l'APK, donc pas besoin de les copier manuellement.
# Sur Apple (macOS/iOS), les ressources sont intégrées dans le bundle .app, donc pas besoin de les copier manuellement.
# ============================================================
if(NOT ANDROID AND NOT APPLE)
  # ---------------------------------------------------------------------------
  # Shaders runtime :
  # On ne copie QUE :
  #   - shaders/compiled
  #   - shaders/reflection
  #
  # tools/ et src/ restent dans le repo :
  #   shaders/tools (utilisé par les scripts .sh/.bat)
  # ---------------------------------------------------------------------------

  # Créer le dossier shaders/ à côté de l'exe (même si compiled/reflection n'existent pas)
  add_custom_command(TARGET ${APP_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${APP_TARGET}>/shaders"
    COMMENT "Prépare le dossier shaders/ à côté de ${APP_TARGET}"
    VERBATIM
  )

  # Copier shaders/compiled/ si présent
  if(EXISTS "${PROJECT_SOURCE_DIR}/shaders/compiled")
    add_custom_command(TARGET ${APP_TARGET} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${APP_TARGET}>/shaders/compiled"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${PROJECT_SOURCE_DIR}/shaders/compiled"
              "$<TARGET_FILE_DIR:${APP_TARGET}>/shaders/compiled"
      COMMENT "Copie shaders/compiled à côté de ${APP_TARGET}"
      VERBATIM
    )
  endif()

  # Copier shaders/reflection/ si présent
  if(EXISTS "${PROJECT_SOURCE_DIR}/shaders/reflection")
    add_custom_command(TARGET ${APP_TARGET} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${APP_TARGET}>/shaders/reflection"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${PROJECT_SOURCE_DIR}/shaders/reflection"
              "$<TARGET_FILE_DIR:${APP_TARGET}>/shaders/reflection"
      COMMENT "Copie shaders/reflection à côté de ${APP_TARGET}"
      VERBATIM
    )
  endif()

  # Assets génériques (images, sons, etc.) :
  add_custom_command(TARGET ${APP_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${APP_TARGET}>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${APP_TARGET}>/assets"
    COMMENT "Copie du dossier assets dans le dossier de build de ${APP_TARGET}"
    VERBATIM
  )
endif()

# ============================================================
# Linux / SteamRT4 — Rendre l'exécutable exécutable via chmod
# ============================================================ 
if(UNIX AND NOT APPLE AND NOT ANDROID) # Linux et SteamRT4
  add_custom_command(TARGET ${APP_TARGET} POST_BUILD
    COMMAND chmod +x "$<TARGET_FILE:${APP_TARGET}>"
    COMMENT "Rendre ${APP_TARGET} exécutable"
    VERBATIM
  )
endif()
