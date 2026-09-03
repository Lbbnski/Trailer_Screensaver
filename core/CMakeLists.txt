add_library(ssvcore STATIC
  src/config/Config.h
  src/config/Config.cpp
  src/config/ConfigPaths.h
  src/config/ConfigPaths.cpp

  src/sources/SourceTypes.h
  src/sources/IMetadataSource.h
  src/sources/GenreTaxonomy.h
  src/sources/GenreTaxonomy.cpp
  src/sources/SourceRegistry.h
  src/sources/SourceRegistry.cpp

  src/sources/steam/SteamGenreMap.h
  src/sources/steam/SteamGenreMap.cpp
  src/sources/steam/SteamAgeGate.h
  src/sources/steam/SteamAgeGate.cpp
  src/sources/steam/SteamAppDetailsClient.h
  src/sources/steam/SteamAppDetailsClient.cpp
  src/sources/steam/SteamGenreCandidateFinder.h
  src/sources/steam/SteamGenreCandidateFinder.cpp
  src/sources/steam/YoutubeFallbackResolver.h
  src/sources/steam/YoutubeFallbackResolver.cpp
  src/sources/steam/SteamTrailerSource.h
  src/sources/steam/SteamTrailerSource.cpp

  src/sources/igdb/IgdbAuthClient.h
  src/sources/igdb/IgdbAuthClient.cpp
  src/sources/igdb/IgdbClient.h
  src/sources/igdb/IgdbClient.cpp
  src/sources/igdb/IgdbGenreMap.h
  src/sources/igdb/IgdbGenreMap.cpp
  src/sources/igdb/IgdbAgeRatingMap.h
  src/sources/igdb/IgdbAgeRatingMap.cpp
  src/sources/igdb/IgdbCandidateFinder.h
  src/sources/igdb/IgdbCandidateFinder.cpp
  src/sources/igdb/IgdbTrailerSource.h
  src/sources/igdb/IgdbTrailerSource.cpp

  src/sources/gog/GogGenreMap.h
  src/sources/gog/GogGenreMap.cpp
  src/sources/gog/GogCandidateFinder.h
  src/sources/gog/GogCandidateFinder.cpp
  src/sources/gog/GogTrailerSource.h
  src/sources/gog/GogTrailerSource.cpp

  src/cache/CacheDatabase.h
  src/cache/CacheDatabase.cpp
  src/cache/CacheRepository.h
  src/cache/CacheRepository.cpp

  src/trailer/TrailerResolver.h
  src/trailer/TrailerResolver.cpp

  src/selection/PlaylistEngine.h
  src/selection/PlaylistEngine.cpp

  src/playback/MpvPlayer.h
  src/playback/MpvPlayer.cpp
  src/playback/MpvGlRenderer.h
  src/playback/MpvGlRenderer.cpp

  src/util/ProcessRunner.h
  src/util/ProcessRunner.cpp
  src/util/Logging.h
  src/util/Logging.cpp
  src/util/NetworkAwait.h
)

target_include_directories(ssvcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(ssvcore PUBLIC
  Qt6::Core
  Qt6::Network
  Qt6::Sql
  Libmpv::Libmpv
)

if(WIN32)
  target_compile_definitions(ssvcore PUBLIC SSV_PLATFORM_WINDOWS)
else()
  target_compile_definitions(ssvcore PUBLIC SSV_PLATFORM_LINUX)
endif()
