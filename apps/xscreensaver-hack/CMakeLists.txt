add_executable(steam-trailer-saver
  src/main.cpp
  src/HackArgParser.h
  src/HackArgParser.cpp
)

target_link_libraries(steam-trailer-saver PRIVATE
  ssvcore
  ssvsettingsui
  Qt6::Widgets
)

target_include_directories(steam-trailer-saver PRIVATE
  ${CMAKE_SOURCE_DIR}/qtui/src
)

install(TARGETS steam-trailer-saver RUNTIME DESTINATION lib/xscreensaver)
install(FILES config/steam-trailer-saver.xml DESTINATION share/xscreensaver/config)
