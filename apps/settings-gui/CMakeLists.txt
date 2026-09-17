add_executable(steam-trailer-saver-settings src/main.cpp)

target_link_libraries(steam-trailer-saver-settings PRIVATE
  ssvsettingsui
  Qt6::Widgets
)

ssv_deploy_qt_plugins(steam-trailer-saver-settings)

# ssvsettingsui includes MpvGLWidget (used by the xscreensaver-hack/.scr
# preview embedding, not by this standalone settings launcher itself), which
# links libmpv — so this target needs the same libmpv runtime DLL next to it
# as win-scr does, or it fails to even start with a missing-DLL system error
# dialog. Same gap, same fix as apps/win-scr/CMakeLists.txt's identical
# block; libmpv is vendored, not vcpkg-managed, so nothing copies this
# automatically.
if(Libmpv_RUNTIME)
  add_custom_command(TARGET steam-trailer-saver-settings POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${Libmpv_RUNTIME}" "$<TARGET_FILE_DIR:steam-trailer-saver-settings>"
    COMMENT "Deploying libmpv runtime next to steam-trailer-saver-settings"
  )
endif()
