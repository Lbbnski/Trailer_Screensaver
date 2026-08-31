# Windows-only. WIN32 marks this a GUI (not console) subsystem executable —
# required for a .scr, which Windows launches with no attached console.
add_executable(SteamTrailerSaver WIN32
  src/main.cpp
  src/ScrArgParser.h
  src/ScrArgParser.cpp
  src/FullscreenController.h
  src/FullscreenController.cpp
  src/FullscreenWindow.h
  src/FullscreenWindow.cpp
  src/InputExitWatcher.h
  src/InputExitWatcher.cpp
  src/PreviewEmbedWindow.h
  src/PreviewEmbedWindow.cpp
  resources/app.rc
)

# Windows' Display Settings screensaver dropdown only scans top-level
# *.scr files — the build output itself must carry that extension, not
# just get renamed at install time, so the installer can enumerate it
# straight from the build/publish directory too.
set_target_properties(SteamTrailerSaver PROPERTIES SUFFIX ".scr")

# resources/app.rc already embeds app.manifest as resource id 1 (see its
# comment on why: per-monitor-v2 DPI awareness without an external
# .manifest file). MSVC's linker generates and embeds its own manifest
# resource by default, which collides with that one at link time
# (CVT1100: duplicate MANIFEST resource) — disable the automatic one.
target_link_options(SteamTrailerSaver PRIVATE /MANIFEST:NO)

target_link_libraries(SteamTrailerSaver PRIVATE
  ssvcore
  ssvsettingsui
  Qt6::Widgets
)

target_include_directories(SteamTrailerSaver PRIVATE
  ${CMAKE_SOURCE_DIR}/qtui/src
)

# See docs/ARCHITECTURE.md's "Build & packaging" section: release builds of
# this target are expected to link Qt statically (SSV_STATIC_QT, set via a
# static vcpkg triplet such as x64-windows-static) since everything this
# .scr depends on must be resolvable from System32 once installed — a full
# dynamic Qt6 DLL set dropped there is unnecessary clutter/collision risk.
# Dev/iteration builds can leave SSV_STATIC_QT off and use windeployqt.
if(NOT SSV_STATIC_QT)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
  if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET SteamTrailerSaver POST_BUILD
      COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations "$<TARGET_FILE:SteamTrailerSaver>"
      COMMENT "Deploying Qt runtime dependencies next to the dev build"
    )
  else()
    # vcpkg's qtbase port doesn't build windeployqt (that's qttools, not a
    # dependency here), so fall back to copying the plugin subset this app
    # actually needs directly from the vcpkg install tree.
    ssv_deploy_qt_plugins(SteamTrailerSaver)
  endif()
endif()

# libmpv is vendored, not vcpkg-managed, so vcpkg's applocal-deps step never
# copies its runtime DLL next to the build output the way it does for every
# vcpkg-tracked dependency (Qt, sqlite3, ...) — do it explicitly.
if(Libmpv_RUNTIME)
  add_custom_command(TARGET SteamTrailerSaver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${Libmpv_RUNTIME}" "$<TARGET_FILE_DIR:SteamTrailerSaver>"
    COMMENT "Deploying libmpv runtime next to SteamTrailerSaver"
  )
endif()
