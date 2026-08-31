#[[
Locates libmpv.

On Linux this delegates to pkg-config against the distro's libmpv-dev package.
On Windows there is no reliable vcpkg/package-manager port, so we expect a
prebuilt shared library + import lib + headers vendored under
third_party/mpv/win64/{include,lib,bin}. See docs/ARCHITECTURE.md for where
to obtain a compatible build (the community "mpv-dev" shared-library
releases at https://sourceforge.net/projects/mpv-player-windows/files/libmpv/).

Defines, on success:
  Libmpv_FOUND
  Libmpv::Libmpv  (imported target: include dirs + link library)
]]

include(FindPackageHandleStandardArgs)

if(UNIX AND NOT APPLE)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(PC_MPV QUIET mpv)
  endif()

  find_path(Libmpv_INCLUDE_DIR
    NAMES mpv/client.h
    HINTS ${PC_MPV_INCLUDE_DIRS}
  )
  find_library(Libmpv_LIBRARY
    NAMES mpv
    HINTS ${PC_MPV_LIBRARY_DIRS}
  )
else()
  set(SSV_MPV_ROOT "${CMAKE_SOURCE_DIR}/third_party/mpv/win64" CACHE PATH "Vendored libmpv root (Windows)")

  find_path(Libmpv_INCLUDE_DIR
    NAMES mpv/client.h
    HINTS "${SSV_MPV_ROOT}/include"
  )
  find_library(Libmpv_LIBRARY
    NAMES mpv mpv.dll libmpv.dll.a
    HINTS "${SSV_MPV_ROOT}/lib"
  )
  find_file(Libmpv_RUNTIME
    NAMES libmpv-2.dll mpv-2.dll mpv-1.dll
    HINTS "${SSV_MPV_ROOT}/bin"
  )
endif()

find_package_handle_standard_args(Libmpv
  REQUIRED_VARS Libmpv_LIBRARY Libmpv_INCLUDE_DIR
)

if(Libmpv_FOUND AND NOT TARGET Libmpv::Libmpv)
  add_library(Libmpv::Libmpv UNKNOWN IMPORTED)
  set_target_properties(Libmpv::Libmpv PROPERTIES
    IMPORTED_LOCATION "${Libmpv_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Libmpv_INCLUDE_DIR}"
  )
  if(Libmpv_RUNTIME)
    set_target_properties(Libmpv::Libmpv PROPERTIES IMPORTED_RUNTIME "${Libmpv_RUNTIME}")
  endif()
endif()

mark_as_advanced(Libmpv_INCLUDE_DIR Libmpv_LIBRARY Libmpv_RUNTIME)
