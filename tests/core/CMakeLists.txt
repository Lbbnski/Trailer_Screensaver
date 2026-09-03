find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_PlaylistEngine test_PlaylistEngine.cpp)
target_link_libraries(test_PlaylistEngine PRIVATE ssvcore Qt6::Test)
add_test(NAME PlaylistEngine COMMAND test_PlaylistEngine)

add_executable(test_Config test_Config.cpp)
target_link_libraries(test_Config PRIVATE ssvcore Qt6::Test)
add_test(NAME Config COMMAND test_Config)

add_executable(test_CacheRepository test_CacheRepository.cpp)
target_link_libraries(test_CacheRepository PRIVATE ssvcore Qt6::Test)
add_test(NAME CacheRepository COMMAND test_CacheRepository)

add_executable(test_IgdbGenreMap test_IgdbGenreMap.cpp)
target_link_libraries(test_IgdbGenreMap PRIVATE ssvcore Qt6::Test)
add_test(NAME IgdbGenreMap COMMAND test_IgdbGenreMap)

add_executable(test_IgdbAgeRatingMap test_IgdbAgeRatingMap.cpp)
target_link_libraries(test_IgdbAgeRatingMap PRIVATE ssvcore Qt6::Test)
add_test(NAME IgdbAgeRatingMap COMMAND test_IgdbAgeRatingMap)

add_executable(test_GogGenreMap test_GogGenreMap.cpp)
target_link_libraries(test_GogGenreMap PRIVATE ssvcore Qt6::Test)
add_test(NAME GogGenreMap COMMAND test_GogGenreMap)

# The QSQLITE driver used by CacheDatabase is a Qt plugin, not linked into
# the binary — it's only found via QT_PLUGIN_PATH (or sitting next to the
# executable, which windeployqt handles for the shipped apps but nothing
# does for these test binaries). Point CTest's runs at vcpkg's Qt6 plugin
# tree directly so the tests find it without needing a deploy step. The
# plugin itself then needs sqlite3.dll on the loader's search path, which
# vcpkg's applocal deployment doesn't copy in (it only follows the test
# binary's own direct link-time dependencies, not a plugin loaded
# dynamically at runtime) — so also prepend vcpkg's bin dir to PATH.
if(WIN32)
  # PATH's own value is semicolon-separated, which collides with the
  # semicolon that separates entries in the ENVIRONMENT list itself -
  # escape it so CTest doesn't mis-split it into bogus environment entries.
  set(_test_path "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin;$ENV{PATH}")
  string(REPLACE ";" "\\;" _test_path_escaped "${_test_path}")
  set_tests_properties(PlaylistEngine Config CacheRepository PROPERTIES
    ENVIRONMENT "QT_PLUGIN_PATH=${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/Qt6/plugins;PATH=${_test_path_escaped}"
  )
endif()
