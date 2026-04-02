function(make_appimage)
  set(optional)
  set(args EXE NAME DIR_ICON ICON OUTPUT_NAME APP_VERSION APP_NAME APP_ARCH APPDIR)
  set(list_args ASSETS)
  cmake_parse_arguments(
    PARSE_ARGV 0
    ARGS
    "${optional}"
    "${args}"
    "${list_args}"
  )

  if(ARGS_UNPARSED_ARGUMENTS)
    message(WARNING "Unparsed arguments: ${ARGS_UNPARSED_ARGUMENTS}")
  endif()

  # ------------------------------------------------------------
  # Validate required args
  # ------------------------------------------------------------
  if(NOT DEFINED ARGS_EXE OR ARGS_EXE STREQUAL "")
    message(FATAL_ERROR "make_appimage: EXE est requis")
  endif()
  if(NOT DEFINED ARGS_NAME OR ARGS_NAME STREQUAL "")
    message(FATAL_ERROR "make_appimage: NAME est requis")
  endif()
  if(NOT DEFINED ARGS_OUTPUT_NAME OR ARGS_OUTPUT_NAME STREQUAL "")
    message(FATAL_ERROR "make_appimage: OUTPUT_NAME est requis")
  endif()
  if(NOT DEFINED ARGS_APP_ARCH OR ARGS_APP_ARCH STREQUAL "")
    message(FATAL_ERROR "make_appimage: APP_ARCH est requis (x86_64 ou aarch64)")
  endif()

  # ------------------------------------------------------------
  # Choose appimagetool URL by arch
  # ------------------------------------------------------------
  if(ARGS_APP_ARCH STREQUAL "x86_64")
    set(AIT_URL "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage")
  elseif(ARGS_APP_ARCH STREQUAL "aarch64")
    set(AIT_URL "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-aarch64.AppImage")
  else()
    message(FATAL_ERROR "make_appimage: APP_ARCH '${ARGS_APP_ARCH}' non supportée (attendu: x86_64 ou aarch64)")
  endif()

  # Cache séparé par arch (évite collisions)
  set(AIT_PATH "${CMAKE_BINARY_DIR}/appimagetool-${ARGS_APP_ARCH}.AppImage" CACHE INTERNAL "")
  if(NOT EXISTS "${AIT_PATH}")
    file(DOWNLOAD "${AIT_URL}" "${AIT_PATH}" SHOW_PROGRESS)
    execute_process(COMMAND chmod +x "${AIT_PATH}")
  endif()

  # ------------------------------------------------------------
  # AppDir path (paramétrable)
  # ------------------------------------------------------------
  if(DEFINED ARGS_APPDIR AND NOT ARGS_APPDIR STREQUAL "")
    set(APPDIR "${ARGS_APPDIR}")
  else()
    set(APPDIR "${CMAKE_BINARY_DIR}/AppDir")
  endif()

  file(MAKE_DIRECTORY "${APPDIR}")
  file(MAKE_DIRECTORY "${APPDIR}/usr/lib")

  # ------------------------------------------------------------
  # Copy executable to AppDir root
  # ------------------------------------------------------------
  file(COPY "${ARGS_EXE}" DESTINATION "${APPDIR}" FOLLOW_SYMLINK_CHAIN)
  get_filename_component(EXE_NAME "${ARGS_EXE}" NAME)

  # ------------------------------------------------------------
  # Copier toutes les libs .so* qui sont à côté de l'exe (build dir)
  # Exemple :
  #   build/linux/x64/Release/rc2d_example
  #   build/linux/x64/Release/libSDL3.so
  #   build/linux/x64/Release/libonnxruntime.so.1
  #
  # On copie :
  #   *.so
  #   *.so.*
  #   (et donc les symlinks si présents, selon ton build)
  # ------------------------------------------------------------
  get_filename_component(EXE_DIR "${ARGS_EXE}" DIRECTORY)

  # On récupère tous les fichiers .so et .so.* présents à côté de l'exe
  file(GLOB SO_FILES
    "${EXE_DIR}/*.so"
    "${EXE_DIR}/*.so.*"
  )

  if(SO_FILES)
    message(STATUS "make_appimage: copie des .so depuis '${EXE_DIR}' vers '${APPDIR}/usr/lib'")
    foreach(_so IN LISTS SO_FILES)
      # COPIE le fichier (suivra la chaîne de symlinks si besoin)
      file(COPY "${_so}" DESTINATION "${APPDIR}/usr/lib" FOLLOW_SYMLINK_CHAIN)
    endforeach()
  else()
    message(STATUS "make_appimage: aucun .so trouvé à côté de l'exe dans '${EXE_DIR}'")
  endif()

  # ------------------------------------------------------------
  # AppRun : utilise usr/lib pour les .so embarquées
  # ------------------------------------------------------------
  file(WRITE "${APPDIR}/AppRun"
"#!/bin/sh
HERE=$(dirname \"$(readlink -f \"\$0\")\")
export LD_LIBRARY_PATH=\"\$HERE/usr/lib:\$LD_LIBRARY_PATH\"
exec \"\$HERE/${EXE_NAME}\" \"\$@\"
"
  )
  execute_process(COMMAND chmod +x "${APPDIR}/AppRun")

  # ------------------------------------------------------------
  # Copy assets (optionnel)
  # ------------------------------------------------------------
  if(DEFINED ARGS_ASSETS)
    file(COPY ${ARGS_ASSETS} DESTINATION "${APPDIR}")
  endif()

  # ------------------------------------------------------------
  # Icons
  # ------------------------------------------------------------
  file(COPY "${ARGS_DIR_ICON}" DESTINATION "${APPDIR}")
  get_filename_component(THUMB_NAME "${ARGS_DIR_ICON}" NAME)
  file(RENAME "${APPDIR}/${THUMB_NAME}" "${APPDIR}/.DirIcon")

  file(COPY "${ARGS_ICON}" DESTINATION "${APPDIR}")
  get_filename_component(ICON_NAME "${ARGS_ICON}" NAME)
  get_filename_component(ICON_EXT "${ARGS_ICON}" EXT)
  file(RENAME "${APPDIR}/${ICON_NAME}" "${APPDIR}/${ARGS_NAME}${ICON_EXT}")

  # ------------------------------------------------------------
  # Desktop file
  # ------------------------------------------------------------
  file(WRITE "${APPDIR}/${ARGS_NAME}.desktop"
"[Desktop Entry]
Type=Application
Name=${ARGS_NAME}
Exec=AppRun
TryExec=AppRun
Icon=${ARGS_NAME}
Terminal=false
Categories=Game;
StartupWMClass=${ARGS_NAME}
X-AppImage-Name=${ARGS_NAME}
X-AppImage-Version=${ARGS_APP_VERSION}
X-AppImage-Arch=${ARGS_APP_ARCH}
"
  )

  # ------------------------------------------------------------
  # Build AppImage
  # ------------------------------------------------------------
  execute_process(COMMAND "${AIT_PATH}" "${APPDIR}" "${ARGS_OUTPUT_NAME}")
endfunction()
