set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_PROVIDED_FORTRAN ON)

# CMake's AUTOMOC/cmake_autogen spawns one moc.exe subprocess per logical CPU
# by default, independent of the build tool's own -j/--parallel setting.
set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DCMAKE_AUTOGEN_PARALLEL=2")

# moc.exe reproducibly crashes with an access violation (0xC0000005) only
# inside the Debug (x64-windows-dbg) build of qtbase on this machine; a
# standalone invocation of the same command succeeds. Third-party
# dependencies don't need debug binaries here, so skip Debug entirely -
# this sidesteps the crash and halves the vcpkg build time.
set(VCPKG_BUILD_TYPE release)
