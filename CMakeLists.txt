cmake_minimum_required(VERSION 3.21)
project(SteamTrailerScreensaver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

option(BUILD_WIN_SCR "Build the Windows .scr screensaver executable" ${WIN32})
option(BUILD_XSCREENSAVER_HACK "Build the Linux xscreensaver hack executable" ${UNIX})
option(BUILD_SETTINGS_GUI "Build the standalone settings GUI executable" ON)
option(BUILD_TESTS "Build unit tests" ON)
option(SSV_STATIC_QT "Link Qt statically (used for win-scr release builds)" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Network Sql Widgets OpenGLWidgets)
find_package(Libmpv REQUIRED)

# vcpkg's qtbase port (our only Qt dependency — qttools is not pulled in)
# does not build windeployqt, so there is no tool to deploy Qt's runtime
# plugins (platform, sqldrivers, tls, ...) next to a GUI executable. Copy
# the subset this project actually needs directly from the vcpkg install
# tree as a post-build step; this is what makes double-clicking the built
# .exe/.scr outside of CTest (which instead just points QT_PLUGIN_PATH at
# this same tree) actually work.
function(ssv_deploy_qt_plugins target)
  if(NOT WIN32)
    return()
  endif()
  set(_plugin_root "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/Qt6/plugins")
  foreach(_category IN ITEMS platforms sqldrivers tls styles imageformats networkinformation)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${_plugin_root}/${_category}" "$<TARGET_FILE_DIR:${target}>/${_category}"
      COMMENT "Deploying Qt ${_category} plugins next to ${target}"
    )
  endforeach()

  # sqldrivers/qsqlite.dll depends on sqlite3.dll, but that dependency is
  # invisible to vcpkg's applocal-deps step (which only walks the *target's
  # own* direct import table, not a plugin loaded dynamically at runtime via
  # QPluginLoader) — so it's never copied automatically. Without it, the
  # QSQLITE driver silently fails to load and every CacheDatabase::open()
  # call fails; since this is a WIN32-subsystem app with no console, that
  # failure is otherwise completely invisible.
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/sqlite3.dll" "$<TARGET_FILE_DIR:${target}>"
    COMMENT "Deploying sqlite3.dll (qsqlite plugin dependency) next to ${target}"
  )
endfunction()

add_subdirectory(core)
add_subdirectory(qtui)

if(BUILD_WIN_SCR AND WIN32)
  add_subdirectory(apps/win-scr)
endif()

if(BUILD_XSCREENSAVER_HACK AND UNIX AND NOT APPLE)
  add_subdirectory(apps/xscreensaver-hack)
endif()

if(BUILD_SETTINGS_GUI)
  add_subdirectory(apps/settings-gui)
endif()

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/core)
endif()
