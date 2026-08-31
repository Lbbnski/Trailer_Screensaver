add_library(ssvsettingsui STATIC
  src/MpvGLWidget.h
  src/MpvGLWidget.cpp
  src/EmbeddedForeignWindow.h
  src/EmbeddedForeignWindow.cpp
  src/DebugOverlay.h
  src/DebugOverlay.cpp
  src/GenreListModel.h
  src/GenreListModel.cpp
  src/SettingsDialog.h
  src/SettingsDialog.cpp
  src/PlaybackSession.h
  src/PlaybackSession.cpp
)

target_include_directories(ssvsettingsui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(ssvsettingsui PUBLIC
  ssvcore
  Qt6::Widgets
  Qt6::OpenGLWidgets
)
