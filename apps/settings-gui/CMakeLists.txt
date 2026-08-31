add_executable(steam-trailer-saver-settings src/main.cpp)

target_link_libraries(steam-trailer-saver-settings PRIVATE
  ssvsettingsui
  Qt6::Widgets
)

ssv_deploy_qt_plugins(steam-trailer-saver-settings)
